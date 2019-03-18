#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// Qt
#include <QMainWindow>
#include <QMap>


// tile
#include "palette.h"

class Layer;
class QAbstractButton;
class QButtonGroup;
class QShortcut;
class SubPaletteItem;


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   explicit MainWindow(QWidget *parent = 0);
   ~MainWindow();

   void init();

   void loadProject();

   void saveChr();
   
   void saveNameTablePalette();

signals:

   void nameTablePalIndexUpdate(int);
   void spritePalIndexUpdate(int);


protected slots:

   void importNameTableImage();
   void importSpriteImage();

   void action(QAction*);


   void addNameTablePalette(Palette pal = Palette());
   void removeNameTablePal();
   void saveNameTableSubPals();
   void nameTableSubPalSelected();


   void addSpritePalette(Palette pal = Palette());
   void removeSpritePal();
   void saveSpriteSubPals();
   void spriteSubPalSelected();
   void changePal();

   void createSprite(int x, int y);
   void blockClicked(int x, int y);

   void applyColorReductionClicked();
   void blendOverOriginalImageClicked();

   void selectpx();

   void updateLayers();
   void updateLayerCurrentRow(int);
   void removeLayer();


protected:

   int getSelectedNameTablePalIndex();
   int getSelectedSpritePalIndex();
   QList<Layer *> getSelectedSpriteLayers();

   Ui::MainWindow *ui;
   QButtonGroup* mNameTablePaletteButtonGroup;
   QButtonGroup* mSpritePaletteButtonGroup;
   QButtonGroup* mToolButtonGroup;
   QButtonGroup* mPenSizeButtonGroup;
   QList<QShortcut*> mShortcuts;

   // sub palettes
   QMap<QAbstractButton*, SubPaletteItem*> mNameTableSubPaletteMap;
   QList<SubPaletteItem*> mNameTableSubPalettes;

   QMap<QAbstractButton*, SubPaletteItem*> mSpriteSubPaletteMap;
   QList<SubPaletteItem*> mSpriteSubPalettes;
};

#endif // MAINWINDOW_H
