#ifndef COLORREDUCTION_H
#define COLORREDUCTION_H


#include <QList>
#include <QImage>

#include "dithercolor.h"


class ColorReduction
{
public:

   ColorReduction();

   void reduceColors(
      QImage &output,
      const QImage &input,
      const QRect &rect,
      const QList<DitherColor> &colors,
      const QVector<QRgb>& palette
   );

   QList<DitherColor> buildPaletteDither(
      QVector<unsigned int> &pal,
      int colorCount,
      int maxColorDifference = 256
   );


protected:

   int colorDistance(QRgb c1, QRgb c2);
   QRgb colorBlend(QRgb c1, QRgb c2);


   QList<DitherColor> buildPalette(
      QVector<unsigned int> &pal,
      int colorCount
   );

   int findBestMatch(
      QRgb color,
      const QList<DitherColor> &colors
   );
};

#endif // COLORREDUCTION_H
