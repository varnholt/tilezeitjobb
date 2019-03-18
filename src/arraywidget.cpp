// base
#include "arraywidget.h"

// Qt
#include <QPainter>

// tile
#include "project.h"
#include "settings.h"


ArrayWidget::ArrayWidget(QWidget *parent) : QWidget(parent)
{
   setFixedSize(256, 240);
}



void ArrayWidget::paintEvent(QPaintEvent * /*e*/)
{
   // init
   Project* project = Project::getInstance();
   Settings* settings = Settings::getInstance();

   int w = settings->getBlockStepsX();
   int h = settings->getBlockStepsY();
   int dx  = width() / w;
   int dy  = height() / h;

   QPainter painter(this);
   QFontMetrics metrics(painter.font());
   int fontOffsetX = 0;
   int fontOffsetY = 0;

   int value = 0;
   QString verbose;

   // draw grid
   painter.fillRect(0, 0, width(), height(), QColor("#111111"));
   painter.setPen(QColor(30,30,30));

   for (int y = 0; y <= height(); y += dy)
      painter.drawLine(0, y, width(), y);
   for (int x = 0; x <= width(); x += dx)
      painter.drawLine(x, 0, x, height());

   painter.setPen(QColor(100,100,100));

   for (int blockY = 0; blockY < h; blockY++)
   {
      for (int blockX = 0; blockX < w; blockX++)
      {
         value = project->getNameTablePalIndex(blockX, blockY);

         // draw value
         verbose = QString::number(value);
         fontOffsetX = (dx - metrics.boundingRect(verbose).width()) / 2;
         fontOffsetY = -(dy - metrics.boundingRect(verbose).height()) / 2;
         painter.drawText(
            fontOffsetX + blockX * dx,
            fontOffsetY + (blockY + 1) * dy,
            verbose
         );
      }
   }
}
