#ifndef SHAREDGLCONTEXT_H
#define SHAREDGLCONTEXT_H

class QGLWidget;

class SharedGlContext
{
public:

   static SharedGlContext* getInstance();

   QGLWidget *getWidget();
   void setWidget(QGLWidget *widget);

protected:

   SharedGlContext();
   QGLWidget* mWidget;

   static SharedGlContext* sContext;
};

#endif // SHAREDGLCONTEXT_H
