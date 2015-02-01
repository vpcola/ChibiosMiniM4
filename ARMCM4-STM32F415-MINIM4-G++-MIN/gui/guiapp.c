/*
 * guiapp.c
 *
 *  Created on: 1 Feb, 2015
 *      Author: Vergil
 */
#include "guiapp.h"

bool_t SaveMouseCalibration(unsigned instance, const void *calbuf, size_t size)
{
    GFILE* f;
    (void)instance;

    f = gfileOpen(CALIBRATION_FILE, "w");
    if (f)
    {
      gfileWrite(f, (void*)calbuf, size);
      gfileClose(f);
      return TRUE;
    }else
      return FALSE;
}
bool_t LoadMouseCalibration(unsigned instance, void *calbuf, size_t size)
{
    GFILE* f;
    (void)instance;

    f = gfileOpen(CALIBRATION_FILE, "r");
    if (f)
    {
      gfileRead(f, (void*)calbuf, size);
      gfileClose(f);

      return TRUE;
    }else
      return FALSE;
}

void startGUI()
{
  font_t        font1;
  coord_t       width, height; //, fheight1;

  gdispClear(Black);

  // Get the screen size
  width = gdispGetWidth();
  height = gdispGetHeight();
  font1 = gdispOpenFont("DejaVuSans24");
  //fheight1 = gdispGetFontMetric(font1, fontHeight)+2;
  gdispFillStringBox(0, 0, width, height, "Hello World", font1, White, Black, justifyCenter);
}




