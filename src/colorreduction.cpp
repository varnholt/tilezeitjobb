#include "colorreduction.h"

// Qt
#include <QImage>
#include <QSet>

ColorReduction::ColorReduction()
{
}


// return distance between the two given rgb colors c1 & c2
// max( deltaR, deltaG, deltaB )
// some sort of weighting between red, green, blue would probably make sense
int ColorReduction::colorDistance(QRgb c1, QRgb c2)
{
   int deltaR= abs( qRed(c1) - qRed(c2) );
   int deltaG= abs( qGreen(c1) - qGreen(c2) );
   int deltaB= abs( qBlue(c1) - qBlue(c2) );

   // get maximum of delta r,g,b
   if (deltaG > deltaR)
      deltaR= deltaG;
   if (deltaB > deltaR)
      deltaR= deltaB;
   return deltaR;
}


// return average of two colors c1 & c2
QRgb ColorReduction::colorBlend(QRgb c1, QRgb c2)
{
   int r= ( qRed(c1) + qRed(c2) ) >> 1;
   int g= ( qGreen(c1) + qGreen(c2) ) >> 1;
   int b= ( qBlue(c1) + qBlue(c2) ) >> 1;
   return qRgb(r,g,b);
}


// create dither palette with all possible color combinations
// pal: input colors
// colorCount: number of colors in pal
// maxColorDifference: only allow combinations with colors differences below a maximum threshold
//                     e.g. don't dither black & white or pure red & blue
QList<DitherColor> ColorReduction::buildPaletteDither(
   QVector<unsigned int>& pal,
   int colorCount,
   int maxColorDifference
)
{
   QList<DitherColor> colors;
   QSet<unsigned int> combinations;
   for (int i=0; i<colorCount; i++)
   {
      for (int j=0; j<colorCount; j++)
      {
         // ein Palettenfarben-Paar (i,j)
         unsigned short colorPair= 0;

         // "obere" Farbe soll immer kleiner sein
         // so muss man nicht zwischen (i,j) und (j,i) unterscheiden
         if (i<j)
            colorPair= (i<<8)|j;
         else
            colorPair= (j<<8)|i;

         if (!combinations.contains(colorPair) && colorDistance(pal[i], pal[j]) < maxColorDifference)
         {
            DitherColor color;
            color.mIndex1= colorPair >> 8;
            color.mIndex2= colorPair & 255;
            color.mColor= colorBlend(pal[i], pal[j]);
            combinations.insert(colorPair);
            colors.append(color);
         }
      }
   }

   return colors;
}


// create dither palette without dithering
QList<DitherColor> ColorReduction::buildPalette(
   QVector<unsigned int>& pal,
   int colorCount
)
{
   QList<DitherColor> colors;
   for (int i=0; i<colorCount; i++)
   {
      DitherColor color;
      color.mIndex1= i;
      color.mIndex2= i;
      color.mColor= pal[i];
      colors.append(color);
   }
   return colors;
}


// find best match for "color" in dither palette
int ColorReduction::findBestMatch(
   QRgb color,
   const QList<DitherColor>& colors
)
{
   int best= 0;
   int bestError= 256; // maximum possible error
   const int colorCount= colors.size();
   for (int index=0; index<colorCount; index++)
   {
      const DitherColor& ditherColor= colors[index];
      int error= colorDistance(color, ditherColor.mColor);
      if (error < bestError)
      {
         bestError= error;
         best= index;
      }
   }
   return best;
}


// process pixels from "input" image (in rect), reduce to given "colors" and store in "output"
void ColorReduction::reduceColors(
   QImage& output,
   const QImage& input,
   const QRect& rect,
   const QList<DitherColor>& colors,
   const QVector<QRgb>& palette
)
{
   int x,y;
   const char ditherMatrix[2*2]= {
      0,1,
      1,0
   };
   for (y=rect.top(); y<=rect.bottom(); y++)
   {
      unsigned int* src= (unsigned int*)input.scanLine(y);
      unsigned int* dst= (unsigned int*)output.scanLine(y);
      for (x=rect.left(); x<=rect.right(); x++)
      {
         int ditherIndex= findBestMatch(src[x], colors);
         unsigned char palIndex;
         if (ditherMatrix[(y<<1&2)|(x&1)])
            palIndex= colors[ditherIndex].mIndex1;
         else
            palIndex= colors[ditherIndex].mIndex2;
         dst[x]= palette[palIndex];
      }
   }
}


//// process pixels from "input" image (in rect), reduce to given "colors" and store in "output"
//void ColorReduction::reduceColors(
//   QImage& output,
//   const QImage& input,
//   const QRect& rect,
//   const QList<DitherColor>& colors
//)
//{
//   int x,y;
//   const char ditherMatrix[2*2]= {
//      0,1,
//      1,0
//   };
//   for (y=rect.top(); y<=rect.bottom(); y++)
//   {
//      unsigned int* src= (unsigned int*)input.scanLine(y);
//      unsigned char* dst= (unsigned char*)output.scanLine(y);
//      for (x=rect.left(); x<=rect.right(); x++)
//      {
//         int index= findBestMatch(src[x], colors);
//         if (ditherMatrix[(y<<1&2)|(x&1)])
//            dst[x]= colors[index].mIndex1;
//         else
//            dst[x]= colors[index].mIndex2;
//      }
//   }
//}

/*
int main(int argc, char** argv)
{
   QApplication app(argc, argv);

   // create window
   Window* window= new Window();

   // create color table for output image (4 colors)
   QVector<QRgb> pal(256);
   pal[0]= 0xff0b1114;
   pal[1]= 0xff476775;
   pal[2]= 0xff93c1d0;
   pal[3]= 0xffebf5f7;

   // create "dither palette" with all 4*4 color combinations for a 2x2 dither
   QList<DitherColor> ditherColors= buildPaletteDither(pal, 4); // process with dither
//   QList<DitherColor> ditherColors= buildPalette(pal, 4); // process without dither

   // load input image (rgb 24bit)
   QImage input("data/acryl-iceplanet.png");

   // create output image in 8bit indexed and set color table
   QImage output(input.width(), input.height(), QImage::Format_Indexed8);
   output.setColorTable(pal);

   // image dimensations must be a multiple of 16!

   // reduce each 16x16 block to given 4 colors
   for (int y=0; y<input.height(); y+=16)
   {
      for (int x=0; x<input.width(); x+=16)
      {
         QRect rect(x,y, 16,16);
         reduceColors(output, input, rect, ditherColors);
      }
   }

   // set both images and show window
   window->setOriginalImage(input);
   window->setReducedImage(output);
   window->show();

   int res= app.exec();

   delete window;

   return res;
}
*/
