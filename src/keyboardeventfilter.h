#ifndef KEYBOARDEVENTFILTER_H
#define KEYBOARDEVENTFILTER_H

#include <QObject>

class KeyboardEventFilter : public QObject
{
   Q_OBJECT

public:

   KeyboardEventFilter(QObject* parent = 0);


signals:

   void keyPressEvent(int key);


protected:

   bool eventFilter(QObject *obj, QEvent *event);

};

#endif // KEYBOARDEVENTFILTER_H
