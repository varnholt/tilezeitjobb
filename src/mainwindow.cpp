// base
#include "mainwindow.h"
#include "ui_mainwindow.h"

// Qt
#include <QButtonGroup>
#include <QFileDialog>
#include <QRadioButton>
#include <QShortcut>

//
#include "arraywidget.h"
#include "keyboardeventfilter.h"
#include "layerstack.h"
#include "nametablelayer.h"
#include "nespalette.h"
#include "overviewwidget.h"
#include "subpaletteitem.h"
#include "subpalettewidget.h"
#include "previewwidget.h"
#include "project.h"
#include "settings.h"
#include "spritelayer.h"
#include "spritelayergenerator.h"
#include "spritewidget.h"
#include "tilegenerator.h"
#include "tilegeneratorwidget.h"
#include "tool.h"
#include "toolerase.h"
#include "toolpixelpencil.h"
#include "toolbox.h"

/*

  TODO:

  - allow editing within sprite

*/


MainWindow::MainWindow(QWidget *parent) :
   QMainWindow(parent),
   ui(new Ui::MainWindow),
   mNameTablePaletteButtonGroup(0),
   mToolButtonGroup(0)
{
   ui->setupUi(this);

   // KeyboardEventFilter* filter = new KeyboardEventFilter(this);
   // installEventFilter(filter);

   QShortcut* f1 = new QShortcut(QKeySequence("F1"), this);
   QShortcut* f2 = new QShortcut(QKeySequence("F2"), this);
   QShortcut* f3 = new QShortcut(QKeySequence("F3"), this);
   QShortcut* f4 = new QShortcut(QKeySequence("F4"), this);
   QShortcut* f5 = new QShortcut(QKeySequence("F5"), this);
   QShortcut* f6 = new QShortcut(QKeySequence("F6"), this);
   QShortcut* f7 = new QShortcut(QKeySequence("F7"), this);
   QShortcut* f8 = new QShortcut(QKeySequence("F8"), this);

   mShortcuts << f1;
   mShortcuts << f2;
   mShortcuts << f3;
   mShortcuts << f4;
   mShortcuts << f5;
   mShortcuts << f6;
   mShortcuts << f7;
   mShortcuts << f8;

   foreach (QShortcut* sc, mShortcuts)
      connect(sc, SIGNAL(activated()), this, SLOT(changePal()));

   ui->mNameTablePalAdd->hide();
   ui->mNameTablePalRemove->hide();
   ui->mSpritePalAdd->hide();
   ui->mSpritePalRemove->hide();

   mNameTablePaletteButtonGroup = new QButtonGroup(this);
   mSpritePaletteButtonGroup = new QButtonGroup(this);
   mToolButtonGroup = new QButtonGroup(this);
   mPenSizeButtonGroup = new QButtonGroup(this);

   mToolButtonGroup->addButton(ui->mPalSetup);
   mToolButtonGroup->addButton(ui->mModeBrush);
   mToolButtonGroup->addButton(ui->mModePencil);
   mToolButtonGroup->addButton(ui->mErase);
   mToolButtonGroup->addButton(ui->mPickColor);
   mToolButtonGroup->addButton(ui->mMove);
   mToolButtonGroup->addButton(ui->mAddSprite);

   mPenSizeButtonGroup->addButton(ui->m1px);
   mPenSizeButtonGroup->addButton(ui->m2px);
   mPenSizeButtonGroup->addButton(ui->m4px);
   mPenSizeButtonGroup->addButton(ui->m8px);

   // tools
   connect(
      ui->mPalSetup,
      SIGNAL(clicked(bool)),
      ToolBox::getInstance(),
      SLOT(selectPaletteSetup())
   );

   connect(
      ui->mModeBrush,
      SIGNAL(clicked(bool)),
      ToolBox::getInstance(),
      SLOT(selectPaletteBrush())
   );

   connect(
      ui->mModePencil,
      SIGNAL(clicked(bool)),
      ToolBox::getInstance(),
      SLOT(selectPixelPencil())
   );

   connect(
      ui->mErase,
      SIGNAL(clicked(bool)),
      ToolBox::getInstance(),
      SLOT(selectErase())
   );

   connect(
      ui->mMove,
      SIGNAL(clicked(bool)),
      ToolBox::getInstance(),
      SLOT(selectMove())
   );

   connect(
      ui->mPickColor,
      SIGNAL(clicked(bool)),
      ToolBox::getInstance(),
      SLOT(selectColorPicker())
   );

   connect(
      ui->mAddSprite,
      SIGNAL(clicked(bool)),
      ToolBox::getInstance(),
      SLOT(selectSprite())
   );


   // brush setup
   connect(
      ui->m1px,
      SIGNAL(clicked(bool)),
      this,
      SLOT(selectpx())
   );

   connect(
      ui->m2px,
      SIGNAL(clicked(bool)),
      this,
      SLOT(selectpx())
   );

   connect(
      ui->m4px,
      SIGNAL(clicked(bool)),
      this,
      SLOT(selectpx())
   );

   connect(
      ui->m8px,
      SIGNAL(clicked(bool)),
      this,
      SLOT(selectpx())
   );

   // menubar
   connect(
      ui->mMenuBar,
      SIGNAL(triggered(QAction*)),
      this,
      SLOT(action(QAction*))
   );

   connect(
      ui->mNameTablePalAdd,
      SIGNAL(clicked(bool)),
      this,
      SLOT(addNameTablePalette())
   );

   connect(
      ui->mNameTablePalRemove,
      SIGNAL(clicked(bool)),
      this,
      SLOT(removeNameTablePal())
   );

   connect(
      ui->mSpritePalAdd,
      SIGNAL(clicked(bool)),
      this,
      SLOT(addSpritePalette())
   );

   connect(
      ui->mSpritePalRemove,
      SIGNAL(clicked(bool)),
      this,
      SLOT(removeSpritePal())
   );

   connect(
      ui->mOverview,
      SIGNAL(blockClicked(int,int)),
      this,
      SLOT(blockClicked(int,int))
   );

   connect(
      ui->mOverview,
      SIGNAL(createSprite(int,int)),
      this,
      SLOT(createSprite(int,int))
   );

   connect(
      this,
      SIGNAL(nameTablePalIndexUpdate(int)),
      ui->mArrayPal,
      SLOT(update())
   );

   connect(
      ui->mBlendOriginal,
      SIGNAL(clicked(bool)),
      this,
      SLOT(blendOverOriginalImageClicked())
   );

   connect(
      ui->mColorReduction,
      SIGNAL(clicked(bool)),
      this,
      SLOT(applyColorReductionClicked())
   );

   connect(
      ui->mZoomIn,
      SIGNAL(clicked(bool)),
      ui->mOverview,
      SLOT(zoomIn())
   );

   connect(
      ui->mZoomOff,
      SIGNAL(clicked(bool)),
      ui->mOverview,
      SLOT(zoomOff())
   );

   connect(
      ui->mZoomOut,
      SIGNAL(clicked(bool)),
      ui->mOverview,
      SLOT(zoomOut())
   );

   connect(
      ui->mOverview,
      SIGNAL(textureChanged(int)),
      ui->mPreviewWidget,
      SLOT(setTexture(int))
   );

   connect(
      this,
      SIGNAL(nameTablePalIndexUpdate(int)),
      NesPalette::getInstance(),
      SLOT(setSelectedPalIndex(int))
   );

   connect(
      ui->mHighlightSelected,
      SIGNAL(clicked(bool)),
      Settings::getInstance(),
      SLOT(setHighlightSelectedPalette(bool))
   );

   connect(
      LayerStack::getInstance(),
      SIGNAL(changed()),
      this,
      SLOT(updateLayers())
   );

   connect(
      ui->mLayers,
      SIGNAL(currentRowChanged(int)),
      this,
      SLOT(updateLayerCurrentRow(int))
   );

   connect(
      ui->mShowSprites,
      SIGNAL(clicked(bool)),
      Settings::getInstance(),
      SLOT(setShowSpritesEnabled(bool))
   );


   connect(
      ui->mShowPalTest,
      SIGNAL(clicked(bool)),
      Settings::getInstance(),
      SLOT(setShowPalTestEffectEnabled(bool))
   );

   connect(
      ui->mLayerUp,
      SIGNAL(clicked(bool)),
      LayerStack::getInstance(),
      SLOT(moveUp())
   );

   connect(
      ui->mLayerDown,
      SIGNAL(clicked(bool)),
      LayerStack::getInstance(),
      SLOT(moveDown())
   );

   connect(
      ui->mLayerRemove,
      SIGNAL(clicked(bool)),
      this,
      SLOT(removeLayer())
   );

   connect(
      LayerStack::getInstance(),
      SIGNAL(changed()),
      Project::getInstance(),
      SLOT(save())
   );
}


