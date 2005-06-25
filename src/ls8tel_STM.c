/* $Id: ls8tel_STM.c,v 1.3 2005/06/25 12:09:26 uehira Exp $ */

/*
 * Copyright (c) 2002 -
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University
 */

/*
 * This program read status file(s) and print time calibration information.
 */

/*
 * 2005-06-14  imported to WIN_pkg tools
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ls8tel.h"

static char  *progname;
static char  rcsid[] =
   "$Id: ls8tel_STM.c,v 1.3 2005/06/25 12:09:26 uehira Exp $";

int main(int, char *[]);

int
main(int argc, char *argv[])
{
  FILE  *fp;
  static unsigned char  buf[LS8_A9_DLEN];
  static unsigned char  *ptr;
  short  err;
  int    i, sum;

  /* set program name */
  if (NULL != (progname = strrchr(argv[0], '/')))
    progname++;
  else
    progname = argv[0];

  fp = stdin;

  while (fread(buf, 1, LS8_A9_DLEN, fp) == LS8_A9_DLEN) {
    ptr = buf;

    /* clock */
    (void)printf("%02x%02x%02x.%02x%02x%02x ",
		 ptr[0],  ptr[1],  ptr[2],  ptr[3],  ptr[4],  ptr[5]);

    /* ch ID */
    ptr = buf + 6;
    (void)printf("%02X%02X ", ptr[0], ptr[1]);

    ptr = buf + 39;
    /* Almanac */
    (void)printf("%02x%02x%02x.%02x%02x%02x ",
		 ptr[0],  ptr[1],  ptr[2],  ptr[3],  ptr[4],  ptr[5]);

    /* positioning info. */
    /* always 1? comment out */
    /*  (void)printf("%d ", ptr[0]); */

    /* use statellite */
    ptr = buf + 31;
    (void)printf("%02X %02X %02X %02X %02X %02X %02X %02X ",
		 ptr[0],  ptr[1], ptr[2], ptr[3],
		 ptr[4],  ptr[5], ptr[6], ptr[7]);
    sum = 0;
    for (i = 0; i < 8; ++i)
      sum += ptr[i];
    if (sum)
      (void)printf(" ");
    else
      (void)printf("*");

    /* error */
    ptr = buf + 28;
    err = ((ptr[0] << 8) & 0x7f00) + (ptr[1] & 0x00ff);
    err <<= 1;
    err >>= 1;
    (void)printf("%7.1lf[msec]", err * 0.1);
    if (err <= -100 || err >= 100)
      (void)printf("**\n");
    else if (err <= -50 || err >= 50)
      (void)printf("*\n");
    else
      (void)printf("\n");
  }
}
