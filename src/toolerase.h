#ifndef TOOLERASE_H
#define TOOLERASE_H

#include "tool.h"

class ToolErase : public Tool
{
public:
   ToolErase();

   int getSize() const;
   void setSize(int size);

protected:

   int mSize;
};

#endif // TOOLERASE_H
