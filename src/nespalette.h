#ifndef NESPALETTE_H
#define NESPALETTE_H

#include <QObject>

class NesPalette : public QObject
{

   Q_OBJECT

public:

   static NesPalette* getInstance();

   unsigned int getBg() const;
   void setBg(unsigned int bg);

   unsigned int getFg() const;
   void setFg(unsigned int fg);

   int getSelectedPalIndex() const;


signals:

   void changed();


public slots:

   void setSelectedPalIndex(int selectedPalIndex);


protected:

   NesPalette(QObject *parent = 0);

   unsigned int mBg;
   unsigned int mFg;

   int mSelectedPalIndex;

   static NesPalette* sInstance;
};

#endif // NESPALETTE_H
