#include "nespalettewidget.h"

// tile
#include "constants.h"
#include "nespalette.h"

// Qt
#include <QMouseEvent>
#include <QPainter>





NesPaletteWidget::NesPaletteWidget(QWidget *parent)
 : QWidget(parent)
{
   connect(
      NesPalette::getInstance(),
      SIGNAL(changed()),
      this,
      SLOT(update())
   );
}


void NesPaletteWidget::paintEvent(QPaintEvent * /*e*/ )
{
   int w = PAL_BLOCK_SIZE;
   int h = PAL_BLOCK_SIZE;

   int palWidth = PAL_WIDTH;
   int palHeight = PAL_HEIGHT;

   int x1 = 0;
   int y1 = 0;

   QPainter painter(this);

   int counter = 0;

   for (int x = 0; x < palWidth; x++)
   {
      for (int y = 0; y < palHeight; y++)
      {
         x1 = x * w;
         y1 = y * w;

         if (counter < 55)
         {
            QColor c = QColor::fromRgba(sNesPalette[y * palWidth + x]);
            QBrush b(c);
            painter.setBrush(b);
            painter.drawRect(x1, y1, w, h);
         }
         else
         {
            QBrush b(Qt::Dense4Pattern);
            painter.setBrush(b);
            painter.drawRect(x1, y1, w, h);
         }

         counter ++;
      }
   }

   // bg
   x1 = 0;
   y1 = (palHeight + 1) * h;
   QColor bg = QColor::fromRgba(NesPalette::getInstance()->getBg());
   QBrush brushBg(bg);
   painter.setBrush(brushBg);
   painter.drawRect(x1, y1, w, h);

   // fg
   x1 = PAL_BLOCK_SIZE;
   QColor fg = QColor::fromRgba(NesPalette::getInstance()->getFg());
   QBrush brushFg(fg);
   painter.setBrush(brushFg);
   painter.drawRect(x1, y1, w, h);

//   QPainterPath path;
//   QPen pen;
//   QFont font("Arial", 10, 5, true);

//   pen.setWidth(2);
//   pen.setColor(Qt::white);
//   painter.setFont(font);
//   painter.setPen(pen);

//   path.addText(PAL_BLOCK_SIZE/2, y1, font, "bg");
//   painter.drawPath(path);
}




void NesPaletteWidget::mousePressEvent(QMouseEvent * e)
{
   if (e->button() == Qt::LeftButton)
   {
      unsigned int color = getColor(e->pos());
      NesPalette::getInstance()->setFg(color);
      update();
   }
   else if (e->button() == Qt::RightButton)
   {
      unsigned int color = getColor(e->pos());
      NesPalette::getInstance()->setBg(color);
      update();
   }
}


unsigned int NesPaletteWidget::getColor(const QPoint &pos)
{
   unsigned int color = 0;

   int x = pos.x() / PAL_BLOCK_SIZE;
   int y = pos.y() / PAL_BLOCK_SIZE;

   color = sNesPalette[y * PAL_WIDTH + x];

   return color;
}


