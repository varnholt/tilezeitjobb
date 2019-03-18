#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>
#include <QObject>
#include <QString>

#include "constants.h"

class Settings : public QSettings
{
   Q_OBJECT

public:

   static Settings* getInstance();


   QString getImage() const;

   QString getProject() const;

   int getWidth();
   int getHeight();

   int getTileCountX();
   int getTileCountY();

   int getBlockSizePx() const;
   int getBlockStepsX();
   int getBlockStepsY();

   int getGridSizePx() const;
   int getGridStepsX();
   int getGridStepsY();

   GridSize getGridSize() const;

   NameTableLayout getNameTableLayout() const;

   bool getApplyColorReduction() const;
   bool getBlendOverOriginalImage() const;
   bool getHighlightSelectedPalette() const;
   bool isShowSpritesEnabled() const;
   bool isShowPalTestEffectEnabled() const;


public slots:

   void save();
   void load();

   void setImage(const QString &image);
   void setProject(const QString &project);

   void setGridSize(const GridSize &gridSize);
   void setNameTableLayout(const NameTableLayout &nameTableLayout);

   void setApplyColorReduction(bool applyColorReduction);
   void setBlendOverOriginalImage(bool blendOverOriginalImage);
   void setHighlightSelectedPalette(bool highlightSelectedPalette);
   void setShowSpritesEnabled(bool showSprites);
   void setShowPalTestEffectEnabled(bool showPalTestEffectEnabled);


protected:

   Settings();

   QString mImage;
   QString mProject;

   GridSize mGridSize;

   NameTableLayout mNameTableLayout;

   bool mApplyColorReduction;
   bool mBlendOverOriginalImage;
   bool mHighlightSelectedPalette;
   bool mShowSpritesEnabled;
   bool mShowPalTestEffectEnabled;

   static Settings* sSettings;

};

#endif // SETTINGS_H
