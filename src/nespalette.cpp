#include "nespalette.h"

NesPalette* NesPalette::sInstance = 0;


NesPalette::NesPalette(QObject* parent)
 : QObject(parent),
   mFg(0),
   mBg(0),
   mSelectedPalIndex(0)
{
   sInstance = this;
}


int NesPalette::getSelectedPalIndex() const
{
    return mSelectedPalIndex;
}


void NesPalette::setSelectedPalIndex(int selectedPalIndex)
{
    mSelectedPalIndex = selectedPalIndex;
}


NesPalette *NesPalette::getInstance()
{
    if (!sInstance)
        new NesPalette();
    
    return sInstance;
}


unsigned int NesPalette::getBg() const
{
    return mBg;
}


void NesPalette::setBg(unsigned int bg)
{
   mBg = bg;
   emit changed();
}


unsigned int NesPalette::getFg() const
{
   return mFg;
}


void NesPalette::setFg(unsigned int fg)
{
   mFg = fg;
   emit changed();
}
