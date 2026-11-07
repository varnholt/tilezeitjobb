#include "core/nes.hpp"
#include "core/tools.hpp"
#include "mcp/server.hpp"
#include <algorithm>
#include <stdexcept>

namespace tzj::mcp {
namespace {
Json text(const Json &value) {
    return {{"content", {{{"type", "text"}, {"text", value.dump()}}}},
            {"structuredContent", value},
            {"isError", false}};
}
Json property(const char *type, const char *description) {
    return {{"type", type}, {"description", description}};
}
Json integer(int low, int high, const char *description) {
    auto p = property("integer", description);
    p["minimum"] = low;
    p["maximum"] = high;
    return p;
}
int number(const Json &a, const char *key, int low, int high, int fallback) {
    if (!a.contains(key))
        return fallback;
    if (!a[key].is_number_integer())
        throw std::runtime_error(std::string(key) + " must be an integer");
    auto n = a[key].get<std::int64_t>();
    if (n < low || n > high)
        throw std::runtime_error(std::string(key) + " outside its allowed range");
    return int(n);
}
std::string name_argument(const Json &a, const std::string &fallback) {
    auto name = a.value("name", fallback);
    if (name.empty() || name.size() > 128)
        throw std::runtime_error("Name must contain 1..128 UTF-8 bytes");
    return name;
}
Json describe(const Project &p, std::size_t frame) {
    const auto &f = p.frames.at(frame);
    Json layers = Json::array(), sprites = Json::array();
    for (std::size_t i = 0; i < f.layers.size(); ++i) {
        const auto &l = f.layers[i];
        layers.push_back({{"index", i},
                          {"name", l.name},
                          {"kind", l.kind == LayerKind::Paint ? "paint" : "image"},
                          {"visible", l.visible},
                          {"locked", l.locked},
                          {"reference_only", l.reference_only},
                          {"guide_opacity", l.guide_opacity},
                          {"dither", int(l.modifiers.dither)},
                          {"strength", l.modifiers.strength}});
    }
    for (std::size_t i = 0; i < f.sprites.size(); ++i) {
        const auto &s = f.sprites[i];
        sprites.push_back({{"index", i},
                           {"name", s.name},
                           {"x", s.x},
                           {"y", s.y},
                           {"palette", s.palette},
                           {"visible", s.visible},
                           {"locked", s.locked}});
    }
    return {{"width", p.width},
            {"height", p.height},
            {"frame", frame},
            {"frame_count", p.frames.size()},
            {"active_layer", f.active_layer},
            {"layers", layers},
            {"sprites", sprites},
            {"background_palettes", p.background},
            {"sprite_palettes", p.sprite_palettes},
            {"background_visible", f.background_visible},
            {"background_locked", f.background_locked},
            {"sprites_visible", f.sprites_visible},
            {"sprites_locked", f.sprites_locked}};
}
std::string base64(const std::vector<Byte> &data) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        unsigned v = unsigned(data[i]) << 16;
        if (i + 1 < data.size())
            v |= unsigned(data[i + 1]) << 8;
        if (i + 2 < data.size())
            v |= data[i + 2];
        out += alphabet[(v >> 18) & 63];
        out += alphabet[(v >> 12) & 63];
        out += i + 1 < data.size() ? alphabet[(v >> 6) & 63] : '=';
        out += i + 2 < data.size() ? alphabet[v & 63] : '=';
    }
    return out;
}
} // namespace
Json tools() {
    Json list = Json::array();
    auto add = [&](const char *name, const char *description, Json properties, Json required,
                   bool read) {
        list.push_back(
            {{"name", name},
             {"description", description},
             {"inputSchema",
              {{"type", "object"},
               {"properties", properties},
               {"required", required},
               {"additionalProperties", false}}},
             {"annotations",
              {{"readOnlyHint", read}, {"destructiveHint", !read}, {"openWorldHint", false}}}});
    };
    const auto frame =
        integer(0, 255, "Zero-based frame; defaults to the frame currently displayed.");
    const auto layer =
        integer(0, 63, "Background-layer index; get_project lists indices bottom to top.");
    const auto name = property("string", "Layer or sprite name (1..128 UTF-8 bytes).");
    const auto path = property("string", "Absolute local file path.");
    const auto visible =
        property("boolean", "Whether this layer contributes to preview and export.");
    const auto locked = property("boolean", "Prevent changes to this layer.");
    const auto required = Json::array();
    add("get_project", "Inspect frames, bottom-to-top layers, sprites, locks and NES palettes.",
        {{"frame", frame}}, required, true);
    add("render_preview", "Return a PNG image of the composed frame, including sprites.",
        {{"frame", frame},
         {"hardware_limits", property("boolean", "Apply NES sprite visibility limits.")}},
        required, true);
    add("validate_nes",
        "Report NES hardware errors and, when valid, exported tile and sprite counts.",
        {{"frame", frame}}, required, true);
    add("create_layer",
        "Add a transparent paint layer or import a local image as a modifier layer. One undo step.",
        {{"frame", frame}, {"name", name}, {"image_path", path}}, required, false);
    add("duplicate_layer",
        "Duplicate a layer; paintable=true bakes the current image conversion into independent "
        "pixels. One undo step.",
        {{"frame", frame},
         {"layer", layer},
         {"paintable", property("boolean", "Bake image modifiers into an editable paint copy.")}},
        {"layer"}, false);
    add("set_layer", "Set a layer name, visibility or lock. Unlock before renaming. One undo step.",
        {{"frame", frame},
         {"layer", layer},
         {"name", name},
         {"visible", visible},
         {"locked", locked}},
        {"layer"}, false);
    add("set_modifiers",
        "Change an unlocked image layer's conversion. Dither: 0 none, 1 Bayer2, 2 Bayer4, 3 "
        "Bayer8, 4 Floyd-Steinberg, 5 Atkinson. One undo step.",
        {{"frame", frame},
         {"layer", layer},
         {"dither", integer(0, 5, "Dithering method.")},
         {"strength", integer(0, 100, "Dithering strength.")},
         {"brightness", integer(-100, 100, "Brightness adjustment.")},
         {"contrast", integer(-100, 100, "Contrast adjustment.")},
         {"effects_enabled", property("boolean", "Enable the layer's modifiers.")}},
        {"layer"}, false);
    add("draw_pixels",
        "Paint up to 65536 [x,y,slot] triples into a visible, unlocked paint layer. Slots 0..3; "
        "255 erases to transparency. All pixels form one undo step.",
        {{"frame", frame},
         {"layer", layer},
         {"pixels",
          {{"type", "array"},
           {"minItems", 1},
           {"maxItems", 65536},
           {"items",
            {{"type", "array"},
             {"minItems", 3},
             {"maxItems", 3},
             {"items", integer(0, 4095, "Coordinate or slot; slot is 0..3 or 255.")}}}}}},
        {"layer", "pixels"}, false);
    add("add_sprite",
        "Add an 8x8 sprite with 64 row-major slots (0 transparent, 1..3 colors). One undo step.",
        {{"frame", frame},
         {"name", name},
         {"x", integer(0, 255, "NES screen X.")},
         {"y", integer(1, 239, "NES screen Y.")},
         {"palette", integer(0, 3, "Sprite subpalette.")},
         {"pixels",
          {{"type", "array"},
           {"minItems", 64},
           {"maxItems", 64},
           {"items", integer(0, 3, "Color slot.")}}}},
        {"x", "y", "pixels"}, false);
    add("undo", "Undo the last editor or MCP edit.", Json::object(), required, false);
    add("redo", "Redo the last undone edit.", Json::object(), required, false);
    add("export_nes",
        "Validate and export .nes/.chr/.nam/.pal/.oam/.h files. Output directory must exist; "
        "existing output files are replaced.",
        {{"frame", frame}, {"prefix", path}}, {"prefix"}, false);
    return list;
}
Json execute(Project &project, History &history, std::size_t &current, bool &edited,
             const std::string &name, const Json &a) {
    edited = false;
    if (!a.is_object())
        throw std::runtime_error("Arguments must be an object");
    // Check the advertised contract before touching project state or writing files.
    const auto catalog = tools();
    auto spec = std::find_if(catalog.begin(), catalog.end(),
                             [&](const Json &t) { return t["name"] == name; });
    if (spec == catalog.end())
        throw std::runtime_error("Unknown tool: " + name);
    const auto &schema = (*spec)["inputSchema"];
    for (const auto &key : schema["required"])
        if (!a.contains(key.get<std::string>()))
            throw std::runtime_error("Missing argument: " + key.get<std::string>());
    for (const auto &[key, value] : a.items()) {
        if (!schema["properties"].contains(key))
            throw std::runtime_error("Unknown argument: " + key);
        auto type = schema["properties"][key]["type"];
        if ((type == "string" && !value.is_string()) ||
            (type == "boolean" && !value.is_boolean()) ||
            (type == "integer" && !value.is_number_integer()) ||
            (type == "array" && !value.is_array()))
            throw std::runtime_error("Wrong argument type: " + key);
    }
    const auto frame =
        std::size_t(number(a, "frame", 0, int(project.frames.size()) - 1, int(current)));
    if (name == "get_project")
        return text(describe(project, frame));
    if (name == "render_preview") {
        Json image{{"type", "image"},
                   {"mimeType", "image/png"},
                   {"data", base64(encode_png(
                                project.render(frame, true, a.value("hardware_limits", false))))}};
        return {{"content", Json::array({image})}, {"isError", false}};
    }
    if (name == "validate_nes") {
        auto errors = hardware_errors(project, frame);
        Json result{{"valid", errors.empty()}, {"errors", errors}};
        if (errors.empty()) {
            const auto scene = compile_scene(project, frame);
            result["background_tiles"] = scene.background_tiles;
            result["sprite_tiles"] = scene.sprite_tiles;
            result["sprite_count"] = scene.sprite_count;
        }
        return text(result);
    }
    if (name == "export_nes") {
        auto prefix = path_from_utf8(a.at("prefix").get<std::string>());
        if (!prefix.is_absolute() || prefix.filename().empty())
            throw std::runtime_error("Export prefix must be an absolute file prefix");
        export_scene(prefix, project, frame);
        return text({{"prefix", path_to_utf8(prefix)},
                     {"extensions", {".nes", ".chr", ".nam", ".pal", ".oam", ".h"}}});
    }
    if (name == "undo" || name == "redo") {
        edited = name == "undo" ? history.undo(project) : history.redo(project);
        current = std::min(current, project.frames.size() - 1);
        return text({{"changed", edited}, {"frame", current}});
    }
    // Transactional edits: a failed operation changes neither project nor history.
    Project next = project;
    next.ensure_layers(frame);
    auto &f = next.frames.at(frame);
    Json result;
    if (name == "create_layer") {
        if (f.background_locked)
            throw std::runtime_error("Unlock the background group first");
        std::size_t index;
        if (a.contains("image_path")) {
            auto path = path_from_utf8(a.at("image_path").get<std::string>());
            if (!path.is_absolute())
                throw std::runtime_error("Image path must be absolute");
            index = next.add_image_layer(frame, load_image(path),
                                         name_argument(a, path_to_utf8(path.stem())),
                                         path_to_utf8(path));
        } else
            index = next.add_paint_layer(frame, name_argument(a, "Paint layer"));
        result = {{"layer", index}};
    } else if (name == "add_sprite") {
        if (f.sprites_locked || f.sprites.size() >= 64)
            throw std::runtime_error("Unlock the sprite group and keep at most 64 sprites");
        const auto &pixels = a.at("pixels");
        if (pixels.size() != 64)
            throw std::runtime_error("A sprite needs exactly 64 slots");
        Sprite s;
        s.name = name_argument(a, "Sprite");
        s.x = number(a, "x", 0, 255, 0);
        s.y = number(a, "y", 1, 239, 1);
        s.palette = number(a, "palette", 0, 3, 0);
        for (std::size_t i = 0; i < 64; ++i) {
            if (!pixels[i].is_number_integer() || pixels[i] < 0 || pixels[i] > 3)
                throw std::runtime_error("Sprite slots must be 0..3");
            s.pixels[i] = pixels[i].get<Byte>();
        }
        f.sprites.insert(f.sprites.begin(), std::move(s));
        result = {{"sprite", 0}};
    } else {
        const auto index = number(a, "layer", 0, int(f.layers.size()) - 1, -1);
        if (index < 0)
            throw std::runtime_error("Choose a background layer");
        auto &l = f.layers.at(index);
        if (name == "set_layer") {
            if (a.contains("name")) {
                if (l.locked || f.background_locked)
                    throw std::runtime_error(
                        "Unlock the layer and background group before renaming");
                l.name = name_argument(a, l.name);
            }
            if (a.contains("visible"))
                l.visible = a.at("visible").get<bool>();
            if (a.contains("locked"))
                l.locked = a.at("locked").get<bool>();
        } else if (name == "duplicate_layer") {
            if (f.background_locked)
                throw std::runtime_error("Unlock the background group first");
            result = {{"layer", next.duplicate_layer(frame, index, a.value("paintable", false))}};
        } else if (name == "set_modifiers") {
            if (l.locked || f.background_locked || l.kind != LayerKind::Image)
                throw std::runtime_error("Choose an unlocked image layer in an unlocked group");
            auto &m = l.modifiers;
            m.dither = DitherMode(number(a, "dither", 0, 5, int(m.dither)));
            m.strength = number(a, "strength", 0, 100, m.strength);
            m.brightness = number(a, "brightness", -100, 100, m.brightness);
            m.contrast = number(a, "contrast", -100, 100, m.contrast);
            if (a.contains("effects_enabled"))
                l.effects_enabled = a.at("effects_enabled").get<bool>();
            l.conversion_stamp = 0;
        } else if (name == "draw_pixels") {
            f.active_layer = index;
            if (!next.paint_layer_editable(frame))
                throw std::runtime_error(
                    "Choose a visible, unlocked paint layer and show/unlock its background group");
            const auto &pixels = a.at("pixels");
            if (pixels.empty() || pixels.size() > 65536)
                throw std::runtime_error("Supply 1..65536 pixels");
            for (const auto &p : pixels) {
                if (!p.is_array() || p.size() != 3 || !p[0].is_number_integer() ||
                    !p[1].is_number_integer() || !p[2].is_number_integer())
                    throw std::runtime_error("Each pixel must be [x,y,slot]");
                if (p[0] < 0 || p[0] >= next.width || p[1] < 0 || p[1] >= next.height ||
                    (p[2] != 255 && (p[2] < 0 || p[2] > 3)))
                    throw std::runtime_error(
                        "Pixel coordinates or color slot outside their allowed range");
                next.write_background_pixel(
                    frame, std::size_t(p[1].get<int>() * next.width + p[0].get<int>()),
                    p[2].get<Byte>());
            }
        }
    }
    next.compose_layers(frame);
    if (next != project) {
        history.checkpoint(project);
        project = std::move(next);
        current = frame;
        edited = true;
    }
    if (result.is_null())
        result = Json::object();
    result["changed"] = edited;
    result["frame"] = frame;
    return text(result);
}
Json Session::receive(const Json &request, const Handler &handler) {
    Json id = request.is_object() ? request.value("id", Json()) : Json();
    auto error = [&](int code, const std::string &message) -> Json {
        return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
    };
    if (!request.is_object() || request.value("jsonrpc", Json()) != "2.0" ||
        !request.contains("method") || !request["method"].is_string())
        return error(-32600, "Invalid JSON-RPC request");
    if (request.contains("id") && !id.is_string() && !id.is_number_integer())
        return error(-32600, "Invalid request ID");
    const auto method = request["method"].get<std::string>();
    if (!request.contains("id")) {
        if (method == "notifications/initialized" && initialized_)
            ready_ = true;
        return nullptr;
    }
    Json result;
    if (method == "initialize") {
        if (initialized_)
            return error(-32600, "Already initialized");
        const auto params = request.value("params", Json::object());
        if (!params.is_object() || !params.contains("protocolVersion") ||
            !params["protocolVersion"].is_string() || !params.contains("capabilities") ||
            !params["capabilities"].is_object() || !params.contains("clientInfo") ||
            !params["clientInfo"].is_object())
            return error(-32602, "Expected protocolVersion, capabilities and clientInfo");
        auto version = params["protocolVersion"].get<std::string>();
        if (version != "2024-11-05" && version != "2025-03-26" && version != "2025-06-18" &&
            version != "2025-11-25")
            version = "2025-11-25";
        result = {{"protocolVersion", version},
                  {"capabilities", {{"tools", Json::object()}}},
                  {"serverInfo", {{"name", "tilezeitjobb"}, {"version", "2.0.0"}}},
                  {"instructions",
                   "Controls the open desktop project. Indices are zero based; background layers "
                   "run bottom to top. Get project state before editing. Each successful edit is "
                   "undoable in the editor. Export writes explicit local output paths."}};
        initialized_ = true;
    } else if (method == "ping")
        result = Json::object();
    else if (!ready_)
        return error(-32000, "Initialize the session first");
    else if (method == "tools/list")
        result = {{"tools", tools()}};
    else if (method == "tools/call") {
        auto params = request.value("params", Json::object());
        if (!params.is_object() || !params.contains("name") || !params["name"].is_string())
            return error(-32602, "Expected tool name");
        const auto catalog = tools();
        if (std::none_of(catalog.begin(), catalog.end(),
                         [&](const Json &tool) { return tool["name"] == params["name"]; }))
            return error(-32602, "Unknown tool");
        try {
            result = handler(params["name"].get<std::string>(),
                             params.value("arguments", Json::object()));
        } catch (const std::exception &e) {
            result = {{"content", {{{"type", "text"}, {"text", e.what()}}}}, {"isError", true}};
        }
    } else
        return error(-32601, "Method not found");
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}
} // namespace tzj::mcp
