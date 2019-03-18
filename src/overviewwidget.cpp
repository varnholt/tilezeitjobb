// base
#include "overviewwidget.h"

// Qt
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QShortcut>
#include <QTimer>

// tile
#include "layerstack.h"
#include "nespalette.h"
#include "project.h"
#include "settings.h"
#include "sharedglcontext.h"
#include "spritelayer.h"
#include "tool.h"
#include "toolbox.h"
#include "toolpixelpencil.h"


OverviewWidget::OverviewWidget(QWidget *parent)
 : QGLWidget(parent, SharedGlContext::getInstance()->getWidget()),
   mTexture(0),
   mNameTableTexture(0),
   mZoom(2),
   mOriginalImage(0),
   mNameTableImageSource(0),
   mNameTableImageTarget(0),
   mPaintArea(0),
   mFileWatcher(0),
   mLeftButtonPressed(false),
   mRightButtonPressed(false),
   mWithinMoveEvent(false)
{
   SharedGlContext::getInstance()->setWidget(this);

   Settings* settings = Settings::getInstance();
   int width = settings->getWidth();
   int height = settings->getHeight();
   QSize size(width, height);

   mPaintArea = new QImage(size, QImage::Format_ARGB32);
   mPaintArea->fill(QColor(0,0,0,0));

   grabKeyboard();
   setMouseTracking(true);

   QShortcut* shortcut = new QShortcut(QKeySequence("Ctrl+S"), this);
   connect(
      shortcut,
      SIGNAL(activated()),
      this,
      SLOT(dump())
   );
}



void OverviewWidget::openImage(const QString & tmpFileName)
{
   QString filename = tmpFileName;
   if (filename.isEmpty())
      filename = mFilePath;

   delete mOriginalImage;
   mOriginalImage = new QImage();

   mOriginalImage->load(filename);
   mOriginalImage->convertToFormat(QImage::Format_Indexed8);
   updateTexture();

   QStringList paths;
   paths << QFileInfo(filename).absoluteFilePath();

   mFilePath = filename;
   delete mFileWatcher;
   mFileWatcher = new QFileSystemWatcher(paths);
   connect(
      mFileWatcher,
      SIGNAL(fileChanged(QString)),
      this,
      SLOT(fileChanged(QString))
   );
}


void OverviewWidget::fileChanged(const QString & filename)
{
   if (QFileInfo(filename).fileName() == QFileInfo(mFilePath).fileName())
   {
      QTimer::singleShot(2000, this, SLOT(openImage()));
      // openImage(mFilePath);
   }
}


void OverviewWidget::reduceToPalette(
   QImage* src,
   QImage* dst
,  int stepsX,
   int stepsY
)
{
   // init
   Settings* settings = Settings::getInstance();
   int blockSizePx = settings->getBlockSizePx();

   // take these from the project settings if not given
   if (stepsX == -1)
      stepsX = settings->getBlockStepsX();
   if (stepsY == -1)
      stepsY = settings->getBlockStepsY();

   Project* project = Project::getInstance();

   QList<Palette> pals = project->getNameTablePalettes();
   Palette pal;

   // apply color reduction depending on the palette for each block
   QList<DitherColor> ditherColors;
   QRect rect;

   int palIndex = 0;
   for (int y = 0; y < stepsY; y++)
   {
      for (int x = 0; x < stepsX; x++)
      {
         rect.setLeft(x * blockSizePx);
         rect.setRight((x + 1) * blockSizePx - 1);
         rect.setTop(y * blockSizePx);
         rect.setBottom((y + 1) * blockSizePx - 1);

         palIndex = project->getNameTablePalIndex(x, y);

         QVector<unsigned int> palColors;
         pal = pals.at(palIndex);
         for (int pi = 0; pi < 4; pi++)
            palColors.push_back(pal.getColor(pi));

         // process with dither
         ditherColors = mColorReduction.buildPaletteDither(palColors, 4);

         mColorReduction.reduceColors(
            *dst,
            *src,
            rect,
            ditherColors,
            palColors
         );
      }
   }
}


