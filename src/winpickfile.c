/* $Id: winpickfile.c,v 1.1.2.4 2010/12/23 03:30:33 uehira Exp $ */

/*
 * Copyright (c) 2001-2010
 *   Uehira Kenji / All Rights Reserved.
 */

#ifdef HAVE_CONFIG_H
#include  "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <math.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include "winlib.h"

#define PICKBUF   1024

struct wp_buf {
  int   lnum;   /* pick file line number */
  int   s_pos;  /* #s position. If 0, there is no #s record. */
  int   f_pos;  /* #f position. If 0, there is no #f record. */
  char  **buf;  /* pick data buffer: buf[lnum][PICKBUF]. 
		   malloc by malloc_wpbuf(), free by free_wpbuf() */
};

/* prototype of local functions */
static int strn2int(char *, size_t, int *);
static int strn2double(char *, size_t, double *);
static time_t tim2time_t(int []);
static time_t tim_adj(int [], int);

static int malloc_wpbuf(struct wp_buf *);
static void free_wpbuf(struct wp_buf *);
static int get_wpbuf(char *, struct wp_buf *);

static int win_pickfile_station_data(char *, struct station_data *);
static int win_pickfile_hypo_data(struct wp_buf *, struct hypo_data *);
static int win_pickfile_final_data(char *, struct station_data *);

static int winpick_station_pol(char [], int *);


/*** Local functions ***/
static int
strn2int(char *src, size_t len, int *value)
{
  char  *buf;

  if ((buf = MALLOC(char, len + 1)) == NULL) {
    (void)fprintf(stderr, "Cannot malloc in 'strn2int'\n");
    return (1);
  }
  (void)strncpy(buf, src, len);
  buf[len] = '\0';
  *value = atoi(buf);
  FREE(buf);
  return (0);
}

static int
strn2double(char *src, size_t len, double *value)
{
  char   *buf;

  if ((buf = MALLOC(char, len + 1)) == NULL) {
    (void)fprintf(stderr, "Cannot malloc in 'strn2double'\n");
    return (1);
  }
  (void)strncpy(buf, src, len);
  buf[len] = '\0';
  *value = atof(buf);
  FREE(buf);
  return (0);
}

static time_t
tim2time_t(int tim[])
{
  struct tm  tim_str;
  time_t  tim_sec;

  tim_str.tm_year = tim[0] - 1900;
  tim_str.tm_mon = tim[1] - 1;
  tim_str.tm_mday = tim[2];
  tim_str.tm_hour = tim[3];
  tim_str.tm_min = tim[4];
  tim_str.tm_sec = tim[5];
  tim_str.tm_isdst = 0;

  if ((tim_sec = mktime(&tim_str)) == -1)
    (void)fprintf(stderr, "mktime error\n");

  return (tim_sec);
}

/*
  tim[6]:YYYYMMDD hhmmss
  */
static time_t
tim_adj(int tim[], int adj_sec)
{
  struct tm  *tim_str;
  time_t  tim_sec;

  if ((tim_sec = tim2time_t(tim)) != -1) {
    tim_sec += (time_t)adj_sec;
    tim_str = localtime(&tim_sec);

    tim[0] = tim_str->tm_year + 1900;
    tim[1] = tim_str->tm_mon + 1;
    tim[2] = tim_str->tm_mday;
    tim[3] = tim_str->tm_hour;
    tim[4] = tim_str->tm_min;
    tim[5] = tim_str->tm_sec;
  }

  return (tim_sec);
}

/*
  malloc 'buf' member of 'struct wp_buf'.
  'lnum' must be set.
  */
static int
malloc_wpbuf(struct wp_buf *wbuf)
{
  int  i, j;

  if ((wbuf->buf = MALLOC(char  *, wbuf->lnum)) == NULL) {
    (void)fprintf(stderr, "Cannot malloc in 'malloc_wpbuf'\n");
    return (1);
  }
  for(i = 0; i < wbuf->lnum; ++i)
    wbuf->buf[i] = NULL;
  for(i = 0; i < wbuf->lnum; ++i)
    if ((wbuf->buf[i] = MALLOC(char, PICKBUF)) == NULL) {
      (void)fprintf(stderr, "Cannot malloc in 'malloc_wpbuf'\n");
      for (j = 0; j < i; ++j)
	FREE(wbuf->buf[j]);
      FREE(wbuf->buf);
      return (1);
    }

  return (0);
}

