#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QGLWidget>

class PreviewWidget : public QGLWidget
{
   Q_OBJECT

public:

   PreviewWidget(QWidget* widget = 0);


public slots:

   void setTexture(int texture);


protected:


   void initializeGL();

   void resizeGL(int w, int h);

   void paintGL();

   void drawTexturedQuad(int texture, int width, int height, float alpha);

   int mTexture;
};

#endif // PREVIEWWIDGET_H
