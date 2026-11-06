#include "core/nes.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
using namespace tzj;
static void help() {
    std::cout
        << "tilezeitjobb-cli\n  demo PREFIX\n  compile PROJECT.tzj PREFIX [--frame N]\n  import "
           "IMAGE PROJECT.tzj [--auto-palettes] [--layout horizontal|vertical]\n  inspect "
           "PROJECT.tzj\n  batch INPUT_DIR OUTPUT_DIR [--crop X Y WIDTH HEIGHT] [--palette "
           "FILE.pal]\nFrame numbers are zero based; crop coordinates and size are in 8x8 tiles.\n";
}
static int integer(const char *value) {
    std::size_t n = 0;
    int i = std::stoi(value, &n);
    if (value[n])
        throw std::runtime_error("Invalid integer");
    return i;
}
int main(int argc, char **argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help") {
            help();
            return 0;
        }
        const std::string command = argv[1];
        if (command == "demo" && argc == 3) {
            auto p = demo_project();
            fs::path prefix = argv[2];
            save_project(prefix.string() + ".tzj", p);
            save_png(prefix.string() + ".png", p.render());
            export_scene(prefix, p);
            std::cout << "Created demo project, PNG, NES ROM and raw assets.\n";
            return 0;
        }
        if (command == "compile" && (argc == 4 || argc == 6)) {
            std::size_t frame = 0;
            if (argc == 6) {
                if (std::string(argv[4]) != "--frame" || integer(argv[5]) < 0)
                    throw std::runtime_error("Expected --frame N");
                frame = static_cast<std::size_t>(integer(argv[5]));
            }
            auto p = load_project(argv[2]);
            auto scene = compile_scene(p, frame);
            export_scene(argv[3], p, frame);
            std::cout << "Exported NROM-128: " << scene.background_tiles << " background tiles, "
                      << scene.sprite_tiles << " sprite tiles, " << scene.sprite_count
                      << " sprites.\n";
            return 0;
        }
        if (command == "import" && argc >= 4) {
            Project p;
            p.reference = load_image(argv[2]);
            p.image_path = argv[2];
            bool automatic = false;
            for (int i = 4; i < argc; ++i) {
                std::string flag = argv[i];
                if (flag == "--auto-palettes")
                    automatic = true;
                else if (flag == "--layout" && i + 1 < argc) {
                    std::string layout = argv[++i];
                    if (layout == "horizontal")
                        p.resize(512, 240);
                    else if (layout == "vertical")
                        p.resize(256, 480);
                    else
                        throw std::runtime_error("Unknown layout");
                } else
                    throw std::runtime_error("Unknown import option: " + flag);
            }
            p.quantize(0, automatic);
            save_project(argv[3], p);
            std::cout << "Imported image.\n";
            return 0;
        }
        if (command == "inspect" && argc == 3) {
            auto p = load_project(argv[2]);
            std::cout << p.width << "x" << p.height << ", " << p.frames.size() << " frame(s)\n";
            for (std::size_t i = 0; i < p.frames.size(); ++i) {
                std::cout << "Frame " << i << ": ";
                try {
                    auto s = compile_scene(p, i);
                    std::cout << s.background_tiles << " BG tiles, " << s.sprite_tiles
                              << " sprite tiles, " << s.sprite_count
                              << " sprites, hardware valid\n";
                } catch (const std::exception &e) {
                    std::cout << e.what() << "\n";
                }
            }
            return 0;
        }
        if (command == "batch" && argc >= 4) {
            int cx = 0, cy = 0, cw = -1, ch = -1;
            Palette palette{15, 1, 33, 48};
            for (int i = 4; i < argc; ++i) {
                std::string flag = argv[i];
                if (flag == "--crop" && i + 4 < argc) {
                    cx = integer(argv[++i]);
                    cy = integer(argv[++i]);
                    cw = integer(argv[++i]);
                    ch = integer(argv[++i]);
                    if (cx < 0 || cy < 0 || cw < 1 || ch < 1)
                        throw std::runtime_error("Invalid crop");
                } else if (flag == "--palette" && i + 1 < argc) {
                    std::ifstream in(argv[++i], std::ios::binary);
                    if (!in.read(reinterpret_cast<char *>(palette.data()), 4) || in.peek() != EOF)
                        throw std::runtime_error("Batch palette must contain four NES color bytes");
                    for (auto c : palette)
                        if (c > 63)
                            throw std::runtime_error("Invalid palette color");
                } else
                    throw std::runtime_error("Unknown batch option");
            }
            fs::create_directories(argv[3]);
            std::vector<fs::path> files;
            for (const auto &e : fs::directory_iterator(argv[2])) {
                auto ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (e.is_regular_file() && (ext == ".bmp" || ext == ".png"))
                    files.push_back(e.path());
            }
            std::sort(files.begin(), files.end());
            if (files.empty())
                throw std::runtime_error("No BMP or PNG images found");
            for (const auto &file : files) {
                auto image = load_image(file);
                int w = cw < 0 ? image.width / 8 : cw, h = ch < 0 ? image.height / 8 : ch;
                if (w < 1 || h < 1 || (static_cast<long long>(cx) + w) * 8 > image.width ||
                    (static_cast<long long>(cy) + h) * 8 > image.height)
                    throw std::runtime_error("Crop outside image: " + file.string());
                std::vector<Byte> data;
                int bank = 0;
                auto flush = [&] {
                    data.resize(4096);
                    write_binary(fs::path(argv[3]) /
                                     (file.stem().string() + "_" + std::to_string(bank++) + ".chr"),
                                 data);
                    data.clear();
                };
                for (int y = cy; y < cy + h; ++y)
                    for (int x = cx; x < cx + w; ++x) {
                        Tile tile{};
                        for (int py = 0; py < 8; ++py)
                            for (int px = 0; px < 8; ++px)
                                tile[py * 8 + px] = nearest_slot(image.at(x * 8 + px, y * 8 + py),
                                                                 palette, px, py, false);
                        auto bytes = encode_tile(tile);
                        data.insert(data.end(), bytes.begin(), bytes.end());
                        if (data.size() == 4096)
                            flush();
                    }
                if (!data.empty())
                    flush();
                std::cout << file.filename().string() << ": " << bank << " CHR bank(s)\n";
            }
            return 0;
        }
        help();
        throw std::runtime_error("Invalid command or argument count");
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
