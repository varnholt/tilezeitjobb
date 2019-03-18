#ifndef CONSTANTS_H
#define CONSTANTS_H

enum LayerType
{
   LayerTypeInvalid,
   LayerTypeNameTable,
   LayerTypeSprite,
};

enum ToolType
{
   ToolTypePaletteSetup,
   ToolTypePaletteBrush,
   ToolTypePixelPencil,
   ToolTypePixelBrush,
   ToolTypeMove,
   ToolTypeErase,
   ToolTypeColorPicker,
   ToolTypeSprite
};


enum GridSize
{
   GridSizeNone  = 0,
   GridSize1x1   = 1,
   GridSize8x8   = 2,
   GridSize16x16 = 3
};


enum NameTableLayout
{
   NameTableLayoutDefault = 0,
   NameTableLayoutVerticalFlipping = 1,
   NameTableLayoutHorizontalFlipping = 2
};

#define TILE_SIZE_PX 8
#define BLOCK_SIZE_PX 16
#define PAL_BLOCK_SIZE 30
#define PAL_WIDTH 4
#define PAL_HEIGHT 14


// 32 tiles in width sharing 1 palette for 2 blocks each, 2 nametables
// 30 tiles in height sharing 1 palette for 2 blocks each, 2 nametables
#define BLOCK_16x16_WIDTH 32
#define BLOCK_16x16_HEIGHT 30
#define BLOCK_16x16_SIZE BLOCK_16x16_WIDTH * BLOCK_16x16_HEIGHT


// colors

#define NTSC_0_0 "#8c1d1d"
#define NTSC_0_1 "#911a1a"
#define NTSC_0_2 "#b1b1af"
#define NTSC_0_3 "#676765"

#define NTSC_1_0 "#b9e5fe"
#define NTSC_1_1 "#4eadff"
#define NTSC_1_2 "#0755eb"
#define NTSC_1_3 "#011e9c"

#define NTSC_2_0 "#d1dbff"
#define NTSC_2_1 "#8795fe"
#define NTSC_2_2 "#483cfe"
#define NTSC_2_3 "#210dad"

#define NTSC_3_0 "#e6d5ff"
#define NTSC_3_1 "#b986ff"
#define NTSC_3_2 "#7631fe"
#define NTSC_3_3 "#45049c"

#define NTSC_4_0 "#fdd2ff"
#define NTSC_4_1 "#f080ff"
#define NTSC_4_2 "#ae2bcf"
#define NTSC_4_3 "#6a036e"

#define NTSC_5_0 "#ffcff5"
#define NTSC_5_1 "#ff7ad3"
#define NTSC_5_2 "#be2a64"
#define NTSC_5_3 "#72041f"


#define PAL_0_3 0xff4c4c4c
#define PAL_0_2 0xff9c9c9c
#define PAL_0_1 0xfff7f7f7
#define PAL_0_0 0xfff7f7f7

#define PAL_1_3 0xff000569
#define PAL_1_2 0xff003fc2
#define PAL_1_1 0xff2499ff
#define PAL_1_0 0xffa2d1ff

#define PAL_2_3 0xff000085
#define PAL_2_2 0xff0522e6
#define PAL_2_1 0xff5e7bff
#define PAL_2_0 0xffb9c5ff

#define PAL_3_3 0xff0f007e
#define PAL_3_2 0xff4c0bdd
#define PAL_3_1 0xffa665ff
#define PAL_3_0 0xffd6bcff

#define PAL_4_3 0xff420055
#define PAL_4_2 0xff8e02a8
#define PAL_4_1 0xffea5bff
#define PAL_4_0 0xfff2b8fc

#define PAL_5_3 0xff630016
#define PAL_5_2 0xffbb0756
#define PAL_5_1 0xffff61b0
#define PAL_5_0 0xffffbadb

#define PAL_6_3 0xff6b0000
#define PAL_6_2 0xffc51b00
#define PAL_6_1 0xffff7456
#define PAL_6_0 0xffffc2b6

#define PAL_7_3 0xff560000
#define PAL_7_2 0xffaa3700
#define PAL_7_1 0xffff910c
#define PAL_7_0 0xfffdce98

#define PAL_8_3 0xff2b1500
#define PAL_8_2 0xff715400
#define PAL_8_1 0xffccae00
#define PAL_8_0 0xffe6da89

#define PAL_9_3 0xff002600
#define PAL_9_2 0xff2a6a00
#define PAL_9_1 0xff84c500
#define PAL_9_0 0xffc8e38d

#define PAL_10_3 0xff002d00
#define PAL_10_2 0xff007400
#define PAL_10_1 0xff40cf26
#define PAL_10_0 0xffade7a2

