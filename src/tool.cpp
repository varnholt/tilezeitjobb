#include "tool.h"

Tool::Tool()
{

}

ToolType Tool::getToolType() const
{
   return mToolType;
}

void Tool::setToolType(const ToolType &toolType)
{
   mToolType = toolType;
}
