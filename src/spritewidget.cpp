#include "spritewidget.h"

// Qt
#include <QMouseEvent>
#include <QPainter>
#include <QRect>

// tile
#include "project.h"
#include "settings.h"

SpriteWidget::SpriteWidget(QWidget *parent)
 : QWidget(parent),
   mTileX(0),
   mTileY(0)
{
   setFixedSize(256, 240);
   setMouseTracking(true);
}


QImage *SpriteWidget::getImage()
{
   return &mImage;
}


void SpriteWidget::openImage(const QString & filename)
{
   mImage.load(filename);
}


void SpriteWidget::paintEvent(QPaintEvent * /*e*/)
{
   //   // init
   //   Project* project = Project::getInstance();
   //   Settings* settings = Settings::getInstance();
   //   int w = settings->getBlockStepsX();
   //   int h = settings->getBlockStepsY();

   QPainter painter(this);

   // painter.fillRect(0, 0, 256, 240, Qt::darkGray);
   painter.drawImage(0,0, mImage);

   // alternate blocks
   QPen pen;
   int count = 0;
   for (int y = 0; y < 240; y+=8)
   {
      for (int x = 0; x < 256; x+=8)
      {
         int alpha = (count % 2) == 0 ? 40 : 0;
         painter.setPen(pen);
         painter.fillRect(x, y, 8, 8, (QColor(0, 0, 0, alpha)));
         count++;
      }
      count++;
   }

   // draw mouse
   int mx = mMousePos.x() & 0xF8;
   int my = mMousePos.y() & 0xF8;
   QRect mouseRect(mx, my, 8, 8);
   QPen mousePen(QColor(255,0,0,60));
   painter.setPen(mousePen);
   painter.drawRect(mouseRect);

   // draw selected tile
   int sx = mTileX * 8;
   int sy = mTileY * 8;
   painter.fillRect(sx, sy, 8, 8, QColor(255,0,0,60));
}


void SpriteWidget::mouseMoveEvent(QMouseEvent * e)
{
   mMousePos = e->pos();
   update();
}


void SpriteWidget::mousePressEvent(QMouseEvent * pos)
{
   mTileX = pos->x() / 8;
   mTileY = pos->y() / 8;
}


int SpriteWidget::getTileY() const
{
   return mTileY;
}


void SpriteWidget::setTileY(int tileY)
{
   mTileY = tileY;
}


int SpriteWidget::getTileX() const
{
   return mTileX;
}


void SpriteWidget::setTileX(int tileX)
{
   mTileX = tileX;
}
