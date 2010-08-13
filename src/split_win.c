/* $Id: split_win.c,v 1.1.2.1 2010/08/13 14:33:38 uehira Exp $ */

/*-
 * Split win file in case of detection of time discontinuity.
 *   - Waa, Wab, .... and so on.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "winlib.h"

static const char  rcsid[] =
   "$Id: split_win.c,v 1.1.2.1 2010/08/13 14:33:38 uehira Exp $";

#define NAMELEN  1024

static char  *progname;
static long  sufflen = 2;       /* File name suffix length. */
static char  fname[NAMELEN];
static FILE  *fpin, *fpout = NULL;
static int   vflag = 0;

static void split_time(void);
static void newfile(void);
static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  int   c;
  char  *ep;

  /* get program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;
  
  while ((c = getopt(argc, argv, "a:vh")) != -1) {
    switch (c) {
    case 'a':    /* Suffix length */
      sufflen = strtol(optarg, &ep, 10);
      if (sufflen <= 0 || *ep) {
	fprintf(stderr, "%s: %s: illegal suffix length\n", progname, optarg);
	exit(1);
      }
      break;
    case 'v':
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

  fpin = stdin;

  if (*argv != NULL) {              /* Input file. */
    if (strcmp(*argv, "-") == 0)
      fpin = stdin;
    else if ((fpin = fopen(*argv, "r")) == NULL) {
      (void)fprintf(stderr, "%s: %s\n", strerror(errno), *argv);
      exit(1);
    }
    ++argv;
  }

  if (*argv != NULL)             /* File name prefix. */
    if (snprintf(fname, sizeof(fname), "%s", *argv++) >= sizeof(fname)) {
      (void)fprintf(stderr, "file name prefix is too long\n");
      exit(1);
    }

  if (strlen(fname) + (size_t)sufflen >= sizeof(fname)) {
    (void)fprintf(stderr, "suffix is too long\n");
    exit(1);
  }

  split_time();


  exit(0);
}

/*
 * split_time
 *   Split win file in case of detection of time discontinuity.
 */
static void
split_time(void)
{
  uint8_w  *dbuf = NULL;
  WIN_bs   dsize;
  time_t   dat_t, dat_t_save = -1;

  while ((dsize = read_onesec_win(fpin, &dbuf)) != 0) {
    /* skip invalid time stamp */
    if ((dat_t = bcd_t(dbuf +  WIN_BLOCKSIZE_LEN)) <= 0)
      continue;

    /* new file */
    if (dat_t != dat_t_save +1)
      newfile();

    if (fwrite(dbuf, 1, dsize, fpout) != dsize) {
      (void)fprintf(stderr, "Error ocuurred when output raw data\n");
      exit(1);
    }

    /* backup timestamp */
    dat_t_save = dat_t;
  }  /* while ((dsize = read_onesec_win(fpin, &dbuf)) != 0) */
}


/*-
 * newfile
 *   open a new output file.
 */
static void
newfile(void)
{
  long  i, maxfiles, tfnum;
  static long fnum = 0;
  static char *fpnt;
  
  if (fpout == NULL) {
    if (fname[0] == '\0') {
      fname[0] = 'W';
      fpnt = fname + 1;
    } else
      fpnt = fname + strlen(fname);

    fpout = stdout;
  }

  /* maxfiles = 26^sufflen. */
  maxfiles = 1;
  for (i = 0; i < sufflen; ++i)
    if ((maxfiles *= 26) <= 0) {
      (void)fprintf(stderr, "suffix is too long (max %ld)\n", i);
      exit(1);
    }
  
  if (fnum == maxfiles) {
    (void)fprintf(stderr, "too many files\n");
    exit(1);
  }

  /* Generate suffix of sufflen letters */
  tfnum = fnum;
  i = sufflen - 1;
  do {
    fpnt[i] = tfnum % 26 + 'a';
    tfnum /= 26;
  } while (i-- > 0);
  fpnt[sufflen] = '\0';

  ++fnum;
  if (freopen(fname, "w", stdout) == NULL) {
    (void)fprintf(stderr, "freopen(): %s\n", strerror(errno));
    exit(1);
  }
  if (vflag)
    fprintf(stderr, "Output : %s\n", fname);

}


static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s [-a suffix_length] [-v] [file [name]]\n", progname);
  (void)fprintf(stderr, "   options: -a    : suffix length\n");
  (void)fprintf(stderr, "            -v    : verbose mode\n");
  (void)fprintf(stderr, "            -h    : print this message\n");
  exit(1);
}

