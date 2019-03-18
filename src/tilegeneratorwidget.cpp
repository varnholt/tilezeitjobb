#include "tilegeneratorwidget.h"
#include "ui_tilegeneratorwidget.h"

// Qt
#include <QFileDialog>
#include <QLineEdit>


TileGeneratorWidget::TileGeneratorWidget(QWidget *parent) :
   QWidget(parent),
   ui(new Ui::TileGeneratorWidget)
{
   ui->setupUi(this);

   connect(
      ui->mGenerate,
      SIGNAL(clicked(bool)),
      this,
      SLOT(generate())
   );

   connect(
      ui->mSelect,
      SIGNAL(clicked(bool)),
      this,
      SLOT(selectPath())
   );
}


TileGeneratorWidget::~TileGeneratorWidget()
{
   delete ui;
}


void TileGeneratorWidget::selectPath()
{
   QString path = QFileDialog::getExistingDirectory();
   ui->mPath->setText(path);
}


void TileGeneratorWidget::generate()
{
   QString path;
   int xOffset = 0;
   int yOffset = 0;
   int width = 0;
   int height = 0;

   path = ui->mPath->text();

   bool xoValid = false;
   bool yoValid = false;
   bool wValid = false;
   bool hValid = false;

   xOffset = ui->mXOffset->text().toInt(&xoValid);
   yOffset = ui->mXOffset->text().toInt(&yoValid);
   width = ui->mXOffset->text().toInt(&wValid);
   height = ui->mXOffset->text().toInt(&hValid);

   if (xoValid && yoValid && wValid && hValid && QDir(path).exists())
   {
      emit processBatch(
         path,
         xOffset,
         yOffset,
         width,
         height
      );
   }
}

