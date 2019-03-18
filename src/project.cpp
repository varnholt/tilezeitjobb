// base
#include "project.h"

// tile
#include "constants.h"
#include "layer.h"
#include "layerstack.h"
#include "nametablelayer.h"
#include "nespalette.h"
#include "spritelayer.h"

// Qt
#include <QFile>
#include <QTextStream>


Project* Project::sProject = 0;


Project::Project(const QString &filename)
 : QSettings(filename, QSettings::IniFormat),
   mOffsetX(0),
   mOffsetY(0)
{
   sProject = this;

   mNameTable = new int[BLOCK_16x16_SIZE];
   for (int i = 0; i < BLOCK_16x16_SIZE; i++)
      mNameTable[i]=0;
}


Project::~Project()
{
   delete mNameTable;
   mNameTable = 0;
}


Project *Project::getInstance()
{
   if (!sProject)
      new Project("default_project.tzj");

   return sProject;
}


int Project::getOffsetX() const
{
   return mOffsetX;
}


void Project::setOffsetX(int offsetX)
{
   mOffsetX = offsetX;
}


int Project::getOffsetY() const
{
   return mOffsetY;
}


void Project::setOffsetY(int offsetY)
{
   mOffsetY = offsetY;
}


void Project::load()
{
   disconnect(
      NesPalette::getInstance(),
      SIGNAL(changed())
   );

   mOffsetX = value("offset_x", 0).toInt();
   mOffsetY = value("offset_y", 0).toInt();
   mNameTableImage = value("image").toString();
   mSpriteImage = value("sprite_image").toString();

   // fore and bg color
   unsigned int fg = value("fg").toUInt();
   unsigned int bg = value("bg").toUInt();
   NesPalette::getInstance()->setFg(fg);
   NesPalette::getInstance()->setBg(bg);

   // read pals
   int nameTablePalCount = value("nametable_pal_count").toInt();
   for (int palIndex = 0; palIndex < nameTablePalCount; palIndex++)
   {
      Palette pal;

      for (int index = 0; index < 4; index++)
      {
         unsigned int color = value(
            QString("nametable_pal_%1_%2")
               .arg(palIndex)
               .arg(index)
            ).toUInt();

         pal.setColor(index, color);
      }

      mNameTablePalettes.push_back(pal);
   }

   while (mNameTablePalettes.size() < 4)
   {
      Palette pal;
      addNameTablePalette(pal);
   }

   int spritePalCount = value("sprite_pal_count").toInt();
   for (int palIndex = 0; palIndex < spritePalCount; palIndex++)
   {
      Palette pal;

      for (int index = 0; index < 4; index++)
      {
         unsigned int color = value(
            QString("sprite_pal_%1_%2")
               .arg(palIndex)
               .arg(index)
            ).toUInt();

         pal.setColor(index, color);
      }

      mSpritePalettes.push_back(pal);
   }

   while (mSpritePalettes.size() < 4)
   {
      Palette pal;
      addSpritePalette(pal);
   }

   // nametable data
   QStringList nameTableData;
   nameTableData = value("nametable").toString().split(";", QString::SkipEmptyParts);
   for (int i = 0; i < nameTableData.size(); i++)
      mNameTable[i] = nameTableData.at(i).toInt();

   connect(
      NesPalette::getInstance(),
      SIGNAL(changed()),
      this,
      SLOT(save())
   );

   // layer data
   LayerStack::getInstance()->setInitializing(true);
   int layerCount = value("layer_count").toInt();
   for (int layerIndex = 0; layerIndex < layerCount; layerIndex++)
   {
      QString layerName = value(QString("layer_%1_name").arg(layerIndex)).toString();
      int layerType = value(QString("layer_%1_type").arg(layerIndex)).toInt();

      Layer* layer = 0;

      if ((LayerType)layerType == (LayerTypeNameTable))
      {
         layer = new NameTableLayer();
         layer->setInitializing(true);
      }

      if ((LayerType)layerType == (LayerTypeSprite))
      {
         layer = new SpriteLayer();
         layer->setInitializing(true);

         SpriteLayer* spriteLayer = (SpriteLayer*)layer;
         spriteLayer->setTileX(value(QString("layer_%1_tile_x").arg(layerIndex)).toInt());
         spriteLayer->setTileY(value(QString("layer_%1_tile_y").arg(layerIndex)).toInt());
         spriteLayer->setScreenOffsetX(value(QString("layer_%1_screen_x").arg(layerIndex)).toInt());
         spriteLayer->setScreenOffsetY(value(QString("layer_%1_screen_y").arg(layerIndex)).toInt());
         spriteLayer->setPalIndex(value(QString("layer_%1_pal").arg(layerIndex)).toInt());
      }

      if (layer)
      {
         layer->setName(layerName);
         layer->setType((LayerType)layerType);
         layer->setInitializing(false);

         connect(
            layer,
            SIGNAL(changed()),
            this,
            SLOT(save())
         );

         LayerStack::getInstance()->addLayer(layer);
      }
   }
   LayerStack::getInstance()->setInitializing(false);
}


