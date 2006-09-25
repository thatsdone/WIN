/* $Id: winadd.c,v 1.4.4.1 2006/09/25 15:01:00 uehira Exp $ */

/*
 * winadd.c  (Uehira Kenji)
 *  last modified  2003/11/3
 *
 *  98/12/23  malloc bug (indx[i][j].name)
 *  99/1/6    skip no data file
 *  2000/2/29  bye-order free. delete NR code.
 *  2003/11/2-3  memory mode.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

#include <memory.h>

#include "winlib.h"
#include "win_system.h"
#include "subst_func.h"

struct data_index {
  unsigned long  len;
  unsigned long  point;
  int            flag;
  int            fnum;    /* in case of memory mode */
  char           *name;   /* in case of file mode */
};
typedef struct data_index  INDX;

/* global variables */
static const char rcsid[] =
   "$Id: winadd.c,v 1.4.4.1 2006/09/25 15:01:00 uehira Exp $";
static int  dummy_flag, verbose_flag;

/* prototypes */
static void memory_mode_run(int, char *[]);
static void get_index_from_buf(unsigned char *, off_t, int,
			       INDX **, WIN_ch *, int,
			       time_t *, int);
static void win_file_read_from_buf(unsigned char *, off_t,
				   WIN_ch **, int *, int *,
				   time_t **, int *, int *);
static void file_mode_run(int, char *[]);
static void get_index(char *, INDX **, WIN_ch *, int,
		      time_t *, int);
static void win_file_read(char *, WIN_ch **, int *, int *,
			  time_t **, int *, int *);
static int time_cmpq(const void *, const void *);
static void time2bcd(time_t, unsigned char *);
static time_t bcd2time(unsigned char *);
static void memory_error(void);
static void usage(void);
int main(int, char *[]);

/*  #define   DEBUG  0 */
#define  CH_INC     100
#define  TIME_INC   60
#define  MIN_LEN    8  /* minimum channel block length */

