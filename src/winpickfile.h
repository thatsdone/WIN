/* $Id: winpickfile.h,v 1.1.6.1 2010/12/28 12:55:44 uehira Exp $ */

/*
 * Copyright (c) 2001-2010
 *   Uehira Kenji / All Rights Reserved.
 */

#ifndef _WIN_PICKFILE_H_
#define _WIN_PICKFILE_H_

#include <sys/types.h>

#include <stdio.h>

#define FNAME_MAX 1024

#define FLAG_HYPO    1  /* in case of hypocenter was calculated */
#define FLAG_NOHYPO  0

#define WIN_YEAR_BASE  WIN_YEAR

#define USERNAME_MAX 100
#define WIN_STACODE_MAX  10
#define WIN_STACODE_MAX_S  4

#define WIN_STACODE_BUF  WIN_STANAME_LEN  /* station code(10 char+1) */
#define WIN_COMP_BUF  WIN_STACOMP_LEN     /* component(6 char+1) */
#define WIN_LABEL_BUF  WIN_LABEL_LEN      /* label(18 char+1) */

#define WIN_NOISE_LABEL     "NOISE"

#define WINPICK_P_PREFIX    "#p"
#define WINPICK_S_PREFIX    "#s"
#define WINPICK_F_PREFIX    "#f"

#define WINPICK_S_LEN_S   82    /* in case of short station code */
#define WINPICK_S_LEN_S_SC   96    /* in case of short station code + sc */
#define WINPICK_S_LEN     88    /* in case of long station code */
#define WINPICK_S_LEN_SC 102    /* in case of long station code + sc */

#define WINPICK_F_LEN_S  84  /* in case of short station code */
#define WINPICK_F_LEN    98  /* in case of long station code */


/* Column position of station record */
#define WPS_CODE  3

#define WPS_POL  14
#define WPS_P_ARRIV  15
#define WPS_P_ERR  23
#define WPS_S_ARRIV  29
#define WPS_S_ERR  37
#define WPS_FP  43
#define WPS_AMP 49
#define WPS_LAT 58
#define WPS_LON 69
#define WPS_H 80
#define WPS_P_CORR  87
#define WPS_S_CORR  94

#define WPS_POL_S  8
#define WPS_P_ARRIV_S  9
#define WPS_P_ERR_S  17
#define WPS_S_ARRIV_S  23
#define WPS_S_ERR_S  31
#define WPS_FP_S  37
#define WPS_AMP_S  43
#define WPS_LAT_S  52
#define WPS_LON_S  63
#define WPS_H_S  74
#define WPS_P_CORR_S 81
#define WPS_S_CORR_S 88

/* Column position of hypocenter record */
#define WPH_YR   3
#define WPH_MON  6
#define WPH_DAY  9
#define WPH_HOUR 15
#define WPH_MIN  18
#define WPH_SEC  21
#define WPH_LAT  29
#define WPH_LON  40
#define WPH_DEP  51
#define WPH_MAG  59
#define WPH_DT   WPH_SEC
#define WPH_DX   WPH_LAT
#define WPH_DY   WPH_LON
#define WPH_DZ   WPH_DEP
#define WPH_H00   3
#define WPH_H01  13
#define WPH_H02  23
#define WPH_H11  33
#define WPH_H12  43
#define WPH_H22  53
#define WPH_I_LAT     15
#define WPH_I_LAT_UCR 23
#define WPH_I_LON     29
#define WPH_I_LON_UCR 37
#define WPH_I_DEP     43
#define WPH_I_DEP_UCR 51

/* Column position of final record */
#define WPF_EPI  16
#define WPF_AZM  24
#define WPF_TKO  30
#define WPF_INC  36
#define WPF_P_ARRIV  42
#define WPF_P_ERR  49
#define WPF_POC  55
#define WPF_S_ARRIV  62
#define WPF_S_ERR  69
#define WPF_SOC  75
#define WPF_AMPF  83
#define WPF_MAG  92

#define WPF_EPI_S  10
#define WPF_AZM_S  16
#define WPF_TKO_S  22
#define WPF_INC_S  28
#define WPF_P_ARRIV_S  34
#define WPF_P_ERR_S  40
#define WPF_POC_S  45
#define WPF_S_ARRIV_S  51
#define WPF_S_ERR_S  57
#define WPF_SOC_S  62
#define WPF_AMPF_S  69
#define WPF_MAG_S  78

/*** define struct ***/
/* struct of each station data */
struct station_data {
  char  code[WIN_STACODE_BUF];
  int   polarity;
  double  p_arrival, p_corr, p_error, p_arr_cor, p_w_error;
  double  s_arrival, s_corr, s_error, s_arr_cor, s_w_error;
  double  fp_time;
  double  amp_arrival, amp, amp_f;
  double  lat, lon;
  short   h;
  int  amp_unit;
  double  epi_dist, azimuth, takeoff_ang, incidnt_ang, p_oc, s_oc, mag;
};

/* struct of hypocenter data */
struct hypo_data {
  int  ot[6];  /* origin time */
  int  bot[5];  /* origin time by base-time */
  time_t  ott;
  double  sec;
  double  bosec;  /* origin time by base-time */
  double  lat, lon, dep, mag;  /* position & magnitude */
  double dt, dx, dy, dz;  /* error */
  double h00, h01, h02, h11, h12, h22;
  double i_lat, i_lon, i_dep;  /* init. position */
  double i_lat_ucr, i_lon_ucr, i_dep_ucr;  /* uncertainty of init. postition */
  int  num_sta, num_P, num_S;  /* number of staion, P & S phase */
  double  sd_P, sd_S;  /* standard deviation */
  double  init_P, init_S, init_dep;   /* dependency of initial hypocenter 0-100% */
  char  structname[4];
  char  judge[5];  /* hypomh judge */
};

/* struct of pick_file data of win system */
struct win_pickfile {
  char  pick_name[FNAME_MAX]; /* pick file name */
  char  trg_name[FNAME_MAX];  /* trg file name */
  char  label[WIN_LABEL_BUF];    /* label */
  char  picker[USERNAME_MAX];    /* picker's name */
  int   hypo_flag;
  int   sta_num;
  int   base_time[6];  /* base time for station data */
  struct station_data  *station;  /* each station data */
  struct hypo_data  hypo;  /* hypocenter info. */
};

/*** prototypes of function ***/
void win_pickfile_close(struct win_pickfile *);
int win_pickfile_read(char *, struct win_pickfile *);

#endif  /* _WIN_PICKFILE_H_ */
