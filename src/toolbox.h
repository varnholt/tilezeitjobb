#ifndef TOOLBOX_H
#define TOOLBOX_H

// tile
#include "constants.h"

// Qt
#include <QObject>

class Tool;
class ToolPaletteBrush;
class ToolPaletteSetup;
class ToolPixelBrush;
class ToolPixelPencil;
class ToolErase;
class ToolColorPicker;
class ToolMove;
class ToolSprite;


class ToolBox : public QObject
{
   Q_OBJECT

public:

   static ToolBox* getInstance();

   Tool *getCurrentTool() const;
   void setCurrentTool(Tool *currentTool);

   void selectTool(ToolType type);


public slots:

   void selectPaletteSetup();
   void selectPaletteBrush();
   void selectPixelPencil();
   void selectPixelBrush();
   void selectErase();
   void selectColorPicker();
   void selectMove();
   void selectSprite();


protected:

   ToolBox(QObject* parent = 0);


   Tool* mCurrentTool;

   ToolPaletteSetup* mPaletteSetup;
   ToolPaletteBrush* mPaletteBrush;
   ToolPixelBrush* mPixelBrush;
   ToolPixelPencil* mPixelPencil;
   ToolErase* mErase;
   ToolColorPicker* mColorPicker;
   ToolMove* mMove;
   ToolSprite* mSprite;

   static ToolBox* sToolBox;
};

#endif // TOOLBOX_H



