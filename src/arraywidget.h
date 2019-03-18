#ifndef ARRAYWIDGET_H
#define ARRAYWIDGET_H

#include <QWidget>

#include "constants.h"

class ArrayWidget : public QWidget
{
   Q_OBJECT
public:
   explicit ArrayWidget(QWidget *parent = 0);

signals:

public slots:

protected:

   virtual void paintEvent(QPaintEvent *);

};

#endif // ARRAYWIDGET_H