MainWindow::~MainWindow()
{
   delete ui;
}


void MainWindow::loadProject()
{
   QString projectName = Settings::getInstance()->getProject();
   Project* project = 0;

   if (projectName.isEmpty())
   {
      // load default project
      project = Project::getInstance();
   }
   else
   {
      project = new Project(projectName);
   }

   project->load();
}


void MainWindow::init()
{
   loadProject();

   Project* project = Project::getInstance();

   // sprites/layers
   QString spriteImage = project->getSpriteImage();
   if (!spriteImage.isEmpty())
   {
      ui->mSpriteWidget->openImage(spriteImage);
   }

   LayerStack* layerStack = LayerStack::getInstance();

   foreach (Layer* layer, layerStack->getLayers())
   {
      if (layer->getType() == LayerTypeSprite)
      {
         ((SpriteLayer*)layer)->setImage(ui->mSpriteWidget->getImage());
      }
   }

   updateLayers();

   // nametable
   QString nameTableImage = project->getNameTableImage();
   if (!nameTableImage.isEmpty())
   {
      ui->mOverview->openImage(nameTableImage);
   }

   if (layerStack->getLayers().isEmpty())
   {
      NameTableLayer* nameTableLayer = new NameTableLayer();
      nameTableLayer->initialize();
      layerStack->addLayer(nameTableLayer);
   }

   // sub palettes
   QList<Palette> nameTablePals = project->getNameTablePalettes();
   foreach (Palette pal, nameTablePals)
   {
      addNameTablePalette(pal);
   }

   if (!mNameTableSubPalettes.isEmpty())
      mNameTableSubPalettes.first()->getRadioButton()->setChecked(true);

   QList<Palette> spritePals = project->getSpritePalettes();
   foreach (Palette pal, spritePals)
   {
      addSpritePalette(pal);
   }

   if (!mSpriteSubPalettes.isEmpty())
      mSpriteSubPalettes.first()->getRadioButton()->setChecked(true);
}