void Project::save()
{
   setValue("offset_x", mOffsetX);
   setValue("offset_y", mOffsetY);
   setValue("image", mNameTableImage);
   setValue("sprite_image", mSpriteImage);

   // fore and bg color
   unsigned int fg = NesPalette::getInstance()->getFg();
   unsigned int bg = NesPalette::getInstance()->getBg();
   setValue("fg", fg);
   setValue("bg", bg);

   // save palettes
   setValue("nametable_pal_count", mNameTablePalettes.size());
   int palIndex = 0;
   foreach (Palette pal, mNameTablePalettes)
   {
      for (int index = 0; index < 4; index++)
      {
         setValue(
            QString("nametable_pal_%1_%2")
               .arg(palIndex)
               .arg(index),
            pal.getColor(index)
         );
      }

      palIndex++;
   }

   setValue("sprite_pal_count", mSpritePalettes.size());
   palIndex = 0;
   foreach (Palette pal, mSpritePalettes)
   {
      for (int index = 0; index < 4; index++)
      {
         setValue(
            QString("sprite_pal_%1_%2")
               .arg(palIndex)
               .arg(index),
            pal.getColor(index)
         );
      }

      palIndex++;
   }

   // nametable data
   QStringList nameTableData;
   for (int i = 0; i < BLOCK_16x16_SIZE; i++)
      nameTableData << QString::number(mNameTable[i]);
   setValue("nametable", nameTableData.join(";"));

   // layer data
   QList<Layer *> layers = LayerStack::getInstance()->getLayers();
   setValue("layer_count", layers.count());
   int layerIndex = 0;
   foreach (Layer* layer, layers)
   {
      setValue(QString("layer_%1_name").arg(layerIndex), layer->getName());
      setValue(QString("layer_%1_type").arg(layerIndex), layer->getType());

      if (layer->getType() == (LayerTypeNameTable))
      {

      }

      if (layer->getType() == (LayerTypeSprite))
      {
         SpriteLayer* spriteLayer = (SpriteLayer*)layer;
         setValue(QString("layer_%1_tile_x").arg(layerIndex), spriteLayer->getTileX());
         setValue(QString("layer_%1_tile_y").arg(layerIndex), spriteLayer->getTileY());
         setValue(QString("layer_%1_screen_x").arg(layerIndex), spriteLayer->getScreenOffsetX());
         setValue(QString("layer_%1_screen_y").arg(layerIndex), spriteLayer->getScreenOffsetY());
         setValue(QString("layer_%1_pal").arg(layerIndex), spriteLayer->getPalIndex());
      }

      layerIndex++;
   }

   sync();
}


void Project::saveNameTable(const QString &name)
{
   QString fileName = name.toLower().replace(".h", "");

   QFile nameTableFile(QString("%1.h").arg(fileName));
   nameTableFile.open(QIODevice::WriteOnly);

   QTextStream out(&nameTableFile);

   // TODO: look up nametable setup in project settings!


   out << QString("const unsigned char nametablePal[%1]={\n").arg(BLOCK_16x16_SIZE);

   for (int i = 0; i < BLOCK_16x16_SIZE; i++)
   {
      out << QString::number(mNameTable[i]);

      if (i + 1 < BLOCK_16x16_SIZE)
         out << ",";

      if ( (i != 0) && ( (i + 1) & 0xf) == 0)
         out << "\n";
   }

   out << "\n";
   out << "};\n";

   nameTableFile.close();
}


int *Project::getNameTable() const
{
   return mNameTable;
}


void Project::setNameTable(int *nameTable)
{
   mNameTable = nameTable;
}

int Project::getNameTableSize() const
{
   // fixed for now
   return BLOCK_16x16_SIZE;
}


void Project::saveNameTablePalettes(const QString& fileName)
{
   QList<Palette> pals = getNameTablePalettes();

   // QString fileName = name.toLower().replace(".h", "");
   // QString fileName = "ntpal";

   QFile paletteFile(QString("%1.h").arg(fileName));
   paletteFile.open(QIODevice::WriteOnly);

   QTextStream out(&paletteFile);

   int palIndex = 0;
   foreach (const Palette pal, pals)
   {
      QStringList cols;

      for (int i = 0; i < 4; i++)
      {
         int color = pal.getColor(i);
         int colorIndex = 0;

         for (int ci = 0; ci < 56; ci++)
         {
            if (color == sNesPalette[ci])
            {
               colorIndex = ci;
            }
         }

         cols << QString::number(colorIndex);
      }

      out <<
         QString("const unsigned char pal%1[4]={%2};\n")
            .arg(palIndex)
            .arg(cols.join(","));

      palIndex++;
   }

   paletteFile.close();
}


QList<Palette> Project::getSpritePalettes() const
{
   return mSpritePalettes;
}


void Project::setSpritePalettes(const QList<Palette> &spritePalettes)
{
   mSpritePalettes = spritePalettes;
}


void Project::addSpritePalette(const Palette &pal)
{
   mSpritePalettes.push_back(pal);
}


QString Project::getSpriteImage() const
{
   return mSpriteImage;
}


void Project::setSpriteImage(const QString &spriteImage)
{
   mSpriteImage = spriteImage;
}


QList<Palette> Project::getNameTablePalettes() const
{
   return mNameTablePalettes;
}


Palette Project::getNameTablePalette(int index) const
{
   return mNameTablePalettes.at(index);
}


void Project::addNameTablePalette(const Palette &pal)
{
   mNameTablePalettes.push_back(pal);
}


void Project::setNameTablePalettes(const QList<Palette> &palettes)
{
   mNameTablePalettes = palettes;
}


int Project::getNameTablePalIndex(int blockX, int blockY)
{
   int palIndex = 0;
   int index = blockY * BLOCK_16x16_WIDTH + blockX;

   if (index < BLOCK_16x16_SIZE)
      palIndex = mNameTable[index];

   return palIndex;
}


void Project::setNameTablePalIndex(int blockX, int blockY, int palIndex)
{
   int index = blockY * BLOCK_16x16_WIDTH + blockX;

   if (index < BLOCK_16x16_SIZE)
      mNameTable[index] = palIndex;
}


QString Project::getNameTableImage() const
{
   return mNameTableImage;
}


void Project::setNameTableImage(const QString &image)
{
   mNameTableImage = image;
}
