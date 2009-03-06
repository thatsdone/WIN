/* $Id: ls8tel_STS.c,v 1.2.2.1.2.1 2009/03/06 13:03:03 uehira Exp $ */

/*
 * Copyright (c) 2005 -
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University
 */

/*
 * This program read setting file(s) and print setting information.
 */

/*
 * 2005-06-14  imported from ls8tel_STM.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "winlib.h"
#include "ls8tel.h"

static char  *progname;
static char  rcsid[] =
   "$Id: ls8tel_STS.c,v 1.2.2.1.2.1 2009/03/06 13:03:03 uehira Exp $";

int main(int, char *[]);

int
main(int argc, char *argv[])
{
  FILE  *fp;
  uint8_w  buf[LS8_A8_DLEN];
  uint8_w  *ptr;
  int    i, gain;

  /* set program name */
  if (NULL != (progname = strrchr(argv[0], '/')))
    progname++;
  else
    progname = argv[0];

  fp = stdin;

  while (fread(buf, 1, LS8_A8_DLEN, fp) == LS8_A8_DLEN) {
    ptr = buf;

    /* clock */
    (void)printf("%02x%02x%02x.%02x%02x%02x ",
		 ptr[0],  ptr[1],  ptr[2],  ptr[3],  ptr[4],  ptr[5]);

    /* Firmware name */
    ptr = buf + 12;
    for (i = 0; i < 8; ++i)
      (void)printf("%c", ptr[i]);
    
    /* firmware date */
    ptr = buf + 20;
    (void)printf("(%02X/%02x/%02x-%02x:%02x:%02x) ",
		 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);

    /* communication speed */
    ptr = buf + 27;
    (void)printf("%dbps ", A8_speed[ptr[0]]);

    /* sampling rate */
    ptr = buf + 46;
    (void)printf("%dHz\n", A8_sampling[ptr[0]]);

    /* ch ID */
    ptr = buf + 6;
    (void)printf("%02X%02X ", ptr[0], ptr[1]);

    /* gain */
    ptr = buf + 47;
    for (i = 0; i < 4; ++i) {
      gain = A8_gain[ptr[i] & A8_gain_mask];
      if (gain)
	(void)printf("ch%d:%d ", i + 1, gain);
      else
	(void)printf("ch%d:OFF ", i + 1);
    }
    (void)printf("\n");
  }
}
