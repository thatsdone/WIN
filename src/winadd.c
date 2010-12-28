/* $Id: winadd.c,v 1.4.2.2 2010/12/28 12:55:44 uehira Exp $ */

/*
 * winadd.c  (Uehira Kenji)
 *  last modified  2010/09/16
 *
 *  1998/12/23  malloc bug (indx[i][j].name)
 *  1999/01/06  skip no data file
 *  2000/02/29  bye-order free. delete NR code.
 *  2003/11/2-3 memory mode.
 *  2009/08/01  64bit clean?
 *  2010/01/12  bcd2time()-->bcd_t(), time2bcd()-->t_bcd()
 *  2010/09/14  added two options (-f, -s).
 *               -f : force file mode.
 *               -s : channel sort mode.
 *  2010/09/16  added a option (-m).
 *               -m : only try memory mode.
 *  2010/12/21  added a option (-M).
 *               -M : MON mode.
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
/* #include "win_system.h" */

#define   DEBUG2  0
#define   DEBUG2  0
#define   DEBUG3  0
#define   DEBUG5  0

#define  CH_INC     1000
#define  TIME_INC   3600
#define  MIN_LEN    8  /* minimum channel block length */

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
   "$Id: winadd.c,v 1.4.2.2 2010/12/28 12:55:44 uehira Exp $";

static int  dummy_flag, verbose_flag, chsort_flag, MON_mode;

/* prototypes */
static void memory_mode_run(int, char *[]);
static void get_index_from_buf(uint8_w *, off_t, int,
			       INDX **, WIN_ch *, int,
			       time_t *, int, char *[]);
static void win_file_read_from_buf(uint8_w *, off_t,
				   WIN_ch **, int *, int *,
				   time_t **, int *, int *);
static void file_mode_run(int, char *[]);
static void get_index(char *, INDX **, WIN_ch *, int,
		      time_t *, int);
static void win_file_read(char *, WIN_ch **, int *, int *,
			  time_t **, int *, int *);
static void memory_error(void);
static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  int  c;
  int  f_mode, m_mode;

  dummy_flag = 1;
  verbose_flag =  chsort_flag = 0;
  f_mode = m_mode = 0;
  MON_mode = 0;
  while ((c = getopt(argc, argv, "fnMmsvh")) != -1) {
    switch (c) {
    case 'f':   /* force file mode */
      f_mode = 1;
      break;
    case 'M':   /* MON mode */
      MON_mode = 1;
      break;
    case 'm':   /* only try memory mode */
      m_mode = 1;
      break;
    case 'n':   /* don't append dummy data */
      dummy_flag = 0;
      break;
    case 's':   /* channnel sort mode */
      chsort_flag = 1;
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

  if (verbose_flag) {
    if (MON_mode)
      (void)fputs("MON mode\n", stderr);
    else
      (void)fputs("RAW mode\n", stderr);
  }

#if DEBUG
  fprintf(stderr,"verbose flag = %d, dummy flag=%d\n",
	  verbose_flag, dummy_flag);
#endif

  if (!f_mode)
    memory_mode_run(argc, argv);

  if (!m_mode)
    file_mode_run(argc, argv);

  exit(0);
}


static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fputs("Usage : winadd [options] file1 file2 ... > output\n", stderr);
  (void)fputs("  options : -f         : force file mode.\n", stderr);
  (void)fputs("            -M         : MON mode. Inputs are MON files.\n",
	      stderr);
  (void)fputs("            -m         : only try memory mode.\n", stderr);
  (void)fputs("            -n         : do not append dummy headers.\n",
	      stderr);
  (void)fputs("            -s         : channel sort mode.\n", stderr);
  (void)fputs("            -v         : verbose mode.\n", stderr);
  (void)fputs("            -h         : print this message.\n", stderr);
}

static void
memory_error(void)
{

  (void)fputs("Cannot allocate memory!\n", stderr);
  exit(1);
}

/*
 * file mode functions
 */
