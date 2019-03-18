#ifndef LAYERSTACK_H
#define LAYERSTACK_H

// Qt
#include <QList>
#include <QObject>

class Layer;

class LayerStack : public QObject
{
   Q_OBJECT

public:

   static LayerStack* getInstance();

   void addLayer(Layer*);


   QList<Layer *> getLayers() const;

   Layer* getLayer(int index);

   void setLayers(const QList<Layer *> &layers);

   int getNameTableLayerIndex() const;

   int getCurrentIndex() const;

   Layer* getSelectedLayer();
   int getLayerCount() const;

   bool isInitializing() const;


public slots:

   void setCurrentIndex(int selectedIndex);
   void setInitializing(bool initializing);


   void removeLayers(QList<Layer*> indices);
   void removeLayer(Layer* = 0);
   void moveUp(Layer* = 0);
   void moveDown(Layer* = 0);


signals:

   void changed();


protected:

   LayerStack(QObject *parent = 0);

   QList<Layer*> mLayers;

   int mCurrentIndex;

   bool mInitializing;

   static LayerStack* sLayerStack;
};

#endif // LAYERSTACK_H
