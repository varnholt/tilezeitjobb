#ifndef TOOLPIXELPENCIL_H
#define TOOLPIXELPENCIL_H

#include "tool.h"

class ToolPixelPencil : public Tool
{

public:

   ToolPixelPencil();

   int getSize() const;
   void setSize(int size);


protected:

   int mSize;
};

#endif // TOOLPIXELPENCIL_H
