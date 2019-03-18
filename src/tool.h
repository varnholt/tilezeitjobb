#ifndef TOOL_H
#define TOOL_H

#include "constants.h"

class Tool
{

public:
   Tool();

   ToolType getToolType() const;
   void setToolType(const ToolType &toolType);


protected:

   ToolType mToolType;

};

#endif // TOOL_H
