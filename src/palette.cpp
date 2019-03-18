#include "palette.h"

#include <math.h>


Palette::Palette()
{
   for (int i = 0; i < 4; i++)
      mColors[i] = 0;
}



void Palette::setColors(unsigned int c1, unsigned int c2, unsigned int c3, unsigned int c4)
{
   mColors[0] = c1;
   mColors[1] = c2;
   mColors[2] = c3;
   mColors[3] = c4;
}


void Palette::setColor(int index, unsigned int color)
{
   switch (index)
   {
      case 0:
         mColors[0] = color;
         break;
      case 1:
         mColors[1] = color;
         break;
      case 2:
         mColors[2] = color;
         break;
      case 3:
         mColors[3] = color;
         break;
      default:
         break;
   }
}


unsigned int Palette::getColor(int index) const
{
   return mColors[index];
}


int Palette::getIndex(unsigned int color) const
{
   int index = -1;

   for (int i = 0; i< 4; i++)
   {
      if (mColors[i] == color)
      {
         index = i;
         break;
      }
   }

   return index;
}


int Palette::getIndexFuzzy(unsigned int color, unsigned int eps) const
{
   int index = -1;

   for (int i = 0; i< 4; i++)
   {
      unsigned int delta = (mColors[i] > color) ? (mColors[i] - color) : (color - mColors[i]);

      if (delta <= eps)
      {
         index = i;
         break;
      }
   }

   return index;
}