void MainWindow::importNameTableImage()
{
   QString fileName = QFileDialog::getOpenFileName();
   ui->mOverview->openImage(fileName);

   Project* project = Project::getInstance();
   project->setOffsetX(0);
   project->setOffsetY(0);
   project->setNameTableImage(fileName);
   project->save();
}


void MainWindow::importSpriteImage()
{
   QString fileName = QFileDialog::getOpenFileName();
   ui->mSpriteWidget->openImage(fileName);

   Project* project = Project::getInstance();
   project->setSpriteImage(fileName);
   project->save();
}


void MainWindow::saveChr()
{
   QString fileName = QFileDialog::getSaveFileName();

   Project* project = Project::getInstance();
   TileGenerator* tileGenerator = TileGenerator::getInstance();

   // if visible range only
   //    QImage* img = ui->mOverview->getNameTableImageTarget();
   // else
   QImage* src = ui->mOverview->getOriginalImage();
   QImage img = src->copy();

   ui->mOverview->reduceToPalette(
      src,
      &img,
      src->width() / BLOCK_SIZE_PX,
      src->height() / BLOCK_SIZE_PX
   );

   img.save("debug.png");

   // generate tiles
   QList<Palette> ntPals = project->getNameTablePalettes();
   int* nameTable = project->getNameTable();
   int nameTableSize = project->getNameTableSize();
   tileGenerator->setFileName(fileName);
   tileGenerator->setPals(ntPals);
   tileGenerator->setNameTable(nameTable);
   tileGenerator->setNameTableSize(nameTableSize);
   tileGenerator->process(&img);
}