static void
win_file_read(char *name, WIN_ch **ch, int *ch_num, int *ch_num_arr,
	      time_t **time, int *time_num, int *time_num_arr)
{
  FILE            *fp;
  uint8_w   tt[WIN_TIME_LEN], re_c[4];
  uint8_w   *ptr, *buf;
  WIN_bs   re, re_debug, gsize;
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
    re = mkuint4(re_c);
    re -= WIN_BLOCKSIZE_LEN;
    if (NULL == (buf = MALLOC(uint8_w, re))) {
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
    time_tmp = bcd_t(tt);
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
      if (MON_mode)
	gsize = get_sysch_mon(ptr, &ch_tmp);
      else
	gsize = get_sysch(ptr, &ch_tmp);

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
  uint8_w  tt[WIN_TIME_LEN], re_c[4];
  uint8_w  *ptr, *buf;
  unsigned long  point;
  WIN_bs  re, gsize;
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
    re = mkuint4(re_c);
    re -= 4;
    if (NULL == (buf = MALLOC(uint8_w, re))) {
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
    time_tmp = bcd_t(tt);
    for (i = 0; i < time_num; ++i)
      if (time_tmp == time[i])
	break;
    time_sfx = i;
    /* read & compare channel number */
    while (ptr < buf + re){
      if (MON_mode)
	gsize = get_sysch_mon(ptr, &ch_tmp);
      else
	gsize = get_sysch(ptr, &ch_tmp);
      
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
#if DEBUG5
	fprintf(stderr, "indx[%d][%d]-->%d  name=%s  point=%d  len=%d\n",
		ch_sfx,time_sfx,indx[ch_sfx][time_sfx].flag,
		indx[ch_sfx][time_sfx].name,indx[ch_sfx][time_sfx].point,
		indx[ch_sfx][time_sfx].len);
#endif
      } else if (verbose_flag)
	fprintf(stderr,
		"Duplicate data in %s: %02X%02X%02X.%02X%02X%02X(%04X)\n", 
		name, tt[0], tt[1], tt[2], tt[3], tt[4], tt[5], ch_tmp);

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
  size_t  i, j, max_name_len;
  INDX   **indx;
  size_t  *sortin, *sortin_ch;
  time_t  *time_sort;
  WIN_ch  *ch_sort;
  unsigned long secbuf_len, len_max;
  uint8_w *outbuf, *secbuf, *ptr;
  uint8_w tt[WIN_TIME_LEN];
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
  if (NULL == (time_sort = MALLOC(time_t, time_num)))
    memory_error();
  if (NULL == (sortin = MALLOC(size_t, time_num)))
    memory_error();
  for (i = 0; i < time_num; ++i)
    time_sort[i] = (*time)[i];
  qsort(time_sort, time_num, sizeof(time_t), time_cmpq);
  for (i = 0; i < time_num; ++i) {
    for (j = 0; j < time_num; ++j) {
      if (time_sort[i] == (*time)[j]) {
	sortin[i] = j;
	break;
      }
    }
  }
#if DEBUG2
  for(i=0;i<time_num;++i){
    fprintf(stderr,"%ld  %ld:  %ld\n",
	    (*time)[i],time_sort[i],(*time)[sortin[i]]);
    /* sleep(1); */
  }
#endif
  FREE(time_sort);

  /* sort channel if chsort_flag == 1 */
  if (NULL == (sortin_ch = MALLOC(size_t, ch_num)))
    memory_error();
  if (chsort_flag) {
    if (NULL == (ch_sort = MALLOC(WIN_ch, ch_num)))
      memory_error();
    for (i = 0; i < ch_num; ++i)
      ch_sort[i] = (*ch)[i];
    qsort(ch_sort, ch_num, sizeof(WIN_ch), ch_cmpq);
    for (i = 0; i < ch_num; ++i) {
      for (j = 0; j < ch_num; ++j) {
	if (ch_sort[i] == (*ch)[j]) {
	  sortin_ch[i] = j;
	  break;
	}
      }
    }
#if DEBUG3
    for(i=0;i<ch_num;++i){
      fprintf(stderr,"%04X  %04X:  %04X\n",
	      (*ch)[i], ch_sort[i], (*ch)[sortin_ch[i]]);
      /* sleep(1); */
    }
#endif
    FREE(ch_sort);
  } else {  /* not sort */
    for (i = 0; i < ch_num; ++i)
      sortin_ch[i] = i;
#if DEBUG3
    for(i=0;i<ch_num;++i){
      fprintf(stderr,"%04X  %04X\n",
	      (*ch)[i], (*ch)[sortin_ch[i]]);
      /* sleep(1); */
    }
#endif
  }/* if (chsort_flag) */

  /*
   *  output data 
   */
  /* make first time data */
  len_max = MIN_LEN;
  secbuf_len = 10;
  for (i = 0; i < ch_num; ++i) {
    if (indx[sortin_ch[i]][sortin[0]].flag) {
      secbuf_len += indx[sortin_ch[i]][sortin[0]].len;
      if (len_max < indx[sortin_ch[i]][sortin[0]].len)
	len_max = indx[sortin_ch[i]][sortin[0]].len;
    } else {
      if (dummy_flag)
	secbuf_len += MIN_LEN;
    }
  }
  if (NULL == (secbuf = MALLOC(uint8_w, secbuf_len)))
    memory_error();
  if (NULL == (outbuf = MALLOC(uint8_w, len_max)))
    memory_error();
  ptr = secbuf;
  ptr[0] = secbuf_len >> 24;
  ptr[1] = secbuf_len >> 16;
  ptr[2] = secbuf_len >> 8;
  ptr[3] = secbuf_len;
  ptr += WIN_BLOCKSIZE_LEN;
  t_bcd((*time)[sortin[0]], tt);
  memcpy(ptr, tt, WIN_TIME_LEN);  /* copy time stamp */
  ptr += WIN_TIME_LEN;
  for (i = 0; i < ch_num; ++i){
    if (indx[sortin_ch[i]][sortin[0]].flag) {
      fp_in = fopen(indx[sortin_ch[i]][sortin[0]].name, "r");
      (void)fseek(fp_in, indx[sortin_ch[i]][sortin[0]].point, 0);
      (void)fread(outbuf, 1, indx[sortin_ch[i]][sortin[0]].len, fp_in);
      memcpy(ptr, outbuf, indx[sortin_ch[i]][sortin[0]].len);
      ptr += indx[sortin_ch[i]][sortin[0]].len;
      (void)fclose(fp_in);
    } else {
      if (dummy_flag) {
	outbuf[0] = (*ch)[sortin_ch[i]] >> 8;
	outbuf[1] = (*ch)[sortin_ch[i]];
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
      if (NULL == (secbuf = MALLOC(uint8_w, secbuf_len)))
	memory_error();
      if (NULL == (outbuf = MALLOC(uint8_w, len_max)))
	memory_error();
      ptr = secbuf;
      ptr[0] = secbuf_len >> 24;
      ptr[1] = secbuf_len >> 16;
      ptr[2] = secbuf_len >> 8;
      ptr[3] = secbuf_len;
      ptr += WIN_BLOCKSIZE_LEN;
      t_bcd((*time)[sortin[j - 1]] + 1, tt);
      memcpy(ptr, tt, WIN_TIME_LEN);  /* copy time stamp */
      ptr += WIN_TIME_LEN;
      outbuf[0] = (*ch)[sortin_ch[0]] >> 8;
      outbuf[1] = (*ch)[sortin_ch[0]];
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
      if (indx[sortin_ch[i]][sortin[j]].flag) {
	secbuf_len += indx[sortin_ch[i]][sortin[j]].len;
	if (len_max < indx[sortin_ch[i]][sortin[j]].len)
	  len_max = indx[sortin_ch[i]][sortin[j]].len;
      }
    if (NULL == (secbuf = MALLOC(uint8_w, secbuf_len)))
      memory_error();
    if (NULL == (outbuf = MALLOC(uint8_w, len_max)))
      memory_error();
    ptr = secbuf;
    ptr[0] = secbuf_len >> 24;
    ptr[1] = secbuf_len >> 16;
    ptr[2] = secbuf_len >> 8;
    ptr[3] = secbuf_len;
    ptr += WIN_BLOCKSIZE_LEN;
    t_bcd((*time)[sortin[j]], tt);
    memcpy(ptr, tt, WIN_TIME_LEN);  /* copy time stamp */
    ptr += WIN_TIME_LEN;
    for (i = 0; i < ch_num; ++i)
      if (indx[sortin_ch[i]][sortin[j]].flag) {
	fp_in = fopen(indx[sortin_ch[i]][sortin[j]].name, "r");
	(void)fseek(fp_in, indx[sortin_ch[i]][sortin[j]].point, 0);
	(void)fread(outbuf, 1, indx[sortin_ch[i]][sortin[j]].len, fp_in);
	memcpy(ptr, outbuf,indx[sortin_ch[i]][sortin[j]].len);
	ptr += indx[sortin_ch[i]][sortin[j]].len;
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
win_file_read_from_buf(uint8_w *rawbuf, off_t rawsize,
		       WIN_ch **ch, int *ch_num, int *ch_num_arr,
		       time_t **time, int *time_num, int *time_num_arr)
{
  uint8_w   tt[WIN_TIME_LEN];
  uint8_w   *ptr, *ptr_limit, *buf;
  WIN_bs   re, gsize;
  time_t          time_tmp;
  WIN_ch          ch_tmp;
  int             i;

  ptr = rawbuf;
  ptr_limit = rawbuf + rawsize;

  while (ptr < ptr_limit) {
    /* get 1 sec block size */
    re = mkuint4(ptr);
    re -= WIN_BLOCKSIZE_LEN;

    buf = (ptr += WIN_BLOCKSIZE_LEN);

    /* read & compare time */
    for (i = 0; i < WIN_TIME_LEN; ++i)
      tt[i] = *ptr++;
    time_tmp = bcd_t(tt);
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
      if (MON_mode)
	gsize = get_sysch_mon(ptr, &ch_tmp);
      else
	gsize = get_sysch(ptr, &ch_tmp);

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
get_index_from_buf(uint8_w *rawbuf, off_t rawsize, int main_sfx,
		   INDX **indx, WIN_ch *ch, int ch_num,
		   time_t *time, int time_num, char *argvv[])
{
  uint8_w  tt[WIN_TIME_LEN];
  uint8_w  *ptr, *ptr_limit, *buf;
  WIN_bs  re, gsize;
  time_t         time_tmp;
  WIN_ch         ch_tmp;
  int            i;
  int            time_sfx, ch_sfx;
  unsigned long  point;

  ptr = rawbuf;
  ptr_limit = rawbuf + rawsize;
  point = 0;

  while (ptr < ptr_limit) {
    re = mkuint4(ptr);
    re -= WIN_BLOCKSIZE_LEN;

    buf = (ptr += WIN_BLOCKSIZE_LEN);
    point +=  WIN_BLOCKSIZE_LEN;

    /* read & compare time */
    for (i = 0; i < WIN_TIME_LEN; ++i) {
      tt[i] = *ptr++;
      point++;
    }
    time_tmp = bcd_t(tt);
    for (i = 0; i < time_num; ++i)
      if (time_tmp == time[i])
	break;
    time_sfx = i;

    /* read & compare channel number */
    while (ptr < buf + re){
      if (MON_mode)
	gsize = get_sysch_mon(ptr, &ch_tmp);
      else
	gsize = get_sysch(ptr, &ch_tmp);

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
	   fprintf(stderr,"indx[%d][%d]-->%d  fnum=%d  point=%ld  len=%ld\n",
		   ch_sfx,time_sfx,indx[ch_sfx][time_sfx].flag,
		   indx[ch_sfx][time_sfx].fnum,indx[ch_sfx][time_sfx].point,
		   indx[ch_sfx][time_sfx].len);
#endif
      } else if (verbose_flag)
	fprintf(stderr,
		"Duplicate data in %s: %02X%02X%02X.%02X%02X%02X(%04X)\n", 
		argvv[main_sfx], tt[0], tt[1], tt[2], tt[3], tt[4], tt[5],
		ch_tmp);

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
  size_t re_debug;
  FILE  *fp_in;
  struct stat  sb;
  static uint8_w  **rawbuf;
  off_t  *raw_size;
  int  *fopen_flag;
  size_t  i, j;
  INDX   **indx;
  size_t  *sortin, *sortin_ch;
  time_t  *time_sort;
  WIN_ch  *ch_sort;
  unsigned long  secbuf_len;
  uint8_w  hbuf[10]; /* WIN_BLOCKSIZE_LEN + WIN_TIME_LEN */
  uint8_w tt[WIN_TIME_LEN];

  if (verbose_flag)
    (void)fprintf(stderr, "Memory mode\n");

  if (NULL == (*time = MALLOC(time_t, TIME_INC)))
    memory_error();
  if (NULL == (*ch = MALLOC(WIN_ch, CH_INC)))
    memory_error();
  time_num_arr = TIME_INC;
  ch_num_arr = CH_INC;

  if ((rawbuf = MALLOC(uint8_w *, argcc)) == NULL)
    memory_error();
  if ((fopen_flag = MALLOC(int, argcc)) == NULL)
    memory_error();
  if ((raw_size = MALLOC(off_t, argcc)) == NULL)
    memory_error();

  /*** file loop ***/
  /* First, check malloc memory */
  for (i = 0; i < argcc; ++i) {
    fp_in = fopen(argvv[i], "r");
    if (fp_in == NULL) {  /* skip file */
      (void)fprintf(stderr, "Skip file : %s (%s)\n",
		    argvv[i], strerror(errno));
      fopen_flag[i] = 0;
      continue;
    } else
      fopen_flag[i] = 1;
    (void)fclose(fp_in);
    
    /* get file size */
    if (stat(argvv[i], &sb)) {
      (void)fprintf(stderr, "%s: %s\n", argvv[i], strerror(errno));
      exit(1);
    }
    raw_size[i] = sb.st_size;
    if (verbose_flag)
      (void)fprintf(stderr, "%s: %lld bytes\n", argvv[i], raw_size[i]);
    /* skip 0 byte file. */
    if (raw_size[i] == 0) {
      (void)fprintf(stderr, "%s: %lld bytes. Skip!\n", argvv[i], raw_size[i]);
      fopen_flag[i] = 0;
      continue;
    }

    /* malloc memory. unless get memory, memory mode tunes off */
    rawbuf[i] = MALLOC(uint8_w, raw_size[i]);
    if (rawbuf[i] == NULL) {
      (void)fprintf(stderr, "Memory mode off\n");
      for (j = 0; j < i; ++j)
	FREE(rawbuf[j]);
      return;  /* go back to file mode */
    }
  }  /* for (i = 0; i < argcc; ++i) */

  /* Then read data */
  for (i = 0; i < argcc; ++i) {
    if (!fopen_flag[i])
      continue;
    fp_in = fopen(argvv[i], "r");

    if (verbose_flag)
      (void)fprintf(stderr, "Reading '%s' ......\n", argvv[i]);

    /* read win raw file */
    re_debug = fread(rawbuf[i], 1, (size_t)raw_size[i], fp_in);
    if (re_debug != raw_size[i]) {
      (void)fprintf(stderr, "Input file is strange! : %s\n", argvv[i]);
      (void)fprintf(stderr, "  Require=%lld [byte], but %zu [byte]\n",
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
		       indx, *ch, ch_num, *time, time_num, argvv);
  }

  /* sort time */
  if (NULL == (time_sort = MALLOC(time_t, time_num)))
    memory_error();
  if (NULL == (sortin = MALLOC(size_t, time_num)))
    memory_error();
  for (i = 0; i < time_num; ++i)
    time_sort[i] = (*time)[i];
  qsort(time_sort, time_num, sizeof(time_t), time_cmpq);
  for (i = 0; i < time_num; ++i) {
    for (j = 0; j < time_num; ++j) {
      if (time_sort[i] == (*time)[j]) {
	sortin[i] = j;
	break;
      }
    }
  }
#if DEBUG2
  for(i=0;i<time_num;++i){
    fprintf(stderr,"%ld  %ld:  %ld\n",
	    (*time)[i],time_sort[i],(*time)[sortin[i]]);
    /* sleep(1); */
  }
#endif
  FREE(time_sort);

  /* sort channel if chsort_flag == 1 */
  if (NULL == (sortin_ch = MALLOC(size_t, ch_num)))
    memory_error();
  if (chsort_flag) {
    if (NULL == (ch_sort = MALLOC(WIN_ch, ch_num)))
      memory_error();
    for (i = 0; i < ch_num; ++i)
      ch_sort[i] = (*ch)[i];
    qsort(ch_sort, ch_num, sizeof(WIN_ch), ch_cmpq);
    for (i = 0; i < ch_num; ++i) {
      for (j = 0; j < ch_num; ++j) {
	if (ch_sort[i] == (*ch)[j]) {
	  sortin_ch[i] = j;
	  break;
	}
      }
    }
#if DEBUG3
    for(i=0;i<ch_num;++i){
      fprintf(stderr,"%04X  %04X:  %04X\n",
	      (*ch)[i], ch_sort[i], (*ch)[sortin_ch[i]]);
      /* sleep(1); */
    }
#endif
    FREE(ch_sort);
  } else {  /* not sort */
    for (i = 0; i < ch_num; ++i)
      sortin_ch[i] = i;
#if DEBUG3
    for(i=0;i<ch_num;++i){
      fprintf(stderr,"%04X  %04X\n",
	      (*ch)[i], (*ch)[sortin_ch[i]]);
      /* sleep(1); */
    }
#endif
  }/* if (chsort_flag) */

  /*
   *  output data 
   */
  /** make first time data **/
  secbuf_len = 10;
  for (i = 0; i < ch_num; ++i) {
    if (indx[sortin_ch[i]][sortin[0]].flag)
      secbuf_len += indx[sortin_ch[i]][sortin[0]].len;
    else {
      if (dummy_flag)
	secbuf_len += MIN_LEN;
    }
  }
  hbuf[0] = (uint8_w)(secbuf_len >> 24);
  hbuf[1] = (uint8_w)(secbuf_len >> 16);
  hbuf[2] = (uint8_w)(secbuf_len >> 8);
  hbuf[3] = (uint8_w)secbuf_len;
  t_bcd((*time)[sortin[0]], tt);
  memcpy(&hbuf[4], tt, WIN_TIME_LEN);  /* copy time stamp */
  (void)fwrite(hbuf, 1, 10, stdout);   /* output secsize and time stamp */

  /* output wave data */
  for (i = 0; i < ch_num; ++i) {
    if (indx[sortin_ch[i]][sortin[0]].flag)
      (void)
	fwrite(&rawbuf[indx[sortin_ch[i]][sortin[0]].fnum][indx[sortin_ch[i]][sortin[0]].point],
	       1, indx[sortin_ch[i]][sortin[0]].len, stdout);
    else {
      if (dummy_flag) {
	hbuf[0] = (*ch)[sortin_ch[i]] >> 8;
	hbuf[1] = (*ch)[sortin_ch[i]];
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
      hbuf[0] = (uint8_w)(secbuf_len >> 24);
      hbuf[1] = (uint8_w)(secbuf_len >> 16);
      hbuf[2] = (uint8_w)(secbuf_len >> 8);
      hbuf[3] = (uint8_w)secbuf_len;
      t_bcd((*time)[sortin[j - 1]] + 1, tt);
      memcpy(&hbuf[4], tt, WIN_TIME_LEN);  /* copy time stamp */
      (void)fwrite(hbuf, 1, 10, stdout);   /* output secsize and time stamp */

      hbuf[0] = (*ch)[sortin_ch[0]] >> 8;
      hbuf[1] = (*ch)[sortin_ch[0]];
      hbuf[2] = 0;
      hbuf[3] = 1;
      hbuf[4] = hbuf[5] = hbuf[6] = hbuf[7] = 0;
      (void)fwrite(hbuf, 1, MIN_LEN, stdout);
    }

    secbuf_len = 10;
    for (i = 0; i < ch_num; ++i)
      if (indx[sortin_ch[i]][sortin[j]].flag)
	secbuf_len += indx[sortin_ch[i]][sortin[j]].len;
    hbuf[0] = (uint8_w)(secbuf_len >> 24);
    hbuf[1] = (uint8_w)(secbuf_len >> 16);
    hbuf[2] = (uint8_w)(secbuf_len >> 8);
    hbuf[3] = (uint8_w)secbuf_len;
    t_bcd((*time)[sortin[j]], tt);
    memcpy(&hbuf[4], tt, WIN_TIME_LEN);  /* copy time stamp */
    (void)fwrite(hbuf, 1, 10, stdout);   /* output secsize and time stamp */

    for (i = 0; i < ch_num; ++i)
      if (indx[sortin_ch[i]][sortin[j]].flag)
	(void)
	  fwrite(&rawbuf[indx[sortin_ch[i]][sortin[j]].fnum][indx[sortin_ch[i]][sortin[j]].point],
		 1, indx[sortin_ch[i]][sortin[j]].len, stdout);
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
