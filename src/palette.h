#ifndef PALETTE_H
#define PALETTE_H


class Palette
{

public:

   Palette();

   void setColors(unsigned int c1, unsigned int c2, unsigned int c3, unsigned int c4);
   void setColor(int index, unsigned int color);

   unsigned int getColor(int index) const;
   int getIndex(unsigned int color) const;
   int getIndexFuzzy(unsigned int color, unsigned int eps) const;

protected:

   unsigned int mColors[4];
};

#endif // PALETTE_H