void OverviewWidget::updateTexture()
{
   if (!mOriginalImage)
      return;

   // init
   Settings* settings = Settings::getInstance();
   int width = settings->getWidth();
   int height = settings->getHeight();
   QSize size(width, height);

   Project* project = Project::getInstance();
   int offsetX = project->getOffsetX();
   int offsetY = project->getOffsetY();

   deleteTexture(mTexture);
   deleteTexture(mNameTableTexture);

   delete mNameTableImageSource;
   delete mNameTableImageTarget;
   mNameTableImageSource = new QImage(size, QImage::Format_ARGB32);
   mNameTableImageTarget = new QImage(size, QImage::Format_ARGB32);

   mNameTableImageSource->fill(
      QColor::fromRgba(NesPalette::getInstance()->getBg())
   );

   // copy artist's image into texture
   QPainter painter(mNameTableImageSource);
   painter.drawImage(
      offsetX,
      offsetY,
      *mOriginalImage
   );
   painter.end();

   // copy drawn data into texture
   QPainter paintAreaPainter(mNameTableImageSource);
   paintAreaPainter.drawImage(
      0,
      0,
      *mPaintArea
   );
   paintAreaPainter.end();

   if (settings->getApplyColorReduction())
   {
      // apply palettes
      reduceToPalette(mNameTableImageSource, mNameTableImageTarget);
   }
   else
   {
      *mNameTableImageTarget = *mNameTableImageSource;
   }

//   // copy drawn data onto the image
//   QPainter paintAreaPainter(mNameTableImageTarget);
//   paintAreaPainter.drawImage(
//      0,
//      0,
//      *mPaintArea
//   );
//   paintAreaPainter.end();

   // copy sprite data onto image
   if (settings->isShowSpritesEnabled())
   {
      // apply palettes if needed
      QList<Palette> spritePals = project->getSpritePalettes();
      Palette spritePal;
      QList<DitherColor> ditherColors;

      unsigned int bg = NesPalette::getInstance()->getBg();
      QPainter spritePainter(mNameTableImageTarget);
      LayerStack* stack = LayerStack::getInstance();
      foreach (Layer* layer, stack->getLayers())
      {
         if (layer->getType() == LayerTypeSprite)
         {
            SpriteLayer* sprite = (SpriteLayer*)layer;
            if (sprite && !sprite->getImage()->isNull())
            {
               QImage spriteImage =
                  sprite->getImage()->copy(
                     sprite->getTileX() * 8,
                     sprite->getTileY() * 8,
                     8,
                     8
                  );

               for (int y = 0; y < 8; y++)
               {
                  for (int x = 0; x < 8; x++)
                  {
                     if (spriteImage.pixel(x, y) == bg)
                     {
                        spriteImage.setPixel(x, y, 0);
                     }
                  }
               }

               if (settings->getApplyColorReduction())
               {
                  QImage spriteImageReduced = spriteImage.copy(0, 0, 8, 8);

                  QVector<unsigned int> palColors;
                  spritePal = spritePals.at(sprite->getPalIndex());
                  for (int pi = 0; pi < 4; pi++)
                     palColors.push_back(spritePal.getColor(pi));

                  // process with dither
                  ditherColors = mColorReduction.buildPaletteDither(palColors, 4);

                  mColorReduction.reduceColors(
                     spriteImageReduced,
                     spriteImage,
                     QRect(0, 0, 8, 8),
                     ditherColors,
                     palColors
                  );

                  spritePainter.drawImage(
                     sprite->getScreenOffsetX(),
                     sprite->getScreenOffsetY(),
                     spriteImageReduced
                  );
               }
               else
               {
                  spritePainter.drawImage(
                     sprite->getScreenOffsetX(),
                     sprite->getScreenOffsetY(),
                     spriteImage
                  );
               }
            }
         }
      }
      spritePainter.end();
   }

   // finally build the textures shown in paintgl
   mTexture = bindTexture(*mOriginalImage);
   mNameTableTexture = bindTexture(*mNameTableImageTarget);

   emit textureChanged(mNameTableTexture);

   update();
}


void OverviewWidget::dump()
{
   if (mNameTableImageTarget)
      mNameTableImageTarget->save("dump.png");
}


