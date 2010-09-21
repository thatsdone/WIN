/* $Id: evdetect.c,v 1.1.2.4 2010/09/21 05:40:40 uehira Exp $ */

/*
 * evedetect.c
 *  Event detect program. Modified pmon.c
 */

/************************************************************************
*************************************************************************
**  program "pmon.c" for NEWS/SPARC                             *********
**  plot ddr system monitor data                                *********
**  7/19/89 - 5/22/90 urabe (PC-9801 version)                   *********
**  5/24/90 - 9/26/91 urabe (NEWS version)                      *********
**  4/27/92 - 4/27/92 urabe (86 ch -> 103 ch max.)              *********
**  5/25/92 - 5/25/92 urabe (touch "off" file)                  *********
**  1/14/93 - 1/14/93 for RISC NEWS                             *********
**  2/24/93 - 3/27/93 for SUN SPARC                             *********
**  4/25/93 - 7/12/93 for new data format                       *********
**  2/ 7/94 - 2/ 7/94 increased channels (#define MAXCH 1)      *********
**  3/15/94 - 3/15/94 fixed bug of long trigger for absent channel ******
**  5/16/94 - 5/16/94 fixed bug for illegal data file           *********
**  5/19/94 - 5/19/94 report no-data stations to syslog         *********
**  5/25/94 - 5/25/94 directory for just signal files           *********
**  9/17/94 - 9/17/94 TIME_TOO_LOW/DEBUG                        *********
**  10/11/94 syslog deleted                                     *********
**  12/16/94 bug fix(read USED directory)                       *********
**  1/6/95 bug fix(adj_time(tm[0]--))                           *********
**  2/23/96-2/28/96 MAXCH deleted and lock file introduced      *********
**  8/21/96 - 8/22/96 each station can belong upto 10 zones     *********
**  97.8.21   allows empty line in zones.tbl                    *********
**  97.10.1   FreeBSD                                           *********
**  97.10.21  use PID in lock file                              *********
**  98.1.22   not use lpr -s if defined(__FreeBSD__)            *********
**  98.5.12   use lp in __SVR4(Solaris2.X)                      *********
**  98.6.24   yo2000                                            *********
**  98.6.29   eliminate get_time()                              *********
**  99.2.3    confirm_off() fixed (cnt_zone=0 in if rep_level)  *********
**  99.2.4    put signal(HUP) in hangup()                       *********
**  99.4.20   byte-order-free / LONGNAME - 8(stn)+2(cmp)        *********
**  99.9.13   bugs in "LONGNAME block" fixed                    *********
**  2000.7.3  overflow bug in line[] fixed                      *********
**            zone names in "off" line not to duplicate         *********
**            not abort one-min file even when no ch found to use *******
**  2001.1.22 check_path                                        *********
**  2001.1.23 leave RAS files                                   *********
**  2001.1.29 call conv program with arguments                  *********
**                            "[infile] [outdir] [basename]"    *********
**  2001.5.31 introduced FILENAME_MAX as size of path names     *********
**  2002.1.22 FILENAME_MAX --> WIN_FILENAME_MAX                 *********
**  2002.3.19 logfile (-l) / remove offset (-o)                 *********
**  2002.3.19 delete illegal USED file                          *********
**  2002.3.24 cancel offset (-o)                                *********
**  2002.9.12 delay time (-d)                                   *********
**  2005.8.10 bug in strcmp2()/strncmp2() fixed : 0-6 > 7-9     *********
**  2007.1.15 'ch_file not found' message fixed                 *********
**  2010.9.21 64bit check (Uehira)                              *********
**                                                              *********
**  font files ("font16", "font24" and "font32") are            *********
**  not necessary                                               *********
**   link with "-lm" option                                     *********
**                                                              *********
**  rep_level = 0: no report, 1: begins and ends                *********
**              2: groups,    3: channels                       *********
*************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <sys/types.h>
#include  <sys/file.h>
#include  <sys/ioctl.h>

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <math.h>
#include  <signal.h>
#include  <unistd.h>
#include  <dirent.h>
#include  <fcntl.h>
#include  <ctype.h>

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

#include "winlib.h"

#define LONGNAME      1
/* #define DEBUG         0 */
#define DEBUG1        0
#define M_CH          1000   /* absolute max n of traces */
                             /* n of chs in data file is unlimited */
#define MAX_ZONES     10     /* max n of zones for a station */
#define MIN_PER_LINE  10     /* min/line */
#define WIDTH_LBP     392    /* in bytes (must be even) */
#define HEIGHT_LBP    4516   /* in pixels */
#define LENGTH        200000 /* buffer size */
/* #define SR_MON        5 */
#define TOO_LOW       0.0
#define TIME_TOO_LOW  10.0
#define STNLEN        11   /* (length of station code)+1 */
#define CMPLEN        7    /* (length of component code)+1 */
#define LEN           1024

/* check path macros */
#define FILE_R 0
#define FILE_W 1
#define DIR_R  2
#define DIR_W  3

#define WIN_FILENAME_MAX 1024

static const char  rcsid[] =
   "$Id: evdetect.c,v 1.1.2.4 2010/09/21 05:40:40 uehira Exp $";

char *progname, *logfile;
int  syslog_mode = 0, exit_status;

static int fd, min_trig[M_CH], tim[6], n_zone, n_trig[M_CH], n_stn[M_CH],
  max_trig[M_CH], rep_level, n_zone_trig, cnt_zone,
  i_zone[M_CH], not_yet, m_ch, m_limit, made_lock_file, max_ch;
