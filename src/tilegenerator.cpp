// base
#include "tilegenerator.h"

// tile
#include "constants.h"

// Qt
#include <QColor>
#include <QDir>
#include <QFile>

// defines
#define DEBUG_TILES 1


TileGenerator* TileGenerator::lInstance = 0;


TileGenerator::TileGenerator(QObject *parent)
 : QObject(parent),
   mNameTable(0),
   mNameTableSize(0),
   mCompressTilesEnabled(true)
{
   lInstance = this;
}


TileGenerator *TileGenerator::getInstance()
{
   if (!lInstance)
      new TileGenerator();

   return lInstance;
}


void TileGenerator::loadPalette(const QString &fileName)
{
   QFile file(fileName);
   file.open(QIODevice::ReadOnly);
   QByteArray data = file.readAll();

   if (data.size() % 4 != 0)
   {
      qWarning("bad palette data");
   }
   else
   {
      mPals.clear();

      int palCount = data.size() / 4;

      for (int palIndex = 0; palIndex < palCount; palIndex++)
      {
         Palette pal;

         for (int i = 0; i < 4; i++)
         {
            pal.setColor(i, data.at(i));
         }

         mPals.push_back(pal);
      }
   }

   file.close();
}


void TileGenerator::savePalette(const QString &fileName)
{
   QFile file(fileName);
   file.open(QIODevice::WriteOnly);

   QByteArray data(mPals.size() * 4, 0);

   int palIndex = 0;
   foreach (Palette pal, mPals)
   {
      for (int i = 0; i < 4; i++)
      {
         data[palIndex * 4 + i] = pal.getColor(i);
      }

      palIndex++;
   }

   file.close();
}


void TileGenerator::processBatch(
   const QString &tmpPath,
   int tileX0,
   int tileY0,
   int tileWidth,
   int tileHeight
)
{
   // init
   mNameTable = new int[BLOCK_16x16_SIZE];
   for (int i = 0; i < BLOCK_16x16_SIZE; i++)
      mNameTable[i]=0;

   mPals.clear();

   setCompressTilesEnabled(false);

   QString path;
   if (!tmpPath.isEmpty())
      path = tmpPath;
   else
      path = QDir::currentPath();

   // init palette
   Palette pal;
   pal.setColor(0, qRgb(0x00, 0x00, 0x7c));
   pal.setColor(1, qRgb(0xfc, 0x00, 0x00));
   pal.setColor(2, qRgb(0x03, 0xfc, 0x00));
   pal.setColor(3, qRgb(0x00, 0x03, 0xfc));
   mPals.push_back(pal);

   // process a number of images
   QStringList filter;
   filter << "*.bmp";
   filter << "*.BMP";
   QFileInfoList files = QDir(path).entryInfoList(filter, QDir::Files);

   foreach (QFileInfo fileInfo, files)
   {
      printf("processing '%s'\n", qPrintable(fileInfo.fileName()));
      setFileName(fileInfo.baseName());
      QImage img(fileInfo.absoluteFilePath());
      process(&img, tileX0, tileY0, tileWidth, tileHeight);
   }
}


void TileGenerator::process(
   QImage* image,
   int tileX0,
   int tileY0,
   int tileWidth,
   int tileHeight
)
{
   qDeleteAll(mTiles);
   mTiles.clear();

   unsigned char* emptyTile = new unsigned char[16];
   memset(emptyTile, 0, 16 * sizeof(unsigned char));

   if (tileWidth == -1)
      tileWidth = image->width() / TILE_SIZE_PX;
   if (tileHeight == -1)
      tileHeight = image->height() / TILE_SIZE_PX;

   #ifdef DEBUG_TILES
      QStringList lines;
      QStringList line;
   #endif

   for (int y = tileY0; y < tileY0 + tileHeight; y++)
   {
      #ifdef DEBUG_TILES
         line.clear();
      #endif

      for (int x = tileX0; x < tileX0 + tileWidth; x++)
      {
         int tileIndex = processTile(image, x, y);

         #ifdef DEBUG_TILES
            QString idxStr;
            idxStr.sprintf("0x%02X", tileIndex);
            line.push_back(idxStr);
         #endif
      }

      #ifdef DEBUG_TILES
         lines.push_back(line.join(","));
      #endif
   }

   #ifdef DEBUG_TILES
      foreach (QString l, lines)
      {
         qDebug("%s", qPrintable(l));
      }

      qDebug("");
   #endif

   // convert tile to chr
   unsigned char* bytes = 0;
   int index = 0;
   int fileIndex = 0;
   QFile* chrFile = 0;
   QString fileName = getFileName().toLower().replace(".chr", "");
   foreach (Tile* tile, mTiles)
   {
      if (index == 0 || index == 256)
      {
         if (chrFile)
         {
            chrFile->close();
            delete chrFile;
         }

         chrFile = new QFile(QString("%1_%2.chr").arg(fileName).arg(fileIndex));
         chrFile->open(QIODevice::WriteOnly);
         index = 0;
         fileIndex++;
      }

      bytes = tileToChr(tile);
      chrFile->write((const char*)bytes, 16);
      delete bytes;

      index++;
   }

   if (chrFile)
   {
      // finalize file if needed
      while (index < 64)
      {
         chrFile->write((const char*)emptyTile, 16);
         index++;
      }

      chrFile->close();
      delete chrFile;
   }
}


