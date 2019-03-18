#ifndef TILE_H
#define TILE_H

#include "constants.h"

struct Tile
{

   Tile();

   virtual ~Tile();

   bool operator==(const Tile&) const;

   int* mData;
};

#endif // TILE_H
