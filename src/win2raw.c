/*
 * $Id: win2raw.c,v 1.5 2004/09/13 10:10:37 uehira Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "win_system.h"
#include "subst_func.h"

static const char  rcsid[] =
   "$Id: win2raw.c,v 1.5 2004/09/13 10:10:37 uehira Exp $";
static char  *progname;

static void usage(void);
int main(int, char *[]);

#define NAMELEN  1025

/*
 * Divide win file into
 *  mode 5: 1 minute file (YYMMDDhh.mm)
 *  mode 4: 1 hour file   (YYMMDD.hh)
 *  mode 3: 1 day file    (YYMMDD)
 */
int
main(int argc, char *argv[])
{
  FILE  *fpin, *fpraw = NULL;
  int  c, uflag = 0, vflag = 0, mode = 5;
  char  *rawdir, fullname[NAMELEN];
  unsigned char  *dbuf = NULL;
  WIN_blocksize  dsize;
  int  dtime[WIN_TIME_LEN], dtime_save[5];
  int  i;

  /* get program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  /* get option */
  while ((c = getopt(argc, argv, "m:huv")) != -1) {
    switch (c) {
    case 'm':  /* set mode */
      if (optarg[0] == 'm')      /* minute mode (default) */
	mode = 5;
      else if (optarg[0] == 'h') /* hour mode */
	mode = 4;
      else if (optarg[0] == 'd') /* day mode */
	mode = 3;
      else {
	(void)fprintf(stderr, "Invalid option: -m\n");
	usage();
      }
      break;
    case 'u':  /* unlink mode */
      uflag = 1;
      break;
    case 'v':  /* verbose mode */
      vflag = 1;
      break;
    case 'h':
    default:
      usage();
      /* NOTREACHED */
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 1) {
    usage();
  } else if (argc == 1) {
    fpin = stdin;
    uflag = 0;
  } else {
    fpin = fopen(argv[1], "r");
    if (fpin == NULL) {
      (void)fprintf(stderr, "%s: %s\n", strerror(errno), argv[1]);
      usage();
    }
  }
  rawdir = argv[0];
  if(vflag) {
    (void)printf("ourdir: %s\n", rawdir);
    (void)printf("mode = %d\n", mode);
  }

  /* init array */
  for (i = 0; i < 5; ++i)
    dtime_save[i] = -1;

  /*** main loop ***/
  while ((dsize = read_onesec_win(fpin, &dbuf)) != 0) {
    /* skip invalid time stamp */
    if (bcd_dec(dtime, dbuf + WIN_BLOCKSIZE_LEN) == 0)
      continue;

    /* change new file */
    if (time_cmp(dtime, dtime_save, mode) != 0) {
      /* first, close file */
      if (fpraw != NULL)
	if(fclose(fpraw)) {
	  (void)fprintf(stderr, "%s: %s\n", strerror(errno), fullname);
	  exit(1);
	}

      /* new file name */
      if (mode == 5) {
	if (snprintf(fullname, sizeof(fullname),
		     "%s/%02d%02d%02d%02d.%02d", rawdir, dtime[0], dtime[1],
		     dtime[2], dtime[3], dtime[4]) >= sizeof(fullname)) {
	  (void)fprintf(stderr, "Buffer Overrun!\n");
	  exit(1);
	}
      } else if (mode == 4) {
	if (snprintf(fullname, sizeof(fullname),
		     "%s/%02d%02d%02d.%02d", rawdir, dtime[0], dtime[1],
		     dtime[2], dtime[3]) >= sizeof(fullname)) {
	  (void)fprintf(stderr, "Buffer Overrun!\n");
	  exit(1);
	}
      } else if (mode == 3) {
	if (snprintf(fullname, sizeof(fullname),
		     "%s/%02d%02d%02d", rawdir, dtime[0], dtime[1],
		     dtime[2]) >= sizeof(fullname)) {
	  (void)fprintf(stderr, "Buffer Overrun!\n");
	  exit(1);
	}
      }
      if (vflag)
	(void)printf("%s\n", fullname);

      /* open new file */
      fpraw = fopen(fullname, "w");
      if (fpraw == NULL) {
	(void)fprintf(stderr, "%s: %s\n", strerror(errno), fullname);
	exit(1);
      }

      /* save time stamp */
      for (i = 0; i < mode; ++i)
	dtime_save[i] = dtime[i];
    } /* if (time_cmp(dtime, dtime_save, mode) != 0) */

    /* output raw data */
    if (fwrite(dbuf, 1, dsize, fpraw) != dsize) {
      (void)fprintf(stderr, "Error ocuurred when output raw data\n");
      exit(1);
    }
  } /* while ((dsize = read_onesec_win(fpin, &dbuf)) != 0) */

  if(fclose(fpraw)) {
    (void)fprintf(stderr, "%s: %s\n", strerror(errno), fullname);
    exit(1);
  }

  /* close input file */
  if (fpin != stdin) {
    if (fclose(fpin)) {
      (void)fprintf(stderr, "%s: %s\n", strerror(errno), argv[1]);
      exit(1);
    }
  }
  
  /* if uflag, unlink file */
  if (uflag) {
    if (unlink(argv[1])) {
      (void)fprintf(stderr, "%s: %s\n", strerror(errno), argv[1]);
      exit(1);
    }
  }

  exit(0);
}

static void
usage(void)
{

  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s [options] rawdir [data]\n", progname);
  (void)fprintf(stderr, "   options: -u    : unlink input data file\n");
  (void)fprintf(stderr, "            -m [h|d]    : set mode\n");
  (void)fprintf(stderr, "                h: 1 hour file (YYMMDD.hh)\n");
  (void)fprintf(stderr, "                d: 1 day file (YYMMDD)\n");
  (void)fprintf(stderr, "            -v    : verbose mode\n");
  (void)fprintf(stderr, "            -h    : print this message\n");
  exit(1);
}
