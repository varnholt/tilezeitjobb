#ifndef NESPALETTEWIDGET_H
#define NESPALETTEWIDGET_H

#include <QWidget>

class NesPaletteWidget : public QWidget
{
   Q_OBJECT

public:

   NesPaletteWidget(QWidget* widget = 0);


protected:

   void paintEvent(QPaintEvent *);

   void mousePressEvent(QMouseEvent *);

   unsigned int getColor(const QPoint& pos);
};

#endif // NESPALETTEWIDGET_H
