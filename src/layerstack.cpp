#include "layerstack.h"

#include "layer.h"

LayerStack* LayerStack::sLayerStack = 0;


LayerStack::LayerStack(QObject* parent)
 : QObject(parent),
   mCurrentIndex(0),
   mInitializing(false)
{
   sLayerStack = this;
}


bool LayerStack::isInitializing() const
{
   return mInitializing;
}


void LayerStack::setInitializing(bool initializing)
{
   mInitializing = initializing;
}


void LayerStack::removeLayers(QList<Layer*> layers)
{
   int row = INT_MAX;
   int tmpRow = 0;
   foreach (Layer* layer, layers)
   {
      if (layer->getType() != LayerTypeNameTable)
      {
         tmpRow = mLayers.indexOf(layer);
         if (tmpRow < row)
            row = tmpRow;

         mLayers.removeOne(layer);
      }
   }

   mCurrentIndex = qMax(tmpRow - 1, 0);

   if (!isInitializing())
      emit changed();
}


int LayerStack::getCurrentIndex() const
{
   return mCurrentIndex;
}


void LayerStack::setCurrentIndex(int selectedIndex)
{
   mCurrentIndex = selectedIndex;
}


Layer *LayerStack::getSelectedLayer()
{
   if (mCurrentIndex > -1 && mCurrentIndex < mLayers.size())
   {
      return mLayers[mCurrentIndex];
   }

   return 0;
}


int LayerStack::getLayerCount() const
{
   return mLayers.size();
}


QList<Layer *> LayerStack::getLayers() const
{
   return mLayers;
}


Layer *LayerStack::getLayer(int index)
{
   return mLayers.at(index);
}


void LayerStack::setLayers(const QList<Layer *> &layers)
{
   mLayers = layers;
}

int LayerStack::getNameTableLayerIndex() const
{
   int index = 0;

   foreach (Layer* layer, mLayers)
   {
      if (layer->getType() == LayerTypeNameTable)
      {
         break;
      }
      else
      {
         index++;
      }
   }

   return index;
}


void LayerStack::moveUp(Layer* selectedLayer)
{
   int index = 0;

   if (selectedLayer)
      index = mLayers.indexOf(selectedLayer);
   else
      index = mCurrentIndex;

   if (index != -1)
   {
      // check if layer is already on top
      if (index != mLayers.size() - 1)
      {
         mLayers.swap(index, index + 1);
         mCurrentIndex = index + 1;
      }
   }

   if (!isInitializing())
      emit changed();
}


void LayerStack::moveDown(Layer* selectedLayer)
{
   int index = 0;

   if (selectedLayer)
      index = mLayers.indexOf(selectedLayer);
   else
      index = mCurrentIndex;

   if (index != -1)
   {
      // check if layer is already on top
      if (index != 0)
      {
         mLayers.swap(index, index - 1);
         mCurrentIndex = index - 1;
      }
   }

   if (!isInitializing())
      emit changed();
}


LayerStack *LayerStack::getInstance()
{
   if (!sLayerStack)
      new LayerStack;

   return sLayerStack;
}


void LayerStack::addLayer(Layer * layer)
{
   mLayers.push_back(layer);

   if (!isInitializing())
      emit changed();
}


void LayerStack::removeLayer(Layer * layer)
{
   if (layer->getType() != LayerTypeNameTable)
      mLayers.removeOne(layer);

   if (mCurrentIndex > 0)
      mCurrentIndex--;

   if (!isInitializing())
      emit changed();
}
