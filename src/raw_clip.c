/* $Id: raw_clip.c,v 1.1.8.2 2012/01/06 10:55:48 uehira Exp $ */

/* raw_clip.c -- clip waveform data */

/*
 * Copyright (c) 2005-
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *   2005-06-08  Initial version. imported from raw_shift.c
 *   2010-11-09  64bit clean. eobsize_in(auto).
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else				/* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else				/* !HAVE_SYS_TIME_H */
#include <time.h>
#endif				/* !HAVE_SYS_TIME_H */
#endif				/* !TIME_WITH_SYS_TIME */

#include <syslog.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif
#include "daemon_mode.h"
#include "winlib.h"

#define MAX_SR      HEADER_4B

#define BELL        0
/* #define DEBUG       0 */

static char rcsid[] =
  "$Id: raw_clip.c,v 1.1.8.2 2012/01/06 10:55:48 uehira Exp $";

char *progname, *logfile;
int  syslog_mode, exit_status;

static int     daemon_mode;
static char    *chfile;
static uint8_w	ch_table[WIN_CH_MAX_NUM];
static int	n_ch, negate_channel;

/* prototypes */
static void read_chfile(void);
static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  FILE            *fp_log;
  struct Shm      *shr, *shm;
  key_t		  inkey, outkey;
  /* int		  shmid_in, shmid_out; */
  uint32_w	  uni;
  char		  tb[1024];
  uint8_w         *ptr, *ptw, *ptr_lim, *ptr_save;
  unsigned long	  c_save;  /* 64bit ok */
  WIN_ch          ch, ch1;
  WIN_sr          sr, sr1;
  WIN_bs          gs, gs1;
  static int32_w  buf1[MAX_SR], buf2[MAX_SR];
  int32_w         min, max;
  int             c;
  int		  i, tow, rest, bits_shift;
  size_t          size_shm;
  WIN_bs          size;
  int             eobsize_in, eobsize_in_count;

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;

  if (strcmp(progname, "raw_clipd") == 0)
    daemon_mode = 1;

  while ((c = getopt(argc, argv, "D")) != -1)
    switch (c) {
    case 'D':
      daemon_mode = 1;  /* daemon mode */
      break;
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  if (argc < 4)
    usage();

  inkey = (key_t)atol(argv[0]);
  outkey = (key_t)atol(argv[1]);
  size_shm = (size_t)atol(argv[2]) * 1000;
  bits_shift = atoi(argv[3]) - 1;
  if (bits_shift < 0 || 31 < bits_shift) {
    (void)fprintf(stderr, "bit num must 1 - 32\n");
    exit(1);
  }
  
  min = -1 << bits_shift;
  max = (1 << bits_shift) -1 ;

  /* channel file */
  rest = 1;
  eobsize_in = 0;
  if (argc > 4) {
    if (strcmp("-", argv[4]) == 0)
      chfile = NULL;
    else {
      if (argv[4][0] == '-') {
	chfile = argv[4] + 1;
	negate_channel = 1;
      } else if (argv[4][0] == '+') {
	chfile = argv[4] + 1;
	negate_channel = 0;
      } else {
	chfile = argv[4];
	rest = negate_channel = 0;
      }
    }
  } else
    chfile = NULL;
  /*  fprintf(stderr, "ch_file=%s\n", chfile);   */

  /* log file */
  if (argc > 5) {
    logfile = argv[5];
    /* check logfile can open */
    if ((fp_log = fopen(logfile, "a")) == NULL) {
      (void)fprintf(stderr, "logfile '%s': %s\n", logfile, strerror(errno));
      exit(1);
    }
    (void)fclose(fp_log);
  } else {
    logfile = NULL;
    if (daemon_mode)
      syslog_mode = 1;
  }

  /* daemon mode */
  if (daemon_mode) {
    daemon_init(progname, LOG_USER, syslog_mode);
    umask(022);
  }

  /***** shared memory *****/
  /* in shared memory */
  shr = Shm_read(inkey, "in");
  /* if ((shmid_in = shmget(inkey, 0, 0)) == -1) */
  /*   err_sys("shmget in"); */
  /* if ((shr = (struct Shm *)shmat(shmid_in, 0, 0)) == (struct Shm *)-1) */
  /*   err_sys("shmat in"); */

  /* out shared memory */
  shm = Shm_create(outkey, size_shm, "out");
  /* if ((shmid_out = shmget(outkey, size_shm, IPC_CREAT | 0644)) == -1) */
  /*   err_sys("shmget out"); */
  /* if ((shm = (struct Shm *)shmat(shmid_out, 0, 0)) == (struct Shm *)-1) */
  /*   err_sys("shmat out"); */

  /* (void)snprintf(tb, sizeof(tb), */
  /* 		 "start in_key=%d id=%d out_key=%d id=%d size=%d", */
  /* 		 inkey, shmid_in, outkey, shmid_out, size_shm); */
  /* write_log(tb); */
  (void)snprintf(tb, sizeof(tb),
		 "%d bit: %d -- %d", bits_shift + 1, min, max);
  write_log(tb);

  /* set signal handler */
  signal(SIGTERM, (void *)end_program);
  signal(SIGINT, (void *)end_program);

  read_chfile();

reset:
  /* initialize buffer */
  Shm_init(shm, size_shm);
  /* shm->p = shm->c = 0; */
  /* shm->pl = (size_shm - sizeof(*shm)) / 10 * 9; */
  /* shm->r = (-1); */
  /* ptr = shr->d; */  /* verbose */
  while (shr->r == (-1))
    sleep(1);
  ptr = shr->d + shr->r;
  tow = (-1);

  size = mkuint4(ptr);
  if (mkuint4(ptr + size - 4) == size)
    eobsize_in = 1;
  else
    eobsize_in = 0;
  eobsize_in_count = eobsize_in;
  snprintf(tb, sizeof(tb), "eobsize_in=%d", eobsize_in);
  write_log(tb);

  /***** main loop *****/
  for (;;) {
    /* ptr_lim = ptr + (size = mkuint4(ptr_save = ptr)); */
    size = mkuint4(ptr_save = ptr);
    if (mkuint4(ptr + size - 4) == size) {
      if (++eobsize_in_count == 0)
	eobsize_in_count = 1;
    } else
      eobsize_in_count = 0;
    if (eobsize_in && eobsize_in_count == 0)
      goto reset;
    if (!eobsize_in && eobsize_in_count > 3)
      goto reset;
    ptr_lim = ptr + size;
    if(eobsize_in)
      ptr_lim -= 4;

    c_save = shr->c;
    ptr += WIN_BLOCKSIZE_LEN;
#if DEBUG
    for (i = 0; i < 6; i++)
      printf("%02X", ptr[i]);
    printf(" : %d R\n", size);
#endif
    /* make output data */
    ptw = shm->d + shm->p;
    ptw += 4;			/* size (4) */
    uni = (uint32_w)(time(NULL) - TIME_OFFSET);
    i = uni - mkuint4(ptr);
    if (i >= 0 && i < 1440) {	/* with tow */
      if (tow != 1) {
	(void)snprintf(tb, sizeof(tb), "with TOW (diff=%ds)", i);
	write_log(tb);
	if (tow == 0) {
	  write_log("reset");
	  goto reset;
	}
	tow = 1;
      }
      ptr += 4;
      *ptw++ = uni >> 24;	/* tow (H) */
      *ptw++ = uni >> 16;
      *ptw++ = uni >> 8;
      *ptw++ = uni;		/* tow (L) */
    } else if (tow != 0) {
      write_log("without TOW");
      if (tow == 1) {
	write_log("reset");
	goto reset;
      }
      tow = 0;
    }
    for (i = 0; i < WIN_TIME_LEN; i++)
      *ptw++ = (*ptr++);	/* YMDhms (6) */

    /*** channel loop ***/
    do {
      gs = win_get_chhdr(ptr, &ch, &sr);
      if (ch_table[ch]) {
#if DEBUG
	fprintf(stderr, " %u", gs);
#endif
	if (sr < MAX_SR) {
	  win2fix(ptr, buf1, &ch1, &sr1);
	  for (i = 0; i < sr1; i++) {
	    if (buf1[i] < min)
	      buf2[i] = min;
	    else if (buf1[i] > max)
	      buf2[i] = max;
	    else
	      buf2[i] = buf1[i];
	    /*  printf("%d %d\n", buf1[i], buf2[i]); */
	  }
	  ptw += (gs1 = winform(buf2, ptw, sr1, ch1));
#if DEBUG
	  fprintf(stderr, "->%u ", gs1);
#endif
	} else {
	  memcpy(ptw, ptr, gs);
	  ptw += gs;
	}
      } else if (rest) {
	memcpy(ptw, ptr, gs);
	ptw += gs;
      }
      ptr += gs;
    } while (ptr < ptr_lim);

    if (tow)
      i = 14;
    else
      i = 10;
    if((uni = (uint32_w)(ptw - (shm->d + shm->p))) > i) {
      /* uni = ptw - (shm->d + shm->p); */ /* verbose */
      shm->d[shm->p] = uni >> 24;	/* size (H) */
      shm->d[shm->p + 1] = uni >> 16;
      shm->d[shm->p + 2] = uni >> 8;
      shm->d[shm->p + 3] = uni;	/* size (L) */

#if DEBUG
      if (tow)
	for (i = 0; i < 6; i++)
	  printf("%02X", shm->d[shm->p + 8 + i]);
      else
	for (i = 0; i < 6; i++)
	  printf("%02X", shm->d[shm->p + 4 + i]);
      printf(" : %d M\n", uni);
#endif

      shm->r = shm->p;
      if (ptw > shm->d + shm->pl)
	ptw = shm->d;
      shm->p = ptw - shm->d;
      shm->c++;
    }
#if BELL
    fprintf(stderr, "\007");
    fflush(stderr);
#endif
    if ((ptr = ptr_save + size) > shr->d + shr->pl)
      ptr = shr->d;
    while (ptr == shr->d + shr->p)
      usleep(100000);
    if (shr->c < c_save || mkuint4(ptr_save) != size) {
      write_log("reset");
      goto reset;
    }
#ifdef GC_MEMORY_LEAK_TEST
    CHECK_LEAKS();
#endif
  }  /* for (;;) */
}