void OverviewWidget::initializeGL()
{
   glClearColor(0.15f, 0.15f, 0.15f, 1.0f);

   // have blending
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   connect(
      NesPalette::getInstance(),
      SIGNAL(changed()),
      this,
      SLOT(update())
   );

   QTimer* t = new QTimer(this);
   connect(
      t,
      SIGNAL(timeout()),
      this,
      SLOT(updateTexture())
   );

   t->setInterval(100);
   t->start();
}


void OverviewWidget::resizeGL(int width, int height)
{
   // reset the current viewport
   glViewport(0, 0, width, height);

   // setup projection matrix
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0, width, height, 0, -1, 1);

   // setup modelview matrix
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}


void OverviewWidget::drawTexturedQuad(int texture, int width, int height, float alpha)
{
   glColor4f(1.0f, 1.0f, 1.0f, alpha);

   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   glBegin(GL_QUADS);
   glTexCoord2f(0, 1); glVertex2i(0,             0             );
   glTexCoord2f(1, 1); glVertex2i(width * mZoom, 0             );
   glTexCoord2f(1, 0); glVertex2i(width * mZoom, height * mZoom);
   glTexCoord2f(0, 0); glVertex2i(0,             height * mZoom);
   glEnd();

   glDisable(GL_TEXTURE_2D);
}



void OverviewWidget::drawHighlightedBlocks()
{
   // init
   Settings* settings = Settings::getInstance();

   if (settings->getHighlightSelectedPalette())
   {
      int blockSizePx = settings->getBlockSizePx();
      int stepsX = settings->getBlockStepsX();
      int stepsY = settings->getBlockStepsY();

      Project* project = Project::getInstance();

      int vxLeft = 0;
      int vxRight = 0;
      int vyTop = 0;
      int vyBottom = 0;

      int selectedPalIndex = NesPalette::getInstance()->getSelectedPalIndex();

      for (int by = 0; by < stepsY; by++)
      {
         for (int bx = 0; bx < stepsX; bx++)
         {
            int palIndex = project->getNameTablePalIndex(bx, by);

            if (palIndex == selectedPalIndex)
            {
               vxLeft   = bx * blockSizePx * mZoom;
               vxRight  = vxLeft + BLOCK_SIZE_PX * mZoom;
               vyTop    = by * blockSizePx * mZoom;
               vyBottom = vyTop + BLOCK_SIZE_PX * mZoom;

               glColor4f(0.0f, 0.0f, 1.0f, 0.2f);
               glBegin(GL_QUADS);
               glVertex2i(vxLeft,  vyTop);
               glVertex2i(vxLeft,  vyBottom);
               glVertex2i(vxRight, vyBottom);
               glVertex2i(vxRight, vyTop);
               glEnd();
            }
         }
      }
   }
}


void OverviewWidget::drawCursor()
{
   Settings* settings = Settings::getInstance();
   int blockSizePx = settings->getBlockSizePx();

   int vxLeft   = 0;
   int vxRight  = 0;
   int vyTop    = 0;
   int vyBottom = 0;

   if (ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeSprite)
   {
      vxLeft   = mCurrentPixel.x() * mZoom;
      vxRight  = vxLeft + mZoom;
      vyTop    = mCurrentPixel.y() * mZoom;
      vyBottom = vyTop + mZoom;

      // TODO: use selected sprite as texture
   }
   else if (ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypePixelPencil)
   {
      vxLeft   = mCurrentPixel.x() * mZoom;
      vxRight  = vxLeft + mZoom;
      vyTop    = mCurrentPixel.y() * mZoom;
      vyBottom = vyTop + mZoom;
   }
   else
   {
      vxLeft   = mCurrentBlock.x() * blockSizePx * mZoom;
      vxRight  = vxLeft + BLOCK_SIZE_PX * mZoom;
      vyTop    = mCurrentBlock.y() * blockSizePx * mZoom;
      vyBottom = vyTop + BLOCK_SIZE_PX * mZoom;
   }

   glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
   glBegin(GL_QUADS);
   glVertex2i(vxLeft,  vyTop);
   glVertex2i(vxLeft,  vyBottom);
   glVertex2i(vxRight, vyBottom);
   glVertex2i(vxRight, vyTop);
   glEnd();
}


