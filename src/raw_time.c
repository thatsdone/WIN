/* $Id: raw_time.c,v 1.4.2.1 2010/10/12 13:08:56 uehira Exp $ */

/* raw_time.c -- online version of wtime(1W) */

/*
 * Copyright (c) 2005-
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *   2005-03-17  Initial version.  imported from wtime.c raw_raw.c ... etc.
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
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else  /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif  /* !TIME_WITH_SYS_TIME */

#include <syslog.h>
#include <math.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif
#include "daemon_mode.h"
#include "subst_func.h"
#include "win_log.h"
#include "win_system.h"


#define MAXMESG   2048


static char rcsid[] =
  "$Id: raw_time.c,v 1.4.2.1 2010/10/12 13:08:56 uehira Exp $";

char *progname, *logfile;
int  daemon_mode, syslog_mode;
int  exit_status;

static char    *chfile;
static int     ms_add[WIN_CH_MAX_NUM], s_add[WIN_CH_MAX_NUM];
static int     ch_mask[WIN_CH_MAX_NUM];

/* prototypes */
static void usage(void);
static void read_chtbl(void);
static time_t shift_sec(unsigned char *, int);
int main(int, char *[]);


int
main(int argc, char *argv[])
{
  FILE        *fp_log;
  key_t       shm_key_in, shm_key_out;
  int         shmid_in, shmid_out, size;
  int         eobsize_in, eobsize_out, eobsize_in_count;
  unsigned long  pl_out, uni;
  struct Shm  *shm_in, *shm_out;
  unsigned char  *ptr, *ptr_save, *ptw;
  unsigned char  *ptr1, *ptr1_lim, *ptr2;
  WIN_blocksize  sizein, sizein2;
  unsigned long  c_save;
  long  *fixbuf1 = NULL, *fixbuf2 = NULL;
  WIN_sr fixbuf_num;
  time_t      ltime[WIN_CH_MAX_NUM], ltime_prev[WIN_CH_MAX_NUM];
  char        msg[MAXMESG];
  WIN_ch         chn;
  WIN_sr         sr, sr_check[WIN_CH_MAX_NUM];
  WIN_blocksize  gsize, new_size;
  static long    *sbuf[WIN_CH_MAX_NUM];
  long  chdum, srdum;
  int   c, sr_shift;
  int   i;


  if (progname = strrchr(argv[0], '/'))
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

  while ((c = getopt(argc, argv, "BD")) != -1)
    switch (c) {
    case 'B':
      eobsize_out = 1;  /* write blksiz at EOB in shm_out */
      break;
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

  shm_key_in = (key_t)atoi(argv[0]);
  shm_key_out = (key_t)atoi(argv[1]);
  size = atoi(argv[2]) * 1000;
  chfile = argv[3];

#if DEBUG
  printf("in=%d out=%d size=%d chfile=%s\n",
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
  if ((shmid_in = shmget(shm_key_in, 0, 0)) == -1)
    err_sys("shmget");
  if ((shm_in = (struct Shm *)shmat(shmid_in, 0, 0)) == (struct Shm *)-1)
    err_sys("shmat");
  (void)snprintf(msg, sizeof(msg), "in : shm_key_in=%d id=%d",
		 shm_key_in, shmid_in);
  write_log(msg);

  /* output */
  if ((shmid_out = shmget(shm_key_out, size, IPC_CREAT | 0644)) == -1)
    err_sys("shmget");
  if ((shm_out = (struct Shm *)shmat(shmid_out, 0, 0)) == (struct Shm *)-1)
    err_sys("shmat");
  (void)snprintf(msg, sizeof(msg), "shm_key_out=%d id=%d size=%d",
		 shm_key_out, shmid_out, size);
  write_log(msg);

  /* initialize output buffer */
  shm_out->p = shm_out->c = 0;
  shm_out->pl = pl_out = (size - sizeof(*shm_out)) / 10 * 9;
  shm_out->r = (-1);

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
  sizein = mklong(ptr);
  if (mklong(ptr + sizein - WIN_BLOCKSIZE_LEN) == sizein)
    eobsize_in = 1;
  else
    eobsize_in = 0;
  eobsize_in_count = eobsize_in;
  (void)snprintf(msg, sizeof(msg),
		 "eobsize_in=%d, eobsize_out=%d", eobsize_in, eobsize_out);
  write_log(msg);

  /***** main loop *****/
  for (;;) {
    sizein = mklong(ptr_save = ptr);
    if (sizein == mklong(ptr + sizein - WIN_BLOCKSIZE_LEN)) {
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
	  sbuf[chn] = (long *)realloc(sbuf[chn], (size_t)(sr * sizeof(long)));
	  if (sbuf[chn] == NULL)
	    err_sys("malloc sbuf[ch]");
	}
	else if (sr_check[chn] != sr) {
	  (void)snprintf(msg, sizeof(msg), "04X: %dHz-->%dHz\n",
			 chn, sr_check[chn], sr);
	  write_log(msg);
	  goto reset;
	  /* break? skip? reset? */
	}
	  
	if (fixbuf_num < sr) {
	  /*  printf("sr=%d\n",sr); */
	  fixbuf1 = (long *)realloc(fixbuf1, (size_t)(sr * sizeof(long)));
	  fixbuf2 = (long *)realloc(fixbuf2, (size_t)(sr * sizeof(long)));
	  if (fixbuf1 == NULL || fixbuf2 == NULL)
	    err_sys("fixbuf realloc");
	  fixbuf_num = sr;
	}

	/* set wrtite point */
	ptw = shm_out->d + shm_out->p;
	ptw += WIN_BLOCKSIZE_LEN;  /* size */
	uni = (unsigned long)time(NULL);  /* writing time */
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
	ptw += winform(fixbuf2, ptw, sr, chn);

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

    if (shm_in->c - c_save > 1000000 || mklong(ptr_save) != sizein) {
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
    if (sscanf(buf, "%x%d%d", &chtmp, &flag, &delaytmp) < 3)
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

static time_t
shift_sec(unsigned char *tm_bcd, int sec)
{
  int        tm[6];
  struct tm  *nt, mt;
  time_t     ltime;
  static int b2d[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,  /* 0x00 - 0x0F */
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0, 0, 0, 0, 0, 0,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 0, 0, 0, 0, 0, 0,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 0, 0, 0, 0, 0, 0,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 0, 0, 0, 0, 0, 0,
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 0, 0, 0, 0, 0, 0,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 0, 0, 0, 0, 0, 0,
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 0, 0, 0, 0, 0, 0,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 0, 0, 0, 0, 0, 0,
    90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 0, 0, 0, 0, 0, 0}; /* 0x90-0x9F */
  static unsigned char d2b[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99};

  memset(&mt, 0, sizeof(mt));
  if ((mt.tm_year = b2d[tm_bcd[0]]) < 50)
    mt.tm_year += 100;
  mt.tm_mon = b2d[tm_bcd[1]] - 1;
  mt.tm_mday = b2d[tm_bcd[2]];
  mt.tm_hour = b2d[tm_bcd[3]];
  mt.tm_min = b2d[tm_bcd[4]];
  mt.tm_sec = b2d[tm_bcd[5]];
  mt.tm_isdst = 0;
  ltime=mktime(&mt);

  if (sec)
    ltime += sec;
  else
    return (ltime);
    
  nt = localtime(&ltime);
  tm_bcd[0] = d2b[nt->tm_year % 100];
  tm_bcd[1] = d2b[nt->tm_mon + 1];
  tm_bcd[2] = d2b[nt->tm_mday];
  tm_bcd[3] = d2b[nt->tm_hour];
  tm_bcd[4] = d2b[nt->tm_min];
  tm_bcd[5] = d2b[nt->tm_sec];
  return (ltime);
}

/* print usage & exit */
static void
usage(void)
{

  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage of %s :\n", progname);
  if (daemon_mode)
    (void)fprintf(stderr,
		  "  %s (-B) [inkey] [outkey] [shmsize] [chfile] (logfile)\n",
		  progname);
  else
    (void)fprintf(stderr,
		  "  %s (-BD) [inkey] [outkey] [shmsize] [chfile] (logfile)\n",
		  progname);
  exit(1);
}
