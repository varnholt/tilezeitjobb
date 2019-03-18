#ifndef LAYER_H
#define LAYER_H

#include "constants.h"

#include <QObject>
#include <QString>


class Layer : public QObject
{
   Q_OBJECT

public:

   Layer();

   virtual void initialize();

   int getId() const;
   void setId(int id);

   LayerType getType() const;
   void setType(const LayerType &type);

   QString getName() const;
   void setName(const QString &name);

   bool isInitializing() const;
   void setInitializing(bool initializing);


signals:

   void changed();


protected:

   LayerType mType;


private:

   int mId;
   QString mName;
   bool mInitializing;

   static int sCounter;
};

#endif // LAYER_H