void OverviewWidget::paintGL()
{
   if (!mOriginalImage)
      return;

   /*
      http://wiki.nesdev.com/w/index.php/PPU_nametables
      30 rows of 32 tiles each
   */

   Settings* settings = Settings::getInstance();
   int gridSizePx = settings->getGridSizePx();
   int gridStepsX = settings->getGridStepsX();
   int gridStepsY = settings->getGridStepsY();
   int width = settings->getWidth();
   int height = settings->getHeight();

   Project* p = Project::getInstance();
   int offsetX = p->getOffsetX();
   int offsetY = p->getOffsetY();

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();

   glTranslatef(
      - mScrollOffset.x(),
      - mScrollOffset.y(),
      0.0f
   );

   // fill background
   QColor bg = QColor::fromRgba(NesPalette::getInstance()->getBg());
   glColor4f(bg.redF(), bg.greenF(), bg.blueF(), 1.0f);
   glBegin(GL_QUADS);
   glVertex2f(0.0f, 0.0f);
   glVertex2f(0.0f, height * mZoom);
   glVertex2f(width * mZoom, height * mZoom);
   glVertex2f(width * mZoom, 0);
   glEnd();

   // draw texture
   glPushMatrix();

   drawTexturedQuad(
      mNameTableTexture,
      mNameTableImageTarget->width(),
      mNameTableImageTarget->height()
   );

   glTranslatef(
      offsetX * mZoom,
      offsetY * mZoom,
      0.0f
   );

   if (settings->getBlendOverOriginalImage())
   {
      drawTexturedQuad(
         mTexture,
         mOriginalImage->width(),
         mOriginalImage->height(),
         0.6f
      );
   }

   glPopMatrix();

   if (settings->getGridSize() != GridSizeNone)
   {
      glColor4f(1.0f, 0.0f, 0.0f, 0.2f);
      glBegin(GL_LINES);

      // horizontal lines
      for (int y = 0; y <= gridStepsY; y++)
      {
         glVertex2i(0,             y * gridSizePx * mZoom);
         glVertex2i(width * mZoom, y * gridSizePx * mZoom);
      }

      // vertical lines
      for (int x = 0; x <= gridStepsX; x++)
      {
         glVertex2i(x * gridSizePx * mZoom, 0             );
         glVertex2i(x * gridSizePx * mZoom, height * mZoom);
      }

      glEnd();
   }

   // draw border around selected layer
   int index = LayerStack::getInstance()->getCurrentIndex();
   Layer* layer = LayerStack::getInstance()->getLayer(index);
   if (layer->getType() == LayerTypeSprite)
   {
      glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
      glBegin(GL_LINES);

      SpriteLayer* spriteLayer = (SpriteLayer*)layer;
      int sx = spriteLayer->getScreenOffsetX();
      int sy = spriteLayer->getScreenOffsetY();

      glVertex2i(sx       * mZoom, sy * mZoom); // -
      glVertex2i((sx + 8) * mZoom, sy * mZoom);

      glVertex2i(sx * mZoom, sy       * mZoom); // |
      glVertex2i(sx * mZoom, (sy + 8) * mZoom);

      glVertex2i(sx       * mZoom, (sy + 8) * mZoom); // _
      glVertex2i((sx + 8) * mZoom, (sy + 8) * mZoom);

      glVertex2i((sx + 8) * mZoom, sy       * mZoom); //  |
      glVertex2i((sx + 8) * mZoom, (sy + 8) * mZoom);

      glEnd();
   }

   // draw current block
   drawCursor();
   drawHighlightedBlocks();
}


QPoint OverviewWidget::getCurrentBlock(const QPoint& pixelPos)
{
   Settings* settings = Settings::getInstance();

   int blockSizePx = settings->getBlockSizePx();
   int x = 0;
   int y = 0;

   x = (pixelPos.x() + mScrollOffset.x()) / (blockSizePx * mZoom);
   y = (pixelPos.y() + mScrollOffset.y()) / (blockSizePx * mZoom);

   return QPoint(x, y);
}


