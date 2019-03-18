#include "toolpixelpencil.h"


ToolPixelPencil::ToolPixelPencil()
 : mSize(1)
{
   mToolType = ToolTypePixelPencil;
}


int ToolPixelPencil::getSize() const
{
   return mSize;
}


void ToolPixelPencil::setSize(int size)
{
   mSize = size;
}
