#include "spritelayer.h"

#include <QApplication>
#include <QString>

int SpriteLayer::sSpriteId = 0;


SpriteLayer::SpriteLayer()
 : Layer(),
   mImage(0),
   mTileX(0),
   mTileY(0),
   mScreenOffsetX(0),
   mScreenOffsetY(0),
   mPalIndex(0)
{
   mType = LayerTypeSprite;
   mSpriteId = sSpriteId;
   sSpriteId++;
}

void SpriteLayer::initialize()
{
   setName(QString(qApp->tr("sprite %1").arg(mSpriteId)));
}

QImage *SpriteLayer::getImage() const
{
   return mImage;
}

void SpriteLayer::setImage(QImage *image)
{
   mImage = image;
}

int SpriteLayer::getTileX() const
{
   return mTileX;
}

void SpriteLayer::setTileX(int tileX)
{
   mTileX = tileX;

   if (!isInitializing())
      emit changed();
}

int SpriteLayer::getTileY() const
{
   return mTileY;
}

void SpriteLayer::setTileY(int tileY)
{
   mTileY = tileY;

   if (!isInitializing())
      emit changed();
}

int SpriteLayer::getScreenOffsetX() const
{
   return mScreenOffsetX;
}

void SpriteLayer::setScreenOffsetX(int screenOffsetX)
{
   mScreenOffsetX = screenOffsetX;

   if (!isInitializing())
      emit changed();
}

int SpriteLayer::getScreenOffsetY() const
{
   return mScreenOffsetY;
}

void SpriteLayer::setScreenOffsetY(int screenOffsetY)
{
   mScreenOffsetY = screenOffsetY;

   if (!isInitializing())
      emit changed();
}

int SpriteLayer::getPalIndex() const
{
   return mPalIndex;
}

void SpriteLayer::setPalIndex(int palIndex)
{
   mPalIndex = palIndex;

   if (!isInitializing())
      emit changed();
}

int SpriteLayer::getSpriteId() const
{
   return mSpriteId;
}

void SpriteLayer::setSpriteId(int spriteId)
{
   mSpriteId = spriteId;
}
