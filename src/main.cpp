#include "mainwindow.h"

// Qt
#include <QApplication>

// std
#include <stdio.h>
#include <stdlib.h>

// tile
#include "tilegenerator.h"

// --generate --path=F:\svn\nes\tile\cubes --x0=0 --y0=0 --width=7 --height=9

int main(int argc, char *argv[])
{
   printf("tilezeitjobb by mueslee^hjb\n");
   printf("---------------------------\n");

   if (argc >= 2)
   {
      QString param(argv[1]);

      if (param == "--help" || param == "-?" || param == "-h")
         printf(
            "usage: %s\n"
            "   [--generate\n"
            "      [--path=path]\n"
            "      [--x0==tile x0]\n"
            "      [--y0=tile y0]\n"
            "      [--width = image width / 8]\n"
            "      [--height = image height / 8]\n"
            "   ]\n",
            argv[0]
         );

      else if (param ==("--generate"))
      {
         QString path;
         int x0 = 0;
         int y0 = 0;
         int width = -1;
         int height = -1;
         bool ok = false;

         for (int paramIndex = 2; paramIndex < argc; paramIndex++)
         {
            param = QString(argv[paramIndex]);
            QStringList elems = QString(param).split("=");

            if (elems.size() == 2)
            {
               int tmp = elems.at(1).toInt(&ok);

               if (param.startsWith("--path"))
               {
                  path = elems.at(1);
               }

               if (param.startsWith("--x0"))
               {
                  if (ok)
                     x0 = tmp;
               }

               if (param.startsWith("--y0"))
               {
                  if (ok)
                     y0 = tmp;
               }

               if (param.startsWith("--width"))
               {
                  if (ok)
                     width = tmp;
               }

               if (param.startsWith("--height"))
               {
                  if (ok)
                     height = tmp;
               }
            }
         }

         TileGenerator* generator = TileGenerator::getInstance();
         generator->processBatch(
            path,
            x0,
            y0,
            width,
            height
         );
      }

      return 0;
   }
   else
   {
      QApplication a(argc, argv);
      MainWindow w;
      w.show();
      w.setWindowTitle("tilezeitjobb 1.0 done by mueslee/haujobb in 2016");
      w.init();

      return a.exec();
   }
}