/*
  Free 'buf' member of 'struct wp_buf' malloced by 'malloc_wpbuf' function.
  */
static void
free_wpbuf(struct wp_buf *wbuf)
{
  int  i;

  for (i = 0; i < wbuf->lnum; ++i)
    FREE(wbuf->buf[i]);
  FREE(wbuf->buf);
}

/*
  return station number
  if error, return -1
  */
static int
get_wpbuf(char *fname, struct wp_buf *wbuf)
{
  FILE  *fp;
  int   n_station;
  char  buf[PICKBUF];
  int   i;

  if ((fp = fopen(fname, "r")) == NULL) {
    (void)fprintf(stderr, "Cannot open file: %s\n", fname);
    return (-1);
  }

  /* count lines of pickfile and set to 'wbuf->lnum' */
  wbuf->lnum = 0;
  while (fgets(buf, PICKBUF, fp) != NULL)
    wbuf->lnum++;
  if (wbuf->lnum < 2) {
    (void)fprintf(stderr, "Illegal line number: %s\n", fname);
    return (-1);
  }
    
#if DEBUG>50
  (void)fprintf(stderr, "lnum=%d\n", wbuf->lnum);
#endif

  /* malloc buffer */
  if (malloc_wpbuf(wbuf))
    return (-1);

  /* read pick file and store to buffer & count '#s' line */
  rewind(fp);
  n_station = 0;
  for (i = 0; i < wbuf->lnum; ++i) {
    (void)fgets(wbuf->buf[i], PICKBUF, fp);
    if (strncmp(wbuf->buf[i], WINPICK_S_PREFIX, 2) == 0)
      n_station++;
  }
  (void)fclose(fp);
#if DEBUG>50
  (void)fprintf(stderr, "#s lines = %d\n", n_station);
#endif

  /* #s and #f line position */
  wbuf->s_pos = wbuf->f_pos = 0;
  for (i = 0; i < wbuf->lnum; ++i)
    if (strncmp(wbuf->buf[i], WINPICK_S_PREFIX, 2) == 0) {
      wbuf->s_pos = i;
      break;
    }
  for (i = 0; i < wbuf->lnum; ++i)
    if (strncmp(wbuf->buf[i], WINPICK_F_PREFIX, 2) == 0) {
      wbuf->f_pos = i;
      break;
    }
#if DEBUG>50
  (void)fprintf(stderr, "#s = %d  #f = %d\n", wbuf->s_pos, wbuf->f_pos);
#endif

  /* number of station from #s line number */
  if (0 < n_station && n_station < 3) {  /* invalid pickfile */
    (void)fprintf(stderr, "Invalid pickfile: %s\n", fname);
    free_wpbuf(wbuf);
    return (-1);
  } else if (n_station >= 3) /* in case of phase pick */
    n_station -= 2;

  return (n_station);
}


