/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.hpp"
#include "hal.h"
#include "test.h"
#include "chprintf.h"
#include "shell.h"

#include "ff.h"
#include "gfx.h"
#include "guiapp.h"

#include <string.h>

using namespace chibios_rt;

/*===========================================================================*/
/* Hardware defines                                                          */
/*===========================================================================*/
/* Define the I2C driver used */
#define I2C_DRIVER  I2CD1
/* I2C LED Blinkers */
#define I2C_CNT_ADDR 0x21

/*===========================================================================*/
/* Hardware configurations                                                   */
/*===========================================================================*/
static const SerialConfig serialcfg = {
    115200, // baud rate
    0,
    0,
    0,
};

// Configuration for SPI ILI9341
static const SPIConfig spi1cfg = {
  NULL,
  GPIOA,
  GPIOA_SPI1_CS,
  SPI_CR1_BR_1 | SPI_CR1_BR_0
};


/* I2C interface #2 */
static const I2CConfig i2cfg1 = {
    OPMODE_I2C,
    100000,
    FAST_DUTY_CYCLE_2,
};

/**
 * @brief FS object.
 */
FATFS MMC_FS;

/**
 * MMC driver instance.
 */
MMCDriver MMCD1;

/* BR bits and clock speeds (For this setup
 * Core clock is 84 MHz
 *  000  /2   =   42 MHz
 *  001  /4   =   21 MHz
 *  010  /8   =   10.5 MHz
 *  011  /16  =   5.25 MHz
 *  100  /32  =   2.625 MHz
 *  101  /64  =   1.3125 MHz
 *  110  /128 =   656250 kHz
 *  111  /256 =   328125 kHz
 **/

/* Maximum speed SPI configuration (18MHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig hs_spicfg = {NULL, GPIOB, GPIOB_SPI2_NSS, SPI_CR1_BR_0 }; // 010 (15 MHz)

/* Low speed SPI configuration (281.250kHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig ls_spicfg = {NULL, GPIOB, GPIOB_SPI2_NSS,
                              SPI_CR1_BR_2 | SPI_CR1_BR_1 }; // | SPI_CR1_BR_0 }; // 111 (468 kHz)

/* MMC/SD over SPI driver configuration.*/
static MMCConfig mmccfg = {&SPID2, &ls_spicfg, &hs_spicfg};

/*===========================================================================*/
/* Initialization Routines                                                   */
/*===========================================================================*/

static void initI2CHw(void)
{
    i2cStart(&I2C_DRIVER, &i2cfg1);
}

static void initMMCSpiHw(void)
{
    mmcObjectInit(&MMCD1);
    mmcStart(&MMCD1, &mmccfg);
}

/*===========================================================================*/
/* FatFs related.                                                            */
/*===========================================================================*/

/* FS mounted and ready.*/
static bool_t fs_ready = FALSE;

/* Generic large buffer.*/
uint8_t fbuff[1024];

/* Root Path */
char rootpath[50] = "sd";

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

/**
 * Mount a default filesystem on the root FS
 **/
static int mountFS(void)
{
    if (mmcConnect(&MMCD1))
    {
        chprintf((BaseSequentialStream *)&SD1, "Can not connect to MMC!\r\n");
        return 0;
    }
    // err = f_mount(0, &MMC_FS);
    if (!gfileMount('F', rootpath))
    {
        mmcDisconnect(&MMCD1);
        return 0;
    }
    chprintf((BaseSequentialStream *)&SD1, "Card mounted!\r\n");
    fs_ready = TRUE;
    return 1;
}

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WA_SIZE(1024)
#define TEST_WA_SIZE    THD_WA_SIZE(256)


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

static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"threads", cmd_threads},
  {"test", cmd_test},
  {"ls", cmd_listfiles},
  {"cat", cmd_cat },
  {"display", cmd_display},
  {NULL, NULL}
};

static const ShellConfig shell_cfg = {
  (BaseSequentialStream *)&SD1,
  commands
};


/*===========================================================================*/
/* Threads                                                                   */
/*===========================================================================*/

/*
 * Counter thread, times are in milliseconds.
 */
static WORKING_AREA(waThread2, 128);
static msg_t Thread2(void *arg) {

    (void)arg;
    uint8_t data[2], count = 0;
    msg_t status = RDY_OK;

    chRegSetThreadName("counter");

    // First set the direction registers
    data[0] = 0x00; // direction reg
    data[1] = 0x00; // all outputs
    i2cAcquireBus(&I2C_DRIVER);
    status = i2cMasterTransmitTimeout(&I2C_DRIVER, I2C_CNT_ADDR, data, 2, NULL, 0, 1000);
    i2cReleaseBus(&I2C_DRIVER);

    data[0] = 0x14; // output reg
    while (TRUE) {
        data[1] = count;
        i2cAcquireBus(&I2C_DRIVER);
        status = i2cMasterTransmitTimeout(&I2C_DRIVER, I2C_CNT_ADDR, data, 2, NULL, 0, 1000);
        i2cReleaseBus(&I2C_DRIVER);
        chThdSleepMilliseconds(100);
        count++;
    }

    // keep compiler happy
    return status;
}

/*===========================================================================*/
/* User functions                                                            */
/*===========================================================================*/


/*===========================================================================*/
/* Main Function                                                             */
/*===========================================================================*/

/*
 * Application entry point.
 */
int main(void) {

  Thread *shelltp = NULL;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  System::init();

  /**
   * Activates the serial driver using the driver default configuration.
   * PB6(TX) and PB7(RX) are routed to USART1.
   **/
  sdStart(&SD1, &serialcfg);


  /**
   * Initialize I2C1
   **/
  chprintf((BaseSequentialStream *)&SD1, "Initializing I2C...\r\n");
  initI2CHw();

  /*
   * Initialize the MMC over SPI hardware
   */
   chprintf((BaseSequentialStream *)&SD1, "Initializing MMC Hardware...\r\n");
   initMMCSpiHw();
   chprintf((BaseSequentialStream *)&SD1, "Mounting filesystem ...\r\n");
   /* Do an initial ls */
   if (mountFS())
    chprintf((BaseSequentialStream *)&SD1, "Filesystem mounted ...\r\n");
   else
    chprintf((BaseSequentialStream *)&SD1, "WARNING: Failed mounting filesystem ...\r\n");



  /*
   * Creates the blinker thread.
   */
  chprintf((BaseSequentialStream *)&SD1, "Starting counter thread...\r\n");
  chThdCreateStatic(waThread2, sizeof(waThread2), NORMALPRIO, Thread2, NULL);

  /**
   * Initialize the display
   **/
  gfxInit();
  startGUI();

  /**
   * Shell manager initialization.
   **/
  chprintf((BaseSequentialStream *)&SD1, "Initializing shell ...\r\n");
  shellInit();

  chprintf((BaseSequentialStream *)&SD1, "Starting shell ...\r\n");
  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and listen for events.
   */
  while (TRUE) {
    if (shelltp == NULL)
      shelltp = shellCreate(&shell_cfg, SHELL_WA_SIZE, NORMALPRIO);
    else if (chThdTerminated(shelltp))
    {
      chThdRelease(shelltp);    // Recovers memory of the previous shell.
      shelltp = NULL;           // Triggers spawning of a new shell.
    }
    chThdSleepMilliseconds(100);
  }

  return 0;
}
