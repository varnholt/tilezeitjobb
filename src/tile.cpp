#include "tile.h"



Tile::Tile()
 : mData(0)
{
   mData = new int[TILE_SIZE_PX * TILE_SIZE_PX];

   for (int i = 0; i < TILE_SIZE_PX * TILE_SIZE_PX; i++)
      mData[i] = 0;
}


Tile::~Tile()
{
   delete mData;
   mData = 0;
}


bool Tile::operator==(const Tile &other) const
{
   for (int i = 0; i < TILE_SIZE_PX * TILE_SIZE_PX; i++)
   {
      if (mData[i] != other.mData[i])
      {
         return false;
      }
   }

   return true;
}