static int32_w long_max[M_CH][SR_MON], long_min[M_CH][SR_MON];
static int16_w  idx[WIN_CHMAX];
static char file_trig[WIN_FILENAME_MAX], line[LEN], time_text[20],
  last_line[LEN], file_trig_lock[WIN_FILENAME_MAX], *param_file,
  temp_done[WIN_FILENAME_MAX], latest[WIN_FILENAME_MAX],
  ch_file[WIN_FILENAME_MAX], file_zone[WIN_FILENAME_MAX], zone[M_CH][20],
  file_zone_min_trig[WIN_FILENAME_MAX];
static uint8_w idx2[WIN_CHMAX];
static uint8_w buf[LENGTH];
static double dt = 1.0 / (double)SR_MON, time_on, time_off, time_lta,
  time_lta_off, time_sta, a_sta, b_sta, a_lta, b_lta, a_lta_off, b_lta_off;

struct
{
  int ch;
  int gain;
  char name[STNLEN];
  char comp[CMPLEN];
  int zone[MAX_ZONES];
  int n_zone;
  int alive;       /* 0:alive 1:dead */
  int use;         /* 0:unuse 1:use */
  int status;      /* 0:OFF, 1:ON but not comfirmed */
                   /* 2:ON confirmed, 3:OFF but not confirmed */
  double lta;      /* long term average */
  double sta;      /* short term average */
  double ratio;    /* sta/lta ratio */
  double lta_save; /* lta just before trigger */
  int32_w max;     /* maximum deflection */
  double sec_on;   /* duration time */
  double sec_off;  /* time after on->off */
  double cnt;      /* counter for initialization */
  double sec_zero; /* length of successive zeros */
  int tm[7];       /* year to sec/10 */
  int32_w zero;    /* offset */
  int zero_cnt;    /* counter */
  double zero_acc; /* accumulator for offset calculation */
} static tbl[M_CH];


/* prototypes */
static void write_file(char *, char *);
static void confirm_on(int);
static void sprintm(char *, int *);
static void cptm(int *, int *);
static void confirm_off(int, int, int);
static int read_one_sec(int *);
static void check_trg(void);
static void hangup(void);
static void owari(void);
static int check_path(char *, int);
static void get_lastline(char *, char *);
static void usage(void);
int main(int, char *[]);

static void
write_file(char *fname, char *text)
{
  FILE *fp;
  char tb[254];

  while ((fp = fopen(fname,"a")) == NULL) {
    (void)snprintf(tb, sizeof(tb), "'%s' not open (%d)", fname, getpid());
    write_log(tb);
    sleep(60);
  }
  (void)fputs(text, fp);
  while (fclose(fp) == EOF) {
    (void)snprintf(tb, sizeof(tb), "'%s' not close (%d)", fname, getpid());
    write_log(tb);
    sleep(60);
  }
}

static void
confirm_on(int ch)
{
  int j, tm[6], z;
  static char prev_on[15];
  
  tbl[ch].status = 2;
  sprintm(time_text, tbl[ch].tm);
  (void)sprintf(time_text + strlen(time_text), ".%d",
		tbl[ch].tm[6] * (10 / SR_MON));

  for (j = 0; j < tbl[ch].n_zone; j++) {
    z = tbl[ch].zone[j];
    max_trig[z] = (++n_trig[z]);
    if(rep_level >= 3) {
      (void)snprintf(line, sizeof(line), "  %-4s(%-7.7s) on, %s %5d\n",
		     tbl[ch].name, zone[z], time_text, (int)tbl[ch].lta_save);
      write_file(file_trig, line);
    }
    if (n_trig[z] == min_trig[z]) {
      n_zone_trig++;
      i_zone[cnt_zone++] = z;
      if(rep_level >= 2) {
        (void)snprintf(line, sizeof(line), " %s %-7.7s on,  min=%d\n",
		       time_text, zone[z], min_trig[z]);
        write_file(file_trig, line);
      }
      if (n_zone_trig == 1 && rep_level >= 1) {
        if (strncmp(prev_on, time_text, 13) == 0) {
          cptm(tm, tbl[ch].tm);
          tm[5]++;
          adj_time(tm);
          sprintm(time_text, tm);
	}
        (void)strncpy(prev_on, time_text, 13);
        (void)snprintf(line, sizeof(line), "%13.13s on, at %.7s\n",
		       time_text, zone[z]);
        if (strncmp2(time_text, last_line, 13) > 0) {
          write_file(file_trig, line);
          not_yet=0;
	}
        /* delayed cont message */
        if (tbl[ch].tm[4] != tim[4]) {
          (void)snprintf(line, sizeof(line),
			 "%02d%02d%02d.%02d%02d00 cont, %d+",
			 tim[0], tim[1], tim[2], tim[3], tim[4], n_zone_trig);
          if (n_zone_trig > 1)
	    (void)sprintf(line + strlen(line), " zones\n");
          else
	    (void)sprintf(line + strlen(line), " zone\n");
          if (not_yet == 0)
	    write_file(file_trig, line);
	}
      }  /* if (n_zone_trig == 1 && rep_level >= 1) */
    }  /* if (n_trig[z] == min_trig[z]) */
  }  /* for (j = 0; j < tbl[ch].n_zone; j++) */
}

static void
sprintm(char *tb, int *tm)
{

  sprintf(tb, "%02d%02d%02d.%02d%02d%02d",
	  tm[0], tm[1], tm[2], tm[3], tm[4], tm[5]);
}

static void
cptm(int *dst, int *src)
{
  int i;

  for (i = 0; i < 6; i++)
    dst[i] = src[i];
}

