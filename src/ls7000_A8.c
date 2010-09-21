/* $Id: ls7000_A8.c,v 1.1.2.2 2010/09/21 11:56:58 uehira Exp $ */

/*
 * Copyright (c) 2009 -
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University
 */

/*
 * This program read A8 packet file(s) and print setting information.
 */

/*
 * 2009-03-06  imported from ls8tel_STS.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "winlib.h"
#include "ls7000.h"

static char  *progname;
static const char  rcsid[] =
   "$Id: ls7000_A8.c,v 1.1.2.2 2010/09/21 11:56:58 uehira Exp $";

static int read_STL(FILE *);
static int read_CNT(FILE *);
static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  FILE  *fp;
  uint8_w  buf[128];
  uint8_w  *ptr;
  char   *fname;
  int    i, j, mnum;

  /* set program name */
  if (NULL != (progname = strrchr(argv[0], '/')))
    progname++;
  else
    progname = argv[0];

  if (argc < 2) {
    usage();
    exit(1);
  }

  /* begin file loop */
  for (j = 1; j < argc; ++j) {
    fname = argv[j];
    if ((fp = fopen(fname, "r")) == NULL) {
      (void)fprintf(stderr, "%s : %s\n", fname, (char *)strerror(errno));
      continue;
    }
    
    fread(buf, 1, 9, fp);
    ptr = buf;
    
    /* clock */
    (void)printf("%02x%02x%02x.%02x%02x%02x ",
		 ptr[0],  ptr[1],  ptr[2],  ptr[3],  ptr[4],  ptr[5]);
    
    /* ch ID */
    ptr += LS7_A89_ADDR;
    (void)printf("%02X%02X ", ptr[0], ptr[1]);
    
    /* module num */
    ptr += 2;
    mnum = ptr[0];
    (void)printf("module = %d\n", mnum);

    /* module loop */
    for (i = 0; i < mnum; ++i) {
      fread(buf, 1, 1, fp);
      (void)printf("Module %d: ", i);
      if (buf[0] == LS7_A8_CNT_FLAG) {  /* CTL */
	if (read_CNT(fp)) {
	  printf("Invalid CNT\n");
	  break;
	}
      } else if (buf[0] == LS7_A8_STL_FLAG) {  /* STL */
	if (read_STL(fp)) {
	  printf("Invalid STL\n");
	  break;
	}
      } else {
	printf("Invalid module flag\n");
	break;
      }
    }  /* for (i = 0; i < mnum; ++i) */
    
    (void)fclose(fp);
  }  /* for (j = 1; j < argc; ++j) */

  exit(0);
}


static int
read_STL(FILE *fp)
{
  uint8_w  buf[128];
  uint8_w  *ptr;
  int    i, gain, bit;

  fread(buf, 1, 1, fp);
  if (buf[0] != LS7_A8_STL_SIZ)
    return (1);
  if (fread(buf, 1, LS7_A8_STL_SIZ, fp) != LS7_A8_STL_SIZ)
    return (1);

  /* sampling rate */
  ptr = buf + 15;
  (void)printf("%dHz ", LS7_A8_sampling[ptr[0]]);

  /* gain and bit */
  ptr = buf + 16;
  for (i = 0; i < 3; ++i) {
    gain = (ptr[i] & LS7_A8_gain_mask) >> 4;
    bit = 24 - (ptr[i] & LS7_A8_bit_mask);
    if (gain)
      (void)printf("ch%d:x%d(%dbit) ", i + 1, gain, bit);
    else
      (void)printf("ch%d:OFF ", i + 1);
  }
  (void)printf("\n");

  return (0);
}

static int
read_CNT(FILE *fp)
{
  uint8_w  buf[128];
  uint8_w  *ptr;
  int  i;
  
  fread(buf, 1, 1, fp);
  if (buf[0] != LS7_A8_CNT_SIZ)
    return (1);
  if (fread(buf, 1, LS7_A8_CNT_SIZ, fp) != LS7_A8_CNT_SIZ)
    return (1);

  /* Software name */
  ptr = buf + 1;
  for (i = 0; i < 8; ++i)
    (void)printf("%c", ptr[i]);

  /* firmware date */
  ptr = buf + 9;
  (void)printf("(%02X/%02x/%02x-%02x:%02x:%02x) ",
	       ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);

  /* communication speed */
  ptr = buf + 16;
  if (ptr[0] > 5)
    (void)printf("\n");
  else
    (void)printf("%dbps\n", LS7_A8_speed[ptr[0]]);

  return (0);
}

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr, " usage : %s file file ...\n", progname);
}
