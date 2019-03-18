#include "previewwidget.h"

// Qt
#include <QTimer>

// tile
#include "sharedglcontext.h"


PreviewWidget::PreviewWidget(QWidget *widget)
  : QGLWidget(widget, SharedGlContext::getInstance()->getWidget()),
    mTexture(0)
{
   SharedGlContext::getInstance()->setWidget(this);
   setFixedSize(320, 240);
}


void PreviewWidget::setTexture(int texture)
{
   mTexture = texture;
   updateGL();
}


void PreviewWidget::drawTexturedQuad(int texture, int width, int height, float alpha)
{
   glColor4f(1.0f, 1.0f, 1.0f, alpha);

   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   glBegin(GL_QUADS);
   glTexCoord2f(0, 1); glVertex2i(0,     0     );
   glTexCoord2f(1, 1); glVertex2i(width, 0     );
   glTexCoord2f(1, 0); glVertex2i(width, height);
   glTexCoord2f(0, 0); glVertex2i(0,     height);
   glEnd();

   glDisable(GL_TEXTURE_2D);
}



void PreviewWidget::initializeGL()
{
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

   // have blending
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//   //
//   QTimer* timer = new QTimer(this);
//   connect(
//      timer,
//      SIGNAL(timeout()),
//      this,
//      SLOT(updateGL())
//   );
//   timer->setInterval(1000);
//   timer->start();
}


void PreviewWidget::resizeGL(int width, int height)
{
   // reset the current viewport
   glViewport(0, 0, width, height);

   // setup projection matrix
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0, width, height, 0, -1, 1);

   // setup modelview matrix
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}


void PreviewWidget::paintGL()
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();

   drawTexturedQuad(
      mTexture,
      320,
      240,
      1.0f
   );
}