static void
confirm_off(int ch, int sec, int i)
{
  int ii, j, tm[6], z, jj;
  static char prev_off[15];
  
  tbl[ch].status = 0;

  for (j = 0; j < tbl[ch].n_zone; j++) {
    z = tbl[ch].zone[j];
    if (rep_level >= 3) {
      (void)snprintf(line, sizeof(line), "  %-4s(%-7.7s) off, %5.1f %7d\n",
		     tbl[ch].name, zone[z], tbl[ch].sec_on, tbl[ch].max);
      write_file(file_trig, line);
    }
    if (--n_trig[z] == min_trig[z] - 1) {
      sprintm(time_text, tim);
      (void)sprintf(time_text + strlen(time_text), ".%d", i * (10 / SR_MON));
      if (rep_level >= 2) {
        (void)snprintf(line, sizeof(line), " %s %-7.7s off, max=%d\n",
		       time_text, zone[z], max_trig[z]);
        write_file(file_trig,line);
      }
      if (--n_zone_trig == 0) {
        if (rep_level >= 1) {
          if (strncmp(prev_off, time_text, 13) == 0) {  /* ??? */
            cptm(tm, tim);
            tm[5]++;
            adj_time(tm);
            sprintm(time_text, tm);
	  }
          (void)strncpy(prev_off,time_text, 13);
          (void)snprintf(line, sizeof(line), "%13.13s off,", time_text);
          for (ii = 1; ii < cnt_zone; ii++) {
            for (jj = 1; jj < ii; jj++)
	      if (i_zone[ii] == i_zone[jj])
		break;
            if (jj < ii)
	      continue; 
            (void)sprintf(line + strlen(line), " %.7s", zone[i_zone[ii]]);
	  }
          sprintf(line + strlen(line), "\n");
          if (not_yet == 0)
	    write_file(file_trig, line);
	}
        cnt_zone=0;
      }  /* if (--n_zone_trig == 0) */
    }  /* if (--n_trig[z] == min_trig[z] - 1) */
  }  /* for (j = 0; j < tbl[ch].n_zone; j++) */
}

static int
read_one_sec(int *sec)
{
  int i, j, k, kk;
  int32_w  lower_min, lower_max;
  uint8_w  aa, bb, sys;
  WIN_ch  ch;
  uint32_w  size;
  uint8_w  *ptr, *ptr_lim;
  static uint32_w upper[4][8] = {
    {0x00000000, 0x00000010, 0x00000020, 0x00000030,
     0xffffffc0, 0xffffffd0, 0xffffffe0, 0xfffffff0},
    {0x00000000, 0x00000100, 0x00000200, 0x00000300,
     0xfffffc00, 0xfffffd00, 0xfffffe00, 0xffffff00},
    {0x00000000, 0x00010000, 0x00020000, 0x00030000,
     0xfffc0000, 0xfffd0000, 0xfffe0000, 0xffff0000},
    {0x00000000, 0x01000000, 0x02000000, 0x03000000,
     0xfc000000, 0xfd000000, 0xfe000000, 0xff000000}};

  if (read(fd, buf, 4) < 4)
    return (-1);
  size = mkuint4(buf);
  /* if (size < 0 || size > LENGTH) */
  if (size > LENGTH)
    return (-1);
  if (read(fd, buf + 4, size - 4) < size - 4)
    return (-1);
  ptr = buf + 9;
  ptr_lim = buf + size;
  *sec = (((*ptr) >> 4) & 0x0f) * 10 + ((*ptr) & 0x0f);
  ptr++;
  j=0;
  for (i = 0; i < m_ch; i++)
    idx[tbl[i].ch] = (-1);

  while (ptr < ptr_lim) { /* ch loop */
    sys = (*ptr++);
    ch = (sys << 8) + (*(ptr++));
    for (i = 0; i < SR_MON; i++) {
      aa = (*(ptr++));
      bb = aa & 3;
      if (bb) {
        lower_min = lower_max = 0;
        kk = 0;
        for (k = 0; k < bb; k++) {
          lower_min |= (*ptr++) << kk;
          kk += 8;
	}
        kk = 0;
        for (k = 0; k < bb; k++) {
          lower_max |= (*ptr++) << kk;
          kk += 8;
	}
      } else {
        lower_min = (*ptr >> 4) & 0xf;
        lower_max = (*ptr) & 0xf;
        ptr++;
      }
      
      if(idx2[ch]) {
        long_min[j][i] = lower_min | upper[bb][(aa >> 5) & 7];
        long_max[j][i] = lower_max | upper[bb][(aa >> 2) & 7];
      }
    }  /* for (i = 0; i < SR_MON; i++) */
    if (idx2[ch])
      idx[ch] = j++;
  }  /* while (ptr < ptr_lim) */

  /* 1-sec data prepared */
  return (j);
}

