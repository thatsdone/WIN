/* $Id: raw_time.c,v 1.7 2016/01/05 06:38:47 uehira Exp $ */

/* raw_time.c -- online version of wtime(1W) */

/*
 * Copyright (c) 2005-
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@bosai.go.jp
 *    National Research Institute for Earth Science and Disaster Prevention
 *
 *   2005-03-17  Initial version.  imported from wtime.c raw_raw.c ... etc.
 *   2010-10-13  64bit clean.
 *   2015-12-25  Sample size 5 mode supported.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <math.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else  /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif  /* !TIME_WITH_SYS_TIME */

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

#include "daemon_mode.h"
#include "winlib.h"
/* #include "win_system.h" */


#define MAXMESG   2048


static const char rcsid[] =
  "$Id: raw_time.c,v 1.7 2016/01/05 06:38:47 uehira Exp $";

char *progname, *logfile;
int  syslog_mode, exit_status;

static int     daemon_mode;
static char    *chfile;
static int     ms_add[WIN_CH_MAX_NUM], s_add[WIN_CH_MAX_NUM];
static int     ch_mask[WIN_CH_MAX_NUM];

/* prototypes */
static void usage(void);
static void read_chtbl(void);
int main(int, char *[]);


int
main(int argc, char *argv[])
{
  FILE        *fp_log;
  key_t       shm_key_in, shm_key_out;
  size_t      size;
  int         eobsize_in, eobsize_out, eobsize_in_count;
  uint32_w  uni;
  size_t  pl_out;
  struct Shm  *shm_in, *shm_out;
  uint8_w  *ptr, *ptr_save, *ptw;
  uint8_w  *ptr1, *ptr1_lim, *ptr2;
  WIN_bs  sizein, sizein2;
  unsigned long  c_save;  /* 64bit ok */
  int32_w  *fixbuf1 = NULL, *fixbuf2 = NULL;
  WIN_sr fixbuf_num;
  time_t      ltime[WIN_CH_MAX_NUM], ltime_prev[WIN_CH_MAX_NUM];
  char        msg[MAXMESG];
  WIN_ch         chn;
  WIN_sr         sr, sr_check[WIN_CH_MAX_NUM];
  WIN_bs         gsize, new_size;
  static int32_w   *sbuf[WIN_CH_MAX_NUM];
  WIN_ch  chdum;
  WIN_sr  srdum;
  int   c, sr_shift;
  int   i;
  int   ss_mode = SSIZE5_MODE, ssf_flag = 0;

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  for (i = 0; i < WIN_CH_MAX_NUM; ++i)
    sbuf[i] = NULL;
  
  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;
  eobsize_out = 0;

  if (strcmp(progname, "raw_timed") == 0)
    daemon_mode = 1;

  while ((c = getopt(argc, argv, "BDs:")) != -1)
    switch (c) {
    case 'B':
      eobsize_out = 1;  /* write blksiz at EOB in shm_out */
      break;
    case 'D':
      daemon_mode = 1;  /* daemon mode */
      break;
    case 's':
      if (strcmp(optarg, "4") == 0)
	ss_mode = 0;
      else if (strcmp(optarg, "5") == 0) {
	ss_mode = 1;
	ssf_flag = 0;
      } else if (strcmp(optarg, "5f") == 0) {
	ss_mode = 1;
	ssf_flag = 1;
      } else {
	fprintf(stderr, "Invalid option: -s\n");
	usage();
      }
      break;
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  if (argc < 4)
    usage();

  shm_key_in = (key_t)atol(argv[0]);
  shm_key_out = (key_t)atol(argv[1]);
  size = (size_t)atol(argv[2]) * 1000;
  chfile = argv[3];

#if DEBUG
  printf("in=%ld out=%ld size=%zu chfile=%s\n",
	 shm_key_in, shm_key_out, size, chfile);
#endif

  /* logfile */
  if (argc > 4) {
    logfile = argv[4];
    fp_log = fopen(logfile, "a");
    if (fp_log == NULL) {
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
  /* input */
  shm_in = Shm_read(shm_key_in, "in");
  /* if ((shmid_in = shmget(shm_key_in, 0, 0)) == -1) */
  /*   err_sys("shmget"); */
  /* if ((shm_in = (struct Shm *)shmat(shmid_in, (void *)0, 0)) == (struct Shm *)-1) */
  /*   err_sys("shmat"); */
  /* (void)snprintf(msg, sizeof(msg), "in : shm_key_in=%d id=%d", */
  /* 		 shm_key_in, shmid_in); */
  /* write_log(msg); */

  /* output */
  shm_out = Shm_create(shm_key_out, size, "out");
  /* if ((shmid_out = shmget(shm_key_out, size, IPC_CREAT | 0644)) == -1) */
  /*   err_sys("shmget"); */
  /* if ((shm_out = (struct Shm *)shmat(shmid_out, (void *)0, 0)) == (struct Shm *)-1) */
  /*   err_sys("shmat"); */
  /* (void)snprintf(msg, sizeof(msg), "shm_key_out=%d id=%d size=%d", */
  /* 		 shm_key_out, shmid_out, size); */
  /* write_log(msg); */

  /* initialize output buffer */
  Shm_init(shm_out, size);
  pl_out = shm_out->pl;
  /* shm_out->p = shm_out->c = 0; */
  /* shm_out->pl = pl_out = (size - sizeof(*shm_out)) / 10 * 9; */
  /* shm_out->r = (-1); */

  /* set signal handler */
  signal(SIGTERM, (void *)end_program);
  signal(SIGINT, (void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  read_chtbl();

 reset:
  fixbuf_num = 0;
  for (i = 0; i < WIN_CH_MAX_NUM; ++i)
    ltime_prev[i] = sr_check[i] = 0;

  /* set read point */
  while (shm_in->r == -1)
    sleep (1);
  ptr = shm_in->d + shm_in->r;
  sizein = mkuint4(ptr);
  if (mkuint4(ptr + sizein - WIN_BLOCKSIZE_LEN) == sizein)
    eobsize_in = 1;
  else
    eobsize_in = 0;
  eobsize_in_count = eobsize_in;
  (void)snprintf(msg, sizeof(msg),
		 "eobsize_in=%d, eobsize_out=%d", eobsize_in, eobsize_out);
  write_log(msg);

  /***** main loop *****/
  for (;;) {
    sizein = mkuint4(ptr_save = ptr);
    if (sizein == mkuint4(ptr + sizein - WIN_BLOCKSIZE_LEN)) {
      if (++eobsize_in_count == 0)
	eobsize_in_count = 1;
    } else
      eobsize_in_count = 0;

    if (eobsize_in && eobsize_in_count == 0)
      goto reset;
    if(!eobsize_in && eobsize_in_count > 3)
      goto reset;

    if (eobsize_in)
      sizein2 = sizein - WIN_BLOCKSIZE_LEN;
    else
      sizein2 = sizein;
#if DEBUG
    /*  printf("sizein=%d, sizein2=%d\n", sizein, sizein2); */
#endif

    c_save = shm_in->c;

    /*** channel loop ***/
    ptr1 = ptr + WIN_BLOCKSIZE_LEN + WIN_TIME_LEN;
    ptr1_lim = ptr + sizein2;
    ptr2 = ptr + WIN_BLOCKSIZE_LEN;
    do {
      gsize = win_get_chhdr(ptr1, &chn, &sr);

      if (ch_mask[chn]) {
#if DEBUG
	printf("%0X  %dHz: %d byte\n", chn, sr, gsize);
#endif
	/* check sampling rate */
	if (sr_check[chn] == 0) {
	  sr_check[chn] = sr;
	  sbuf[chn] = (int32_w *) win_xrealloc(sbuf[chn], (size_t)(sr * sizeof(int32_w)));
	  if (sbuf[chn] == NULL)
	    err_sys("malloc sbuf[ch]");
	}
	else if (sr_check[chn] != sr) {
	  (void)snprintf(msg, sizeof(msg), "%04X: %dHz-->%dHz\n",
			 chn, sr_check[chn], sr);
	  write_log(msg);
	  goto reset;
	  /* break? skip? reset? */
	}
	  
	if (fixbuf_num < sr) {
	  /*  printf("sr=%d\n",sr); */
	  fixbuf1 = (int32_w *) win_xrealloc(fixbuf1, (size_t)(sr * sizeof(int32_w)));
	  fixbuf2 = (int32_w *) win_xrealloc(fixbuf2, (size_t)(sr * sizeof(int32_w)));
	  if (fixbuf1 == NULL || fixbuf2 == NULL)
	    err_sys("fixbuf realloc");
	  fixbuf_num = sr;
	}

	/* set wrtite point */
	ptw = shm_out->d + shm_out->p;
	ptw += WIN_BLOCKSIZE_LEN;  /* size */
	uni=(uint32_w)(time(NULL)-TIME_OFFSET);  /* writing time */
	ptw[0] = uni >> 24;
	ptw[1] = uni >> 16;
	ptw[2] = uni >> 8;
	ptw[3] = uni;
	ptw += 4;

	/* time stamp */
	for (i = 0; i < WIN_TIME_LEN; ++i)
	  ptw[i] = ptr2[i];
	ltime[chn] = shift_sec(ptw, s_add[chn]);  /* shift time */
	ptw += WIN_TIME_LEN;
	
	/* shift waveform data */
	(void)win2fix(ptr1, fixbuf1, &chdum, &srdum);
	if (ltime[chn] != ltime_prev[chn] + 1)
	  for (i = 0; i < sr; i++)
	    sbuf[chn][i] = fixbuf1[0];
	sr_shift = (ms_add[chn] * sr + 500) / 1000;
	for (i = 0; i < sr_shift; ++i)
	  fixbuf2[i] = sbuf[chn][sr - sr_shift + i];
	for (i = sr_shift; i < sr; ++i)
	  fixbuf2[i] = fixbuf1[i - sr_shift];
	for (i = 0; i < sr; ++i)
	  sbuf[chn][i] = fixbuf1[i];
	ltime_prev[chn] = ltime[chn];
	/* ptw += winform(fixbuf2, ptw, sr, chn); */
	ptw += mk_windata(fixbuf2, ptw, sr, chn, ss_mode, ssf_flag);

	/* blocksize */
	new_size = ptw - (shm_out->d + shm_out->p);
	if (eobsize_out)
	  new_size += WIN_BLOCKSIZE_LEN;
	shm_out->d[shm_out->p    ] = new_size >> 24;
	shm_out->d[shm_out->p + 1] = new_size >> 16;
	shm_out->d[shm_out->p + 2] = new_size >> 8;
	shm_out->d[shm_out->p + 3] = new_size;
	if (eobsize_out) {
	  ptw[0] = new_size >> 24;
	  ptw[1] = new_size >> 16;
	  ptw[2] = new_size >> 8;
	  ptw[3] = new_size;
	  ptw += 4;
	}

	shm_out->r = shm_out->p;        /* latest */
	if (eobsize_out && ptw > shm_out->d + pl_out) {
	  shm_out->pl = ptw - shm_out->d - 4;
	  ptw = shm_out->d;
	}
	if (!eobsize_out && ptw > shm_out->d + shm_out->pl)
	  ptw = shm_out->d;
	shm_out->c++;                   /* conuter */
	shm_out->p = ptw - shm_out->d;  /* busy */
      }   /* if (ch_mask[chn]) */
      ptr1 += gsize;
    } while (ptr1 < ptr1_lim);
#if DEBUG
    printf("-------------------------\n");
#endif

    /* advance read pointer */
    ptr = ptr_save + sizein;
    if (ptr > shm_in->d + shm_in->pl)
      ptr = shm_in->d;

    /* wait */
    while (ptr == shm_in->d + shm_in->p)
      (void)usleep(10000);

    if (shm_in->c - c_save > 1000000 || mkuint4(ptr_save) != sizein) {
      write_log("reset");
      goto reset;
    }
#ifdef GC_MEMORY_LEAK_TEST
    CHECK_LEAKS();
#endif
  }  /* for (;;) */
}


static void
read_chtbl(void)
{
  FILE  *fp;
  char  buf[MAXMESG];
  WIN_ch  chtmp;
  int     delaytmp, flag, chnum;
  int     i;

  if ((fp = fopen(chfile, "r")) == NULL) {
    (void)snprintf(buf, sizeof(buf),
		   "Cannot open channel tabel file: %s", chfile);
    err_sys(buf);
  }

  /* clear channle mask */
  for (i = 0; i < WIN_CH_MAX_NUM; ++i)
    ch_mask[i] = 0;

  i = 0;
  while(fgets(buf, sizeof(buf), fp) != NULL) {
    if (buf[0] == '#')   /* skip comment line */
      continue;
    if (sscanf(buf, "%hx%d%d", &chtmp, &flag, &delaytmp) < 3)
      continue;
    if (!flag)           /* skip if recording flag = 0 */
      continue;
    if (delaytmp == 0)   /* skip no delay channel */
      continue;

    (void)snprintf(buf, sizeof(buf), "%04X: %dmsec", chtmp, delaytmp);
    write_log(buf);

    delaytmp *= -1;
    ch_mask[chtmp] = 1;
    s_add[chtmp] = (int)floor((double)delaytmp / 1000.0);
    ms_add[chtmp] = delaytmp - s_add[chtmp] * 1000;
    
#if DEBUG
    printf("%04X %ds %dms\n", chtmp, s_add[chtmp], ms_add[chtmp]);
#endif
    i++;
  }
  (void)fclose(fp);

  chnum = i;
  
  (void)snprintf(buf, sizeof(buf), "%d time shift entries in %s",
		 chnum, chfile);
  write_log(buf);
  
  signal(SIGHUP, (void *)read_chtbl);
}

/* print usage & exit */
static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage of %s :\n", progname);
  if (daemon_mode)
    (void)fprintf(stderr,
		  "  %s (-B) (-s [4|5|5f]) [inkey] [outkey] [shmsize] [chfile] (logfile)\n",
		  progname);
  else
    (void)fprintf(stderr,
		  "  %s (-BD) (-s [4|5|5f]) [inkey] [outkey] [shmsize] [chfile] (logfile)\n",
		  progname);
  exit(1);
}
