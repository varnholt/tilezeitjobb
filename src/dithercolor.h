#ifndef DITHERCOLOR_H
#define DITHERCOLOR_H


#include <QRgb>


// a pair of color indices used for dithering
struct DitherColor
{
   unsigned char mIndex1;
   unsigned char mIndex2;
   QRgb mColor;
};


#endif // DITHERCOLOR_H
