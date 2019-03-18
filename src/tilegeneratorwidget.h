#ifndef TILEGENERATORWIDGET_H
#define TILEGENERATORWIDGET_H

#include <QWidget>

namespace Ui {
class TileGeneratorWidget;
}

class TileGeneratorWidget : public QWidget
{
   Q_OBJECT

public:

   explicit TileGeneratorWidget(QWidget *parent = 0);
   ~TileGeneratorWidget();


signals:

   void processBatch(
      const QString& path,
      int tileX0,
      int tileY0,
      int tileWidth,
      int tileHeight
   );


protected slots:

   void selectPath();
   void generate();


private:
   Ui::TileGeneratorWidget *ui;
};

#endif // TILEGENERATORWIDGET_H