void MainWindow::saveNameTablePalette()
{
   QString fileName = QFileDialog::getSaveFileName();
   Project* project = Project::getInstance();
   project->saveNameTable(fileName);
}


void MainWindow::action(QAction * action)
{
   if (action == ui->mActionImportImage)
   {
      importNameTableImage();
   }
   else if (action == ui->mActionImportSpriteImage)
   {
      importSpriteImage();
   }
   else if (action == ui->mGrid1x1)
   {
      Settings::getInstance()->setGridSize(GridSize1x1);
   }
   else if (action == ui->mGrid8x8)
   {
      Settings::getInstance()->setGridSize(GridSize8x8);
   }
   else if (action == ui->mGrid16x16)
   {
      Settings::getInstance()->setGridSize(GridSize16x16);
   }
   else if (action == ui->mGridNone)
   {
      Settings::getInstance()->setGridSize(GridSizeNone);
   }
   else if (action == ui->mActionGenerateSprites)
   {
      SpriteLayerGenerator::generate(ui->mSpriteWidget->getImage());
      updateLayers();
      Project::getInstance()->save();
   }
   else if (action == ui->mActionSaveChr)
   {
      saveChr();
   }
   else if (action == ui->mActionSaveNameTableTileData)
   {
      // TODO
   }
   else if (action == ui->mActionSaveNameTablePalData)
   {
      saveNameTablePalette();
   }
   else if (action == ui->mActionSaveNameTablePalettes)
   {
      QString fileName = QFileDialog::getSaveFileName();
      Project::getInstance()->saveNameTablePalettes(fileName);
   }
   else if (action == ui->mGenerateChr)
   {
      TileGeneratorWidget* w = new TileGeneratorWidget();
      w->show();
      connect(
         w,
         SIGNAL(processBatch(QString,int,int,int,int)),
         TileGenerator::getInstance(),
         SLOT(processBatch(QString,int,int,int,int))
      );
   }
}


void MainWindow::addNameTablePalette(Palette pal)
{
   SubPaletteItem* palItem = new SubPaletteItem();
   palItem->getWidget()->setPal(pal);

   ui->mNameTableSubPaletteContents->layout()->addWidget(palItem);
   mNameTablePaletteButtonGroup->addButton(palItem->getRadioButton());

   connect(
      palItem->getRadioButton(),
      SIGNAL(clicked(bool)),
      this,
      SLOT(nameTableSubPalSelected())
   );

   mNameTableSubPaletteMap.insert(palItem->getRadioButton(), palItem);
   mNameTableSubPalettes.push_back(palItem);

   connect(
      palItem,
      SIGNAL(changed()),
      this,
      SLOT(saveNameTableSubPals())
   );

   saveNameTableSubPals();
}


void MainWindow::removeNameTablePal()
{
   QAbstractButton* button = mNameTablePaletteButtonGroup->checkedButton();
   SubPaletteItem* palItem = mNameTableSubPaletteMap.take(button);
   ui->mNameTableSubPaletteContents->layout()->removeWidget(palItem);
   mNameTableSubPalettes.removeOne(palItem);

   delete palItem;

   saveNameTableSubPals();
}


