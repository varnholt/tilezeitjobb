#ifndef SUBPALETTEITEM_H
#define SUBPALETTEITEM_H

// Qt
#include <QWidget>

class QButtonGroup;
class QRadioButton;
class SubPaletteWidget;

class SubPaletteItem : public QWidget
{
   Q_OBJECT

   public:

      SubPaletteItem(QWidget* parent = 0);

      SubPaletteWidget* getWidget();
      QRadioButton* getRadioButton();

   signals:

      void changed();

   protected:

      SubPaletteWidget* mSubPaletteWidget;
      QRadioButton* mRadioButton;
};

#endif // SUBPALETTEITEM_H
