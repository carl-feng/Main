#include "bitmap_image.hpp"

int main(int argc, char** argv)
{
   if(argc != 7)
   {
       printf("Usage: %s widht height x1 y1 x2 y2\n", argv[0]);
       return -1;
   }

   int widht = atoi(argv[1]);
   int height = atoi(argv[2]);

   int x1 = atoi(argv[3]);
   int y1 = atoi(argv[4]);
   int x2 = atoi(argv[5]);
   int y2 = atoi(argv[6]);
   bitmap_image image(widht, height);

   // set background to orange
   image.set_all_channels(0,0,0);

   image_drawer draw(image);

   draw.pen_width(1000);
   draw.pen_color(255,255,255);
   for(int y= y1; y < y2; y++)
       draw.horiztonal_line_segment(x1-1, x2-1, y);

   image.save_image("mask.bmp");

   return 0;
}
