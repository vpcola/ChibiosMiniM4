#include "ch.hpp"
#include "shellapps.h"
#include "test.h"
#include "chprintf.h"
#include "shell.h"

#include "ff.h"
#include "gfx.h"
#include "guiapp.h"
#include "ds1307.h"
#include "htu21d.h"


#include <string.h>
#include <stdlib.h>

/* Root Path */
extern char rootpath[50];
extern bool_t fs_ready;

/* Generic large buffer.*/
static uint8_t fbuff[1024];


static FRESULT scan_files(BaseSequentialStream *chp, char *path) {
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int i;
    char *fn;

#if _USE_LFN
    fno.lfname = 0;
    fno.lfsize = 0;
#endif
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        i = strlen(path);
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0)
                break;
            if (fno.fname[0] == '.')
                continue;
            fn = fno.fname;
            if (fno.fattrib & AM_DIR) {
                path[i++] = '/';
                strcpy(&path[i], fn);
                res = scan_files(chp, path);
                if (res != FR_OK)
                    break;
                path[--i] = 0;
            }
            else {
                chprintf(chp, "%s/%s\r\n", path, fn);
            }
        }
    }
    return res;
}


/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/


static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreStatus());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
  static const char *states[] = {THD_STATE_NAMES};
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "     name   addr    stack    prio refs   state  time\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%9s %.8lx %.8lx %4lu %4lu %9s %lu\r\n",
            tp->p_name,
            (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
            (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
            states[tp->p_state], (uint32_t)tp->p_time);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: test\r\n");
    return;
  }
  tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriority(),
                           TestThread, chp);
  if (tp == NULL) {
    chprintf(chp, "out of memory\r\n");
    return;
  }
  chThdWait(tp);
}

static void cmd_listfiles(BaseSequentialStream *chp, int argc, char *argv[])
{
    (void)argv;
    uint32_t freeClusters, numSectors;

    if (argc > 0) {
      chprintf(chp, "Usage: ls\r\n");
      return;
    }

    if (!fs_ready) {
      chprintf(chp, "File System not mounted\r\n");
      return;
    }

    freeClusters = gfileGetDiskClusters('F', rootpath);
    numSectors = gfileGetDiskClusterSize('F', rootpath);
    chprintf(chp,
             "FS: %lu free clusters, %lu sectors per cluster, %lu MB bytes free\r\n",
             freeClusters,
             numSectors,
             ( freeClusters * numSectors * 512 ) / 1000000
             );

    strcpy((char *) fbuff, ""); // list top dir
    scan_files(chp, (char *)fbuff);

}

#define CHUNK_SIZ 255
static void cmd_cat(BaseSequentialStream * chp, int argc, char *argv[])
{
  (void)argv;
  char buffer[CHUNK_SIZ+1];
  GFILE * fp = NULL;
  UINT i, bytesread;


  if (argc != 1) {
    chprintf(chp, "Usage: cat <file>\r\n");
    return;
  }

  fp = gfileOpen(argv[0], "r");
  if (fp == NULL)
  {
    chprintf(chp, "Can not open file %s\r\n", argv[0]);
    return;
  }

  do {
      memset(buffer, 0 , sizeof(buffer));
      bytesread = gfileRead(fp, buffer, CHUNK_SIZ);
      if (bytesread > 0)
      {
        for (i = 0; i < bytesread; i++)
          if (buffer[i] == '\n')
            chprintf(chp, "\r%c", buffer[i]);
          else
            chprintf(chp, "%c", buffer[i]);
      }
  }while((bytesread > 0) && !gfileEOF(fp));

  chprintf(chp, "\r\n");
  gfileClose(fp);
}

static gdispImage myImage;
static void cmd_display(BaseSequentialStream * chp, int argc, char *argv[])
{
  (void)argv;
  coord_t         swidth, sheight;

  if (argc != 1) {
    chprintf(chp, "Usage: display <file>\r\n");
    return;
  }


  // Set up IO for our image
  gdispImageOpenFile(&myImage, argv[0]);
  if (myImage.width > gdispGetWidth())
    gdispSetOrientation(GDISP_ROTATE_LANDSCAPE);
  else
    gdispSetOrientation(GDISP_ROTATE_PORTRAIT);

  // Get the display dimensions
  swidth = gdispGetWidth();
  sheight = gdispGetHeight();

  gdispImageDraw(&myImage, 0, 0, swidth, sheight, 0, 0);
  gdispImageClose(&myImage);

}