static int
win_pickfile_hypo_data(struct wp_buf *wbuf, struct hypo_data *hypo)
{
  static const int  tpos[5] = {WPH_YR, WPH_MON, WPH_DAY, WPH_HOUR, WPH_MIN};
  int  sec;
  int  i;

  /* origin time */
  for (i = 0; i < 5; ++i)
    if (strn2int(wbuf->buf[wbuf->f_pos] + tpos[i], 3, &hypo->ot[i]))  /* I3 */
      return (1);
  for (i = 0; i < 5; ++i)
    hypo->bot[i] = hypo->ot[i];
  if (hypo->ot[0] >= WIN_YEAR_BASE)
    hypo->ot[0] += 1900;
  else
    hypo->ot[0] += 2000;
  if (strn2double(wbuf->buf[wbuf->f_pos] + WPH_SEC, 8, &hypo->sec))  /* F8.3 */
    return (1);
  hypo->bosec = hypo->sec;
  sec = (int)floor(hypo->sec);
  hypo->sec -= (double)sec;
  if (sec >= 0)
    hypo->ot[5] = sec;
  else {
    hypo->ot[5] = 0;
    if (tim_adj(hypo->ot, sec) == -1)
      return (1);
  }
  if ((hypo->ott = tim2time_t(hypo->ot)) == -1)
    return (1);
  /* latitude */
  if (strn2double(wbuf->buf[wbuf->f_pos] + WPH_LAT, 11, &hypo->lat))/* F11.5 */
    return (1);
  /* longitude */
  if (strn2double(wbuf->buf[wbuf->f_pos] + WPH_LON, 11, &hypo->lon))/* F11.5 */
    return (1);
  /* depth */
  if (strn2double(wbuf->buf[wbuf->f_pos] + WPH_DEP, 8, &hypo->dep))  /* F8.3 */
    return (1);
  /* magnitude */
  if (strn2double(wbuf->buf[wbuf->f_pos] + WPH_MAG, 6, &hypo->mag))  /* F6.1 */
    return (1);
  /* hypomh status */
  if (sscanf(wbuf->buf[wbuf->f_pos + 1] , "%*s%4s%*lf", hypo->judge) < 1)
    return (1);
  hypo->judge[4] = '\0';
  /* dt */
  if (strn2double(wbuf->buf[wbuf->f_pos + 1] + WPH_DT, 8, &hypo->dt))/* F8.3 */
    return (1);
  /* dx */
  if (strn2double(wbuf->buf[wbuf->f_pos + 1] + WPH_DX, 9, &hypo->dx))/* F9.3 */
    return (1);
  /* dy */
  if (strn2double(wbuf->buf[wbuf->f_pos + 1] + WPH_DY, 9, &hypo->dy))/* F9.3 */
    return (1);
  /* dz */
  if (strn2double(wbuf->buf[wbuf->f_pos + 1] + WPH_DZ, 8, &hypo->dz))/* F8.3 */
    return (1);
  /* h00 */
  if (strn2double(wbuf->buf[wbuf->f_pos + 2] + WPH_H00, 10, &hypo->h00))/* F10.3 */
    return (1);
  /* -h01 */
  if (strn2double(wbuf->buf[wbuf->f_pos + 2] + WPH_H01, 10, &hypo->h01))/* F10.3 */
    return (1);
  /* h02 */
  if (strn2double(wbuf->buf[wbuf->f_pos + 2] + WPH_H02, 10, &hypo->h02))/* F10.3 */
    return (1);
  /* h11 */
  if (strn2double(wbuf->buf[wbuf->f_pos + 2] + WPH_H11, 10, &hypo->h11))/* F10.3 */
    return (1);
  /* -h12 */
  if (strn2double(wbuf->buf[wbuf->f_pos + 2] + WPH_H12, 10, &hypo->h12))/* F10.3 */
    return (1);
  /* h22 */
  if (strn2double(wbuf->buf[wbuf->f_pos + 2] + WPH_H22, 10, &hypo->h22))/* F10.3 */
    return (1);
  /* initial latitude (F7.3) */
  if (strn2double(wbuf->buf[wbuf->f_pos + 3] + WPH_I_LAT, 7, &hypo->i_lat))
    return (1);
  /* uncertainty of initial latitude (F5.1) */
  if (strn2double(wbuf->buf[wbuf->f_pos + 3] + WPH_I_LAT_UCR, 5,
		  &hypo->i_lat_ucr))
    return (1);
  /* initial longitude (F7.3) */
  if (strn2double(wbuf->buf[wbuf->f_pos + 3] + WPH_I_LON, 7, &hypo->i_lon))
    return (1);
  /* uncertainty of initial longitude (F5.1) */
  if (strn2double(wbuf->buf[wbuf->f_pos + 3] + WPH_I_LON_UCR, 5,
		  &hypo->i_lon_ucr))
    return (1);
  /* initial depth (F7.3) */
  if (strn2double(wbuf->buf[wbuf->f_pos + 3] + WPH_I_DEP, 7, &hypo->i_dep))
    return (1);
  /* uncertainty of initial depth (F5.1) */
  if (strn2double(wbuf->buf[wbuf->f_pos + 3] + WPH_I_DEP_UCR, 5,
		  &hypo->i_dep_ucr))
    return (1);

  /* station number */
  if (sscanf(wbuf->buf[wbuf->f_pos + 4],
	     "%*s %d %3s %d (%lf%% ) %d (%lf%% ) %*d (%lf%% )",
	     &hypo->num_sta, hypo->structname, &hypo->num_P, &hypo->init_P,
	     &hypo->num_S, &hypo->init_S, &hypo->init_dep) < 7)
    return (1);
  hypo->structname[3] = '\0';
  /* standard deviation */
  if (sscanf(wbuf->buf[wbuf->lnum -1], "%*s%lf%lf",
	     &hypo->sd_P, &hypo->sd_S) < 2)
    return(1);

#if DEBUG
  (void)fprintf(stderr, "OT = %02d/%02d/%02d %02d:%02d:%02d+%.3f  ",
		hypo->ot[0], hypo->ot[1], hypo->ot[2],
		hypo->ot[3], hypo->ot[4], hypo->ot[5], hypo->sec);
  (void)fprintf(stderr, "%.5fN %.5fE %.3fkm M%.1f\n",
		hypo->lat, hypo->lon, hypo->dep, hypo->mag);
  (void)fprintf(stderr, "dt=%.3fsec dx=%.3fkm dy=%.3fkm dz=%.3fkm\n",
		hypo->dt, hypo->dx, hypo->dy, hypo->dz);
  (void)fprintf(stderr,
		"init. %.3lfN (+-%.1lfkm) %.3lfE (+-%.1lfkm) %.3lfkm (+-%.1lfkm) \n",
		hypo->i_lat, hypo->i_lat_ucr, hypo->i_lon, hypo->i_lon_ucr,
		hypo->i_dep, hypo->i_dep_ucr);
  (void)fprintf(stderr,
		"sta=%d  P=%d  S=%d  sd_P=%.3fsec sd_S=%.3fsec iP=%.1lf%% iS=%.1lf%% idep=%.1f%%\n",
		hypo->num_sta, hypo->num_P, hypo->num_S,
		hypo->sd_P, hypo->sd_S,
		hypo->init_P, hypo->init_S, hypo->init_dep);
  (void)fprintf(stderr, "%s %s\n", hypo->structname, hypo->judge);
#endif

  return (0);
}