static void
check_trg()
{
  int i,ch;
  double data;
  char tb[254];

  if (n_zone_trig > 0 && rep_level >= 1) {
    (void)snprintf(line, sizeof(line), "%02d%02d%02d.%02d%02d00 cont, %d",
		   tim[0], tim[1], tim[2], tim[3], tim[4], n_zone_trig);
    if (n_zone_trig > 1)
      (void)sprintf(line + strlen(line), " zones\n");
    else
      sprintf(line + strlen(line), " zone\n");
    if (not_yet == 0)
      write_file(file_trig, line);
  }

  for (ch = 0;ch < m_ch; ch++) {
    tbl[ch].zero_acc = 0.0;
    tbl[ch].zero_cnt = 0;
  } 

  while (read_one_sec(&tim[5]) >= 0) {
    for (i = 0; i < SR_MON; i++) {
      /* x = xbase + tim[5] * SR_MON + i; */
      /*       yy = ybase + ppt_half; */

      for (ch = 0; ch < m_ch; ch++) {
        if (idx[tbl[ch].ch] != (-1) && tbl[ch].name[0] != '*') {
          if (tim[5] == 0 && i == 0 && m_limit == 0 && !tbl[ch].alive) {
            (void)snprintf(tb, sizeof(tb), "'%s' present (pmon)", tbl[ch].name);
            write_log(tb);
            tbl[ch].alive = 1;
	  }
	} else if (tim[5] == 0 && i == 0 && tbl[ch].name[0] != '*'
		   && m_limit == 0 && tbl[ch].alive) {
          (void)snprintf(tb, sizeof(tb), "'%s' absent (pmon)", tbl[ch].name);
          write_log(tb);
          tbl[ch].alive = 0;
	}

	/* check trigger */
        if(tbl[ch].use == 0)
	  continue;
        if (idx[tbl[ch].ch] == (-1))
	  data = 0.0; /* ch not found */
        else
	  data = (double)(long_max[idx[tbl[ch].ch]][i]
			  - long_min[idx[tbl[ch].ch]][i]);
	
	/* i.e. max==min for 0.2 s period */
	/* this means that the channel is not working */
        if (data <= TOO_LOW) {
	  /* if '0' continued more than 1 s, disable channel */
          if ((tbl[ch].sec_zero += dt) > TIME_TOO_LOW)
            tbl[ch].cnt=0.0;
	  /* when data==0, disable calculation of STA and LTA */
	} else { /* if data is valid */
	  /* restart '0 data' counter */
          tbl[ch].sec_zero = 0.0;
	  /* calculate STA */
          tbl[ch].sta = tbl[ch].sta * a_sta + data * b_sta;
	  /* calculate LTA and LTA_OFF */
          if (tbl[ch].status == 1 || tbl[ch].status == 2)
            tbl[ch].lta = tbl[ch].lta * a_lta_off + data * b_lta_off;
          else
	    tbl[ch].lta = tbl[ch].lta * a_lta + data * b_lta;
	}
#if DEBUG
	/* if(tbl[ch].ch==0x81 && tim[4]>=20) */
	printf(
	       "%5d %s %5d-%5d %5.1f/%5.1f(%5.1f) %5.1f %5.1f %d %5.1f %5.1f\n",
	       tim[5],tbl[ch].name,long_max[idx[tbl[ch].ch]][i],
	       long_min[idx[tbl[ch].ch]][i],
	       tbl[ch].sta,tbl[ch].lta,tbl[ch].lta_save,tbl[ch].sec_on,
	       tbl[ch].sec_off,
	       tbl[ch].status,data,tbl[ch].cnt);
#endif
	/* station is disabled */
	/* not enough time has passed after recovery from '0 data' */
        if ((tbl[ch].cnt += dt) < time_lta * 2.0) {
          switch(tbl[ch].status) {     /* force to OFF */
	  case 0:   /* OFF */
	    break;
	  case 1:   /* ON but not confirmed */
	    tbl[ch].sec_on += dt;
	    tbl[ch].status = 0;
	    break;
	  case 2:   /* ON confirmed */
	    tbl[ch].sec_on += dt;
	  case 3:   /* OFF but not confirmed */
	    confirm_off(ch,tim[5], i);
	    break;
	  }
          continue;
	}

	/* calculate STA/LTA */
        if (tbl[ch].sta / tbl[ch].lta >= tbl[ch].ratio) {
          switch(tbl[ch].status) {
	  case 0:   /* OFF */
	    cptm(tbl[ch].tm, tim);
	    tbl[ch].tm[6] = i;
	    tbl[ch].sec_on = 0.0;
	    tbl[ch].max = data;
	    tbl[ch].lta_save = tbl[ch].lta;
	    tbl[ch].status = 1;
	    break;
	  case 1:   /* ON but not confirmed */
	    if ((tbl[ch].sec_on += dt) >= time_on)
	      confirm_on(ch);
	    if (tbl[ch].max < data)
	      tbl[ch].max = data;
	    break;
	  case 2:   /* ON confirmed */
	    tbl[ch].sec_on += dt;
	    if (tbl[ch].max < data)
	      tbl[ch].max = data;
	    break;
	  case 3:   /* OFF but not confirmed */
	    tbl[ch].sec_on += (tbl[ch].sec_off + dt);
	    tbl[ch].status = 2;
	    if (tbl[ch].max < data)
	      tbl[ch].max = data;
	    break;
	  }
	} else {  /* if (tbl[ch].sta / tbl[ch].lta >= tbl[ch].ratio) */
          switch(tbl[ch].status) {
	  case 0:   /* OFF */
	    break;
	  case 1:   /* ON but not confirmed */
	    tbl[ch].sec_on += dt;
	    tbl[ch].status = 0;
	    break;
	  case 2:   /* ON confirmed */
	    tbl[ch].sec_on += dt;
	    tbl[ch].sec_off = 0.0;
	    tbl[ch].status = 3;
	    break;
	  case 3:   /* OFF but not confirmed */
	    if ((tbl[ch].sec_off += dt) >= time_off)
	      confirm_off(ch, tim[5], i);
	    break;
	  }
	}  /* if (tbl[ch].sta / tbl[ch].lta >= tbl[ch].ratio) */
      }  /* for (ch = 0; ch < m_ch; ch++) */
    }  /* for (i = 0; i < SR_MON; i++) */
    if (m_limit) {
      (void)printf(".");
      (void)fflush(stdout);
    }
    if (tim[5] == 59)
      break;
  }  /*   while (read_one_sec(&tim[5]) >= 0) */

  if (m_limit)
    (void)printf("\n");
}

static void
hangup()
{
  
  /* req_print = 1; */
  signal(SIGHUP, (void *)hangup);
}

static void
owari()
{

  (void)close(fd);
  write_log("end");
  if (made_lock_file)
    (void)unlink(file_trig_lock);
  exit(0);
}