void MainWindow::saveNameTableSubPals()
{
   QList<Palette> pals;

   foreach(SubPaletteItem* item , mNameTableSubPalettes)
      pals.push_back(item->getWidget()->getPal());

   Project* p = Project::getInstance();
   p->setNameTablePalettes(pals);
   p->save();
}


void MainWindow::nameTableSubPalSelected()
{
   int index = getSelectedNameTablePalIndex();
   emit nameTablePalIndexUpdate(index);
}


void MainWindow::addSpritePalette(Palette pal)
{
   SubPaletteItem* palItem = new SubPaletteItem();
   palItem->getWidget()->setPal(pal);

   ui->mSpriteSubPaletteContents->layout()->addWidget(palItem);
   mSpritePaletteButtonGroup->addButton(palItem->getRadioButton());

   connect(
      palItem->getRadioButton(),
      SIGNAL(clicked(bool)),
      this,
      SLOT(spriteSubPalSelected())
   );

   mSpriteSubPaletteMap.insert(palItem->getRadioButton(), palItem);
   mSpriteSubPalettes.push_back(palItem);

   connect(
      palItem,
      SIGNAL(changed()),
      this,
      SLOT(saveSpriteSubPals())
   );

   saveSpriteSubPals();
}


void MainWindow::removeSpritePal()
{
   QAbstractButton* button = mSpritePaletteButtonGroup->checkedButton();
   SubPaletteItem* palItem = mSpriteSubPaletteMap.take(button);
   ui->mSpriteSubPaletteContents->layout()->removeWidget(palItem);
   mSpriteSubPalettes.removeOne(palItem);
   delete palItem;
   saveSpriteSubPals();
}


void MainWindow::saveSpriteSubPals()
{
   QList<Palette> pals;

   foreach(SubPaletteItem* item , mSpriteSubPalettes)
      pals.push_back(item->getWidget()->getPal());

   Project* p = Project::getInstance();
   p->setSpritePalettes(pals);
   p->save();
}


void MainWindow::spriteSubPalSelected()
{
   int palIndex = getSelectedSpritePalIndex();

   // update pal index for selected sprites
   QList<Layer*> sprites = getSelectedSpriteLayers();
   foreach (Layer* layer, sprites)
   {
      ((SpriteLayer*)layer)->setPalIndex(palIndex);
   }

   emit spritePalIndexUpdate(palIndex);
}


void MainWindow::changePal()
{
   QShortcut* s = dynamic_cast<QShortcut*>(sender());

   if (s)
   {
      int idx = mShortcuts.indexOf(s);

      if (idx != -1)
      {
         if (idx < 4)
         {
            mNameTablePaletteButtonGroup->buttons().at(idx)->setChecked(true);
         }
         else if (idx < 8)
         {
            mSpritePaletteButtonGroup->buttons().at(idx-4)->setChecked(true);
         }
      }
   }
}


void MainWindow::createSprite(int x, int y)
{
   // look up sprite selection from sprite widget
   SpriteLayer* sprite = new SpriteLayer();
   sprite->setInitializing(true);
   sprite->setScreenOffsetX(x);
   sprite->setScreenOffsetY(y);
   sprite->setTileX(ui->mSpriteWidget->getTileX());
   sprite->setTileY(ui->mSpriteWidget->getTileY());
   sprite->setImage(ui->mSpriteWidget->getImage());
   sprite->initialize();
   sprite->setInitializing(false);

   connect(
      sprite,
      SIGNAL(changed()),
      Project::getInstance(),
      SLOT(save())
   );

   // add sprite to layer stack
   LayerStack::getInstance()->addLayer(sprite);
}


int MainWindow::getSelectedNameTablePalIndex()
{
   int index = 0;
   QAbstractButton* button = mNameTablePaletteButtonGroup->checkedButton();
   index = mNameTablePaletteButtonGroup->buttons().indexOf(button);
   return index;
}


