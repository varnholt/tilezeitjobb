#include "subpaletteitem.h"

// Qt
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QRadioButton>

// tile
#include "subpalettewidget.h"


SubPaletteItem::SubPaletteItem(QWidget *parent)
 : QWidget(parent),
   mRadioButton(0),
   mSubPaletteWidget(0)
{
   mRadioButton = new QRadioButton();
   mSubPaletteWidget = new SubPaletteWidget();

   QHBoxLayout *layout = new QHBoxLayout;
   layout->addWidget(mRadioButton);
   layout->addWidget(mSubPaletteWidget);
   layout->setMargin(0);

   mRadioButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

   setLayout(layout);

   connect(
      mSubPaletteWidget,
      SIGNAL(changed()),
      this,
      SIGNAL(changed())
   );

   setFixedWidth(140);
   setFixedHeight(40);
}


SubPaletteWidget *SubPaletteItem::getWidget()
{
   return mSubPaletteWidget;
}


QRadioButton *SubPaletteItem::getRadioButton()
{
   return mRadioButton;
}

