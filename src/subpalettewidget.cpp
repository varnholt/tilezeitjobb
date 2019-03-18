#include "subpalettewidget.h"

// Qt
#include <QMouseEvent>
#include <QPainter>

// tile
#include "constants.h"
#include "nespalette.h"
#include "tool.h"
#include "toolbox.h"


SubPaletteWidget::SubPaletteWidget(QWidget *parent)
 : QWidget(parent)
{
}



void SubPaletteWidget::paintEvent(QPaintEvent * /*e*/)
{
   QPainter painter(this);

   for (int i = 0; i < 4; i++)
   {
      QColor c = QColor::fromRgba(mPal.getColor(i));
      QBrush b(c);
      painter.setBrush(b);
      painter.drawRect(i * PAL_BLOCK_SIZE, 0, PAL_BLOCK_SIZE, PAL_BLOCK_SIZE);
   }
}


void SubPaletteWidget::mousePressEvent(QMouseEvent * e)
{
   // palette setup
   if (ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypePaletteSetup)
   {
      if (e->button() == Qt::LeftButton)
      {
         int index = e->pos().x() / PAL_BLOCK_SIZE;
         mPal.setColor(index, NesPalette::getInstance()->getFg());
         update();

         emit changed();
      }
   }

   // all other tools can clicks for color selection
   else
   {
      int index = e->pos().x() / PAL_BLOCK_SIZE;
      unsigned int color = mPal.getColor(index);

      if (e->button() == Qt::LeftButton)
      {
         NesPalette::getInstance()->setFg(color);
      }
      else if (e->button() == Qt::RightButton)
      {
         NesPalette::getInstance()->setBg(color);
      }
   }
}


Palette SubPaletteWidget::getPal() const
{
   return mPal;
}


void SubPaletteWidget::setPal(const Palette &pal)
{
   mPal = pal;
}