static int
check_path(char *path, int idx)
{
  DIR *dir_ptr;
  char tb[1024],tb2[100];
  FILE *fp;

  if(idx == DIR_R || idx == DIR_W) {
    if ((dir_ptr = opendir(path)) == NULL) {
      (void)snprintf(tb2, sizeof(tb2), "directory not open '%s'", path);
      write_log(tb2);
      owari();
    }
    else
      (void)closedir(dir_ptr);

    if (idx == DIR_W) {
      (void)snprintf(tb, sizeof(tb), "%s/%s.test.%d", path, progname, getpid());
      if ((fp = fopen(tb, "w+")) == NULL) {
        (void)snprintf(tb2, sizeof(tb2), "directory not R/W '%s'", path);
        write_log(tb2);
        owari();
      } else {
        (void)fclose(fp);
        (void)unlink(tb);
      }
    }
  } else if (idx == FILE_R) {
    if ((fp = fopen(path, "r")) == NULL) {
      (void)snprintf(tb2, sizeof(tb2), "file not readable '%s'", path);
      write_log(tb2);
      owari();
    } else
      (void)fclose(fp);
  } else if (idx == FILE_W) {
    if ((fp = fopen(path, "r+")) == NULL) {
      (void)snprintf(tb2, sizeof(tb2), "file not R/W '%s'", path);
      write_log(tb2);
      owari();
    } else
      (void)fclose(fp);
  }

  return (1);
}

static void
get_lastline(char *fname, char *lastline)
{
  FILE *fp;
  long dp, last_dp;  /* 64bit ok */

  if ((fp = fopen(fname, "r")) == NULL) {
    *lastline=0;
    return;
  }
  last_dp = (-1);
  dp = 0;
  while (fgets(lastline, LEN, fp) != NULL) {
    if (*lastline != ' ')
      last_dp = dp;
    dp = ftell(fp);
  }
 
 if (last_dp == (-1)) {
    *lastline=0;
    return;
  } else {
    (void)fseek(fp, last_dp, 0);
    (void)fgets(lastline, LEN, fp);
  }

  (void)fclose(fp);
}

static void
usage()
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr," usage of '%s' :\n", progname);
  (void)fprintf(stderr,
		"  '%s (-d [dly min]) (-l [log file]) [param file] ([YYMMDD] [hhmm] [length(min)])'\n",
		progname);
}