#define PAL_11_3 0xff002900
#define PAL_11_2 0xff006e20
#define PAL_11_1 0xff13c979
#define PAL_11_0 0xff9be5c4

#define PAL_12_3 0xff001a31
#define PAL_12_2 0xff005b79
#define PAL_12_1 0xff09b5d4
#define PAL_12_0 0xff97dde9

#define PAL_13_3 0xff000001
#define PAL_13_2 0xff000001
#define PAL_13_1 0xff323232
#define PAL_13_0 0xffa7a7a7



static unsigned int sNesPalette[56]=
{
   PAL_0_0, PAL_0_1, PAL_0_2, PAL_0_3,
   PAL_1_0, PAL_1_1, PAL_1_2, PAL_1_3,
   PAL_2_0, PAL_2_1, PAL_2_2, PAL_2_3,
   PAL_3_0, PAL_3_1, PAL_3_2, PAL_3_3,
   PAL_4_0, PAL_4_1, PAL_4_2, PAL_4_3,
   PAL_5_0, PAL_5_1, PAL_5_2, PAL_5_3,
   PAL_6_0, PAL_6_1, PAL_6_2, PAL_6_3,
   PAL_7_0, PAL_7_1, PAL_7_2, PAL_7_3,
   PAL_8_0, PAL_8_1, PAL_8_2, PAL_8_3,
   PAL_9_0, PAL_9_1, PAL_9_2, PAL_9_3,
   PAL_10_0, PAL_10_1, PAL_10_2, PAL_10_3,
   PAL_11_0, PAL_11_1, PAL_11_2, PAL_11_3,
   PAL_12_0, PAL_12_1, PAL_12_2, PAL_12_3,
   PAL_13_0, PAL_13_1, PAL_13_2, PAL_13_3

   /*
   PAL_0_3, PAL_1_3, PAL_2_3, PAL_3_3, PAL_4_3, PAL_5_3, PAL_6_3, PAL_7_3, PAL_8_3, PAL_9_3, PAL_10_3, PAL_11_3, PAL_12_3, PAL_13_3, // 0xff000001, 0xff000001,
   PAL_0_2, PAL_1_2, PAL_2_2, PAL_3_2, PAL_4_2, PAL_5_2, PAL_6_2, PAL_7_2, PAL_8_2, PAL_9_2, PAL_10_2, PAL_11_2, PAL_12_2, PAL_13_2, // 0xff000001, 0xff000001,
   PAL_0_1, PAL_1_1, PAL_2_1, PAL_3_1, PAL_4_1, PAL_5_1, PAL_6_1, PAL_7_1, PAL_8_1, PAL_9_1, PAL_10_1, PAL_11_1, PAL_12_1, PAL_13_1, // 0xff000001, 0xff000001,
   PAL_0_0, PAL_1_0, PAL_2_0, PAL_3_0, PAL_4_0, PAL_5_0, PAL_6_0, PAL_7_0, PAL_8_0, PAL_9_0, PAL_10_0, PAL_11_0, PAL_12_0, PAL_13_0, // 0xff000001, 0xff000001
   */
};


#endif // CONSTANTS_H

/*
static unsigned int nesPal[55]= {
   0xff4c4c4c, 0xff000569, 0xff000085, 0xff0f007e, 0xff420055, 0xff630016, 0xff6b0000, 0xff560000, 0xff2b1500, 0xff002600, 0xff002d00, 0xff002900, 0xff001a31, // 0xff000001, 0xff000001, 0xff000001,
   0xff9c9c9c, 0xff003fc2, 0xff0522e6, 0xff4c0bdd, 0xff8e02a8, 0xffbb0756, 0xffc51b00, 0xffaa3700, 0xff715400, 0xff2a6a00, 0xff007400, 0xff006e20, 0xff005b79, 0xff000001, // 0xff000001, 0xff000001,
   0xfff7f7f7, 0xff2499ff, 0xff5e7bff, 0xffa665ff, 0xffea5bff, 0xffff61b0, 0xffff7456, 0xffff910c, 0xffccae00, 0xff84c500, 0xff40cf26, 0xff13c979, 0xff09b5d4, 0xff323232, // 0xff000001, 0xff000001,
   0xfff7f7f7, 0xffa2d1ff, 0xffb9c5ff, 0xffd6bcff, 0xfff2b8fc, 0xffffbadb, 0xffffc2b6, 0xfffdce98, 0xffe6da89, 0xffc8e38d, 0xffade7a2, 0xff9be5c4, 0xff97dde9, 0xffa7a7a7, // 0xff000001, 0xff000001
};
*/
