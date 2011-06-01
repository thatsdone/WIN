/*
 * $Id: wck_wdisk.c,v 1.3 2011/06/01 11:09:22 uehira Exp $
 */

/*
 * 2002.03.03  Uehira.
 * 2010.09.17  64bit check
 */

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
   "$Id: wck_wdisk.c,v 1.3 2011/06/01 11:09:22 uehira Exp $";

char  *progname;

static void usage(void);
int main(int, char *[]);

#define  MIN  60

int
main(int argc, char *argv[])
{
  FILE  *chfp, *fp;
  static struct channel_tbl  tbl[WIN_CH_MAX_NUM];
  static int  **index;
  static uint8_w  *mainbuf;
  size_t  mainbuf_size;
  static WIN_ch  trg_ch[WIN_CH_MAX_NUM], trg_chnum;
  WIN_bs  mainsize;
  int  chnum, sec, tim[WIN_TIME_LEN], cflag, secsave, lflag;
  char  nsave[WIN_STANAME_LEN];
  char *rname;
  int  rflag = 0;
  int  c;
  int  i, j;

  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  while ((c = getopt(argc, argv, "r")) != -1)
    switch (c) {
    case 'r':
      rflag = 1;
      break;
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  if (argc < 1) {
    usage();
    exit(1);
  } else if (argc == 1)
    fp = stdin;
  else
    if ((fp = fopen(argv[1], "r")) == NULL) {
      (void)fprintf(stderr, "Cannot open data file: '%s'\n", argv[1]);
      (void)fprintf(stdout, "Cannot open data file: '%s'\n", argv[1]);
      usage();
      exit(1);
    }
  
  /* read channel table file */
  if ((chfp = fopen(argv[0], "r")) == NULL) {
    (void)fprintf(stderr, "Cannot open channel file: '%s'\n", argv[0]);
    (void)fprintf(stdout, "Cannot open channel file: '%s'\n", argv[0]);
    usage();
    exit(1);
  }
  chnum = read_channel_file(chfp, tbl, WIN_CH_MAX_NUM);
  (void)fclose(chfp);
#if DEBUG > 5
  (void)printf("chnum = %d\n", chnum);
  for (i = 0; i < chnum; ++i)
    printf("%x %d %d %s %s %d %s %lf %s %lf %lf %lf %lf %lf %lf %lf %lf %lf\n",
	   tbl[i].sysch, tbl[i].flag, tbl[i].delay, tbl[i].name, tbl[i].comp,
	   tbl[i].scale, tbl[i].bit, tbl[i].sens, tbl[i].unit,
	   tbl[i].t0, tbl[i].h, tbl[i].gain, tbl[i].adc, tbl[i].lat,
	   tbl[i].lng, tbl[i].higt, tbl[i].stcp, tbl[i].stcs);
#endif

  /* initialize */
  index = i_matrix(chnum, MIN + 1);
  mainbuf = NULL;
  sec = 0;

  /* data loop */
  while ((mainsize = read_onesec_win(fp, &mainbuf, &mainbuf_size)) != 0) {
#if DEBUG > 5
    (void)printf("mainsize = %d [bytes]\n", mainsize);
#endif
    /* read time stamp */
    if (bcd_dec(tim, mainbuf + WIN_BLOCKSIZE_LEN) == 0)
      continue;
#if DEBUG
    (void)printf("%02d%02d%02d.%02d%02d%02d\n", tim[0], tim[1], tim[2],
		 tim[3], tim[4], tim[5]);
#endif

    /* get data file channels list */
    trg_chnum = get_sysch_list(mainbuf + WIN_BLOCKSIZE_LEN,
			       mainsize - WIN_BLOCKSIZE_LEN, trg_ch);
#if DEBUG > 5
    (void)printf("trg_chnum = %d\n", trg_chnum);
    for (i = 0; i < trg_chnum; ++i)
      (void)printf("%x\n", trg_ch[i]);
#endif

    /* make check index */
    for (j = 0; j < chnum; ++j) {
      for (i = 0; i < trg_chnum; ++i) {
	if (trg_ch[i] == tbl[j].sysch) {
	  index[j][tim[5]] = 1;
	  break;
	}
      }
    }

    sec++;
    if (sec == MIN)
      break;
  }  /* while (mainsize = read_onesec_win(fp, &mainbuf, &mainbuf_size)) */
  if (fp != stdin)
    (void)fclose(fp);

  /* calculate sum */
  for(j = 0; j < chnum; ++j)
    for(i = 0; i < MIN; ++i)
      index[j][MIN] += index[j][i];
  
#if DEBUG > 3
  for(j = 0; j < chnum; ++j) {
    (void)printf("%04X (%7s%3s): ", tbl[j].sysch, tbl[j].name, tbl[j].comp);
    for (i = 0; i <= MIN; ++i)
      (void)printf("%d", index[j][i]);
    (void)printf("\n");
  }
#endif

  /*** output results ***/
  /* header */
  if (fp == stdin)
    (void)printf("Data from STDIN\n");
  else if (rflag) {
    if ((rname = strrchr(argv[1], '/')) == NULL)
      rname = argv[1];
    else
      rname++;
    (void)printf("%s\n", rname);
  } else
    (void)printf("FILE_NAME :: %s\n", argv[1]);

  /** main results **/
  if (rflag) {
    for(j = 0; j < chnum; ++j) {
      if (index[j][MIN] == MIN) /* skip complete data */
	continue;

      (void)printf("# %s %s\n", tbl[j].name, tbl[j].comp);
      (void)printf("%04X\t", tbl[j].sysch);
      for (i = 0; i < MIN; ++i) {
	if (!index[j][i])
	  (void)printf("%d ", i);
      }
      (void)printf("\n");      
    }  /* for(j = 0; j < chnum; ++j) */
  } else {  /* if (!rflag) */
    nsave[0] = '\0';
    lflag = 0;
    for(j = 0; j < chnum; ++j) {
      if (index[j][MIN] == MIN)  /* skip complete data */
	continue;
      else if (index[j][MIN] == 0) {  /* complete lost data */
	lflag = 1;
	if (strcmp(nsave, tbl[j].name)) {
	  (void)printf("\n  [%04X:%s %s]", tbl[j].sysch, tbl[j].name, tbl[j].comp);
	  (void)strcpy(nsave, tbl[j].name);
	} else
	  (void)printf(" - [%04X:%s %s]",
		       tbl[j].sysch, tbl[j].name, tbl[j].comp);
      } else {  /* incomplete data */
	lflag = 1;
	secsave = MIN;
	cflag = 0;
	(void)printf("\n  incomplete  [%04X:%s %s]  ", tbl[j].sysch,
		     tbl[j].name, tbl[j].comp);
	for (i = 0; i < MIN; ++i) {
	  if (index[j][i] == 0) {
	    if (i == (secsave + 1))
	      cflag = 1;
	    if (!cflag)
	      (void)printf("  %d", i);
	    secsave = i;
	  } else if (cflag) {  /* if (index[j][i] == 0) */
	    (void)printf("-%d ", secsave);
	    cflag = 0;
	  }
	} /* for (i = 0; i < MIN; ++i) */
	if (cflag)
	  (void)printf("-%d ", secsave);
	nsave[0] = '\0';
      }
    }  /* for(j = 0; j < chnum; ++j) */
    if (!lflag)
      (void)printf("\n  No lost packet in this file!\n");

    /* footer */
    (void)printf("\n ====CHECK END=====\n");
  }  /* if (!rflag) */


  exit(0);
}

static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s [-r] [chfile] ([file])\n", progname);
}