static void cmd_time(BaseSequentialStream * chp, int argc, char *argv[])
{
    (void)argv;

    if (argc > 0)
    {
        chprintf(chp, "Usage: time\r\n");
        return;
    }

    // Display the current time 
    chprintf(chp, "%s\r\n", now());
}

static void cmd_settime(BaseSequentialStream * chp, int argc, char *argv[])
{
    (void)argv;
    char tmpbuffer[100];
    DS1307_TIME oldval;
    int hour, min, sec;

    if (argc > 0)
    {
        chprintf(chp, "Usage: settime\r\n");
        return;
    }

    chprintf(chp, "Hour : ");
    memset(tmpbuffer, 0, 100);
    if (shellGetLine(chp, tmpbuffer, 100)) return;
    hour = atoi(tmpbuffer);
    chprintf(chp, "Min : ");
    if (shellGetLine(chp, tmpbuffer, 100)) return;
    min = atoi(tmpbuffer);
    chprintf(chp, "Sec : ");
    if (shellGetLine(chp, tmpbuffer, 100)) return;
    sec = atoi(tmpbuffer);

    // to preserve the date, we read current values
    if (rtcGetTime(&oldval))
    {
       oldval.hour = hour;
       oldval.min = min;
       oldval.sec = sec;

       if (!rtcSetTime(&oldval))
       {
           chprintf(chp, "Error setting time value!\r\n");
       }
    }else
        chprintf(chp, "Error getting old time value!\r\n");

}

static void cmd_setdate(BaseSequentialStream * chp, int argc, char *argv[])
{
    (void)argv;
    char tmpbuffer[100];
    DS1307_TIME olddate;
    int year, month, day, dayofweek;

    if (argc > 0)
    {
        chprintf(chp, "Usage: setdate\r\n");
        return;
    }

    chprintf(chp, "Year : ");
    memset(tmpbuffer, 0, 100);
    if (shellGetLine(chp, tmpbuffer, 100)) return;
    year = atoi(tmpbuffer);

    chprintf(chp, "Month (1-12 Jan ... Dec) :");
    memset(tmpbuffer, 0, 100);
    if (shellGetLine(chp, tmpbuffer, 100)) return;
    month = atoi(tmpbuffer);

    chprintf(chp, "Date : ");
    memset(tmpbuffer, 0, 100);
    if (shellGetLine(chp, tmpbuffer, 100)) return;
    day = atoi(tmpbuffer);

    chprintf(chp, "Day of the Week (1=Sunday .. 7=Saturday) : ");
    memset(tmpbuffer, 0, 100);
    if (shellGetLine(chp, tmpbuffer, 100)) return;
    dayofweek = atoi(tmpbuffer);

    // to preserve the date, we read current values
    if (rtcGetTime(&olddate))
    {
        olddate.year = year;
        olddate.month = month;
        olddate.date = day;
        olddate.wday = dayofweek;

        if (!rtcSetTime(&olddate))
            chprintf(chp, "Error setting date value!\r\n");
    }else
        chprintf(chp, "Error getting old date value!\r\n");
}

static void cmd_sensor(BaseSequentialStream * chp, int argc, char *argv[])
{
  (void)argv;

   chprintf(chp, "Current Temperature = %f\r\n", getTemp());
   chprintf(chp, "Current Humidity = %f\r\n", getHumidity());
}
static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"threads", cmd_threads},
  {"test", cmd_test},
  {"ls", cmd_listfiles},
  {"cat", cmd_cat },
  {"display", cmd_display},
  {"time", cmd_time},
  {"settime", cmd_settime},
  {"setdate", cmd_setdate},
  {"sensor", cmd_sensor},
  {NULL, NULL}
};

const ShellConfig shell_cfg = {
  (BaseSequentialStream *)&SD1,
  commands
};



