#ifndef TILEGENERATOR_H
#define TILEGENERATOR_H

// Qt
#include <QImage>
#include <QList>
#include <QObject>

// tile
#include "palette.h"
#include "tile.h"


class TileGenerator : public QObject
{

Q_OBJECT

public:

   static TileGenerator* getInstance();

   static unsigned char *tileToChr(Tile *tile);

   QList<Palette> getPals() const;

   QList<Tile *> getTiles() const;

   int *getNameTable() const;

   bool isCompressTilesEnabled() const;

   QString getFileName() const;


public slots:

   void setPals(const QList<Palette> &pals);
   void setTiles(const QList<Tile*> &tiles);

   void setNameTable(int *nt);

   void setNameTableSize(int size);

   void setCompressTilesEnabled(bool compressTiles);
   void setFileName(const QString &fileName);

   void loadPalette(const QString& fileName);
   void savePalette(const QString& fileName);

   void processBatch(
      const QString& path,
      int tileX0 = 0,
      int tileY0 = 0,
      int tileWidth = -1,
      int tileHeight = -1
   );

   void process(
      QImage *image,
      int tileX0 = 0,
      int tileY0 = 0,
      int tileWidth = -1,
      int tileHeight = -1
   );


protected:

   TileGenerator(QObject* parent = 0);

   int processTile(QImage *image, int x, int y);
   Tile* generateTile(QImage *image, int x, int y);

   QList<Palette> mPals;
   QList<Tile*> mTiles;

   int* mNameTable;
   int mNameTableSize;

   bool mCompressTilesEnabled;
   QString mFileName;


   static TileGenerator* lInstance;
};

#endif // TILEGENERATOR_H