int MainWindow::getSelectedSpritePalIndex()
{
   int index = 0;
   QAbstractButton* button = mSpritePaletteButtonGroup->checkedButton();
   index = mSpritePaletteButtonGroup->buttons().indexOf(button);
   return index;
}


void MainWindow::blockClicked(int x, int y)
{
   int index = getSelectedNameTablePalIndex();
   if (index > -1)
   {
      Project::getInstance()->setNameTablePalIndex(x, y, index);
      Project::getInstance()->save();
      emit nameTablePalIndexUpdate(index);
   }
}


void MainWindow::applyColorReductionClicked()
{
   Settings::getInstance()->setApplyColorReduction(
      ui->mColorReduction->isChecked()
   );
}


void MainWindow::blendOverOriginalImageClicked()
{
   Settings::getInstance()->setBlendOverOriginalImage(
      ui->mBlendOriginal->isChecked()
   );
}


void MainWindow::selectpx()
{
   int px = 1;

   if (sender() == ui->m1px)
      px = 1;
   else if (sender() == ui->m2px)
      px = 2;
   else if (sender() == ui->m4px)
      px = 4;
   else if (sender() == ui->m8px)
      px = 8;

   Tool* tool = ToolBox::getInstance()->getCurrentTool();

   if (tool->getToolType() == ToolTypePixelPencil)
   {
      ((ToolPixelPencil*)tool)->setSize(px);
   }

   if (tool->getToolType() == ToolTypeErase)
   {
      ((ToolErase*)tool)->setSize(px);
   }
}



void MainWindow::updateLayers()
{
   disconnect(
      ui->mLayers,
      SIGNAL(currentRowChanged(int)),
      this,
      SLOT(updateLayerCurrentRow(int))
   );

   LayerStack* stack = LayerStack::getInstance();
   QList<Layer*> layers = stack->getLayers();
   QStringList layerNames;

   foreach (Layer* layer, layers)
   {
      layerNames.push_front(layer->getName());
   }

   int row = stack->getCurrentIndex();
   ui->mLayers->clear();
   ui->mLayers->insertItems(0, layerNames);
   ui->mLayers->setCurrentRow(stack->getLayerCount() - row - 1);

   connect(
      ui->mLayers,
      SIGNAL(currentRowChanged(int)),
      this,
      SLOT(updateLayerCurrentRow(int))
   );
}


void MainWindow::updateLayerCurrentRow(int row)
{
   LayerStack* stack = LayerStack::getInstance();
   int stackRow = stack->getLayerCount() - row - 1;
   stack->setCurrentIndex(stackRow);

   Layer* layer = stack->getLayer(stackRow);
   if (layer->getType() == LayerTypeSprite)
   {
      int palIndex = ((SpriteLayer*)layer)->getPalIndex();

      if (palIndex > -1 && palIndex < mSpriteSubPalettes.size())
      {
         mSpriteSubPaletteMap.key(mSpriteSubPalettes.at(palIndex))->setChecked(true);
      }
   }
}


QList<Layer *> MainWindow::getSelectedSpriteLayers()
{
   Layer* layer = 0;
   LayerStack* stack = LayerStack::getInstance();

   QList<QListWidgetItem*> selection = ui->mLayers->selectedItems();
   QList<Layer*> selectedLayers;
   int row = 0;
   int index = 0;
   for(int i = 0; i < selection.size(); i++)
   {
      row = ui->mLayers->row(selection[i]);
      index = stack->getLayerCount() - row - 1;
      layer = stack->getLayer(index);

      if (layer->getType() == LayerTypeSprite)
         selectedLayers << layer;
   }

   return selectedLayers;
}


void MainWindow::removeLayer()
{
   QList<Layer*> selectedLayers = getSelectedSpriteLayers();

   LayerStack* stack = LayerStack::getInstance();
   stack->removeLayers(selectedLayers);
}


