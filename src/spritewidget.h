#pragma once

#include <QPoint>
#include <QWidget>

class SpriteWidget : public QWidget
{
   Q_OBJECT

public:

   explicit SpriteWidget(QWidget *parent = 0);
   QImage* getImage();

   int getTileX() const;
   void setTileX(int tileX);

   int getTileY() const;
   void setTileY(int tileY);


signals:

public slots:

   void openImage(const QString&);

protected:

   void paintEvent(QPaintEvent *);

   void mouseMoveEvent(QMouseEvent *);

   void mousePressEvent(QMouseEvent *);

   QImage mImage;

   QPoint mMousePos;

   int mTileX;
   int mTileY;
};

