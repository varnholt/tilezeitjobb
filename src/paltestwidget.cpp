#include "paltestwidget.h"

// Qt
#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QSet>
#include <QTextStream>
#include <QTimer>

// tile
#include "nespalette.h"
#include "palette.h"
#include "project.h"
#include "settings.h"

#define _USE_MATH_DEFINES 1
#define PAL_TEST_DIR "data/paltesteffect"

#include <math.h>


PalTestWidget::PalTestWidget(QWidget *parent)
 : QWidget(parent),
   mHeight(0),
   mWidth(0),
   mFrame(0),
   mEnabled(true)
{
   loadImages();

   QTimer* t = new QTimer(this);
   connect(t, SIGNAL(timeout()), this, SLOT(update()));
   t->setInterval(16);
   t->start();
}


void PalTestWidget::loadImages()
{
   QString path = QString("%1/%2").arg(QDir::currentPath()).arg(PAL_TEST_DIR);
   QDir dir(path);
   QStringList filter;
   filter << "*.bmp";

   QStringList entries = dir.entryList(filter, QDir::Files);

   QFile whiteListFile(QString("%1/%2").arg(path).arg("filter.txt"));

   QSet<QString> whiteList;
   bool filtered = false;
   if (whiteListFile.open(QIODevice::ReadOnly | QIODevice::Text))
   {
      filtered = true;
      QTextStream in(&whiteListFile);
      while (!in.atEnd())
      {
         QString line = in.readLine();
         whiteList.insert(line);
      }
   }

   foreach (QString filename, entries)
   {
      QImage img(QString("%1/%2").arg(path).arg(filename));

      // img = img.convertToFormat(QImage::Format_ARGB32);

      if (filtered)
      {
         if  (whiteList.contains(filename))
         {
            mImages << img;
         }
      }
      else
      {
         mImages << img;
      }
   }

   if (mImages.size() > 0)
   {
      mHeight = mImages[0].height();
      mWidth = mImages[0].width();
   }
}


int PalTestWidget::f(int scanline)
{
   float y = 0.0f;

   y = (scanline / (float)mImages.size()) * M_PI;

   y = sin( cos(mFrame * 0.03 + y * 4.0f) * y * 0.2f + mFrame * 0.02f);

   y += 1.0f;
   y *= 0.5f;

   y *= mImages.size();

   if (y > mImages.size() - 1)
      y = mImages.size() - 1;

   return y;
}


int PalTestWidget::f()
{
   return mFrame % mImages.size();
}


void PalTestWidget::paintEvent(QPaintEvent *event)
{
   int selectedPalIndex = NesPalette::getInstance()->getSelectedPalIndex();
   Palette pal = Project::getInstance()->getNameTablePalette(selectedPalIndex);
   int c1 = pal.getColor(0);
   int c2 = pal.getColor(1);
   int c3 = pal.getColor(2);
   int c4 = pal.getColor(3);

   if (
          (mHeight == 0 || mWidth == 0)
       || (c1 == c2 == c3 == c4)
       || (!Settings::getInstance()->isShowPalTestEffectEnabled())
   )
   {
      QPainter framePainter;
      framePainter.begin(this);
      framePainter.fillRect(0, 0, 320, 240, Qt::black);
      framePainter.end();
      return;
   }

   for (int i = 0; i < mImages.size(); i++)
   {
      mImages[i].setColor(0, c1);
      mImages[i].setColor(1, c2);
      mImages[i].setColor(2, c3);
      mImages[i].setColor(3, c4);
   }

   int y = 0;
   QImage dst(mWidth, mHeight, QImage::Format_ARGB32);
   QPainter dstPainter;
   dstPainter.begin(&dst);

   for (int scanLine = 0; scanLine < mHeight; scanLine++)
   {
      // y = f();
      y = f(scanLine);
      const QImage& src = mImages[y];
      dstPainter.drawImage(0, scanLine, src, 0, scanLine);
   }

   dstPainter.end();

   dst = dst.scaled(mWidth * 3, mHeight * 3);
   QPainter framePainter;
   framePainter.begin(this);

   framePainter.fillRect(0, 0, 320, 240, c1);


   framePainter.drawImage(72, 8, dst);
   framePainter.end();

   mFrame++;
}


bool PalTestWidget::isEnabled() const
{
   return mEnabled;
}


void PalTestWidget::setEnabled(bool enabled)
{
   mEnabled = enabled;
}
