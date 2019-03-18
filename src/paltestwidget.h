#ifndef EFFECTWIDGET_H
#define EFFECTWIDGET_H

#include <QWidget>

class PalTestWidget : public QWidget
{
   Q_OBJECT

public:

   explicit PalTestWidget(QWidget *parent = 0);


   bool isEnabled() const;
   void setEnabled(bool enabled);

signals:

public slots:

   void loadImages();


protected:

   int f(int scanline);
   int f();

   void paintEvent(QPaintEvent * event);


   QList<QImage> mImages;

   int mWidth;
   int mHeight;
   int mFrame;
   bool mEnabled;
};

#endif // EFFECTWIDGET_H