static int
win_pickfile_final_data(char *buf, struct station_data *sd)
{
  size_t  buflen;
  char  code[WIN_STACODE_BUF];

  buflen = strlen(buf);
  /* (void)printf("buflen = %d\n", buflen); */

  /* check station code */
  (void)sscanf(buf, "%*s %s", code);
  /* (void)fprintf(stderr, "%s:%s:\n", code, sd->code); */
  if (strcmp(code, sd->code)) {
    (void)fprintf(stderr, "unconsistent with #s and #f record\n");
    return (1);
  }

  if (buflen == WINPICK_F_LEN) {
    if (strn2double(buf + WPF_EPI, 8, &sd->epi_dist))       /* F8.3 */
      return (1);
    if (strn2double(buf + WPF_AZM, 6, &sd->azimuth))        /* F6.1 */
      return (1);
    if (strn2double(buf + WPF_TKO, 6, &sd->takeoff_ang))    /* F6.1 */
      return (1);
    if (strn2double(buf + WPF_INC, 6, &sd->incidnt_ang))    /* F6.1 */
      return (1);
    if (strn2double(buf + WPF_P_ARRIV, 7, &sd->p_arr_cor))  /* F7.3 */
      return (1);
    if (strn2double(buf + WPF_P_ERR, 6, &sd->p_w_error))    /* F6.3 */
      return (1);
    if (strn2double(buf + WPF_POC, 7, &sd->p_oc))           /* F7.3 */
      return (1);
    if (strn2double(buf + WPF_S_ARRIV, 7, &sd->s_arr_cor))  /* F7.3 */
      return (1);
    if (strn2double(buf + WPF_S_ERR, 6, &sd->s_w_error))    /* F6.3 */
      return (1);
    if (strn2double(buf + WPF_SOC, 7, &sd->s_oc))           /* F7.3 */
      return (1);
    if (strn2double(buf + WPF_AMPF, 9, &sd->amp_f))         /* E9.3? */
      return (1);
    if (strn2double(buf + WPF_MAG, 5, &sd->mag))            /* F5.1 */
      return (1);
  } else if (buflen == WINPICK_F_LEN_S) {
    if (strn2double(buf + WPF_EPI_S, 6, &sd->epi_dist))     /* F6.1 */
      return (1);
    if (strn2double(buf + WPF_AZM_S, 6, &sd->azimuth))      /* F6.1 */
      return (1);
    if (strn2double(buf + WPF_TKO_S, 6, &sd->takeoff_ang))  /* F6.1 */
      return (1);
    if (strn2double(buf + WPF_INC_S, 6, &sd->incidnt_ang))  /* F6.1 */
      return (1);
    if (strn2double(buf + WPF_P_ARRIV_S, 6, &sd->p_arr_cor)) /* F6.2 */
      return (1);
    if (strn2double(buf + WPF_P_ERR_S, 5, &sd->p_w_error))   /* F5.2 */
      return (1);
     if (strn2double(buf + WPF_POC_S, 6, &sd->p_oc))         /* F6.2 */
      return (1);
    if (strn2double(buf + WPF_S_ARRIV_S, 6, &sd->p_arr_cor)) /* F6.2 */
      return (1);
    if (strn2double(buf + WPF_S_ERR_S, 5, &sd->p_w_error))   /* F5.2 */
      return (1);
    if (strn2double(buf + WPF_SOC_S, 6, &sd->s_oc))         /* F6.2 */
      return (1);
    if (strn2double(buf + WPF_AMPF_S, 9, &sd->amp_f))         /* E9.3? */
      return (1);
    if (strn2double(buf + WPF_MAG_S, 5, &sd->mag))          /* F5.1 */
      return (1);
  } else {
    (void)fprintf(stderr, "Invalid buflen: #f\n");
    return (1);
  }

#if DEBUG
  (void)fprintf(stderr, "%s %.3f[km] %.1f %.1f %.1f %.3f %.3f %.1f\n",
		sd->code, sd->epi_dist, sd->azimuth, sd->takeoff_ang,
		sd->incidnt_ang, sd->p_oc, sd->s_oc, sd->mag);
#endif

  return (0);
}


