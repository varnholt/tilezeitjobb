#include "layer.h"

int Layer::sCounter = 0;

Layer::Layer()
 : mType(LayerTypeInvalid),
   mId(0),
   mInitializing(false)
{
   mId = sCounter;
   sCounter++;
}


void Layer::initialize()
{

}


int Layer::getId() const
{
   return mId;
}


void Layer::setId(int id)
{
   mId = id;
}

LayerType Layer::getType() const
{
   return mType;
}

void Layer::setType(const LayerType &type)
{
   mType = type;
}

QString Layer::getName() const
{
   return mName;
}

void Layer::setName(const QString &name)
{
   mName = name;

   if (!isInitializing())
      emit changed();
}

bool Layer::isInitializing() const
{
   return mInitializing;
}

void Layer::setInitializing(bool initializing)
{
   mInitializing = initializing;
}
