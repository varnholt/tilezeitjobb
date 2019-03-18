#ifndef PROJECT_H
#define PROJECT_H

// Qt
#include <QString>
#include <QSettings>

// tile
#include "palette.h"


class Project : public QSettings
{
   Q_OBJECT

public:

   static Project* getInstance();

   Project(const QString& filename);
   virtual ~Project();

   int getOffsetX() const;
   void setOffsetX(int offsetX);

   int getOffsetY() const;
   void setOffsetY(int offsetY);

   QString getNameTableImage() const;
   void setNameTableImage(const QString &image);

   QString getSpriteImage() const;
   void setSpriteImage(const QString &spriteImage);

   QList<Palette> getNameTablePalettes() const;
   Palette getNameTablePalette(int index) const;
   void addNameTablePalette(const Palette& pal);
   void setNameTablePalettes(const QList<Palette> &palettes);

   QList<Palette> getSpritePalettes() const;
   void setSpritePalettes(const QList<Palette> &spritePalettes);
   void addSpritePalette(const Palette& pal);

   int getNameTablePalIndex(int blockX, int blockY);
   void setNameTablePalIndex(int blockX, int blockY, int index);

   int *getNameTable() const;
   void setNameTable(int *nameTable);

   int getNameTableSize() const;




public slots:

   void load();
   void save();

   void saveNameTable(const QString& name);
   void saveNameTablePalettes(const QString& filename);


protected:

   int mOffsetX;
   int mOffsetY;

   int* mNameTable;

   QString mNameTableImage;
   QString mSpriteImage;

   QList<Palette> mNameTablePalettes;
   QList<Palette> mSpritePalettes;

   static Project* sProject;
};

#endif // PROJECT_H