QPoint OverviewWidget::getCurrentPixel(const QPoint &pixelPos)
{
   int x = 0;
   int y = 0;

   x = (pixelPos.x() + mScrollOffset.x()) / mZoom;
   y = (pixelPos.y() + mScrollOffset.y()) / mZoom;

   return QPoint(x, y);
}


QPoint OverviewWidget::getPreviousPixelPosition() const
{
   return mPreviousPixelPosition;
}


void OverviewWidget::setPreviousPixelPosition(const QPoint &previousPixelPosition)
{
   mPreviousPixelPosition = previousPixelPosition;
}

QImage *OverviewWidget::getNameTableImageSource() const
{
   return mNameTableImageSource;
}


QImage *OverviewWidget::getOriginalImage() const
{
   return mOriginalImage;
}


QImage *OverviewWidget::getNameTableImageTarget() const
{
   return mNameTableImageTarget;
}


bool OverviewWidget::isWithinMoveEvent() const
{
   return mWithinMoveEvent;
}


void OverviewWidget::setWithinMoveEvent(bool withinMoveEvent)
{
   mWithinMoveEvent = withinMoveEvent;
}


bool OverviewWidget::isRightButtonPressed() const
{
   return mRightButtonPressed;
}

void OverviewWidget::setRightButtonPressed(bool rightButtonPressed)
{
   mRightButtonPressed = rightButtonPressed;
}

bool OverviewWidget::isLeftButtonPressed() const
{
   return mLeftButtonPressed;
}

void OverviewWidget::setLeftButtonPressed(bool leftButtonPressed)
{
   mLeftButtonPressed = leftButtonPressed;
}


void OverviewWidget::applyTool(
   QEvent::Type type,
   const QPoint& eventPos
)
{
   QPoint pixelPos = getCurrentPixel(eventPos);
   QPoint blockPos = getCurrentBlock(eventPos);

   Tool* tool = ToolBox::getInstance()->getCurrentTool();
   ToolType toolType = tool->getToolType();

   // palette tool
   if (toolType == ToolTypePaletteBrush)
   {
      if (isLeftButtonPressed())
      {
         qDebug("block clicked: %d, %d", blockPos.x(), blockPos.y());
         emit blockClicked(blockPos.x(), blockPos.y());
      }
   }

   // pixel tool
   else if (toolType == ToolTypePixelPencil)
   {
      if (isRightButtonPressed() || isLeftButtonPressed())
      {
         int size = ((ToolPixelPencil*)tool)->getSize();

         unsigned int color = 0;

         if (isRightButtonPressed())
            color = NesPalette::getInstance()->getBg();
         else
            color = NesPalette::getInstance()->getFg();

         QPainter p(mPaintArea);
         QPen pen(QColor::fromRgba(color));
         pen.setWidth(size);
         p.setPen(pen);

         if (isWithinMoveEvent())
         {
            p.drawLine(
               getPreviousPixelPosition(),
               pixelPos
            );
         }
         else
         {
            p.drawPoint(pixelPos);
         }

         setPreviousPixelPosition(pixelPos);
      }
   }

   // erase tool
   else if (toolType == ToolTypeErase)
   {
      if (isRightButtonPressed() || isLeftButtonPressed())
      {
         int size = ((ToolPixelPencil*)tool)->getSize();

         QRgb color = NesPalette::getInstance()->getBg();
         QPainter p(mPaintArea);
         QPen pen(QColor::fromRgba(color));
         pen.setWidth(size);
         p.setPen(pen);

         if (isWithinMoveEvent())
         {
            p.drawLine(
               getPreviousPixelPosition(),
               pixelPos
            );
         }
         else
         {
            p.drawPoint(pixelPos);
         }

         setPreviousPixelPosition(pixelPos);
      }
   }

   else if (toolType == ToolTypeSprite && type == QEvent::MouseButtonPress)
   {
      if (isRightButtonPressed() || isLeftButtonPressed())
      {
         // request sprite create at current pos
         emit createSprite(pixelPos.x(), pixelPos.y());
      }
   }
}


