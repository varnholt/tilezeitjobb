#include "spritelayergenerator.h"

// tile
#include "layerstack.h"
#include "nespalette.h"
#include "spritelayer.h"

// Qt
#include <QImage>


SpriteLayerGenerator::SpriteLayerGenerator()
{

}


bool SpriteLayerGenerator::checkEmpty(
   QImage* spriteImage,
   unsigned int bg
)
{
   for (int yi = 0; yi < 8; yi++)
   {
      for (int xi = 0; xi < 8; xi++)
      {
         if (spriteImage->pixel(xi, yi) != bg)
         {
            return false;
         }
      }
   }

   return true;
}


void SpriteLayerGenerator::generate(QImage* image)
{
   unsigned int bg = NesPalette::getInstance()->getBg();
   LayerStack::getInstance()->setInitializing(true);

   for (int y = 0; y < 240; y+=8)
   {
      for (int x = 0; x < 256; x+=8)
      {
         QImage spriteImage =
            image->copy(
               x,
               y,
               8,
               8
            );

         bool empty = checkEmpty(&spriteImage, bg);

         if (!empty)
         {
            SpriteLayer* sprite = new SpriteLayer();
            sprite->setInitializing(true);

            sprite->setScreenOffsetX(x);
            sprite->setScreenOffsetY(y);
            sprite->setTileX(x >> 3);
            sprite->setTileY(y >> 3);
            sprite->setImage(image);
            sprite->initialize();
            sprite->setInitializing(false);

            // add sprite to layer stack
            LayerStack::getInstance()->addLayer(sprite);
         }
      }
   }

   LayerStack::getInstance()->setInitializing(false);
}

