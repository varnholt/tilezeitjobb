#ifndef NAMETABLELAYER_H
#define NAMETABLELAYER_H

#include "layer.h"

class NameTableLayer : public Layer
{
   Q_OBJECT

public:

   NameTableLayer();

   virtual void initialize();
};

#endif // NAMETABLELAYER_H
