/* $Id: ls7000_A9.c,v 1.2 2011/06/01 11:09:21 uehira Exp $ */

/*
 * Copyright (c) 2009 -
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University
 */

/*
 * This program read A9 packet file(s) and print status information.
 */

/*
 * 2009-03-06  imported from ls7000_A8.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "winlib.h"
#include "ls7000.h"

static char  *progname;
static const char  rcsid[] =
   "$Id: ls7000_A9.c,v 1.2 2011/06/01 11:09:21 uehira Exp $";

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
    /* (void)printf("module = %d\n", mnum); */

    /* module loop */
    for (i = 0; i < mnum; ++i) {
      fread(buf, 1, 1, fp);
      /* (void)printf("Module %d: ", i); */
      if (buf[0] == LS7_A9_CNT_FLAG) {  /* CTL */
	if (read_CNT(fp)) {
	  printf("Invalid CNT\n");
	  break;
	}
      } else if (buf[0] == LS7_A9_STL_FLAG) {  /* STL */
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

  fread(buf, 1, 1, fp);
  if (buf[0] != LS7_A9_STL_SIZ)
    return (1);
  if (fread(buf, 1, LS7_A9_STL_SIZ, fp) != LS7_A9_STL_SIZ)
    return (1);
  
  /* always 0. Just through */
  /* printf("%d\n", buf[0]); */

  return (0);
}

static int
read_CNT(FILE *fp)
{
  uint8_w  buf[128];
  uint8_w  *ptr;
  int16_w  err;
  int  i, sum;
  
  fread(buf, 1, 1, fp);
  if (buf[0] != LS7_A9_CNT_SIZ)
    return (1);
  if (fread(buf, 1, LS7_A9_CNT_SIZ, fp) != LS7_A9_CNT_SIZ)
    return (1);

  /* Almanac */
  ptr = buf + 28;
  (void)printf("%02x%02x%02x.%02x%02x%02x ",
	       ptr[0],  ptr[1],  ptr[2],  ptr[3],  ptr[4],  ptr[5]);

  /* use statellite */
  ptr = buf + 20;
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

  /* speed */
  /* ptr = buf; */
  /*   speed = ((ptr[0] << 8) & 0x7f00) + (ptr[1] & 0x00ff); */
  /*   (void)printf("%dbps ", speed); */

  /* error */
  ptr = buf + 17;
  err = ((ptr[0] << 8) & 0x7f00) + (ptr[1] & 0x00ff);
  err <<= 1;
  err >>= 1;
  (void)printf("%7.1lf[msec]", err * 0.1);
  if (err <= -100 || err >= 100)
    (void)printf("**\n");
  else if (err <= -50 || err >= 50)
    (void)printf("*\n");
  else
    printf("\n");

  return (0);
}

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr, " usage : %s file file ...\n", progname);
}
