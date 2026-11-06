# tilezeitjobb

A NES pixel and tile studio in C++20, using Dear ImGui, GLFW and OpenGL 3.3.
Qt, qmake, generated forms and the old singleton UI/model have been removed.

## Build and run

Windows: install CMake 3.24+ and Visual Studio 2022 with Desktop development with C++.
The first configuration downloads pinned dependencies from their official repositories.

```powershell
cmake --preset windows
cmake --build --preset windows --parallel 1
ctest --preset windows
.\build\Release\tilezeitjobb.exe
```

On Linux, install a C++20 compiler, CMake, OpenGL development libraries and GLFW's
X11/Wayland development prerequisites, then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/tilezeitjobb
```

`-DTZJ_BUILD_EDITOR=OFF` builds just the portable conversion library, CLI and tests.
`cmake --install build --config Release --prefix package` makes a runnable package
with the original palette-test assets. The editor requires an OpenGL 3.3 driver.

## Editing

The editor starts maximized. **F11** or **View > Fullscreen** switches fullscreen
on and off; `--fullscreen` starts directly in fullscreen. The previous window size
and maximized state are restored when leaving fullscreen.

Choose a task above the canvas: **Draw pixels**, **Build with tiles**, **Place
sprites**, **Animate**, or **Export to NES**. Each task shows its relevant panels
and a short instruction. **View > Advanced panels** exposes the entire workspace.
Click a color swatch to choose paint; right-click it to edit the NES color.
Drawing and filling assign the chosen color set to touched 16x16 background blocks,
so the painted pixel matches the swatch. Other pixels in that same block share its
color set, as required by NES hardware.


The dockable workspace includes the canvas, tools, background and sprite palettes,
objects, sprite-sheet picker, preview, attribute grid, hardware report and timeline.

- Import PNG/BMP/JPEG/TGA images or drop a background image onto the window.
- Choose a 256x240, 512x240 or 256x480 layout. Every 16x16 background region uses
  one of four subpalettes; all palettes share the universal backdrop.
- Use pencil, erase, bounded flood fill, palette painting, color picking,
  rectangular selections and captured brushes. Brushes can be flipped horizontally
  or vertically. Brush sizes are 1, 2, 4 and 8 pixels.
- Import a sprite sheet, pick an 8x8 tile and place it. Edit individual sprite
  pixels, visibility, position, palette, X/Y flipping and background priority.
  Reorder sprites to change OAM priority. Generate objects from nonempty sheet tiles.
- Use the automatic palette fitter or reapply the chosen palettes with optional
  ordered dithering. Source image watching and reapplying palettes preserve painted pixels. Clear background paint restores the source conversion.
- Zoom with the wheel and pan with the middle button. Toggle pixel/tile/block grids,
  the reference overlay, palette highlighting, sprite-limit preview and scanlines.
- Duplicate or add frames, set their durations, play the timeline and use onion skin.
- The original animated palette-test patterns are available in the Palette test effect tab.

Shortcuts: **P** pencil, **E** erase, **F** fill, **A** attributes, **I** pick,
**S** sprite, **M** move sprite, **R** select, **V** captured brush, **T** tile brush, **H** pan;
**1..4** select a color slot; **[ / ]** change brush size. Standard save, open,
undo, redo, copy, cut, paste and export use Ctrl shortcuts shown in the menus.
Selections and captured brushes store color-slot indices, so destination palettes
control their displayed colors.

## Project compatibility

Version 2 `.tzj` files are self-contained JSON: indexed frame pixels, attribute
regions, palettes, sprite objects and source images are stored together. Saving
uses a temporary file followed by atomic replacement. Undo/redo keeps up to 32
snapshots with a 64 MiB history target.

The loader also accepts the original QSettings INI `.tzj` format. Referenced image
files must still exist when importing an old project; relative paths resolve beside
that project. Legacy RGB colors are mapped to NES color codes, and source-image
dimensions infer a two-screen layout. Legacy projects did not store layout in the
project file, so confirm the inferred layout. New saves use version 2; the old Qt
application cannot read them. Importing an old file does not modify it until saved.

The NES model deliberately limits background and sprite palettes to four each.
Legacy projects using extra palettes or invalid attribute indices need correction.
Source-image changes are checked once per second while Watch source images is enabled.
Painted background pixels are preserved through a separate paint mask. Sprite-sheet
changes refresh sprite tiles; missing source files do not prevent opening version 2 projects.

## Compile directly to NES

Use **File > Export NES ROM + assets** or:

```powershell
.\build\Release\tilezeitjobb-cli.exe demo build\demo
.\build\Release\tilezeitjobb-cli.exe compile my-project.tzj build\scene --frame 0
.\build\Release\tilezeitjobb-cli.exe import picture.png my-project.tzj --auto-palettes
.\build\Release\tilezeitjobb-cli.exe inspect my-project.tzj
.\build\Release\tilezeitjobb-cli.exe batch images build\chr --crop 0 0 7 9
```

Batch conversion processes PNG/BMP files into sequential 4 KiB CHR banks. Crop
coordinates and dimensions are in tiles. `--palette colors.pal` accepts exactly
four NES color-code bytes; the default is `$0F,$01,$21,$30`.

Export writes:

| File | Contents |
| --- | --- |
| `.nes` | iNES mapper-0/NROM-128 ROM with 16 KiB PRG and 8 KiB CHR |
| `.chr` | 4 KiB deduplicated background patterns, then 4 KiB sprite patterns |
| `.nam` | One or two 1,024-byte nametables, each including its 64-byte attribute table |
| `.pal` | 16 background and 16 sprite palette bytes |
| `.oam` | 256 bytes of sprite OAM; unused objects hidden |
| `.h` | The same assets as static C arrays for integration into a game |

The ROM includes a 6502 startup routine, PPU initialization, palette/nametable
uploads, sprite DMA and per-frame NMI OAM refresh. No external assembler is needed.
It displays the **current frame and first viewport**. A second nametable is uploaded
with the appropriate mirroring; interactive scrolling and animated ROM playback
are future work. The editor's animation timeline is saved, but ROM export does not
compile the whole timeline.

Export rejects more than 256 unique tiles per pattern bank, more than 64 visible
sprites, more than eight sprites on a scanline, and unsafe color `$0D`. Sprite
positions must be X 0..255 and Y 1..239 for the initial viewport. NES sprites cannot
be drawn on the first scanline through ordinary OAM positioning. Palette RGB
appearance varies between console revisions, signals and emulator palette settings.

Open the `.nes` in a NES emulator, or copy it to a flash cartridge that supports
mapper 0. Producing a file does not program a cartridge: a physical cartridge also
needs a matching NROM board and the appropriate programming hardware. Emulator
boot/render tests are included; physical hardware has not been tested.

## Verification

The C++ test suites cover linked tile edits, brush transforms and wrapping, palette
remapping, bitplane order, deduplication, attribute packing including
the bottom row and second-screen boundary, OAM, ROM headers/vectors, project and PNG
roundtrips, legacy import, undo/redo, quantization and hardware-limit rejection.

An additional integration test boots five exported ROMs for 120 frames in pinned
JSNES 2.1.0, checks 61,184 rendered pixels per scene, and verifies every nametable
byte. JSNES reserves framebuffer row zero for its pre-render dummy scanline; that
row is excluded from the pixel comparison.

```powershell
npm ci --prefix tests/emulator
node tests/rom_emulator.mjs tests/emulator/node_modules/jsnes/src/index.js build/test-output
.\build\Release\tilezeitjobb.exe --demo --smoke-test --screenshot build\editor.png
```

## Pro Motion workflows selected for NES work

The priorities come from [Pro Motion NG's documented features](https://www.cosmigo.com/pixel_animation_software),
especially tile maps, captured brushes and indexed palette editing.

| Priority | Workflow | Status |
| --- | --- | --- |
| 1 | Edit shared tiles in place and paint tile maps | Implemented in **Tiles** and the **T** tool |
| 2 | Captured brush transforms, transparency and seamless drawing | Implemented in **Brush** |
| 3 | Palette reordering while preserving artwork | Implemented under **Palettes > Palette operations** |
| 4 | X/Y symmetry, pixel-perfect strokes and shape tools | Next drawing workflow |
| 5 | Richer onion skin, animation editing, sprite-sheet and animated ROM export | Next animation workflow |
| 6 | Metasprites, 8x16 sprites and reusable animation assets | Follow-up NES workflow |

**Tiles:** the catalog lists unique 8x8 background patterns in current export order,
including each pattern's map usage count. Select a pattern, then **T** paints it
on an 8x8 grid. Right-click or Alt-click picks a pattern from the map. Edit the
large tile grid to update all identical patterns in the current frame, or disable
Edit all identical map tiles to change only its picked map location. Each 16x16
region retains its palette. Identical patterns automatically merge, and unused
patterns disappear from the catalog; the catalog is derived from the map rather
than a separate library of unplaced assets. Tile edits support undo and survive
source-image reloads. More than 256 patterns still blocks NROM export.

**Brush:** capture a rectangular selection with Ctrl+C or the Capture button. Rotate
it in exact 90-degree steps, flip it, and paint with **V**. Slot-zero transparency,
8x8 snapping and wrapping across canvas edges are independent options. These brushes
are temporary editing tools, like the clipboard; save their artwork in the project
before closing. Canvas regions keep their own palettes when a brush is pasted.

**Palette operations:** choose a background or sprite palette, then swap slots 1..3.
Preserve existing artwork remaps matching pixels, sprite patterns and paint overrides
across **all frames**, keeping the rendered image unchanged. Without that option,
the same operation recolors the artwork. Copy/Paste colors transfers the three
nonzero entries between subpalettes. Slot zero stays reserved for the universal
background/sprite transparency. Preserving appearance can split formerly shared
patterns when only one subpalette is reordered; the tile count updates accordingly.

The editor is not full Pro Motion NG feature parity. Banked mappers, animated ROM
playback and physical hardware testing also remain follow-up work.

## License

GPL-3.0; see [LICENSE](LICENSE). Dear ImGui and GLFW use their upstream licenses;
nlohmann/json is MIT; stb is public-domain/MIT. JSNES is an optional test dependency
under Apache-2.0.
