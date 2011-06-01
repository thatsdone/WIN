/* $Id: w_struct.h,v 1.2 2011/06/01 11:09:22 uehira Exp $ */

#ifndef _WSTRUCT_H_
#define _WSTRUCT_H_

/* structure of shared memory */
struct Shm {
  size_t  p;         /* write point */
  size_t  pl;        /* write limit */
  size_t  r;         /* latest */
  unsigned long  c;  /* counter */
  uint8_w  d[1];     /* data buffer */
};

/* structure of binary hypo file (28 bytes / event) */
struct FinalB {
  int8_w time[8];  /* Y,M,D,h,m,s,s10,mag10 (in binary, not in BCD) */
  float  alat;     /* latitude */
  float  along;    /* longitude */
  float  dep;      /* depth */
  char   diag[4];  /* label */
  char   owner[4]; /* picker name */
};

/* channel table */
struct channel_tbl {
  WIN_ch  sysch;                  /* channel number [2byte hex] */
  int     flag;                   /* record flag */
  int     delay;                  /* line delay [ms] */
  char    name[WIN_STANAME_LEN];  /* station name */
  char    comp[WIN_STACOMP_LEN];  /* component name */
  int     scale;                  /* display scale */
  char    bit[256];                    /* bit value of A/D converter */
  double  sens;                   /* sensitivity of instruments */
  char    unit[256];              /* unit of sensitivity */
  double  t0;                     /* natural period [s] */
  double  h;                      /* damping factor */
  double  gain;                   /* gain [dB] */
  double  adc;                    /* [V/LSB] */
  double  lat;                    /* latitude of station */
  double  lng;                    /* longitude of station */
  double  higt;                   /* height of station */
  double  stcp;                   /* station correction of P phase */
  double  stcs;                   /* station correction of P phase */
};

#endif  /* !_WSTRUCT_H_*/
