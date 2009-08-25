/* $Id: wchasn.c,v 1.1.4.2.2.1 2009/08/25 04:00:16 uehira Exp $ */

/*-
  2009.7.31  64bit check.
  -*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "winlib.h"
/* #include "win_system.h" */

static const char  rcsid[] =
   "$Id: wchasn.c,v 1.1.4.2.2.1 2009/08/25 04:00:16 uehira Exp $";
static char  *progname;

static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  unsigned int  chtmp;
  WIN_ch  ch, chinit, chdummy;
  WIN_sr  srdummy;
  WIN_bs  dsize, gsize;
  uint8_w  *dbuf = NULL, *ptr, *ptr_limit;
  int  dtime[WIN_TIME_LEN], ssdummy;

  /* get program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  /* set channel number */
  if (argc < 2)
    usage();
  (void)sscanf(argv[1], "%x", &chtmp);
  chinit = (WIN_ch)chtmp;
  (void)fprintf(stderr, "CH = 0x%04X\n", chinit);
	
  /*** 1sec data loop ***/
  while ((dsize = read_onesec_win(stdin, &dbuf)) != 0) {
    /* skip invalid time stamp */
    if (bcd_dec(dtime, dbuf + WIN_BLOCKSIZE_LEN) == 0)
      continue;

    ch = chinit;
    ptr = dbuf + WIN_BLOCKSIZE_LEN + WIN_TIME_LEN;
    ptr_limit = dbuf + dsize;

    /* channel loop */
    do {
      ptr[0] = ch >> 8;
      ptr[1] = ch;

      /* gsize = win_get_chhdr(ptr, &chdummy, &srdummy); */
      gsize = win_chheader_info(ptr, &chdummy, &srdummy, &ssdummy);
      ptr += gsize;
      ch++;
    } while (ptr < ptr_limit);

    /* output data */
    (void)fwrite(dbuf, 1, dsize, stdout);
  } /* while ((dsize = read_onesec_win(stdin, &dbuf)) != 0) */

  exit(0);
}

static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s [CH(in hex)] <[in_file] >[out_file]\n",
		progname);
  exit(1);
}
