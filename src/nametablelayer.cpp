// base
#include "nametablelayer.h"

// Qt
#include <QApplication>


NameTableLayer::NameTableLayer()
{
   mType = LayerTypeNameTable;
}


void NameTableLayer::initialize()
{
   setName(qApp->tr("nametable"));
}
