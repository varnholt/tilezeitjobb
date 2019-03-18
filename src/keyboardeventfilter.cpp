#include "keyboardeventfilter.h"

// Qt
#include <QEvent>
#include <QKeyEvent>


KeyboardEventFilter::KeyboardEventFilter(QObject *parent)
 : QObject(parent)
{

}

bool KeyboardEventFilter::eventFilter(QObject *obj, QEvent *event)
{
   if(event->type() == QEvent::KeyPress)
   {
      emit keyPressEvent(((QKeyEvent*)event)->key());
   }

   return QObject::eventFilter(obj, event);
}