/*
 * return  -1  : Invalid data
 *          0  : normal return
 *	    1  : not cataloged
 */
static int
win_pickfile_station_data(char *buf, struct station_data *sd)
{
  size_t  buflen;
  int     tmpi;

  buflen = strlen(buf);
  /* (void)printf("buflen = %d\n", buflen); */

  (void)sscanf(buf, "%*s %s", sd->code);
  if (buflen == WINPICK_S_LEN || buflen == WINPICK_S_LEN_SC) {
    if (winpick_station_pol(buf + WPS_POL, &sd->polarity))
      (void)fprintf(stderr, "Invalid polarity data: %s\n", sd->code);
    if (strn2double(buf + WPS_P_ARRIV, 8, &sd->p_arrival))  /* F8.3 */
      return (-1);
    if (strn2double(buf + WPS_P_ERR, 6, &sd->p_error))      /* F6.3 */
      return (-1);
    if (strn2double(buf + WPS_S_ARRIV, 8, &sd->s_arrival))  /* F8.3 */
      return (-1);
    if (strn2double(buf + WPS_S_ERR, 6, &sd->s_error))      /* F6.3 */
      return (-1);
    if (strn2double(buf + WPS_FP, 6, &sd->fp_time))         /* F6.1 */
      return (-1);
    if (strn2double(buf + WPS_AMP, 9, &sd->amp))            /* E9.2 */
      return (-1);
    if (strn2double(buf + WPS_LAT, 11, &sd->lat))           /* F11.5 */
      return (-1);
    if (strn2double(buf + WPS_LON, 11, &sd->lon))           /* F11.5 */
      return (-1);
    if (strn2int(buf + WPS_H, 7, &tmpi))                    /* I7 */
      return (-1);
    sd->h = (short)tmpi;
    if (buflen == WINPICK_S_LEN) 
      sd->p_corr = sd->s_corr = 0.0;
    else {
      if (strn2double(buf + WPS_P_CORR, 7, &sd->p_corr))    /* F7.3 */
	return (-1);
      if (strn2double(buf + WPS_S_CORR, 7, &sd->s_corr))    /* F7.3 */
	return (-1);
    }
  } else if (buflen == WINPICK_S_LEN_S || buflen == WINPICK_S_LEN_S_SC) {
    /* Short station code and component code (old format) */
    if (winpick_station_pol(buf + WPS_POL_S, &sd->polarity))
      (void)fprintf(stderr, "Invalid polarity data: %s\n", sd->code);
    if (strn2double(buf + WPS_P_ARRIV_S, 8, &sd->p_arrival))  /* F8.3 */
      return (-1);
    if (strn2double(buf + WPS_P_ERR_S, 6, &sd->p_error))      /* F6.3 */
      return (-1);
    if (strn2double(buf + WPS_S_ARRIV_S, 8, &sd->s_arrival))  /* F8.3 */
      return (-1);
    if (strn2double(buf + WPS_S_ERR_S, 6, &sd->s_error))      /* F6.3 */
      return (-1);
    if (strn2double(buf + WPS_FP_S, 6, &sd->fp_time))         /* F6.1 */
      return (-1);
    if (strn2double(buf + WPS_AMP_S, 9, &sd->amp))            /* E9.2 */
      return (-1);
    if (strn2double(buf + WPS_LAT_S, 11, &sd->lat))           /* F11.5 */
      return (-1);
    if (strn2double(buf + WPS_LON_S, 11, &sd->lon))           /* F11.5 */
      return (-1);
    if (strn2int(buf + WPS_H_S, 7, &tmpi))                    /* I7 */
      return (-1);
    sd->h = (short)tmpi;
    if (buflen == WINPICK_S_LEN) 
      sd->p_corr = sd->s_corr = 0.0;
    else {
      if (strn2double(buf + WPS_P_CORR_S, 7, &sd->p_corr))    /* F7.3 */
	return (-1);
      if (strn2double(buf + WPS_S_CORR_S, 7, &sd->s_corr))    /* F7.3 */
	return (-1);
    }
  } else {
    (void)fprintf(stderr, "Invalid buflen: #s\n");
    return (-1);
  }
#if DEBUG
  (void)fprintf(stderr,
		"%s %d %.3f %.3f %.3f %.3f %.1f %.2e %.5f %.5f %d %.3f %.3f\n",
		sd->code, sd->polarity, sd->p_arrival, sd->p_error,
		sd->s_arrival, sd->s_error, sd->fp_time, sd->amp, sd->lat,
		sd->lon, sd->h, sd->p_corr, sd->s_corr);
#endif

  /* not cataloged data */
  if ((sd->lat == 0.0) && (sd->lon == 0.0) && (sd->h == 0))
    return (1);

  return (0);
}

