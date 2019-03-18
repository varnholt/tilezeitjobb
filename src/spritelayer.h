#ifndef SPRITELAYER_H
#define SPRITELAYER_H

#include "layer.h"

class QImage;

class SpriteLayer : public Layer
{
   Q_OBJECT

public:

   SpriteLayer();

   virtual void initialize();

   QImage *getImage() const;
   void setImage(QImage *image);

   int getTileX() const;
   void setTileX(int tileX);

   int getTileY() const;
   void setTileY(int tileY);

   int getScreenOffsetX() const;
   void setScreenOffsetX(int screenOffsetX);

   int getScreenOffsetY() const;
   void setScreenOffsetY(int screenOffsetY);

   int getPalIndex() const;
   void setPalIndex(int palIndex);

   int getSpriteId() const;
   void setSpriteId(int spriteId);


protected:

   QImage* mImage;

   int mTileX;
   int mTileY;

   int mScreenOffsetX;
   int mScreenOffsetY;

   int mPalIndex;

   int mSpriteId;

   static int sSpriteId;
};

#endif // SPRITELAYER_H
