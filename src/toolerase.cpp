#include "toolerase.h"

ToolErase::ToolErase()
{
   mToolType = ToolTypeErase;
}

int ToolErase::getSize() const
{
   return mSize;
}

void ToolErase::setSize(int size)
{
   mSize = size;
}
