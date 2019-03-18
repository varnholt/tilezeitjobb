#include "sharedglcontext.h"

SharedGlContext* SharedGlContext::sContext = 0;


SharedGlContext::SharedGlContext()
 : mWidget(0)
{
   sContext = this;
}


SharedGlContext *SharedGlContext::getInstance()
{
   if (!sContext)
      new SharedGlContext();

   return sContext;
}


QGLWidget *SharedGlContext::getWidget()
{
   return mWidget;
}


void SharedGlContext::setWidget(QGLWidget *widget)
{
   if (!mWidget)
      mWidget = widget;
}
