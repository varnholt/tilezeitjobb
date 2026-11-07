#include "app/ui_help.hpp"
#include <imgui_internal.h>
#include <string>
#include <string_view>
#include <utility>
namespace tzj::ui {
namespace {
constexpr std::pair<std::string_view, const char *> descriptions[]{
    {"Edit bitmap font", "Edit individual 8x8 glyphs, import a font sheet, or restore the built-in "
                         "font. Font changes are saved with the project."},
    {"Direction", "Choose whether the gradient runs from left to right or from top to bottom."},
    {"Start", "The color slot at the start of the gradient. The four ramp stops use the active NES "
              "color set."},
    {"End", "The color slot at the end of the gradient."},
    {"Ramp stop",
     "An intermediate indexed color in the gradient; choose any slot from the active color set."},
    {"Brush width",
     "Width of the gradient brush in pixels. Selection filling uses the selection size instead."},
    {"Brush height",
     "Height of the gradient brush in pixels. Selection filling uses the selection size instead."},
    {"Ramp name",
     "A name for the current four NES colors and gradient slot order in the project library."},
    {"Delete ramp", "Remove this saved ramp without recoloring existing artwork. Undo restores the "
                    "library entry."},
    {"Center X", "Horizontal symmetry center in target pixels. Half coordinates place the axis "
                 "between pixels."},
    {"Center Y",
     "Vertical symmetry center in target pixels. Half coordinates place the axis between pixels."},
    {"Symmetry settings",
     "Set a custom mirror center and the number of rotated copies for drawing strokes."},
    {"Onion skin enabled", "Overlay nearby animation frames as colored guides. Ghosts never become "
                           "artwork or exported pixels."},
    {"Ghost opacity", "Opacity of the nearest ghost frame. More distant ghosts fade in proportion "
                      "to their distance."},
    {"Brush library", "Save named indexed brushes and multi-tile groups with the project, then "
                      "reuse them across frames. Import libraries from another project."},
    {"Bitmap text", "Build text from editable 8x8 glyphs. Use it as a transparent indexed brush on "
                    "a paint layer."},
    {"Gradient fill", "Create NES-safe gradients using the active color set, reordered color slots "
                      "and optional ordered or noise dithering."},
    {"Brush name", "Name the brush you are about to save, or use Rename saved brush to update the "
                   "selected library entry."},
    {"Save selection as brush", "Store the selected rectangle as indexed pixels with transparency. "
                                "The destination region supplies its NES colors."},
    {"Save captured brush",
     "Save the current captured brush in this project's reusable asset library."},
    {"Save selected sprite", "Save the selected sprite's displayed pixels as an 8x8 brush. Sprite "
                             "color zero becomes transparent."},
    {"Use saved brush", "Load this asset into the stamp tool. Click or drag on a paint layer; one "
                        "stroke is one undo step."},
    {"Create sprite",
     "Make an editable 8x8 sprite from this saved brush using the selected sprite color set."},
    {"Rename saved brush", "Give the selected library asset the text from Brush name."},
    {"Delete saved brush", "Remove this asset from the library. Already stamped artwork stays "
                           "unchanged; undo can restore the asset."},
    {"Import from project...",
     "Copy saved brushes and color ramps from another .tzj project without loading its artwork."},
    {"Import brushes and color sets...", "Import reusable indexed brushes, tile groups and named "
                                         "color ramps from another saved project."},
    {"Letter spacing", "Extra pixels between 8x8 glyph cells before pixel scaling."},
    {"Line spacing", "Extra pixels between rows of text before pixel scaling."},
    {"Pixel scale", "Enlarge each font pixel by an exact integer factor."},
    {"Use current paint color", "Recolor every opaque glyph pixel to the chosen paint slot. Turn "
                                "off to keep the font's own indexed shading."},
    {"Use text as brush", "Prepare the text for stamping on a paint layer. The project font stays "
                          "editable, while stamped text becomes independent pixels."},
    {"Glyph", "Choose one printable ASCII character to edit in the pixel grid below."},
    {"Glyph pixels", "Draw with the active color slot; right-drag erases to transparency. A glyph "
                     "stroke is one undo step."},
    {"Import font sheet...", "Import at least 96 row-major 8x8 cells beginning with ASCII space. "
                             "Pixels map to the current NES color set."},
    {"Restore built-in font",
     "Replace the project font with the built-in 8x8 alphabet, numbers and punctuation. Stamped "
     "text remains unchanged; this can be undone."},
    {"Gradient dithering", "None makes solid color bands; ordered gives a repeating 4x4 "
                           "transition; noise uses a deterministic scattered transition."},
    {"Fill selected area", "Fill the selected rectangle on the active paint layer as one undo "
                           "step. Existing NES color-region assignments remain in place."},
    {"Use gradient as brush",
     "Prepare an indexed gradient of the chosen dimensions for stamping on a paint layer."},
    {"Save current color ramp",
     "Save the current four NES colors and gradient slot order with the project."},
    {"Seamless pattern view", "Edit an 8x8 pattern through a 3x3 repetition. Strokes wrap across "
                              "tile boundaries, exposing seams immediately."},
    {"Custom center", "Place mirror and radial symmetry around a chosen pixel coordinate instead "
                      "of the target's center."},
    {"Radial copies", "Repeat each drawing stroke around the symmetry center. One means no "
                      "rotation; mirrors can also be enabled."},
    {"Onion skin settings...", "Choose previous and following frames, opacity falloff and whether "
                               "ghosts include only sprites."},
    {"Previous frames (green)",
     "Number of earlier frames shown as green ghosts; distant frames fade more."},
    {"Following frames (pink)",
     "Number of later frames shown as pink ghosts; distant frames fade more."},
    {"Sprites only",
     "Ghost only the animated sprite objects. Turn off to include changed background artwork."},
    {"Reference guide (not exported)",
     "Show the original source image as a drawing guide. It contributes no pixels, tiles or colors "
     "to ROM or artwork export."},
    {"Guide opacity", "Blend this source into the editing canvas. NES output still uses only "
                      "exported indexed layers and binary sprite transparency."},
    {"Native size (1:1)", "Show one artwork pixel per display pixel in the overview. Turn off to "
                          "fit the preview window."},
    {"Compact color sets",
     "Show the active color set with a selector; turn off to display all four sets together."},
    {"NES preview", "Run the exported mapper-0 ROM in the built-in C++ emulator. Includes pause, "
                    "reset, frame stepping and optional rebuild after edits."},
    {"Run current artwork", "Compile the selected frame to a ROM and boot it in the NES preview. "
                            "Invalid artwork leaves the last working ROM visible."},
    {"Rebuild after edits", "Recompile and restart the cartridge after an edit finishes. Failed "
                            "validation keeps the last working ROM."},
    {"Step frame", "Pause the NES preview and execute one complete emulated video frame."},
    {"Reset ROM", "Restart the currently loaded cartridge and run its startup routine again."},
    {"Layers", "The scene stack. Sprites are above the background; eye controls determine "
               "visibility and locks protect content. Drag within a group to reorder."},
    {"Around canvas center",
     "Mirrored strokes reflect across the center of the background canvas."},
    {"Around sprite center", "Mirrored strokes reflect within the selected 8x8 sprite."},
    {"+ Paint", "Create a transparent indexed paint layer above the selected background layer."},
    {"+ Image", "Import an original image as a live layer, keeping its source pixels and "
                "conversion modifiers."},
    {"+ Sprite", "Add a blank 8x8 sprite on top of the background and select it for painting."},
    {"Duplicate layer", "Create an independent copy of this layer. Image copies keep their "
                        "original source and editable conversion settings."},
    {"Make paintable copy", "Capture this image layer after its modifiers as an indexed paint "
                            "layer above it. The original stays intact underneath."},
    {"Merge down", "Combine this layer with the background layer below. Live image modifiers are "
                   "baked into the resulting paint layer. Undo restores both originals."},
    {"Delete layer", "Remove the selected layer. At least one background layer must remain; locked "
                     "layers must be unlocked first."},
    {"Move up",
     "Move this layer toward the top of its group, giving it priority over layers below."},
    {"Move down", "Move this layer toward the bottom of its group."},
    {"Layer name",
     "Rename the selected layer. Its pixels, visibility and modifier settings do not change."},
    {"Enable modifiers", "Bypass or enable the image modifiers together. The source is always "
                         "converted to the scene's NES color sets."},
    {"Dithering", "Approximate intermediate colors with pixel patterns. Turn it off for "
                  "nearest-color conversion."},
    {"Method", "Choose no dithering, an ordered Bayer pattern, Floyd-Steinberg error diffusion, or "
               "Atkinson diffusion."},
    {"Dither strength", "Control how strongly conversion uses dithering. Zero keeps nearest-color "
                        "pixels; 100 uses the full pattern or propagated error."},
    {"Color adjustments", "Adjust the source colors without altering the original image. Changes "
                          "are applied before NES conversion."},
    {"Color adjustment", "Enable or bypass brightness and contrast for this image layer."},
    {"Brightness", "Lighten or darken the source before converting it to NES colors. The original "
                   "image is preserved."},
    {"Contrast",
     "Increase or decrease separation between dark and light source colors before conversion."},
    {"Crop / resize / position", "Crop the original image, resize it with nearest-neighbor "
                                 "sampling, and position the result on the background."},
    {"Crop and resize", "Enable the source crop and output-size modifiers. Disable to use the "
                        "original source dimensions."},
    {"Crop origin", "The top-left crop position in original image pixels: X, then Y."},
    {"Crop size",
     "The source crop width and height. Zero uses the remaining original image extent."},
    {"Output size",
     "The converted image width and height after cropping. Zero keeps the crop size."},
    {"Layer position", "Position the converted layer in scene pixels: X, then Y."},
    {"Fit NES colors to this layer",
     "Choose shared NES background color sets and color-area assignments to fit this source. This "
     "also changes colors used by other background layers."},
    {"Clear layer",
     "Make every pixel on the selected paint layer transparent, revealing layers underneath."},
    {"Drawing help",
     "Instructions for the active drawing tool, selection and NES scene constraints."},
    {"Layers - top is in front",
     "The scene stack: sprites above background layers, with visibility and locking per row. Drag "
     "within a group to change order."},
    {"Selected layer", "Properties and operations for the highlighted row in the layer stack."},

    {"Background tiles  %d / 256", "How many distinct 8x8 background patterns this frame needs. "
                                   "The current ROM format allows 256 patterns."},
    {"Sprite tiles      %d / 256",
     "How many distinct sprite patterns this frame needs. Reused sprite pixels share one pattern."},
    {"OAM sprites       %d / 64", "How many sprite objects the NES must draw. It can store 64 "
                                  "objects and show at most eight on one scanline."},
    {"Nametables        %zu", "A nametable is one 256x240 background screen, made from tile "
                              "indices and color-area settings."},
    {"Background CHR: %zu / 256 patterns",
     "CHR is the NES tile-pattern memory. Identical pixel patterns are stored only once."},
    {"ms", "Frame display duration in milliseconds. For example, 100 ms gives ten frames per "
           "second for equally timed frames."},
    {"Captured brush: %d x %d", "The size of the background area copied into your brush. Transform "
                                "it or stamp it onto the canvas."},
    {"Selection: %d x %d",
     "The dimensions of the selected background rectangle. Capture or cut it to create a brush."},
    {"Frame %zu of %zu",
     "The active animation frame. Drawing changes this frame; Duplicate copies it to a new frame."},
    {"Source: %d x %d",
     "The dimensions of the imported source image before positioning or conversion to NES colors."},
    {"%d x %d pixels",
     "Scene size in pixels. A standard NES screen is 256 pixels wide and 240 pixels tall."},
    {"Color set %d / paint color %d", "The active color set and the slot used by drawing tools. "
                                      "Slot one in a sprite set represents transparency."},
    {"Set %d / color %d", "The currently selected paint slot. Choose a different swatch in the "
                          "left panel before drawing."},
    {"Tile %d,%d / palette %d",
     "The selected 8x8 tile position and its assigned background color set."},
    {"Ctrl+Z undoes your last drawing.", "Undo also restores palette changes, sprite edits and "
                                         "frame operations. Ctrl+Y redoes an undone operation."},
    {"Nothing selected", "Choose Select, drag a rectangle on the background, then Capture it to "
                         "make a reusable brush."},

    {"New", "Start a background, a standalone sprite, an imported picture, or the example. You can "
            "save current work first."},
    {"Open", "Load an editable .tzj project, including its frames, sprites and color sets."},
    {"Open project...", "Load an editable .tzj project. Images are loaded with Import instead."},
    {"Open a saved project",
     "Continue an existing .tzj project, including its sprites and animation frames."},
    {"Save",
     "Save an editable .tzj project. In the browser, this downloads the project to your device."},
    {"Save project", "Keep an editable copy of the entire project. This is separate from exporting "
                     "a ROM or image."},
    {"Save as...", "Save another copy of this project with a different filename."},
    {"Import background", "Load a PNG, BMP, JPEG or TGA as background artwork and convert its "
                          "colors to the active NES color sets."},
    {"Import sprites",
     "Load a sprite sheet. Select an 8x8 tile from the sheet and place it over the background."},
    {"Import background...",
     "Load a picture for the background; this does not load an editable project."},
    {"Import sprite sheet...",
     "Load an image containing sprite tiles. Placed sprites are 8x8 pixels."},
    {"Import a picture", "Start a background from an image. Use the color-fitting button to choose "
                         "suitable NES colors."},
    {"Import a picture...", "Load a picture to convert into NES background artwork."},
    {"Export NES", "Compile the current frame to a runnable NES ROM and matching raw data. The "
                   "browser downloads a ZIP."},
    {"Export for NES", "Compile the current frame into a .nes ROM for an emulator or NES flash "
                       "cartridge, plus matching data files."},
    {"Export ROM + assets...",
     "Export a .nes ROM, tile patterns, screen layout, color sets, sprite data and a C header."},
    {"Export NES ROM + assets...", "Export the current frame as a runnable .nes ROM and its raw "
                                   "assets. Animation playback is not included in the ROM yet."},
    {"Export PNG...", "Save a flat image of the current frame. A PNG cannot retain editable "
                      "frames, tile links or sprite objects."},
    {"Quit", "Close the editor after offering to save unsaved artwork."},
    {"Undo",
     "Undo one operation. A drawn shape or completed polygon is one operation. Shortcut: Ctrl+Z."},
    {"Redo", "Restore the last undone operation. Shortcut: Ctrl+Y."},
    {"Import image",
     "Import a picture as a new background image layer, with an editable paint layer above it."},
    {"Tile editor", "Show the catalog of repeated 8x8 background patterns. Selecting a pattern "
                    "lets you paint with it or edit its linked copies."},
    {"Animation timeline", "Show frame duplication, timing and playback below the canvas. You can "
                           "edit any layer while it is open."},
    {"NES limits", "Show hardware validation and tile/sprite counts for the current frame, without "
                   "changing the selected layer."},
    {"Fullscreen (F11)",
     "Use the whole display. In the desktop app, F11 restores the previous window state."},
    {"Windowed (F11)", "Leave fullscreen and restore the desktop window."},
    {"Fullscreen", "Toggle fullscreen. Shortcut: F11."},
    {"Workflow guidance", "Show beginner instructions and a compact color selector. Turn it off to "
                          "show all four color sets together."},
    {"Advanced layout", "Use independently dockable panels instead of the guided scene workflow."},
    {"Start here...",
     "Reopen the starting choices. Unsaved artwork can be saved before starting another picture."},
    {"Draw a background", "Begin a blank 256x240 background. Paint scenery first, then add sprite "
                          "objects in the next stage."},
    {"Create a sprite", "Begin a blank scene with one selected 8x8 sprite. Three slots are paint "
                        "colors; the first slot is transparent."},
    {"Explore the example",
     "Load the built-in scene. Its background and sprite overlays remain separately editable."},
    {"Keep current picture", "Close these choices and continue working on the current picture."},
    {"Fit canvas", "Zoom out or in until the whole background fits. This changes only your view."},
    {"Zoom", "Magnify the view without resizing artwork. Use the mouse wheel to zoom and the "
             "middle button to pan."},
    {"Grid", "Show pixel boundaries or the NES 8x8 tile and 16x16 color-area boundaries. The grid "
             "is never exported."},
    {"Canvas options", "View settings for overlays, grids and previews. These do not add pixels to "
                       "the exported image."},
    {"Filled shape",
     "Fill the interior of rectangles and ellipses. Turn this off to draw their outlines only."},
    {"Filled polygon", "Fill the inside of a completed polygon. Turn it off for a closed outline."},
    {"Finish", "Commit the placed path points as one drawing operation. Enter or right-click on "
               "the canvas also finishes."},
    {"Cancel", "Cancel the current operation without applying it. During a drawing preview, Esc "
               "also cancels."},
    {"Done",
     "Close the color-set editor. Changes made here have already been applied and can be undone."},
    {"Mirror left / right", "Reflect pencil, eraser and shape strokes across the center of the "
                            "canvas or selected sprite."},
    {"Mirror top / bottom", "Reflect pencil, eraser and shape strokes across the horizontal center "
                            "of the canvas or selected sprite."},
    {"Show sprites", "Show or hide sprite objects over the background. This changes the preview, "
                     "not their individual visibility settings."},
    {"Preview sprite limits", "Simulate sprite overflow in the view. NES hardware supports 64 "
                              "sprites in total and only eight on one scanline."},
    {"Onion skin", "Overlay the previous animation frame so you can align changes between frames."},
    {"Highlight active color set",
     "Dim background areas that use a different color set from the one currently selected."},
    {"Reference overlay",
     "Blend the original imported image over the converted artwork to compare the conversion."},
    {"Scanlines", "Darken alternate preview rows for a display effect. This is a viewing effect, "
                  "not simulated NES video output."},
    {"Ordered dithering", "Use a repeating pixel pattern to approximate colors between NES palette "
                          "entries when converting an imported image."},
    {"Reapply palettes", "Convert the imported background again using the current color sets and "
                         "dithering option. Painted pixels remain protected."},
    {"Auto palettes",
     "Choose NES color sets that fit the imported background, then apply the conversion."},
    {"Fit imported picture's colors", "Automatically choose color sets to better match the source "
                                      "image, then convert it to NES pixels."},
    {"Clear background paint", "Remove manual background paint and repaint from the imported "
                               "reference. You can undo this operation."},
    {"Image offset", "Position the imported reference in background pixels: first X, then Y. This "
                     "does not resize the image."},
    {"Watch source images", "Reload source images when their files change outside the editor. "
                            "Manual background strokes are preserved."},
    {"Edit color sets...", "Open the dedicated color-set editor to change NES colors, copy sets, "
                           "or reorder paint slots across frames."},
    {"Artwork",
     "Choose whether to edit the background color sets or the separate sprite color sets."},
    {"Color set", "Choose which of the four sets to edit. Editing a set changes every pixel or "
                  "sprite that uses it."},
    {"##active-color-set", "Choose the active NES color set. Background pixels share one set per "
                           "16x16 area; a selected sprite uses one set."},
    {"Edit NES color", "Choose a hardware NES color for this slot. Color changes affect all "
                       "artwork using this color set."},
    {"NES color",
     "Assign this hardware color to the selected slot. NES color $0D is unsafe for real hardware."},
    {"Current paint", "The color that the active drawing tool will apply. Choose another swatch in "
                      "the paint panel."},
    {"##color", "Click to paint with this slot. Right-click to change its hardware color. The "
                "first sprite slot erases to transparency."},
    {"##nes",
     "Choose this hardware NES color for the slot being edited. $0D is unsafe for real hardware."},
    {"First", "The first paint slot to exchange. Slot zero is reserved for the shared background "
              "or sprite transparency."},
    {"Second", "The second paint slot to exchange with the first."},
    {"Preserve existing artwork", "Remap stored pixel indices along with the color swap so the "
                                  "image keeps its appearance across every frame."},
    {"Swap slots", "Exchange the two chosen paint colors. With preservation enabled, pixel indices "
                   "are remapped too."},
    {"Copy colors",
     "Copy the three paint colors from this set. The reserved first slot is not pasted."},
    {"Paste colors", "Replace this set's three paint colors with the copied colors. Existing "
                     "pixels will use the new colors."},
    {"+ New sprite",
     "Add a blank 8x8 sprite over the current background, select it, and zoom in for drawing."},
    {"Background picture",
     "Select the background as the drawing target instead of a sprite object."},
    {"Generate from sheet", "Create sprite objects from the imported sheet. This may exceed NES "
                            "sprite limits; check the Export stage."},
    {"Name", "Rename the selected sprite so it is easier to identify in the object list."},
    {"Position",
     "Move the selected sprite in background pixels: X is horizontal and Y is vertical."},
    {"Palette", "Choose the selected sprite's color set. Its pixel indices stay the same and "
                "appear in the chosen colors."},
    {"Flip X", "Mirror the selected sprite horizontally using the NES hardware flip flag."},
    {"Flip Y", "Mirror the selected sprite vertically using the NES hardware flip flag."},
    {"Behind background", "Place this sprite behind opaque background pixels. It can still show "
                          "through the background-color pixels."},
    {"##visible", "Include this sprite in the picture and exported scene. Turn it off to hide the "
                  "object without deleting it."},
    {"sprite-pixels", "Draw directly in the selected 8x8 sprite. Left paints with the chosen slot; "
                      "right erases to transparency."},
    {"Delete sprite", "Remove the selected sprite from this frame. Ctrl+Z restores it."},
    {"Up", "Move the sprite earlier in the object list. Earlier sprites have higher priority where "
           "sprites overlap."},
    {"Down", "Move the sprite later in the object list, giving earlier sprites priority over it."},
    {"Play", "Preview frame animation at the saved frame durations. This does not change ROM "
             "export to an animated ROM."},
    {"Pause", "Stop animation playback so you can edit the active frame."},
    {"Duplicate",
     "Copy the current frame, including its background, sprites and duration, as a new frame."},
    {"Blank frame", "Add a new empty frame at the same scene dimensions."},
    {"Delete frame", "Remove the active frame. The project must retain at least one frame."},
    {"ms", "How long this animation frame is displayed, in milliseconds. 100 ms is one tenth of a "
           "second."},
    {"Rotate left",
     "Rotate the captured brush 90 degrees counterclockwise, keeping exact indexed pixels."},
    {"Rotate right",
     "Rotate the captured brush 90 degrees clockwise, keeping exact indexed pixels."},
    {"Flip horizontal", "Mirror the captured brush from left to right."},
    {"Flip vertical", "Mirror the captured brush from top to bottom."},
    {"Paint with brush", "Switch to stamping the captured brush on the background. Shortcut: V."},
    {"Paste brush", "Select the captured brush for stamping. This does not immediately paste "
                    "pixels until you click the canvas."},
    {"Copy selection",
     "Capture the selected background pixels as a reusable brush. Shortcut: Ctrl+C."},
    {"Cut selection",
     "Capture the selected pixels as a brush, then erase the selected area. Shortcut: Ctrl+X."},
    {"Capture",
     "Copy the selected background area into the reusable brush. Then stamp, rotate or mirror it."},
    {"Cut", "Capture the selected area as a brush and erase the source pixels."},
    {"Deselect", "Clear the selection rectangle. This does not erase the selected artwork."},
    {"Select an area", "Drag a rectangle on the canvas, then capture it as a reusable brush."},
    {"Slot zero is transparent", "Skip brush pixels with color index zero, leaving the destination "
                                 "artwork visible underneath."},
    {"Snap stamps to 8x8 grid",
     "Align captured-brush stamps to tile boundaries instead of placing them at any pixel."},
    {"Wrap across canvas edges",
     "Continue a brush stamp at the opposite edge when it extends beyond the background."},
    {"Edit all identical map tiles", "Update every matching background tile when you edit this "
                                     "pattern. Turn it off to change only the picked tile."},
    {"Paint tile (T)", "Stamp the selected 8x8 pattern onto the map. Right-click or Alt-click "
                       "picks a pattern from the canvas."},
    {"Rotate tile", "Rotate this tile 90 degrees clockwise. Linked editing also updates every "
                    "identical map instance."},
    {"tile", "Select this background pattern for editing or painting. Identical patterns share a "
             "CHR tile when exported."},
    {"edit-tile", "Paint individual pixels in this 8x8 tile. Linked editing can update every "
                  "identical map tile."},
    {"Parent directory", "Browse one folder above the current folder."},
    {"Project directory", "Browse the folder containing the current project, or the current "
                          "working folder for an unsaved project."},
    {"Path", "Type a filename in the current folder or an absolute path. Choose confirms the file "
             "operation."},
    {"Choose", "Confirm the selected file or typed path. Double-click a folder to browse into it."},
    {"Replace", "Overwrite the existing output file. Choose a different name if you want to keep "
                "the old copy."},
    {"Discard", "Continue without saving the current unsaved changes."},
    {"OK", "Dismiss this message and return to the editor."},
    {"What next?", "Step-by-step help for making a scene: paint the background, add sprites, "
                   "animate if needed, then export."},
    {"NES limits", "Check pattern counts, sprite counts, scanline limits and color safety before "
                   "compiling a ROM."},
    {"Small preview", "Show the whole scene at a smaller scale to check its overall appearance."},
    {"Sprite sheet (optional)", "Import pre-drawn sprite tiles if you have them. New Sprite can "
                                "create an object without a sheet."},
    {"Image preview", "A view of the current artwork. The canvas and pixel grids are the editing "
                      "surfaces; overlays are not saved as artwork."},
    {"Active tool", "Instructions for the currently selected drawing tool and its shortcuts."},
    {"Selection / brush", "Capture part of the background and use it as a reusable brush."},
    {"Selection", "A selection marks the background area used by Copy or Cut. It does not hide or "
                  "erase pixels."},
    {"Picture", "Information about the active scene dimensions, frame and source image."},
    {"Tile map",
     "NES backgrounds are built from repeated 8x8 patterns. Paint and edit those patterns here."},
    {"Sprite objects",
     "Separate 8x8 objects placed over the background. Select one to draw into it or move it."},
    {"Reorder paint slots", "Move colors between indexed slots. Preserve artwork keeps their "
                            "appearance by updating stored pixel indices."},
    {"1. Choose a paint color",
     "Pick one of the four slots before drawing. For sprites, the first slot is transparent."},
    {"2. Choose a tool",
     "Choose how to apply the paint color. Hover each icon for instructions and a shortcut."},
    {"Paint color", "The active paint slot is outlined. Click a swatch to select it; right-click "
                    "to edit its NES color."},
    {"Tools", "Choose a drawing, selection, placement or navigation tool."},
    {"Symmetry",
     "Mirror new pencil, eraser and shape strokes around the canvas or selected sprite center."},
    {"Pencil", "Drag to draw connected pixels. Right-drag erases. P selects this tool; [ and ] "
               "change brush size."},
    {"Eraser",
     "Drag to paint the background color or erase sprite pixels to transparency. Shortcut: E."},
    {"Line", "Drag a straight line and release to commit it. Shift locks horizontal, vertical or "
             "45-degree directions; Esc cancels. Shortcut: L."},
    {"Rectangle", "Drag a rectangle; Shift makes a square. Filled shape controls its interior. "
                  "Release to draw, Esc to cancel. Shortcut: B."},
    {"Ellipse", "Drag an ellipse; Shift makes a circle. Filled shape controls its interior. "
                "Release to draw, Esc to cancel. Shortcut: O."},
    {"Polyline", "Click connected points. Enter or right-click finishes, Backspace removes the "
                 "last point, Esc cancels. Shortcut: K."},
    {"Polygon", "Click the outline points. Click the first point or press Enter to finish; Filled "
                "polygon adds its interior. Shortcut: G."},
    {"Fill", "Click to replace a connected region of the same color. On a selected sprite, filling "
             "stays within that sprite. Shortcut: F."},
    {"Pick color", "Click the artwork to select its paint slot and color set. Shortcut: I."},
    {"Select", "Drag a rectangle around background pixels, then use Copy or Cut to capture a "
               "brush. Shortcut: R."},
    {"Brush", "Stamp the captured selection. Capture a brush with Ctrl+C first; choose "
              "transparency and snapping below. Shortcut: V."},
    {"Place", "Select a tile in the sprite sheet, then click the canvas to create an 8x8 sprite "
              "object. Shortcut: S."},
    {"Move", "Click a sprite and drag to reposition it over the background. Shortcut: M."},
    {"Pan", "Drag to move around the canvas without painting. Middle-drag also pans; the mouse "
            "wheel zooms. Shortcut: H."},
    {"Paint tile",
     "Stamp an 8x8 tile pattern. Right-click or Alt-click picks a pattern. Shortcut: T."},
    {"Set colors", "Assign the selected color set to a 16x16 background area without repainting "
                   "pixel indices. Shortcut: A."},
};
}
void help(const char *label, const char *fallback) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        return;
    std::string key(label ? label : "");
    if (key.starts_with("##tool-")) {
        key.erase(0, 7);
        auto shortcut = key.find_last_of(' ');
        if (shortcut != std::string::npos)
            key.resize(shortcut);
        while (!key.empty() && key.back() == ' ')
            key.pop_back();
    }
    const char *description = nullptr;
    for (const auto &[name, text] : descriptions)
        if (name == key) {
            description = text;
            break;
        }
    if (!description && key.ends_with(" px"))
        description = "Choose the square brush footprint in pixels. It affects pencil, eraser and "
                      "shape outlines.";
    if (!description && key.starts_with("[dir]"))
        description =
            "Double-click this folder to browse into it. No file is opened or changed yet.";
    if (!description)
        description = fallback;
    if (!description)
        return;
    const auto item = GImGui->LastItemData;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
    ImGui::TextUnformatted(description);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    GImGui->LastItemData = item;
}
} // namespace tzj::ui
