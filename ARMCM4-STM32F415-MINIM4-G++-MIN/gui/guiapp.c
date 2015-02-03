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

static GListener gl;
static GHandle   ghButton1;
static GEvent* pe;

static void createWidgets(void) {
    GWidgetInit wi;

    // Apply some default values for GWIN
    gwinWidgetClearInit(&wi);
    wi.g.show = TRUE;

    // Apply the button parameters
    wi.g.width = 100;
    wi.g.height = 30;
    wi.g.y = 10;
    wi.g.x = 10;
    wi.text = "Press Me!";

    // Create the actual button
    ghButton1 = gwinButtonCreate(NULL, &wi);
}

static WORKING_AREA(waGUIThread, 512);
static msg_t GUIThread(void *arg)
{
  (void)arg;
  font_t        font1;
  coord_t       width, height;

  while(1) {
      // Get an Event
      pe = geventEventWait(&gl, TIME_INFINITE);

      switch(pe->type) {
          case GEVENT_GWIN_BUTTON:
              if (((GEventGWinButton*)pe)->gwin == ghButton1) {
                  // Our button has been pressed
                width = gdispGetWidth();
                height = gdispGetHeight();
                font1 = gdispOpenFont("DejaVuSans24");
                gdispFillStringBox(0, (height - 24) / 2, width, 24, "Hello World", font1, White, Black, justifyCenter);
                gdispCloseFont(font1);
              }
              break;

          default:
              break;
      }
  }

  return RDY_OK;
}

void startGUI()
{
    //font_t        font1;
    //coord_t       width, height; //, fheight1;

    // Set the widget defaults
    gwinSetDefaultFont(gdispOpenFont("UI2"));
    gwinSetDefaultStyle(&WhiteWidgetStyle, FALSE);
    gdispClear(Black);

    createWidgets();

    geventListenerInit(&gl);
    gwinAttachListener(&gl);

    // Start the GUI event loop
    chThdCreateStatic(waGUIThread, sizeof(waGUIThread), NORMALPRIO, GUIThread, NULL);

}