int
main(int argc, char *argv[])
{
  int  c;

  dummy_flag = 1;
  verbose_flag = 0;
  while ((c = getopt(argc, argv, "nvh")) != -1) {
    switch (c) {
    case 'n':   /* don't append dummy data */
      dummy_flag = 0;
      break;
    case 'v':   /* verbose mode */
      verbose_flag = 1;
      break;
    case 'h':   /* print usage */
    default:
      usage();
      exit(0);
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 1) {
    usage();
    exit(0);
  }

#if DEBUG
  fprintf(stderr,"verbose flag = %d, dummy flag=%d\n",
	  verbose_flag, dummy_flag);
#endif

  memory_mode_run(argc, argv);

  file_mode_run(argc, argv);

  exit(0);
}


static void
usage(void)
{

  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fputs("Usage : winadd [options] file1 file2 ... > output\n", stderr);
  (void)fputs("  options : -n         : do not append dummy headers.\n",
	      stderr);
  (void)fputs("            -v         : verbose mode.\n", stderr);
  (void)fputs("            -h         : print this message.\n", stderr);
}

static void
memory_error(void)
{

  (void)fputs("Cannot allocate memory!\n", stderr);
  exit(1);
}

static time_t
bcd2time(unsigned char *bcd)
{
  int  t[6];
  struct tm  time_str;
  time_t     time;

  bcd_dec(t,bcd);
  if (t[0] >= 70)
    time_str.tm_year = t[0];
  else
    time_str.tm_year = 100 + t[0]; /* 2000+t[0]-1900 */
  time_str.tm_mon = t[1] - 1;
  time_str.tm_mday = t[2];
  time_str.tm_hour = t[3];
  time_str.tm_min = t[4];
  time_str.tm_sec = t[5];
  time_str.tm_isdst = 0;

  if ((time = mktime(&time_str)) == (time_t)-1) {
    (void)fputs("mktime error.\n", stderr);
    exit(1);
  }
  return (time);
}

static void
time2bcd(time_t time, unsigned char *bcd)
{
  unsigned int  t[6];
  struct tm    time_str;

  time_str = *localtime(&time);
  if (time_str.tm_year >= 100)
    t[0] = time_str.tm_year - 100;
  else
    t[0] = time_str.tm_year;
  t[1] = time_str.tm_mon + 1;
  t[2] = time_str.tm_mday;
  t[3] = time_str.tm_hour;
  t[4] = time_str.tm_min;
  t[5] = time_str.tm_sec;
  dec_bcd(bcd,t);
}

static int
time_cmpq(const void *_a, const void *_b)
{
  unsigned long  a, b;

  a = *(unsigned long *)_a;
  b = *(unsigned long *)_b;

  if (a < b)
    return (-1);
  else if (a > b)
    return (1);
  else
    return (0);
}

/*
 * file mode functions
 */
static void
win_file_read(char *name, WIN_ch **ch, int *ch_num, int *ch_num_arr,
	      time_t **time, int *time_num, int *time_num_arr)
{
  FILE            *fp;
  unsigned char   tt[WIN_TIME_LEN], re_c[4];
  unsigned char   *ptr, *buf;
  WIN_blocksize   re, re_debug, gsize;
  WIN_sr          srdummy;
  time_t          time_tmp;
  WIN_ch  ch_tmp;
  int             i;
  
  if (NULL == (fp = fopen(name, "r"))) {
    (void)fprintf(stderr, "Warring! Skip file : %s\n", name);
    /* exit(1); */ 
    return;
  }

  /* read 1 sec block size */
  while (fread(re_c, 1, WIN_BLOCKSIZE_LEN, fp) == WIN_BLOCKSIZE_LEN) {
    re = mklong(re_c);
    re -= WIN_BLOCKSIZE_LEN;
    if (NULL == (buf = MALLOC(unsigned char, re))) {
      (void)fclose(fp);
      memory_error();
    }
    if (re != (re_debug = fread(buf, 1, re, fp))) {
      (void)fprintf(stderr, "Input file is strange! : %s\n", name);
      (void)fprintf(stderr, "  Require=%u [byte], but %u [byte]\n",
		    re, re_debug);
      FREE(buf);
      break;
    }
    ptr = buf;

    /* read & compare time */
    for (i = 0; i < WIN_TIME_LEN; ++i)
      tt[i] = *ptr++;
    time_tmp = bcd2time(tt);
    for (i = 0; i < *time_num; ++i)
      if (time_tmp == (*time)[i])
	break;
    if (i == *time_num) {
      (*time_num)++;
      if (*time_num > *time_num_arr) {
	if(NULL == (*time = REALLOC(time_t, *time, *time_num_arr + TIME_INC)))
	  memory_error();
	*time_num_arr += TIME_INC;
      }
      (*time)[i] = time_tmp;
    }

    /* read & compare channel number */
    while (ptr < buf + re) {
      gsize = win_get_chhdr(ptr, &ch_tmp, &srdummy);

      /* ch number */
      for (i = 0; i < *ch_num; ++i)
	if (ch_tmp == (*ch)[i])
	  break;
      if (i == *ch_num) {
	(*ch_num)++;
	if (*ch_num > *ch_num_arr) {
	  if (NULL == (*ch = REALLOC(WIN_ch, *ch, *ch_num + CH_INC)))
	    memory_error();
	  *ch_num_arr += CH_INC;
	}
	(*ch)[i] = ch_tmp;
      }
	 
      ptr += gsize;
    } /* while (ptr < buf + re) */
    FREE(buf);
  }  /* while(fread(re_c, 1 ,WIN_BLOCKSIZE_LEN , fp) == WIN_BLOCKSIZE_LEN) */

  (void)fclose(fp);
}

static void
get_index(char *name, INDX **indx, WIN_ch *ch, int ch_num,
	  time_t *time, int time_num)
{
  FILE   *fp;
  unsigned char  tt[WIN_TIME_LEN], re_c[4];
  unsigned char  *ptr, *buf;
  unsigned long  point;
  WIN_blocksize  re, gsize;
  WIN_sr         srdummy;
  time_t         time_tmp;
  WIN_ch ch_tmp;
  int            i;
  int            time_sfx, ch_sfx;
   
  if (NULL == (fp = fopen(name, "r"))) {
    /* (void)fprintf(stderr, "Cannot open input file : %s\n", name); */
    /* exit(1); */
    return;
  }
  point = (unsigned long)ftell(fp);

  /* read 1 sec block size */
  while (fread(re_c, 1, WIN_BLOCKSIZE_LEN, fp) == WIN_BLOCKSIZE_LEN) {
    re = mklong(re_c);
    re -= 4;
    if (NULL == (buf = MALLOC(unsigned char, re))) {
      (void)fclose(fp);
      memory_error();
    }
    if (re != fread(buf, 1, re, fp)) {
      FREE(buf);
      (void)fclose(fp);
      return;
    }
    ptr = buf;
    /* read & compare time */
    for (i = 0; i < WIN_TIME_LEN; ++i)
      tt[i] = *ptr++;
    time_tmp = bcd2time(tt);
    for (i = 0; i < time_num; ++i)
      if (time_tmp == time[i])
	break;
    time_sfx = i;
    /* read & compare channel number */
    while (ptr < buf + re){
      gsize = win_get_chhdr(ptr, &ch_tmp, &srdummy);
      
      /* ch number */
      for (i = 0; i < ch_num; ++i)
	if(ch_tmp == ch[i])
	  break;
      ch_sfx = i;

      /* make index */
      if (!indx[ch_sfx][time_sfx].flag) {
	indx[ch_sfx][time_sfx].flag = 1;
	strcpy(indx[ch_sfx][time_sfx].name, name);
	indx[ch_sfx][time_sfx].point = point + ptr - buf + 4;
	indx[ch_sfx][time_sfx].len = gsize;
#if DEBUG > 5
	   fprintf(stdout,"indx[%d][%d]-->%d  name=%s  point=%d  len=%d\n",
		   ch_sfx,time_sfx,indx[ch_sfx][time_sfx].flag,
		   indx[ch_sfx][time_sfx].name,indx[ch_sfx][time_sfx].point,
		   indx[ch_sfx][time_sfx].len);
#endif
      }
      ptr += gsize;
    }  /* while (ptr < buf + re) */
    point = (unsigned long)ftell(fp);
    FREE(buf);
  }  /*  while(fread(re_c, 1, WIN_BLOCKSIZE_LEN, fp) == WIN_BLOCKSIZE_LEN) */
  (void)fclose(fp);
}

static void
file_mode_run(int argcc, char *argvv[])
{
  static time_t  *time[1];
  static WIN_ch  *ch[1];
  int  ch_num = 0, time_num = 0, ch_num_arr, time_num_arr;
  int  i, j, max_name_len;
  INDX   **indx;
  unsigned long  *sortin, *time_sort;
  unsigned long secbuf_len, len_max;
  unsigned char *outbuf, *secbuf, *ptr;
  unsigned char tt[WIN_TIME_LEN];
  FILE  *fp_in;

  if (verbose_flag)
    (void)fprintf(stderr, "File mode\n");

  max_name_len = 0;

  if (NULL == (*time = MALLOC(time_t, TIME_INC)))
    memory_error();
  if (NULL == (*ch = MALLOC(WIN_ch, CH_INC)))
    memory_error();
  time_num_arr = TIME_INC;
  ch_num_arr = CH_INC;

  /* sweep all input file(s) and get channel number and time length */
  for (i = 0; i < argcc; ++i) { 
    if(max_name_len < strlen(argvv[i]))
      max_name_len = strlen(argvv[i]);
    win_file_read(argvv[i], ch, &ch_num, &ch_num_arr,
		  time, &time_num, &time_num_arr);
  }
  if ((ch_num == 0) || (time_num == 0))
      exit(1);

  /* malloc memory for INDX */
  if (NULL == (indx = MALLOC(INDX *, ch_num)))
    memory_error();
  for (i = 0; i < ch_num; ++i)
    if (NULL == (indx[i] = MALLOC(INDX, time_num)))
      memory_error();
  for (i = 0; i < ch_num; ++i)
    for (j = 0; j < time_num; ++j){
      indx[i][j].flag = 0;  /* clear flag */
      if(NULL == (indx[i][j].name = MALLOC(char, max_name_len + 1)))
	memory_error();
    }
  /* Make INDX table */
  for (i = 0; i < argcc; ++i)
    get_index(argvv[i], indx, *ch, ch_num, *time, time_num);
   
  /* sort time */
  if (NULL == (time_sort = MALLOC(unsigned long, time_num)))
    memory_error();
  if (NULL == (sortin = MALLOC(unsigned long, time_num)))
    memory_error();
  for (i = 0; i < time_num; ++i)
    time_sort[i] = (*time)[i];
  qsort(time_sort, time_num, sizeof(unsigned long), time_cmpq);
  for (i = 0; i < time_num; ++i) {
    for (j = 0; j < time_num; ++j) {
      if (time_sort[i] == (*time)[j]) {
	sortin[i] = j;
	break;
      }
    }
  }
#if DEBUG > 1
  for(i=0;i<time_num;++i){
    fprintf(stderr,"%d  %d:  %d\n",
	    (*time)[i],time_sort[i],(*time)[sortin[i]]);
    /* sleep(1); */
  }
#endif
  FREE(time_sort);

  /* make first time data */
  len_max = MIN_LEN;
  secbuf_len = 10;
  for (i = 0; i < ch_num; ++i) {
    if (indx[i][sortin[0]].flag) {
      secbuf_len += indx[i][sortin[0]].len;
      if (len_max < indx[i][sortin[0]].len)
	len_max = indx[i][sortin[0]].len;
    } else {
      if (dummy_flag)
	secbuf_len += MIN_LEN;
    }
  }
  if (NULL == (secbuf = MALLOC(unsigned char, secbuf_len)))
    memory_error();
  if (NULL == (outbuf = MALLOC(unsigned char, len_max)))
    memory_error();
  ptr = secbuf;
  ptr[0] = secbuf_len >> 24;
  ptr[1] = secbuf_len >> 16;
  ptr[2] = secbuf_len >> 8;
  ptr[3] = secbuf_len;
  ptr += WIN_BLOCKSIZE_LEN;
  time2bcd((*time)[sortin[0]], tt);
  memcpy(ptr, tt, WIN_TIME_LEN);  /* copy time stamp */
  ptr += WIN_TIME_LEN;
  for (i = 0; i < ch_num; ++i){
    if (indx[i][sortin[0]].flag) {
      fp_in = fopen(indx[i][sortin[0]].name, "r");
      (void)fseek(fp_in, indx[i][sortin[0]].point, 0);
      (void)fread(outbuf, 1, indx[i][sortin[0]].len, fp_in);
      memcpy(ptr, outbuf, indx[i][sortin[0]].len);
      ptr += indx[i][sortin[0]].len;
      (void)fclose(fp_in);
    } else {
      if (dummy_flag) {
	outbuf[0] = (*ch)[i] >> 8;
	outbuf[1] = (*ch)[i];
	outbuf[2] = 0;
	outbuf[3] = 1;
	outbuf[4] = outbuf[5] = outbuf[6] = outbuf[7] = 0;
	memcpy(ptr, outbuf, MIN_LEN);  /* copy dummy data */
	ptr += MIN_LEN;
      }
    }
  }
  (void)fwrite(secbuf, 1, secbuf_len, stdout);
  FREE(secbuf);
  FREE(outbuf);
   
  /* make another time */
  for (j = 1; j < time_num; ++j) {
    /* if time jumps, add dummy data */
    if (dummy_flag && (((*time)[sortin[j]] - (*time)[sortin[j - 1]]) != 1)) {
      secbuf_len = 18;
      len_max = MIN_LEN;
      if (NULL == (secbuf = MALLOC(unsigned char, secbuf_len)))
	memory_error();
      if (NULL == (outbuf = MALLOC(unsigned char, len_max)))
	memory_error();
      ptr = secbuf;
      ptr[0] = secbuf_len >> 24;
      ptr[1] = secbuf_len >> 16;
      ptr[2] = secbuf_len >> 8;
      ptr[3] = secbuf_len;
      ptr += WIN_BLOCKSIZE_LEN;
      time2bcd((*time)[sortin[j - 1]] + 1, tt);
      memcpy(ptr, tt, WIN_TIME_LEN);  /* copy time stamp */
      ptr += WIN_TIME_LEN;
      outbuf[0] = (*ch)[0] >> 8;
      outbuf[1] = (*ch)[0];
      outbuf[2] = 0;
      outbuf[3] = 1;
      outbuf[4] = outbuf[5] = outbuf[6] = outbuf[7] = 0;
      memcpy(ptr, outbuf, MIN_LEN);  /* copy dummy data */
      (void)fwrite(secbuf, 1, secbuf_len, stdout);
      FREE(secbuf);
      FREE(outbuf);
    }      
    len_max = MIN_LEN;
    secbuf_len = 10;
    for (i = 0; i < ch_num; ++i)
      if (indx[i][sortin[j]].flag) {
	secbuf_len += indx[i][sortin[j]].len;
	if (len_max < indx[i][sortin[j]].len)
	  len_max = indx[i][sortin[j]].len;
      }
    if (NULL == (secbuf = MALLOC(unsigned char, secbuf_len)))
      memory_error();
    if (NULL == (outbuf = MALLOC(unsigned char, len_max)))
      memory_error();
    ptr = secbuf;
    ptr[0] = secbuf_len >> 24;
    ptr[1] = secbuf_len >> 16;
    ptr[2] = secbuf_len >> 8;
    ptr[3] = secbuf_len;
    ptr += WIN_BLOCKSIZE_LEN;
    time2bcd((*time)[sortin[j]], tt);
    memcpy(ptr, tt, WIN_TIME_LEN);  /* copy time stamp */
    ptr += WIN_TIME_LEN;
    for (i = 0; i < ch_num; ++i)
      if (indx[i][sortin[j]].flag) {
	fp_in = fopen(indx[i][sortin[j]].name, "r");
	(void)fseek(fp_in, indx[i][sortin[j]].point, 0);
	(void)fread(outbuf, 1, indx[i][sortin[j]].len, fp_in);
	memcpy(ptr, outbuf,indx[i][sortin[j]].len);
	ptr += indx[i][sortin[j]].len;
	(void)fclose(fp_in);
      }
    (void)fwrite(secbuf, 1, secbuf_len, stdout);
    FREE(secbuf);
    FREE(outbuf);
  }

#if DEBUG > 5
  for(i=0;i<ch_num;++i){
    for(j=0;j<time_num;++j)
      fprintf(stdout,"%d",indx[i][j].flag);
    fprintf(stdout,"\n");
  }
  fprintf(stderr,"ch_num=%d  ch_num_arr=%d time_num=%d  time_num_arr=%d\n",
	  ch_num,ch_num_arr,time_num,time_num_arr);
  for(i=0;i<time_num;++i)
    fprintf(stdout,"TIME=%ld\n",time[0][i]);
  for(i=0;i<ch_num;++i)
    fprintf(stdout,"CH=%04X\n",ch[0][i]);
#endif

  return;
}

/*
 * memory mode functions
 */
static void
win_file_read_from_buf(unsigned char *rawbuf, off_t rawsize,
		       WIN_ch **ch, int *ch_num, int *ch_num_arr,
		       time_t **time, int *time_num, int *time_num_arr)
{
  unsigned char   tt[WIN_TIME_LEN];
  unsigned char   *ptr, *ptr_limit, *buf;
  WIN_blocksize   re, gsize;
  WIN_sr          srdummy;
  time_t          time_tmp;
  WIN_ch          ch_tmp;
  int             i;

  ptr = rawbuf;
  ptr_limit = rawbuf + rawsize;

  while (ptr < ptr_limit) {
    /* get 1 sec block size */
    re = mklong(ptr);
    re -= WIN_BLOCKSIZE_LEN;

    buf = (ptr += WIN_BLOCKSIZE_LEN);

    /* read & compare time */
    for (i = 0; i < WIN_TIME_LEN; ++i)
      tt[i] = *ptr++;
    time_tmp = bcd2time(tt);
    for (i = 0; i < *time_num; ++i)
      if (time_tmp == (*time)[i])
	break;
    if (i == *time_num) {
      (*time_num)++;
      if (*time_num > *time_num_arr) {
	if(NULL == (*time = REALLOC(time_t, *time, *time_num_arr + TIME_INC)))
	  memory_error();
	*time_num_arr += TIME_INC;
      }
      (*time)[i] = time_tmp;
    }
    /* read & compare channel number */
    while (ptr < buf + re) {
      gsize = win_get_chhdr(ptr, &ch_tmp, &srdummy);

      /* ch number */
      for (i = 0; i < *ch_num; ++i)
	if (ch_tmp == (*ch)[i])
	  break;
      if (i == *ch_num) {
	(*ch_num)++;
	if (*ch_num > *ch_num_arr) {
	  if (NULL == (*ch = REALLOC(WIN_ch, *ch, *ch_num + CH_INC)))
	    memory_error();
	  *ch_num_arr += CH_INC;
	}
	(*ch)[i] = ch_tmp;
      }

      ptr += gsize;
    } /* while (ptr < buf + re) */
  }  /* while (ptr < ptr_limit) */
}


static void
get_index_from_buf(unsigned char *rawbuf, off_t rawsize, int main_sfx,
		   INDX **indx, WIN_ch *ch, int ch_num,
		   time_t *time, int time_num)
{
  unsigned char  tt[WIN_TIME_LEN], re_c[4];
  unsigned char  *ptr, *ptr_limit, *buf;
  WIN_sr         srdummy;
  WIN_blocksize  re, gsize;
  time_t         time_tmp;
  WIN_ch         ch_tmp;
  int            i;
  int            time_sfx, ch_sfx;
  unsigned long  point;

  ptr = rawbuf;
  ptr_limit = rawbuf + rawsize;
  point = 0;

  while (ptr < ptr_limit) {
    re = mklong(ptr);
    re -= WIN_BLOCKSIZE_LEN;

    buf = (ptr += WIN_BLOCKSIZE_LEN);
    point +=  WIN_BLOCKSIZE_LEN;

    /* read & compare time */
    for (i = 0; i < WIN_TIME_LEN; ++i) {
      tt[i] = *ptr++;
      point++;
    }
    time_tmp = bcd2time(tt);
    for (i = 0; i < time_num; ++i)
      if (time_tmp == time[i])
	break;
    time_sfx = i;

    /* read & compare channel number */
    while (ptr < buf + re){
      gsize = win_get_chhdr(ptr, &ch_tmp, &srdummy);

      /* ch number */
      for (i = 0; i < ch_num; ++i)
	if(ch_tmp == ch[i])
	  break;
      ch_sfx = i;

      /* make index */
      if (!indx[ch_sfx][time_sfx].flag) {
	indx[ch_sfx][time_sfx].flag = 1;
	indx[ch_sfx][time_sfx].fnum = main_sfx;
	indx[ch_sfx][time_sfx].point = point;
	indx[ch_sfx][time_sfx].len = gsize;
#if DEBUG
	   fprintf(stderr,"indx[%d][%d]-->%d  fnum=%d  point=%d  len=%d\n",
		   ch_sfx,time_sfx,indx[ch_sfx][time_sfx].flag,
		   indx[ch_sfx][time_sfx].fnum,indx[ch_sfx][time_sfx].point,
		   indx[ch_sfx][time_sfx].len);
#endif
      }
      ptr += gsize;
      point += gsize;
    }  /* while (ptr < buf + re) */
  }  /* while (ptr < ptr_limit) */
}


static void
memory_mode_run(int argcc, char *argvv[])
{
  static time_t  *time[1];
  static WIN_ch  *ch[1];
  int  ch_num = 0, time_num = 0, ch_num_arr, time_num_arr;
  WIN_blocksize   re_debug;
  FILE  *fp_in;
  struct stat  sb;
  static unsigned char  **rawbuf;
  off_t  *raw_size;
  int  *fopen_flag;
  int  i, j;
  INDX   **indx;
  unsigned long  *sortin, *time_sort;
  unsigned long  secbuf_len;
  unsigned char  hbuf[10]; /* WIN_BLOCKSIZE_LEN + WIN_TIME_LEN */
  unsigned char tt[WIN_TIME_LEN];

  if (verbose_flag)
    (void)fprintf(stderr, "Momory mode\n");

  if (NULL == (*time = MALLOC(time_t, TIME_INC)))
    memory_error();
  if (NULL == (*ch = MALLOC(WIN_ch, CH_INC)))
    memory_error();
  time_num_arr = TIME_INC;
  ch_num_arr = CH_INC;

  if ((rawbuf = MALLOC(unsigned char *, argcc)) == NULL)
    memory_error();
  if ((fopen_flag = MALLOC(int, argcc)) == NULL)
    memory_error();
  if ((raw_size = MALLOC(off_t, argcc)) == NULL)
    memory_error();

  /*** file loop ***/
  for (i = 0; i < argcc; ++i) { 
    fp_in = fopen(argvv[i], "r");
    if (fp_in == NULL) {  /* skip file */
      (void)fprintf(stderr, "Skip file : %s (%s)\n",
		    argvv[i], strerror(errno));
      fopen_flag[i] = 0;
      continue;
    } else
      fopen_flag[i] = 1;
    
    /* get file size */
    if (stat(argvv[i], &sb)) {
      (void)fprintf(stderr, "%s: %s\n", argvv[i], strerror(errno));
      exit(1);
    }
    raw_size[i] = sb.st_size;
    if (verbose_flag)
      (void)fprintf(stderr, "%s: %d\n", argvv[i], raw_size[i]);

    /* malloc memory. unless get memory, memory mode tunes off */
    rawbuf[i] = MALLOC(unsigned char, raw_size[i]);
    if (rawbuf[i] == NULL) {
      (void)fprintf(stderr, "Memory mode off\n");
      for (j = 0; j < i; ++j)
	FREE(rawbuf[j]);
      return;  /* go back to file mode */
    }

    /* read win raw file */
    re_debug = fread(rawbuf[i], 1, (size_t)raw_size[i], fp_in);
    if (re_debug != raw_size[i]) {
      (void)fprintf(stderr, "Input file is strange! : %s\n", argvv[i]);
      (void)fprintf(stderr, "  Require=%u [byte], but %u [byte]\n",
		    raw_size[i], re_debug);
      exit(1);
    }
    (void)fclose(fp_in);

    /* sweep buffer and get channel number and time length */
    win_file_read_from_buf(rawbuf[i], raw_size[i], ch, &ch_num, &ch_num_arr,
		  time, &time_num, &time_num_arr);
  }  /* for (i = 0; i < argcc; ++i) */
  if ((ch_num == 0) || (time_num == 0))
      exit(1);

  /* malloc memory for INDX */
  if (NULL == (indx = MALLOC(INDX *, ch_num)))
    memory_error();
  for (i = 0; i < ch_num; ++i)
    if (NULL == (indx[i] = MALLOC(INDX, time_num)))
      memory_error();
  for (i = 0; i < ch_num; ++i)
    for (j = 0; j < time_num; ++j)
      indx[i][j].flag = 0;  /* clear flag */

  /* Make INDX table */
  for (i = 0; i < argcc; ++i) {
    if (!fopen_flag[i])
      continue;
    get_index_from_buf(rawbuf[i], raw_size[i], i,
		       indx, *ch, ch_num, *time, time_num);
  }

  /* sort time */
  if (NULL == (time_sort = MALLOC(unsigned long, time_num)))
    memory_error();
  if (NULL == (sortin = MALLOC(unsigned long, time_num)))
    memory_error();
  for (i = 0; i < time_num; ++i)
    time_sort[i] = (*time)[i];
  qsort(time_sort, time_num, sizeof(unsigned long), time_cmpq);
  for (i = 0; i < time_num; ++i) {
    for (j = 0; j < time_num; ++j) {
      if (time_sort[i] == (*time)[j]) {
	sortin[i] = j;
	break;
      }
    }
  }
#if DEBUG > 2
  for(i=0;i<time_num;++i){
    fprintf(stderr,"%d  %d:  %d\n",
	    (*time)[i],time_sort[i],(*time)[sortin[i]]);
    /* sleep(1); */
  }
#endif
  FREE(time_sort);

  /*
   *  output data 
   */
  /** make first time data **/
  secbuf_len = 10;
  for (i = 0; i < ch_num; ++i) {
    if (indx[i][sortin[0]].flag)
      secbuf_len += indx[i][sortin[0]].len;
    else {
      if (dummy_flag)
	secbuf_len += MIN_LEN;
    }
  }
  hbuf[0] = (unsigned char)(secbuf_len >> 24);
  hbuf[1] = (unsigned char)(secbuf_len >> 16);
  hbuf[2] = (unsigned char)(secbuf_len >> 8);
  hbuf[3] = (unsigned char)secbuf_len;
  time2bcd((*time)[sortin[0]], tt);
  memcpy(&hbuf[4], tt, WIN_TIME_LEN);  /* copy time stamp */
  (void)fwrite(hbuf, 1, 10, stdout);   /* output secsize and time stamp */

  /* output wave data */
  for (i = 0; i < ch_num; ++i) {
    if (indx[i][sortin[0]].flag)
      (void)
	fwrite(&rawbuf[indx[i][sortin[0]].fnum][indx[i][sortin[0]].point],
	       1, indx[i][sortin[0]].len, stdout);
    else {
      if (dummy_flag) {
	hbuf[0] = (*ch)[i] >> 8;
	hbuf[1] = (*ch)[i];
	hbuf[2] = 0;
	hbuf[3] = 1;
	hbuf[4] = hbuf[5] = hbuf[6] = hbuf[7] = 0;
	(void)fwrite(hbuf, 1, MIN_LEN, stdout);
      }
    }
  }

  /** make another time data **/
  for (j = 1; j < time_num; ++j) {
    /* if time jumps, add dummy data */
    if (dummy_flag && (((*time)[sortin[j]] - (*time)[sortin[j - 1]]) != 1)) {
      secbuf_len = 18;
      hbuf[0] = (unsigned char)(secbuf_len >> 24);
      hbuf[1] = (unsigned char)(secbuf_len >> 16);
      hbuf[2] = (unsigned char)(secbuf_len >> 8);
      hbuf[3] = (unsigned char)secbuf_len;
      time2bcd((*time)[sortin[j - 1]] + 1, tt);
      memcpy(&hbuf[4], tt, WIN_TIME_LEN);  /* copy time stamp */
      (void)fwrite(hbuf, 1, 10, stdout);   /* output secsize and time stamp */

      hbuf[0] = (*ch)[0] >> 8;
      hbuf[1] = (*ch)[0];
      hbuf[2] = 0;
      hbuf[3] = 1;
      hbuf[4] = hbuf[5] = hbuf[6] = hbuf[7] = 0;
      (void)fwrite(hbuf, 1, MIN_LEN, stdout);
    }

    secbuf_len = 10;
    for (i = 0; i < ch_num; ++i)
      if (indx[i][sortin[j]].flag)
	secbuf_len += indx[i][sortin[j]].len;
    hbuf[0] = (unsigned char)(secbuf_len >> 24);
    hbuf[1] = (unsigned char)(secbuf_len >> 16);
    hbuf[2] = (unsigned char)(secbuf_len >> 8);
    hbuf[3] = (unsigned char)secbuf_len;
    time2bcd((*time)[sortin[j]], tt);
    memcpy(&hbuf[4], tt, WIN_TIME_LEN);  /* copy time stamp */
    (void)fwrite(hbuf, 1, 10, stdout);   /* output secsize and time stamp */

    for (i = 0; i < ch_num; ++i)
      if (indx[i][sortin[j]].flag)
	(void)
	  fwrite(&rawbuf[indx[i][sortin[j]].fnum][indx[i][sortin[j]].point],
		 1, indx[i][sortin[j]].len, stdout);
  }  /* for (j = 1; j < time_num; ++j) */

#if DEBUG 
  for(i=0;i<ch_num;++i){
    for(j=0;j<time_num;++j)
      fprintf(stderr,"%d",indx[i][j].flag);
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"ch_num=%d  ch_num_arr=%d time_num=%d  time_num_arr=%d\n",
	  ch_num,ch_num_arr,time_num,time_num_arr);
/*    for(i=0;i<time_num;++i) */
/*      fprintf(stderr,"TIME=%ld\n",time[0][i]); */
/*    for(i=0;i<ch_num;++i) */
/*      fprintf(stderr,"CH=%04X\n",ch[0][i]); */
#endif

  exit(0);
}