/*
  In case of 
    polarity UP   -->  1
    polarity DOWN --> -1
    unkown        -->  0
    invalid       --> -2
    */
static int
winpick_station_pol(char buf[], int *pol)
{
  int   status;

  status = 0;

  if (buf[0] == 'U')
    *pol = 1;
  else if (buf[0] == 'D')
    *pol = -1;
  else if (buf[0] == '.')
    *pol = 0;
  else {
    *pol = -2;
    status = 1;
  }

  return (status);
}

/***** Global functions *****/
void
win_pickfile_close(struct win_pickfile *wp)
{

  if (wp->sta_num > 0)
    FREE(wp->station);
}

int
win_pickfile_read(char *pickname, struct win_pickfile *wp)
{
  static struct wp_buf  wbuf;
  char  *ptr;
  int   scount;
  int   status;
  int  i;

  /* get number of stations */
  if ((wp->sta_num = get_wpbuf(pickname, &wbuf)) < 0)
    return (1);

  /* get pickfile name */
  if ((ptr = strrchr(pickname, '/')) != NULL)
    ptr++;
  else
    ptr = pickname;
  if (strlen(ptr) > FILENAME_MAX - 1) {
    (void)fprintf(stderr, "Buffer overflow!\n");
    free_wpbuf(&wbuf);
    return (1);
  }
  (void)strncpy(wp->pick_name, ptr, strlen(ptr)+1);

  /* trg file name & label & picker's name */
  (void)sscanf(wbuf.buf[0], "%*s%s%s%s", wp->trg_name, wp->label, wp->picker);

  if (wbuf.s_pos == 0) {
    /* In case of there is no phase pick */
    wp->hypo_flag = FLAG_NOHYPO;
  } else {
    if ((wp->station = CALLOC(struct station_data, wp->sta_num)) == NULL) {
      (void)fprintf(stderr, "win_pickfile_read\n");
      free_wpbuf(&wbuf);
      return (1);
    }

    (void)sscanf(wbuf.buf[wbuf.s_pos], "%*s %d/%d/%d %d:%d",
		 &wp->base_time[0], &wp->base_time[1], &wp->base_time[2],
		 &wp->base_time[3], &wp->base_time[4]);
    wp->base_time[5] = 0;
    
    /* read pick data of each station */
    scount = 0;
    for (i = 0; i < wp->sta_num; ++i) {
      status =
	win_pickfile_station_data(wbuf.buf[wbuf.s_pos + 1 + i],
				  &wp->station[scount]);
      /*  (void)fprintf(stderr, "  %d pick file: %s\n", status, pickname); */
      if (status < 0) {
	(void)fprintf(stderr, "Invalid pick file: %s\n", pickname);
	FREE(wp->station);
	free_wpbuf(&wbuf);
	return (1);
      }
      else if (status > 0) {
	(void)fprintf(stderr, "not cataloged data in %s : %s\n",
		      pickname, wp->station[scount].code);
	continue;
      }
      scount++;   /* increment buf */
    }
    wp->sta_num = scount;

    /* read hypocenter data and final data of each station */
    if (wbuf.f_pos == 0)
      wp->hypo_flag = FLAG_NOHYPO;
    else {
      wp->hypo_flag = FLAG_HYPO;
      if (win_pickfile_hypo_data(&wbuf, &wp->hypo)) {
	(void)fprintf(stderr, "Invalid pick file: %s\n", pickname);
	FREE(wp->station);
	free_wpbuf(&wbuf);
	return (1);
      }
      /* final data */
      for (i = 0; i < wp->sta_num; ++i) {
	if (win_pickfile_final_data(wbuf.buf[wbuf.f_pos + 5 + i],
				    &wp->station[i])) {
	  (void)fprintf(stderr, "Invalid pick file: %s\n", pickname);
	  FREE(wp->station);
	  free_wpbuf(&wbuf);
	  return (1);
	}
      }
    } /* if (wbuf.f_pos == 0) */
  } /* if (wbuf.s_pos == 0) */

#if DEBUG
  (void)fprintf(stderr, "------- %s -------\n", wp->pick_name);
  (void)fprintf(stderr, "%s %s %s\n", wp->trg_name, wp->label, wp->picker);
  (void)fprintf(stderr, "n_station=%d\n", wp->sta_num);
  (void)fprintf(stderr, "base_time= %02d%02d%02d.%02d%02d%02d\n",
		wp->base_time[0], wp->base_time[1], wp->base_time[2],
		wp->base_time[3], wp->base_time[4], wp->base_time[5]);
#endif

  free_wpbuf(&wbuf);
  return (0);
}
