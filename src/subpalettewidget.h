#ifndef SUBPALETTEWIDGET_H
#define SUBPALETTEWIDGET_H

// Qt
#include <QWidget>

// tile
#include "palette.h"


class SubPaletteWidget : public QWidget
{
   Q_OBJECT

public:

   explicit SubPaletteWidget(QWidget *parent = 0);

   Palette getPal() const;
   void setPal(const Palette &pal);


signals:

   void changed();


protected:

   void paintEvent(QPaintEvent *);
   void mousePressEvent(QMouseEvent *);


   Palette mPal;
};

#endif // SUBPALETTEWIDGET_H
