// base
#include "toolbox.h"

// tile
#include "toolcolorpicker.h"
#include "toolerase.h"
#include "toolmove.h"
#include "toolsprite.h"
#include "toolpalettebrush.h"
#include "toolpalettesetup.h"
#include "toolpixelbrush.h"
#include "toolpixelpencil.h"


ToolBox* ToolBox::sToolBox = 0;


ToolBox::ToolBox(QObject *parent)
 : QObject(parent)
{
   mPaletteSetup = new ToolPaletteSetup();
   mPaletteBrush = new ToolPaletteBrush();
   mPixelBrush = new ToolPixelBrush();
   mPixelPencil = new ToolPixelPencil();
   mErase = new ToolErase();
   mColorPicker = new ToolColorPicker();
   mMove = new ToolMove();
   mSprite = new ToolSprite();

   mCurrentTool = mPaletteBrush;

   sToolBox = this;
}


Tool *ToolBox::getCurrentTool() const
{
   return mCurrentTool;
}


void ToolBox::selectTool(ToolType type)
{
   switch (type)
   {
      case ToolTypePaletteSetup:
         mCurrentTool = mPaletteSetup;
         break;
      case ToolTypePixelBrush:
         mCurrentTool = mPixelBrush;
         break;
      case ToolTypePaletteBrush:
         mCurrentTool = mPaletteBrush;
         break;
      case ToolTypePixelPencil:
         mCurrentTool = mPixelPencil;
         break;
      case ToolTypeMove:
         mCurrentTool = mMove;
         break;
      case ToolTypeErase:
         mCurrentTool = mErase;
         break;
      case ToolTypeColorPicker:
         mCurrentTool = mColorPicker;
         break;
      case ToolTypeSprite:
         mCurrentTool = mSprite;
         break;

      default:
         break;
   }
}


void ToolBox::selectPaletteSetup()
{
   selectTool(ToolTypePaletteSetup);
}


void ToolBox::selectPaletteBrush()
{
   selectTool(ToolTypePaletteBrush);
}


void ToolBox::selectPixelPencil()
{
   selectTool(ToolTypePixelPencil);
}


void ToolBox::selectPixelBrush()
{
   selectTool(ToolTypePixelBrush);
}


void ToolBox::selectErase()
{
   selectTool(ToolTypeErase);
}


void ToolBox::selectColorPicker()
{
   selectTool(ToolTypeColorPicker);
}


void ToolBox::selectMove()
{
   selectTool(ToolTypeMove);
}


void ToolBox::selectSprite()
{
   selectTool(ToolTypeSprite);
}


void ToolBox::setCurrentTool(Tool *currentTool)
{
   mCurrentTool = currentTool;
}


ToolBox *ToolBox::getInstance()
{
   if (!sToolBox)
      new ToolBox();

   return sToolBox;
}