int TileGenerator::processTile(QImage* image, int tileX, int tileY)
{
   // generate temporary tile from image / palette data
   Tile* t = generateTile(image, tileX, tileY);

   // compare tile contents with existing tiles
   bool compress = isCompressTilesEnabled();
   int i = 0;
   int tileIndex = -1;
   if (compress)
   {
      foreach (Tile* tmp, mTiles)
      {
         if (*t == *tmp)
         {
            delete t;
            tileIndex = i;
            break;
         }

         i++;
      }
   }

   // int tileIndex = mTiles.indexOf(t);

   // look up tile
   if (tileIndex == -1)
   {
      mTiles.push_back(t);
      tileIndex = mTiles.size() - 1;
   }

   return tileIndex;
}


Tile* TileGenerator::generateTile(QImage* image, int tileX, int tileY)
{
   Tile* t = new Tile();
   int imageBlockWidth = (image->width() / TILE_SIZE_PX) / 2;

   // nametable width and height is supposed to be half the size of
   // the images tile count
   int index = (tileY / 2) * imageBlockWidth + (tileX / 2);

   int palIndex = 0;
   if (mNameTableSize > 0)
   {
      if (index < mNameTableSize)
         palIndex = mNameTable[index];
      // otherwise leave palIndex at 0
   }
   else
   {
      // if nametable size is not known, do a blind read
      mNameTable[index];
   }

   Palette pal = mPals.at(palIndex);

   int colorIndex = 0;
   int px = 0;
   int py = 0;

   for (int yi = 0; yi < 8; yi++)
   {
      py = tileY * TILE_SIZE_PX + yi;

      for (int xi = 0; xi < 8; xi++)
      {
         px = tileX * TILE_SIZE_PX + xi;

         QRgb color = image->pixel(px, py);
         colorIndex = pal.getIndex(color);

         if (colorIndex == -1)
         {
            colorIndex = pal.getIndexFuzzy(color, 3);
         }

         if (colorIndex != -1)
         {
            t->mData[yi * TILE_SIZE_PX + xi] = colorIndex;
         }
         else
         {
            printf("color index for given color not found (%u)!\n", color);
         }
      }
   }

   return t;
}


bool TileGenerator::isCompressTilesEnabled() const
{
   return mCompressTilesEnabled;
}


QString TileGenerator::getFileName() const
{
    return mFileName;
}


void TileGenerator::setCompressTilesEnabled(bool compressTiles)
{
   mCompressTilesEnabled = compressTiles;
}


void TileGenerator::setFileName(const QString &fileName)
{
    mFileName = fileName;
}


int *TileGenerator::getNameTable() const
{
    return mNameTable;
}


void TileGenerator::setNameTable(int *nt)
{
   mNameTable = nt;
}

void TileGenerator::setNameTableSize(int size)
{
   mNameTableSize = size;
}


QList<Tile*> TileGenerator::getTiles() const
{
   return mTiles;
}


void TileGenerator::setTiles(const QList<Tile*> &tiles)
{
   mTiles = tiles;
}


QList<Palette> TileGenerator::getPals() const
{
   return mPals;
}


void TileGenerator::setPals(const QList<Palette> &pals)
{
   mPals = pals;
}


// 1 tile => 16 bytes
// first 8 byte contain bit 1 of the color
// other 8 byte contain bit 2 of the color

unsigned char* TileGenerator::tileToChr(Tile* tile)
{
   unsigned char* bytes = new unsigned char[16];

   int color = 0;
   unsigned char bits0 = 0;
   unsigned char bits1 = 0;

   for (int y = 0; y < 8; y++)
   {
      // reset bits
      bits0 = 0;
      bits1 = 0;

      for (int x = 0; x < 8; x++)
      {
         color = tile->mData[y * TILE_SIZE_PX + x];

         // inverse bitorder... *sigh*
         bits0 |= ( (color      & 1) << (7 - x) );
         bits1 |= ( (color >> 1 & 1) << (7 - x) );
      }

      bytes[y    ] = bits0;
      bytes[y + 8] = bits1;
   }

   return bytes;
}


/*

   I just looked at the diagram, and...I understand it.
   It is amazingly simple how it works. I see it now:

   bytes 0-7 control color bits 0, storing them in plane 0
   bytes 7-15 control color bits 1, storing them in plane 1

   http://wiki.nesdev.com/w/index.php/PPU_pattern_tables

   Each tile in the pattern table is 16 bytes, made of two planes.

   The first plane controls bit 0 of the color;
   the second plane controls bit 1

   Bit Planes            Pixel Pattern
   $0xx0=$41  01000001
   $0xx1=$C2  11000010
   $0xx2=$44  01000100
   $0xx3=$48  01001000
   $0xx4=$10  00010000
   $0xx5=$20  00100000         .1.....3
   $0xx6=$40  01000000         11....3.
   $0xx7=$80  10000000  =====  .1...3..
                               .1..3...
   $0xx8=$01  00000001  =====  ...3.22.
   $0xx9=$02  00000010         ..3....2
   $0xxA=$04  00000100         .3....2.
   $0xxB=$08  00001000         3....222
   $0xxC=$16  00010110
   $0xxD=$21  00100001
   $0xxE=$42  01000010
   $0xxF=$87  10000111

   color: 2 bit: 11 => 0,1,2,3
      00
      01
      10
      11
*/