/* read channel control file */
static void
read_chfile(void)
{
  FILE           *fp;
  int		  i, j, k;
  char		  tbuf[1024];

  if (chfile != NULL) {
    if ((fp = fopen(chfile, "r")) != NULL) {
#if DEBUG
      fprintf(stderr, "ch_file=%s\n", chfile);
#endif
      if (negate_channel)
	for (i = 0; i < WIN_CH_MAX_NUM; i++)
	  ch_table[i] = 1;
      else
	for (i = 0; i < WIN_CH_MAX_NUM; i++)
	  ch_table[i] = 0;
      i = j = 0;
      while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
	if (*tbuf == '#' || sscanf(tbuf, "%x", &k) < 0)
	  continue;
	k &= 0xffff;
#if DEBUG
	fprintf(stderr, " %04X", k);
#endif
	if (negate_channel) {
	  if (ch_table[k] == 1) {
	    ch_table[k] = 0;
	    j++;
	  }
	} else {
	  if (ch_table[k] == 0) {
	    ch_table[k] = 1;
	    j++;
	  }
	}
	i++;
      }
#if DEBUG
      fprintf(stderr, "\n");
#endif
      n_ch = j;
      if (negate_channel)
	(void)snprintf(tbuf, sizeof(tbuf), "-%d channels", n_ch);
      else
	(void)snprintf(tbuf, sizeof(tbuf), "%d channels", n_ch);
      write_log(tbuf);
      (void)fclose(fp);
    } else {
#if DEBUG
      fprintf(stderr, "ch_file '%s' not open\n", chfile);
#endif
      (void)snprintf(tbuf, sizeof(tbuf), 
		     "channel list file '%s' : %s", chfile, strerror(errno));
      err_sys(tbuf);
    }
  } else {
    for (i = 0; i < WIN_CH_MAX_NUM; i++)
      ch_table[i] = 1;
    n_ch = i;
    write_log("all channels");
  }

  signal(SIGHUP, (void *)read_chfile);
}


/* print usage & exit */
static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage of %s :\n", progname);
  (void)fprintf(stderr,
		" usage : '%s [in_key] [out_key] [shm_size(KB)] [bits]\\\n",
		progname);
  (void)fprintf(stderr,
		"                       (-/[ch_file]/-[ch_file]/+[ch_file] ([log file]))'\n");
  exit(1);
}
