#include "settings.h"


Settings* Settings::sSettings = 0;


Settings::Settings()
 : QSettings("tilezeitjobb.ini", QSettings::IniFormat),
   mGridSize(GridSize16x16),
   mNameTableLayout(NameTableLayoutDefault),
   mApplyColorReduction(true),
   mBlendOverOriginalImage(false),
   mHighlightSelectedPalette(false),
   mShowSpritesEnabled(true),
   mShowPalTestEffectEnabled(true)
{
   sSettings = this;
   load();
}

bool Settings::isShowPalTestEffectEnabled() const
{
   return mShowPalTestEffectEnabled;
}

void Settings::setShowPalTestEffectEnabled(bool showPalTestEffectEnabled)
{
   mShowPalTestEffectEnabled = showPalTestEffectEnabled;
}


bool Settings::isShowSpritesEnabled() const
{
   return mShowSpritesEnabled;
}


void Settings::setShowSpritesEnabled(bool showSprites)
{
   mShowSpritesEnabled = showSprites;
}


bool Settings::getHighlightSelectedPalette() const
{
   return mHighlightSelectedPalette;
}


void Settings::setHighlightSelectedPalette(bool highlightSelectedPalette)
{
   mHighlightSelectedPalette = highlightSelectedPalette;
}


bool Settings::getBlendOverOriginalImage() const
{
   return mBlendOverOriginalImage;
}


void Settings::setBlendOverOriginalImage(bool blendOverOriginalImage)
{
   mBlendOverOriginalImage = blendOverOriginalImage;
}


bool Settings::getApplyColorReduction() const
{
    return mApplyColorReduction;
}


void Settings::setApplyColorReduction(bool applyColorReduction)
{
    mApplyColorReduction = applyColorReduction;
}


NameTableLayout Settings::getNameTableLayout() const
{
    return mNameTableLayout;
}


void Settings::setNameTableLayout(const NameTableLayout &nameTableLayout)
{
    mNameTableLayout = nameTableLayout;
}


GridSize Settings::getGridSize() const
{
   return mGridSize;
}


void Settings::setGridSize(const GridSize &gridSize)
{
   mGridSize = gridSize;
}


Settings *Settings::getInstance()
{
   if (!sSettings)
      new Settings();

   return sSettings;
}


void Settings::save()
{
   setValue("general/image", mImage);
   setValue("general/project", mProject);
   setValue("general/gridsize", mGridSize);
   setValue("general/layout", mNameTableLayout);
   sync();
}


void Settings::load()
{
   mImage = value("general/image").toString();
   mProject = value("general/project").toString();
   mGridSize = (GridSize)value("general/gridsize").toInt();
   mNameTableLayout = (NameTableLayout)value("layout").toInt();

   if (mGridSize == GridSizeNone)
   {
      mGridSize = GridSize16x16;
   }
}


QString Settings::getImage() const
{
   return mImage;
}


void Settings::setImage(const QString &image)
{
   mImage = image;
}


QString Settings::getProject() const
{
   return mProject;
}


void Settings::setProject(const QString &project)
{
   mProject = project;
}


int Settings::getWidth()
{
   return getTileCountX() * 8;
}


int Settings::getHeight()
{
   return getTileCountY() * 8;
}


int Settings::getBlockStepsX()
{
   int steps = 0;
   steps = getTileCountX() / 2;
   return steps;
}


int Settings::getBlockStepsY()
{
   int steps = 0;
   steps = getTileCountY() / 2;
   return steps;
}


int Settings::getTileCountX()
{
   int count = 32;

   if (mNameTableLayout == NameTableLayoutVerticalFlipping)
   {
      count *= 2;
   }

   return count;
}


int Settings::getTileCountY()
{
   int count = 30;

   if (mNameTableLayout == NameTableLayoutHorizontalFlipping)
   {
      count *= 2;
   }

   return count;
}


int Settings::getBlockSizePx() const
{
   return BLOCK_SIZE_PX;
}


int Settings::getGridSizePx() const
{
   int px = 0;

   switch (mGridSize)
   {
      case GridSize1x1:
         px = 1;
         break;
      case GridSize8x8:
         px = 8;
         break;
      case GridSize16x16:
         px = 16;
         break;
      default:
         break;
   }

   return px;
}


int Settings::getGridStepsX()
{
   int steps = 0;
   int tileCountX = getTileCountX();

   switch (getGridSize())
   {
      case GridSize16x16:
         steps = tileCountX / 2;
         break;
      case GridSize8x8:
         steps = tileCountX;
         break;
      case GridSize1x1:
         steps = tileCountX * 8;
         break;
      default:
         break;
   }

   return steps;
}


int Settings::getGridStepsY()
{
   int steps = 0;
   int tileCountY = getTileCountY();

   switch (getGridSize())
   {
      case GridSize16x16:
         steps = tileCountY / 2;
         break;
      case GridSize8x8:
         steps = tileCountY;
         break;
      case GridSize1x1:
         steps = tileCountY * 8;
         break;
      default:
         break;
   }

   return steps;
}