int
main(int argc, char *argv[])
{
  FILE *f_param, *fp;
  int i, j, k, maxlen, ret, m_count, ch, tm1[6], minutep, hourp, c,
    count_max, wait_min, n_min_trig;
  char *ptr, textbuf[500], textbuf1[500], fn1[200], conv[256], tb[100],
    path_temp[WIN_FILENAME_MAX], path_mon[WIN_FILENAME_MAX], area[20],
    timebuf1[80], timebuf2[80], name[STNLEN], comp[CMPLEN],
    path_mon1[WIN_FILENAME_MAX];
  
  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  wait_min = 0;
  logfile = NULL;
  exit_status = EXIT_SUCCESS;
  while ((c = getopt(argc, argv, "d:l:")) != -1) {
    switch (c) {
    case 'd':   /* delay minute */
      wait_min = atoi(optarg);
      break;
    case 'l':   /* logfile */
      logfile = optarg;
      break;
    default:
      (void)fprintf(stderr, " option -%c unknown\n", c);
      usage();
      exit(1);
    }
  }
  optind--;
  if(argc < 2 + optind) {
    usage();  
    exit(1);
  }

  write_log("start");
  param_file = argv[1 + optind];

  /* open parameter file */ 
  if ((f_param = fopen(param_file, "r")) == NULL) {
    (void)snprintf(tb, sizeof(tb), "'%s' not open", param_file);
    write_log(tb);
    owari();
  }
  read_param_line(f_param, fn1, sizeof(fn1));    /* (1) footnote */  /* skip */
  /* fn1[strlen(fn1) - 1] = 0; */      /* delete CR */
  read_param_line(f_param, textbuf1, sizeof(textbuf1)); /* (2) mon data directory */
  (void)sscanf(textbuf1, "%s", textbuf);
  if ((ptr = strchr(textbuf, ':')) == NULL) {
    (void)sscanf(textbuf, "%s", path_mon);
    (void)sscanf(textbuf, "%s", path_mon1);
    check_path(path_mon, DIR_R);
  } else {
    *ptr = 0;
    (void)sscanf(textbuf, "%s", path_mon);
    check_path(path_mon, DIR_R);
    (void)sscanf(ptr + 1, "%s", path_mon1);
    check_path(path_mon1, DIR_R);
  }
  read_param_line(f_param, textbuf1, sizeof(textbuf1)); /* (3) temporary work directory : N of files */
  (void)sscanf(textbuf1, "%s", textbuf);
  if ((ptr = strchr(textbuf, ':')) == 0) {
    (void)sscanf(textbuf, "%s", path_temp);
    check_path(path_temp, DIR_W);
    count_max = (-1);
  } else {
    *ptr=0;
    (void)sscanf(textbuf, "%s", path_temp);
    check_path(path_temp, DIR_W);
    (void)sscanf(ptr + 1, "%s", textbuf1);
    if ((ptr = strchr(textbuf1, ':')) == 0) {
      (void)sscanf(textbuf1, "%d", &count_max);
      *conv=0;
    } else {
      *ptr=0;
      (void)sscanf(textbuf1, "%d", &count_max);
      (void)sscanf(ptr + 1, "%s", conv); /* path of conversion program */
    }
  }
  read_param_line(f_param, textbuf, sizeof(textbuf));  /* (4) channel table file */
  (void)sscanf(textbuf, "%s", ch_file);
  check_path(ch_file, FILE_R);
  read_param_line(f_param, textbuf, sizeof(textbuf));  /* (5) printer name */  /* skip */
  /* sscanf(textbuf,"%s",printer); */
  read_param_line(f_param,textbuf, sizeof(textbuf));  /* (6) rows/sheet */  /* skip */
  /* sscanf(textbuf,"%d",&n_rows); */
  read_param_line(f_param,textbuf, sizeof(textbuf));  /* (7) N of channels */
  (void)sscanf(textbuf,"%d",&max_ch);
/**********************************************
  n_rows    max_ch(just for suggestion; i.e. pixels_per_trace=20)
    1        209
    2        103 (approx. 140 is possible actually)
    3         67
    6         32
    9         20
   12         14
 **********************************************/
  read_param_line(f_param, textbuf, sizeof(textbuf));  /* (8) trigger report file */
  (void)sscanf(textbuf, "%s", file_trig);
  (void)strcpy(file_trig_lock, file_trig);
  (void)strcat(file_trig_lock, ".lock");
  read_param_line(f_param, textbuf, sizeof(textbuf));  /* (9) zone table file */
  (void)sscanf(textbuf, "%s", file_zone);
  check_path(file_zone, FILE_R);
  read_param_line(f_param, textbuf, sizeof(textbuf));  /* (10) min trg list file */
  (void)sscanf(textbuf, "%s", file_zone_min_trig);
  check_path(file_zone_min_trig, FILE_R);
  (void)fclose(f_param);
  if (sizeof(temp_done)
      <= snprintf(temp_done, sizeof(temp_done), "%s/USED", path_mon1)) {
    (void)snprintf(tb, sizeof(tb), "buffer overflow1");
    write_log(tb);
    owari();
  }
  if (sizeof(latest)
      <= snprintf(latest, sizeof(latest), "%s/LATEST", path_mon)) {
    (void)snprintf(tb, sizeof(tb), "buffer overflow2");
    write_log(tb);
    owari();
  }
  /* min_per_sheet = MIN_PER_LINE * n_rows; */    /* min/sheet */

  if (argc < 5 + optind) {
  retry:
    /* read USED file for when to start */
    if ((fp = fopen(temp_done, "r")) == NULL) {
      (void)snprintf(tb, sizeof(tb), "'%s' not found.  Use '%s' instead.",
		     temp_done, latest);
      write_log(tb);
      while ((fp = fopen(latest, "r")) == NULL) {
        (void)sprintf(tb, "'%s' not found. Waiting ...", latest);
        write_log(tb);
        sleep(60);
      }
    }
    if(fscanf(fp, "%2d%2d%2d%2d.%2d", tim, tim+1, tim+2, tim+3, tim+4) < 5) {
      (void)fclose(fp);
      (void)unlink(temp_done);
      (void)snprintf(tb, sizeof(tb), "'%s' illegal -  deleted.", temp_done);
      write_log(tb);
      sleep(60);
      goto retry;
    }
    (void)fclose(fp);
    for(k=0; k < wait_min; ++k) {
      tim[5] = (-1);
      adj_time(tim);
    }
    /* decide 'even' start time */
    /* i=((tim[3]*60+tim[4])/min_per_sheet)*min_per_sheet; */
    /*  tim[3]=i/60; */
    /*  tim[4]=i%60; */
    /* no limit */
    m_limit=0;
  } else {  /* if (argc < 5 + optind) */
    (void)sscanf(argv[2 + optind], "%2d%2d%2d", tim, tim+1, tim+2);
    (void)sscanf(argv[3 + optind], "%2d%2d", tim+3, tim+4);
    (void)sscanf(argv[4 + optind], "%d", &m_limit);
    if (m_limit > 0)
      (void)strcat(temp_done, "_");
  }  /* if (argc < 5 + optind) */
  made_lock_file = 0;
  if (m_limit == 0) {
    if ((fp = fopen(file_trig_lock, "r")) != NULL) {
      (void)fscanf(fp, "%d", &i);
      (void)fclose(fp);
      if (kill(i, 0) == 0) {
        (void)snprintf(tb, sizeof(tb),
		       "Can't run because lock file '%s' is valid for PID#%d.",
		       file_trig_lock, i);
        write_log(tb);
        owari();
      }
      (void)unlink(file_trig_lock);
    }
    if ((fp = fopen(file_trig_lock, "w+")) != NULL) {
      (void)fprintf(fp, "%d\n", getpid());  /* write process ID */
      (void)fclose(fp);
      made_lock_file = 1;
    }
  }

  /* make_fonts(font16,font24,font32); */

  /* get the last line in trg file */
  get_lastline(file_trig, last_line);
  if (*last_line && m_limit)
    (void)printf("last line in trig file = %s", last_line);

  /* pixels_per_trace=(((PLOTHEIGHT-(n_rows-1)*Y_SPACE)/n_rows)-Y_SCALE*2)/max_ch; */
  /* if(pixels_per_trace<20) max_ch_flag=1; */  /* not use 24dot font */
/*   else max_ch_flag=0; */
/* #if LONGNAME */
/*   max_ch_flag=0; */
/* #endif */
/*   ppt_half=pixels_per_trace/2; */
  m_count=0;
  fd = (-1);
  if (*last_line)
    not_yet = 1;
  else
    not_yet = 0;
  n_zone_trig = cnt_zone = 0;
  /* x_base=X_BASE-28*max_ch_flag; */
  /* y_base=Y_BASE; */
  for (i = 0; i < M_CH; i++)
    tbl[i].use = 0;
  /* for(i=0;i<HEIGHT_LBP;i++) for(j=0;j<WIDTH_LBP;j++) frame[i][j]=0; */

  signal(SIGINT, (void *)owari);
  signal(SIGTERM, (void *)owari);
  signal(SIGPIPE, (void *)owari);
  signal(SIGHUP, (void *)hangup);
  /* req_print=0; */
  *timebuf1 = 0;

  /* Begin main loop */
  for(;;) {
    if (m_count % MIN_PER_LINE == 0) {  /* print info */
      /* (void)snprintf(textbuf, sizeof(textbuf), "%02d/%02d/%02d %02d:%02d", */
/* 		     tim[0], tim[1], tim[2], tim[3], tim[4]); */
/*       put_font((uint8_w *)frame,WIDTH_LBP,0,y_base-Y_SCALE-HEIGHT_FONT32, */
/* 	       (uint8_w *)textbuf,font32,HEIGHT_FONT32,WIDTH_FONT32,0); */
      /* update channels */
      while ((f_param = fopen(param_file, "r")) == NULL) {
        (void)snprintf(tb, sizeof(tb), "'%s' file not found (%d)", 
		       param_file, getpid());
        write_log(tb);
        sleep(60);
      }
      for(i = 0; i < 10; i++)
	read_param_line(f_param, textbuf, sizeof(textbuf));
      /* read_param_line(f_param, textbuf); */    /* (10) */
      /* (void)sscanf(textbuf, "%d", &n_min_trig); */
      read_param_line(f_param, textbuf, sizeof(textbuf));    /* (11) */
      (void)sscanf(textbuf, "%lf", &time_sta);
      read_param_line(f_param, textbuf, sizeof(textbuf));    /* (12) */
      (void)sscanf(textbuf, "%lf", &time_lta);
      read_param_line(f_param, textbuf, sizeof(textbuf));    /* (13) */
      (void)sscanf(textbuf, "%lf", &time_lta_off);
      read_param_line(f_param, textbuf, sizeof(textbuf));    /* (14) */
      (void)sscanf(textbuf, "%lf", &time_on);
      read_param_line(f_param, textbuf, sizeof(textbuf));    /* (15) */
      (void)sscanf(textbuf, "%lf", &time_off);
      read_param_line(f_param, textbuf, sizeof(textbuf));    /* (16) */
      (void)sscanf(textbuf, "%d", &rep_level);
      j = 1;
      for (i = 0; i < max_ch; i++) {
        if (tbl[i].use && tbl[i].status)
	  j = 0;
        tbl[i].alive = 1;
      }
      if (j) {  /* there is no triggered channel now */
        n_zone = 0;
        for (i = 0; i < WIN_CHMAX; i++)
	  idx2[i] = 0;
        maxlen = 0;
        for (i = 0; i < max_ch; i++) {
          if(read_param_line(f_param, textbuf, sizeof(textbuf)))
	    break;
          if(!isalnum(*textbuf)) { /* make a blank line */
            tbl[i].use = 0;
            tbl[i].name[0] = '*';
            continue;
	  }
          (void)sscanf(textbuf, "%s%s%d%lf", tbl[i].name,
		       tbl[i].comp, &tbl[i].gain, &tbl[i].ratio);
          tbl[i].n_zone = 0;
          /* get sys_ch from channel table file */
          while((fp = fopen(ch_file, "r")) == NULL) {
            (void)snprintf(tb, sizeof(tb), "'%s' file not found (%d)",
			   ch_file, getpid());
            write_log(tb);
            sleep(60);
	  }
          while((ret = read_param_line(fp, textbuf1, sizeof(textbuf1))) == 0) {
            (void)sscanf(textbuf1, "%x%d%*d%s%s", &j, &k, name, comp);
            /*if(k==0) continue;*/  /* check 'record' flag */
            if(strcmp(tbl[i].name,name))
	      continue;
            if(strcmp(tbl[i].comp,comp))
	      continue;
            ch = j;
            ret = 0;
            break; /* channel (j) found */
	  }
          (void)fclose(fp);
          if (ret) {  /* channel not found in file */
            (void)snprintf(tb, sizeof(tb), "'%s-%s' not found",
			   tbl[i].name, tbl[i].comp);
            write_log(tb);
            tbl[i].use = 0;
            tbl[i].name[0] = '*';
            continue;
	  }

	  /* read zone table */
          while ((fp = fopen(file_zone, "r")) == NULL) {
            (void)snprintf(tb, sizeof(tb),
		     "'%s' file not found (%d)", file_zone, getpid());
            write_log(tb);
            sleep(60);
	  }
          while ((ptr = fgets(textbuf, sizeof(textbuf), fp)) != NULL) {
            if (*ptr == '#')
	      ptr++;
            if ((ptr = strtok(ptr, " +/\t\n")) == NULL)
	      continue;
            (void)strcpy(area, ptr);
            while ((ptr = strtok(0, " +/\t\n")) != NULL)
	      if (strcmp(ptr, tbl[i].name) == 0)
		break;
            if (ptr) {   /* a zone for station "tbl[i].name" found */
              if (tbl[i].ratio != 0.0) {
                for (j = 0; j < n_zone; j++)
		  if (strcmp(area, zone[j]) == 0)
		    break;
                if (j == n_zone) { /* register a new zone */
                  n_stn[n_zone] = n_trig[n_zone] = 0;
                  (void)strcpy(zone[n_zone++], area);
		}
                if (tbl[i].n_zone < MAX_ZONES) {
                  tbl[i].zone[tbl[i].n_zone] = j; /* add zone for the stn */
                  tbl[i].n_zone++; /* n of zones for the stn */
                  n_stn[j]++;      /* n of stns for the zone */
		} else {
                  (void)snprintf(tb, sizeof(tb),
				 "too many zones for station '%s'",
				 tbl[i].name);
                  write_log(tb);
		}
	      }  /* if (tbl[i].ratio != 0.0) */
	    }  /* if (ptr) */
	  }  /* while((ptr = fgets(textbuf, sizeof(textbuf), fp)) != NULL) */
          (void)fclose(fp);

          if (tbl[i].ratio != 0.0) {
            if (tbl[i].n_zone == 0) {
              (void)snprintf(tb, sizeof(tb), "zone for '%s' not found %d",
			     tbl[i].name, i);
              write_log(tb);
              tbl[i].ratio=0.0;
	    } else {
              tbl[i].sec_zero = 0.0;
              tbl[i].status = 0;
              if (!(ch == tbl[i].ch) || !tbl[i].use)
                tbl[i].lta = tbl[i].sta = tbl[i].cnt = 0.0;
              tbl[i].use = 1;
	    }
	  } else
	    tbl[i].use = 0;
          tbl[i].ch = ch;
          idx2[ch] = 1;
          if (maxlen < strlen(tbl[i].name) + strlen(tbl[i].comp))
            maxlen = strlen(tbl[i].name) + strlen(tbl[i].comp);
	}  /* for (i = 0; i < max_ch; i++) */
        m_ch = i;
        if (m_limit)
          (void)printf("N of valid channels = %d (in %d frames)\n",
		       m_ch, max_ch);

	while ((fp = fopen(file_zone_min_trig, "r")) == NULL) {
	  (void)snprintf(tb, sizeof(tb), "'%s' file not found (%d)",
			 file_zone_min_trig, getpid());
	  write_log(tb);
	  sleep(60);
	}
	for (i = 0; i < n_zone; ++i)
	  min_trig[i] = -1;
	while (fgets(textbuf, sizeof(textbuf), fp) != NULL) {
	  if (textbuf[0] == '#')  /* skip comment */
	    continue;
	  if (sscanf(textbuf, "%s%d", area, &n_min_trig) < 2)
	    continue;
	  for (i = 0; i < n_zone; ++i)
	    if (strcmp(area, zone[i]) == 0) {
	      min_trig[i] = n_min_trig;
	      break;
	    }
	}
	(void)fclose(fp);
        for (i = 0; i < n_zone; i++) {
          if (n_stn[i] <= min_trig[i])
	    min_trig[i] = n_stn[i];
          /* else */
	  /*   min_trig[i] = n_min_trig; */
          if (m_limit)
	    (void)printf("zone #%2d %-12s n=%2d min=%2d\n",
			 i, zone[i], n_stn[i], min_trig[i]);
	}
      }  /* if (j) */
      (void)fclose(f_param);

      /* calculate coefs a & b */
      a_sta = exp(-dt / time_sta);          /* nearly 1 */
      b_sta = 1.0 - a_sta;                  /* nearly 0 */
      a_lta = exp(-dt / time_lta);          /* nearly 1 */
      b_lta = 1.0 - a_lta;                  /* nearly 0 */
      a_lta_off = exp(-dt / time_lta_off);  /* nearly 1 */
      b_lta_off = 1.0 - a_lta_off;          /* nearly 0 */
    }  /* if (m_count % MIN_PER_LINE == 0) */

    /* if(m_count%min_per_sheet==0)  /\* make fns *\/ */
    /*       sprintf(fn2,"%02d/%02d/%02d %02d:%02d", */
    /*         tim[0],tim[1],tim[2],tim[3],tim[4]); */

/*
  timebuf1 : LATEST
  timebuf2 : tim[] - data time
*/
    sprintf(timebuf2,"%02d%02d%02d%02d.%02d",
        tim[0],tim[1],tim[2],tim[3],tim[4]);
    if (*timebuf1 == 0 || strcmp2(timebuf1, timebuf2) < 0)
      for(;;) {  /* wait LATEST */
	if ((fp = fopen(latest, "r")) == NULL)
	  sleep(15);
	else {
	  (void)fscanf(fp, "%s", timebuf1);
	  (void)fclose(fp);
	  if(wait_min) {
	    (void)sscanf(timebuf1, "%02d%02d%02d%02d.%02d",
			 &tm1[0], &tm1[1], &tm1[2], &tm1[3], &tm1[4]);
	    for (k = 0; k < wait_min; ++k) {
              tm1[5]=(-1);
              adj_time(tm1);
	    }
	    snprintf(timebuf1, sizeof(textbuf1), "%02d%02d%02d%02d.%02d",
		     tm1[0], tm1[1], tm1[2], tm1[3], tm1[4]);
          }
	  if (strlen(timebuf1) >= 11 && strcmp2(timebuf1, timebuf2) >= 0)
	    break;
        }
	/* if(req_print) */
	/* insatsu((uint8_w *)fn1,(uint8_w *)fn2,(uint8_w* )fn3,path_temp, */
	/*   printer,count,count_max,conv); */
	sleep(15);
      }  /* while (1) */

    (void)snprintf(textbuf, sizeof(textbuf), "%s/%s", path_mon, timebuf2);
    if ((fd = open(textbuf, O_RDONLY)) != -1) {
      if(m_limit)
        (void)printf("%02d%02d%02d %02d%02d",
		     tim[0], tim[1], tim[2], tim[3], tim[4]);
      (void)fflush(stdout);
      /*       if (m_count % MIN_PER_LINE == 0 && offset == 1) */
      /*   get_offset(); */

      check_trg(); /* one minute data */

      (void)close(fd);
      if (m_limit == 0) {
        fp = fopen(temp_done, "w+");
        (void)fprintf(fp, "%s\n", timebuf2);  /* write done file */
        (void)fclose(fp);
      }
    } else if (m_limit)
      (void)printf("%s failed\n", timebuf2);
    minutep = tim[4];
    hourp = tim[3];
    if (++minutep == 60) {
      if (++hourp == 24)
	hourp = 0;
      minutep = 0;
    }
    /* (void)snprintf(fn3, sizeof(fn3), " - %02d:%02d", hourp, minutep); */
/*     x_base += SR_MON * 60; */
    if (++m_count == m_limit)
      owari();

    if (m_limit) {
      (void)printf("\007");
      (void)fflush(stdout);
    }
    tim[5] = 60;
    adj_time(tim);
  }  /* while (1) */
}