void OverviewWidget::mousePressEvent(QMouseEvent * e)
{
   if (e->button() == Qt::LeftButton)
      setLeftButtonPressed(true);

   if (e->button() == Qt::RightButton)
      setRightButtonPressed(true);

   if (
          e->button() == Qt::MiddleButton
       || e->button() == Qt::LeftButton && ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove
       || e->button() == Qt::RightButton && ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove
   )
   {
      mMousePos = e->pos();
   }

   applyTool(
      e->type(),
      e->pos()
   );
}


void OverviewWidget::mouseMoveEvent(QMouseEvent * e)
{
   setWithinMoveEvent(true);

   bool updateWidget = false;

   // movement within image
   if (
          (e->buttons() & Qt::MiddleButton)
       || ((e->buttons() & Qt::LeftButton) && ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove)
       || ((e->buttons() & Qt::RightButton) && ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove)
   )
   {
      mScrollOffset += (mMousePos - e->pos());
      mMousePos = e->pos();
      updateWidget = true;
   }

   // apply tool
   applyTool(
      e->type(),
      e->pos()
   );

   // update block or pixel highlights
   QPoint currentPixel = getCurrentPixel(e->pos());
   QPoint currentBlock = getCurrentBlock(e->pos());

   if (currentPixel != mCurrentPixel)
   {
      mCurrentPixel = currentPixel;
      updateWidget = true;
   }

   if (currentBlock != mCurrentBlock)
   {
      mCurrentBlock = currentBlock;
      updateWidget = true;
   }

   if (updateWidget)
   {
      update();
   }

   setWithinMoveEvent(false);
}


void OverviewWidget::mouseReleaseEvent(QMouseEvent* e)
{
   if (e->button() == Qt::LeftButton)
      setLeftButtonPressed(false);

   if (e->button() == Qt::RightButton)
      setRightButtonPressed(false);
}


void OverviewWidget::zoomOut()
{
   if (mZoom - 1 > 0)
   {
      mZoom--;
      update();
   }
}


void OverviewWidget::zoomIn()
{
   if (mZoom + 1 < 32)
   {
      mZoom++;
      update();
   }
}


void OverviewWidget::zoomOff()
{
   mZoom = 1;
   update();
}


void OverviewWidget::wheelEvent(QWheelEvent * e)
{
   if (e->angleDelta().y() < 0)
   {
      zoomOut();
   }
   else
   {
      zoomIn();
   }
}


void OverviewWidget::keyPressEvent(QKeyEvent * e)
{
   bool changed = false;
   Project* p = Project::getInstance();
   int dirX = 0;
   int dirY = 0;

   if (e->key() == Qt::Key_Up)
   {
      if (ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove)
      {
         changed = true;
         dirY--;
      }
   }
   else if (e->key() == Qt::Key_Down)
   {
      if (ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove)
      {
         changed = true;
         dirY++;
      }
   }
   else if (e->key() == Qt::Key_Left)
   {
      if (ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove)
      {
         changed = true;
         dirX--;
      }
   }
   else if (e->key() == Qt::Key_Right)
   {
      if (ToolBox::getInstance()->getCurrentTool()->getToolType() == ToolTypeMove)
      {
         changed = true;
         dirX++;
      }
   }
   else if (e->key() == Qt::Key_Plus)
   {
      zoomIn();
   }
   else if (e->key() == Qt::Key_Minus)
   {
      zoomOut();
   }

   if (changed)
   {
      Layer* layer = LayerStack::getInstance()->getSelectedLayer();
      if (layer->getType() == LayerTypeNameTable)
      {
         int offsetX = p->getOffsetX();
         int offsetY = p->getOffsetY();
         offsetX += dirX;
         offsetY += dirY;
         p->setOffsetX(offsetX);
         p->setOffsetY(offsetY);
         p->save();
      }
      else if (layer->getType() == LayerTypeSprite)
      {
         // move sprite
         SpriteLayer* spriteLayer = (SpriteLayer*)layer;
         int offsetX = spriteLayer->getScreenOffsetX();
         int offsetY = spriteLayer->getScreenOffsetY();
         offsetX += dirX;
         offsetY += dirY;
         spriteLayer->setScreenOffsetX(offsetX);
         spriteLayer->setScreenOffsetY(offsetY);
      }

      updateTexture();
   }
}
