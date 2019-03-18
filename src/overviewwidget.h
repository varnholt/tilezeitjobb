#ifndef OVERVIEWWIDGET_H
#define OVERVIEWWIDGET_H

#include <QEvent>
#include <QGLWidget>

#include "colorreduction.h"

class QFileSystemWatcher;


class OverviewWidget : public QGLWidget
{
   Q_OBJECT

public:

   explicit OverviewWidget(QWidget *parent = 0);


   QImage *getNameTableImageTarget() const;

   QImage *getOriginalImage() const;

   QImage *getNameTableImageSource() const;


   void reduceToPalette(
      QImage *src,
      QImage *dst,
      int stepsX = -1,
      int stepsY = -1
   );


   
signals:

   void blockClicked(int x, int y);

   void textureChanged(int texture);

   void createSprite(int x, int y);


public slots:

   void openImage(const QString& = QString::null);


   void zoomOut();

   void zoomIn();

   void zoomOff();


protected slots:

   void updateTexture();

   void dump();

   void fileChanged(const QString& filename);


protected:

   bool isLeftButtonPressed() const;
   void setLeftButtonPressed(bool leftButtonPressed);

   bool isRightButtonPressed() const;
   void setRightButtonPressed(bool rightButtonPressed);

   bool isWithinMoveEvent() const;
   void setWithinMoveEvent(bool withinMoveEvent);

   void initializeGL();
   void resizeGL(int w, int h);
   void paintGL();

   void drawTexturedQuad(int texture, int width, int height, float alpha = 1.0f);
   void drawHighlightedBlocks();
   void drawCursor();

   void applyTool(QEvent::Type type, const QPoint &pixelPos);

   virtual void mousePressEvent(QMouseEvent *);
   virtual void mouseReleaseEvent(QMouseEvent *);

   virtual void mouseMoveEvent(QMouseEvent *);

   virtual void wheelEvent(QWheelEvent *);
   virtual void keyPressEvent(QKeyEvent *);

   QPoint getCurrentBlock(const QPoint& pixelPos);
   QPoint getCurrentPixel(const QPoint& pixelPos);

   QPoint getPreviousPixelPosition() const;
   void setPreviousPixelPosition(const QPoint &previousPixelPosition);

   QImage* mOriginalImage;
   QImage* mNameTableImageSource;
   QImage* mNameTableImageTarget;
   QImage* mPaintArea;

   int mTexture;
   int mNameTableTexture;

   int mZoom;

   QPoint mMousePos;
   QPoint mScrollOffset;

   QPoint mCurrentPixel;
   QPoint mCurrentBlock;

   QPoint mPreviousPixelPosition;

   ColorReduction mColorReduction;

   QString mFilePath;
   QFileSystemWatcher* mFileWatcher;

   bool mLeftButtonPressed;
   bool mRightButtonPressed;
   bool mWithinMoveEvent;
};

#endif // OVERVIEWWIDGET_H
