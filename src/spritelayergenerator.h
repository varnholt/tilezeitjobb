#ifndef SPRITELAYERGENERATOR_H
#define SPRITELAYERGENERATOR_H

class QImage;

class SpriteLayerGenerator
{

public:

   SpriteLayerGenerator();

   static void generate(QImage* image);
   static bool checkEmpty(QImage* spriteImage, unsigned int bg);
};

#endif // SPRITELAYERGENERATOR_H
