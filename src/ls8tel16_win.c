/* $Id: ls8tel16_win.c,v 1.2.2.1 2010/12/28 12:55:42 uehira Exp $ */

/*
 * Copyright (c) 2005
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University
 */

/*
 * Datamark LS-8000SH of LS8TEL16 utility
 *  Convert LS8TEL16 winformat to normal winformat.
 *  Don't input normal winformat data.
 *
 *  2010-10-12  64bit clean.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

#include "winlib.h"
#include "ls8tel.h"
/* #include "win_system.h" */

static const char  rcsid[] =
   "$Id: ls8tel16_win.c,v 1.2.2.1 2010/12/28 12:55:42 uehira Exp $";

static char  *progname;

static void usage(void);
int main(int, char *[]);


int
main(int argc, char *argv[])
{
  WIN_bs         wsize;
  uint32_w       dsize, gsize;
  uint8_w  *dbuf = NULL, *ptr, *ptr_limit;
  uint8_w  *obuf = NULL, *ptw, *ptw_limit;
  size_t         obuf_size = 0;
  int            dtime[WIN_TIME_LEN];
  WIN_ch         ch;
  WIN_sr         sr;
  static int32_w fixbuf[HEADER_4B];
  int            i, c;

  /* get program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  while ((c = getopt(argc, argv, "h")) != -1) {
    switch (c) {
    case 'h':
    default:
      usage();
      /* NOTREACHED */
    }
  }
  argc -= optind;
  argv += optind;

  /*** 1sec data loop ***/
  while ((dsize = read_onesec_win(stdin, &dbuf)) != 0) {
    /* skip invalid time stamp */
    if (bcd_dec(dtime, dbuf + WIN_BLOCKSIZE_LEN) == 0)
      continue;

    /* malloc obuf */
    if (obuf_size < (dsize << 3)) {
      obuf_size = (dsize << 3);
      obuf = (uint8_w *)win_xrealloc(obuf, obuf_size);
      if (obuf == NULL) {
	(void)fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
      }
    }
    ptw = obuf + WIN_BLOCKSIZE_LEN;
    ptw_limit = obuf + obuf_size;

    ptr = dbuf + WIN_BLOCKSIZE_LEN;
    ptr_limit = dbuf + dsize;

    /* copy time stamp */
    for (i = 0; i < WIN_TIME_LEN; ++i)
      *ptw++ = *ptr++;
    
    /* channel loop */
    do {
      if ((gsize = ls8tel16_fix(ptr, fixbuf, &ch, &sr)) == 0) {
	(void)fprintf(stderr, "This data is not LS8TEL format.\n");
	exit(1);
      }
      ptw += winform(fixbuf, ptw, sr, ch);
      if (ptw > ptw_limit) {
	(void)fprintf(stderr, "Buffer overrun!\n");
	exit(1);
      }
      ptr += gsize;
    } while (ptr < ptr_limit);

    /* write block size */
    wsize = ptw - obuf;
    obuf[0] = wsize >> 24;
    obuf[1] = wsize >> 16;
    obuf[2] = wsize >> 8;
    obuf[3] = wsize;

    /* output to stdout */
    (void)fwrite(obuf, wsize, 1, stdout);
  } /* while ((dsize = read_onesec_win(stdin, &dbuf)) != 0) */

#ifdef GC_MEMORY_LEAK_TEST
  CHECK_LEAKS();
#endif
  exit(0);
}

static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s < [LS8TEL16 file] > [out_file]\n",
		progname);
  exit(1);
}
