/*-
  $Id: hypomhc.c,v 1.6.2.3 2009/01/05 14:55:54 uehira Exp $
   hypomhc.c    : main program for hypocenter location
     original version was made on March 13, 1984 and
     modified by N.H. on Feb. 8, 1985, May 8, 1985.

********** MS-DOS VERSION / TRANSPLANTED BY T.U. 5/27/85 **********
          INCIDENT ANGLES TO STATIONS ARE OUTPUTTED

********** micro VAX / VMS VERSION                8/10/88 **********
********** NEWS VERSION                           6/23/90 **********
       F-P MAGNITUDE IS CALCULATED IN FINAL       1/ 9/91
       MAXIMUM N OF STATIONS IS 100               6/12/91
       AMPLITUDE MAGNITUDE IS CALCULATED IN FINAL 6/30/92
       BUG IN SDATA (read station height) FIXED   7/28/93
       Translate to C language (by Uehira Kenji)  4/ 8/97
       fixed format of 'year' in format#2200 in sub final()  11/19/2001
       TRAVEL-TIME CALCULATION MODE               5/24/2003
       bug fixed by Honda/Nagai for initialization of IYEAR 6/9/2003

     HYPOCENTER LOCATION USING A BAYESIAN APPROACH DEVELOPED BY
        MATSU'URA (1984): PROGRAMED BY MATSU'URA AND HIRATA ON
        MARCH 13, 1984: GEOPHYSICAL INSTITUTE, FACULTY OF SCIENCE,
        THE UNIVERSITY OF TOKYO, TOKYO, JAPAN.

     INPUT:  FT11F001 - VELOCITY STRUCTURE
             FT13F001 - ARRIVAL TIME DATA
     OUTPUT: FT21F001 - STRUCTURE AND INTERIM REPORT
             FT22F001 - FINAL RESULTS

     NUMBER OF CHARACTERS IN A STATION NAME IS CHANGED TO 6 FROM 2
                                             2/8/1985 N.H.
*/
/* #define    DEBUG   0 */
#define    DEBUGS  0

#define    CHK_RSLT 0

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include   <stdio.h>
#include   <stdlib.h>
#include   <string.h>
#include   <unistd.h>
#include   <math.h>

#if (defined(__FreeBSD__) && (__FreeBSD__ < 4))
#include <floatingpoint.h>
#endif

#include "subst_func.h"

#define    VPVS2   3.0		/* (Vp/Vs)^2 */
#define    VPVS    (sqrt(VPVS2))/* Vp/Vs */

#define    MIN(a,b) (a)<(b) ? (a) : (b)
#define    MULT2(x) ((x)*(x))

#define    PI      3.14159265358979323846
#define    P2      1.57079632679489661923
#define    PD      ((180.0)/(PI))
#define    EPS1    1.0e-8
#define    EPS2    1.0e-14
#define    LIT1    30
#define    LIT2    10
#if CHK_RSLT
#define    FACTOR   3
#endif

#define    LINELEN 256

struct struct_data {
  int		  n1;		/* Number of Layer */
  double         *y;		/* Depth of top of each layer */
  double         *vr;		/* P-wave velocity of each layer */
  double         *v;
  double         *vlg;
};
typedef struct struct_data STRUCT;

struct station_data {
  char		  sa1     [11];	/* STATION ABBREVIATION   (WITHIN 10
				 * CHARACTERS) */
  char		  pola1   [2];	/* POLARITY OF THE FIRST MOTION                 */
  double	  pt1;		/* P-PHASE ARRIVAL TIME             (IN SECOND) */
  double	  st1;		/* S-PHASE ARRIVAL TIME             (IN SECOND) */
  double	  pe1;		/* STANDARD ERROR IN P-ARRIVAL DATA (IN SECOND) */
  double	  se1;		/* STANDARD ERROR IN S-ARRIVAL DATA (IN SECOND) */
  double	  fmp;		/* F-P TIME DATA (IN SECOND, 0.0 FOR NO USE)    */
  double	  amp;		/* MAXIMUM AMPLITUDE DATA (IN m/s, 0.0 FOR NO
				 * USE) */
  double	  alat;		/* Latitude of Station */
  double	  alng;		/* Longitude of Station */
  double	  ahgt;		/* Height of station (in KM) */
  double	  stcp;		/* Station correction of P-wave (in SEC) */
  double	  stcs;		/* Station correction of S-wave (in SEC) */
  double	  xst   , yst;
  int		  flag;		/* structure flag 1=special */
};
typedef struct station_data STATION;

struct station_for_calc_data {
  int		  org_num;
  char		  sa      [11];
  double	  sc     [3];
  double	  pt    , st;
  double	  pe    , se;
  double	  apt   , ast;
  double	  vpt   , vst;
  double	  vps;
  double         *fp, *fs;
  double	  tpt   , tag, tbg;
  double	  a      [3];
  double	  rpt   , rst;
  double	  tw;
  double	  cp     [3], cs[3];
  double	  dl    , az, ta, tb, bmag;
  char		  pola    [2];
  int		  flag;		/* structure flag */
};
typedef struct station_for_calc_data FOR_CALC;

str2double(t, n, m, d)
  char           *t;
  int		  n       , m;
  double         *d;
{
  char           *tb;

  if (strlen(t) < n + m)
    *d = 9999.0;
  else {
    if (NULL == (tb = (char *)malloc(sizeof(char) * (m + 1)))) {
      fputs("hypomhc : Allocation failure !!\n", stderr);
      exit(0);
    }
    strncpy(tb, t + n, m);
    tb[m] = '\0';
    *d = atof(tb);
    free(tb);
  }
}

/* PLTXY TRANSFORMS (X,Y) TO (ALAT,ALONG) IF IND.EQ.1  */
/* PLTXY TRANSFORMS (ALAT,ALONG) TO (X,Y) IF IND.EQ.0  */
pltxy(alt0, alng0, alat, along, x, y, ind)
  double	  alt0  , alng0, *alat, *along, *x, *y;
  int		  ind;
{
  static double	  a = 6.378160e3, e2 = 6.6944541e-3, e12 = 6.7395719e-3,
  		  d = 5.72958e1;
  double	  rd    , rlat, slat, clat, v2, al, ph1, rph1, rph2, an, c1, c2,
  		  rlato      , slato, tphi1, cphi1, r;

  rd = 1.0e0 / d;
  if (ind == 0) {
    rlat = rd * (*alat);
    slat = sin(rlat);
    clat = cos(rlat);
    v2 = 1.0e0 + e12 * clat * clat;
    al = (*along) - alng0;
    ph1 = (*alat) + (v2 * al * al * slat * clat) / (2.0e0 * d);
    rph1 = ph1 * rd;
    rph2 = (ph1 + alt0) * 0.5e0 * rd;
    r = a * (1.0e0 - e2) / sqrt(pow(1.0e0 - e2 * pow(sin(rph2), 2.0), 3.0));
    an = a / sqrt(1.0e0 - e2 * pow(sin(rph1), 2.0));
    c1 = d / r;
    c2 = d / an;
    *y = (ph1 - alt0) / c1;
    *x = (al * clat) / c2 + (al * al * al * clat * cos(2.0e0 * rlat)) / (6.0e0 * c2 * d * d);
  } else {
    rlato = alt0 * rd;
    slato = sin(rlato);
    r = a * (1.0e0 - e2) / sqrt(pow(1.0e0 - e2 * slato * slato, 3.0));
    an = a / sqrt(1.0e0 - e2 * slato * slato);
    v2 = 1.0e0 + e12 * pow(cos(rlato), 2.0);
    c1 = d / r;
    c2 = d / an;
    ph1 = alt0 + c1 * (*y);
    rph1 = ph1 * rd;
    tphi1 = tan(rph1);
    cphi1 = cos(rph1);
    *alat = ph1 - (c2 * (*x)) * (c2 * (*x)) * v2 * tphi1 / (2.0e0 * d);
    *along = alng0 + c2 * (*x) / cphi1 - pow(c2 * (*x), 3.0) *
      (1.0e0 + 2.0e0 * tphi1 * tphi1) / (6.0e0 * d * d * cphi1);
  }
}

/*-
   SEARCH FOR THE LAYER NUMBER
   n1 : Layer Number
   y  : Top depth of each layer (array)
   yy : Depth
   *ln : Layer number of the given depth
*/
where(strc, yy, ln)
  int            *ln;
  double	  yy;
  STRUCT         *strc;
{
  int		  i;

  for (i = 0; i < strc->n1; ++i)
    if (strc->y[i + 1] >= yy)
      break;
  *ln = i;
}

/*-
   IF ID=0, Y1 TO Y2 (Y1.GT.Y2)
   IF ID=1, Y2 TO Y1 (Y2.GT.Y1)
*/
rxinc(id, pp, y1, l1, y2, l2, x, a, strc)
  int		  id      , l1, l2;
  double	  pp    , y1, y2, *x, *a;
  STRUCT         *strc;
{
  int		  k1      , lm, i, j;
  double         *sn, *cn, *rn;

  *x = *a = 0.0;
  if (y1 == y2)
    return;

  if (NULL == (sn = (double *)malloc(sizeof(double) * (strc->n1 + 1)))) {
    fputs("hypomhc : Allocation failure !!\n", stderr);
    exit(0);
  }
  if (NULL == (cn = (double *)malloc(sizeof(double) * (strc->n1 + 1)))) {
    fputs("hypomhc : Allocation failure !!\n", stderr);
    exit(0);
  }
  if (NULL == (rn = (double *)malloc(sizeof(double) * (strc->n1 + 1)))) {
    fputs("hypomhc : Allocation failure !!\n", stderr);
    exit(0);
  }
  k1 = l2 - 1;
  if (id == 0)
    lm = l1 + 1;
  else if (id == 1)
    lm = l2 + 1;
  /* printf("k1=%d  l1=%d\n",k1,l1); OK */
  for (i = k1; i <= l1; ++i) {
    j = i + 1;
    if (i == l1)
      sn[j] = pp * strc->vlg[l1] * (y1 + strc->v[l1]);
    else if (i == k1)
      sn[j] = pp * strc->vlg[l2] * (y2 + strc->v[l2]);
    else
      sn[j] = pp * strc->vr[j];
    if (sn[j] > 1.0)
      sn[j] = 1.0;
    cn[j] = sqrt(1.0 - MULT2(sn[j]));
  }
  for (i = k1; i <= l1; ++i) {
    j = i + 1;
    if (i == l1) {
      if (id == 1)
	rn[j] = 0.0;
      else			/* i.e. id=0 */
	rn[j] = 1.0;
    } else if (i == k1) {
      if (id == 1)
	rn[j] = 1.0;
      else			/* i.e. id=0 */
	rn[j] = cn[lm] / cn[j];
    } else
      rn[j] = cn[lm] / cn[j];
  }
  for (i = l2; i <= l1; ++i) {
    j = i + 1;
    *x += (cn[i] - cn[j]) / strc->vlg[i];
    *a += (rn[i] - rn[j]) / strc->vlg[i];
  }
  *x /= pp;
  *a /= -(pp * sn[lm]);
  free(sn);
  free(cn);
  free(rn);
}

rtinc(pp, y1, l1, y2, l2, tt, strc)
  double	  pp    , y1, y2, *tt;
  int		  l1      , l2;
  STRUCT         *strc;
{
  double         *sn, *cn, cnr;
  int		  k1      , i, j;

  *tt = 0.0;
  if (y1 == y2)
    return;

  if (NULL == (sn = (double *)malloc(sizeof(double) * (strc->n1 + 1)))) {
    fputs("hypomhc : Allocation failure !!\n", stderr);
    exit(0);
  }
  if (NULL == (cn = (double *)malloc(sizeof(double) * (strc->n1 + 1)))) {
    fputs("hypomhc : Allocation failure !!\n", stderr);
    exit(0);
  }
  k1 = l2 - 1;
  for (i = k1; i <= l1; ++i) {
    j = i + 1;
    if (i == l1)
      sn[j] = pp * strc->vlg[l1] * (y1 + strc->v[l1]);
    else if (i == k1)
      sn[j] = pp * strc->vlg[l2] * (y2 + strc->v[l2]);
    else
      sn[j] = pp * strc->vr[j];
    if (fabs(sn[j]) > 1.0)
      sn[j] = 1.0;
#if DEBUG
    /*
     * printf("%d %lf %lf %lf %lf %lf %lf %lf %d %d\n",
     * j,sn[j],pp,strc->vlg[l1],strc->vlg[l2],y1,y2,strc->vr[j],l1,l2);
     */
#endif
    cn[j] = sqrt(1.0 - MULT2(sn[j]));
  }
  for (i = l2; i <= l1; ++i) {
    cnr = (1.0 - cn[i + 1]) * (1.0 + cn[i]) / ((1.0 + cn[i + 1]) * (1.0 - cn[i]));
    *tt += log(cnr) / 2.0 / strc->vlg[i];
#if DEBUG
    printf("i=%d tt=%.14lf cnr=%.14lf cn[i+1]=%.14lf\n", i, *tt, cnr, cn[i + 1]);
#endif
  }

  free(sn);
  free(cn);
}

rpcod(y1, l1, y2, l2, xc, tac, strc)
  double	  y1    , y2, *xc, *tac;
  int		  l1      , l2;
  STRUCT         *strc;
{
  int		  i       , j;
  double	  pp    , xa, xb, sc, a, b;

  xc[l1] = 0.0;
  tac[l1] = PI;
  /* printf("l1=%d  n1=%d\n",l1,strc->n1);  OK */
  for (i = l1; i < strc->n1; ++i) {
    j = i + 1;
    pp = 1.0 / strc->vr[j];
    rxinc(0, pp, y1, l1, y2, l2, &xa, &a, strc);
    rxinc(1, pp, strc->y[j], i, y1, l1, &xb, &b, strc);
    xc[j] = xa + 2.0 * xb;
    sc = pp * strc->vlg[l1] * (y1 + strc->v[l1]);
    if (sc > 1.0)
      sc = 1.0;
    tac[j] = asin(sc);
  }
}

rxcod(y1, l1, y2, l2, ta, x, a, strc)
  double	  y1    , y2, ta, *x, *a;
  int		  l1      , l2;
  STRUCT         *strc;
{
  double	  pp    , pn, xd, b, yn;
  int		  nl;

  pp = sin(ta) / (strc->vlg[l1] * (y1 + strc->v[l1]));
  rxinc(0, pp, y1, l1, y2, l2, x, a, strc);
  if (ta >= P2) {		/* Not ta<P2 */
    *a = -*a;
    return;
  }
  pn = 1.0 / pp;
  for (nl = 0; nl < strc->n1; ++nl) {
    if (strc->vr[nl + 1] >= pn)
      break;
  }
  yn = pn / strc->vlg[nl] - strc->v[nl];
  rxinc(1, pp, yn, nl, y1, l1, &xd, &b, strc);
  *x += 2.0 * xd;
  *a += 2.0 * b;
}

/* COMPUTATION OF TRAVEL TIME */
trvel(y1, l1, y2, l2, ta, tt, strc)
  double	  y1    , y2, ta, *tt;
  int		  l1      , l2;
  STRUCT         *strc;
{
  double	  pp    , pn, yn, td;
  int		  nl;

  pp = sin(ta) / (strc->vlg[l1] * (y1 + strc->v[l1]));
  rtinc(pp, y1, l1, y2, l2, tt, strc);
  if (ta >= P2)
    return;
  pn = 1.0 / pp;
  for (nl = 0; nl < strc->n1; ++nl) {
    if (strc->vr[nl + 1] >= pn)
      break;
  }
  yn = pn / strc->vlg[nl] - strc->v[nl];
  rtinc(pp, yn, nl, y1, l1, &td, strc);
  *tt += 2.0 * td;
}

/*-
   CALCULATION OF TAKE-OFF ANGLES AND TRAVEL TIMES

   JULY-31-84. REVISED BY N.H.

   SEARCH FOR RAY PATH

   RR  : HORIZONTAL DISTANCE BETWEEN TWO POINTS
   YA  : DEPTH OF POINT 'A'
   YB  : DEPTH OF POINT 'B'
   NP  : NUMBER OF POSSIBLE RAYS BETWEEN 'A' AND 'B'
   ANG : TAKE-OFF ANGLES FROM DWONWARD
   TRV : TRAVEL TIMES
   BNG : INCIDENT ANGLES FROM DOWNWARD
*/
travel(rr, ya, yb, np, ang, trv, bng, strc)
  double	  rr    , ya, yb, *ang, *trv, *bng;
  int            *np;
  STRUCT         *strc;
{
  double	  y1    , y2;
  double         *xc, *tac;
  int		  l1      , l2;
  int		  i       , j, k;
  double	  t1    , t2, x1, x2, ta1, ta2, xr, ta0, t0, x0, a0, dt0, dta, tag;
  double	  dtc   , sang;

  /* First, malloc */
  if (NULL == (xc = (double *)malloc(sizeof(double) * (strc->n1 + 1)))) {
    fputs("hypomhc : Allocation failure !!\n", stderr);
    exit(0);
  }
  if (NULL == (tac = (double *)malloc(sizeof(double) * (strc->n1 + 1)))) {
    fputs("hypomhc : Allocation failure !!\n", stderr);
    exit(0);
  }
  /* begin */
  if (ya >= yb) {
    y1 = ya;
    y2 = yb;
  } else {
    y1 = yb;
    y2 = ya;
  }
  where(strc, y1, &l1);
  where(strc, y2, &l2);
  rpcod(y1, l1, y2, l2, xc, tac, strc);
  *np = 0;
  /* printf("l1=%d  n1=%d\n",l1,strc->n1); OK */
  for (i = l1; i < strc->n1; ++i) {
    j = i + 1;
    t1 = rr - xc[i];
    t2 = xc[j] - rr;
    if ((t1 * t2) < 0.0)
      continue;
    (*np)++;
    x1 = xc[i];
    x2 = xc[j];
    ta1 = tac[i];
    ta2 = tac[j];
    for (k = 0; k < LIT1; ++k) {
      xr = x1 - x2;
      if (fabs(xr / rr) < EPS1)
	break;
      ta0 = (ta1 + ta2) / 2.0;
      rxcod(y1, l1, y2, l2, ta0, &x0, &a0, strc);
      t0 = rr - x0;
      if ((t0 * t1) <= 0.0) {
	x2 = x0;
	ta2 = ta0;
      } else {
	x1 = x0;
	ta1 = ta0;
      }
    }
    dt0 = fabs((rr - x0) / rr);
    /* printf("y1=%lf heohei\n",y1); */
    for (k = 0; k < LIT2; ++k) {
      dta = (rr - x0) / a0;
      tag = ta0 + dta;
      if (fabs(dta) < EPS2)
	break;
      /* printf("y1=%lf\n",y1); */
      rxcod(y1, l1, y2, l2, tag, &x0, &a0, strc);
      dtc = fabs((rr - x0) / rr);
      if (dt0 <= dtc) {
	tag = ta0;
	break;
      } else {			/* dt0>dtc */
	if (dtc < EPS2)
	  goto line55;
      }
    }
line55:
    /* printf("y11=%lf\n",y1); */
    trvel(y1, l1, y2, l2, tag, &trv[*np - 1], strc);
    if (ya >= yb) {
      ang[*np - 1] = tag;
      sang = sin(tag) * strc->vlg[l2] * (y2 + strc->v[l2])
	/ strc->vlg[l1] / (y1 + strc->v[l1]);
      bng[*np - 1] = asin(sang);
    } else {
      sang = sin(tag) * strc->vlg[l2] * (y2 + strc->v[l2])
	/ strc->vlg[l1] / (y1 + strc->v[l1]);
      ang[*np - 1] = asin(sang);
      bng[*np - 1] = tag;
    }
  }
  free(xc);
  free(tac);
}

usage()
{
  fputs("Usage : hypomhc <STATION, STRUCTURE> <ARRIVAL TIME DATA> <FINAL RESULTS> <REPORT> (<INITIAL GUESS>)\n", stderr);
}

end_hypomhc(status)
  int		  status;
{
#if (defined(__FreeBSD__) && (__FreeBSD__ < 4))
  fpresetsticky(FP_X_DZ | FP_X_INV);
  fpsetmask(FP_X_DZ | FP_X_INV);
#endif
  exit(status);
}

memory_error()
{
  fputs("hypomhc : Allocation failure !!\n", stderr);
  end_hypomhc(2);
}

/****** Begin MAIN ******/
main(argc, argv)
  int		  argc;
  char          **argv;
{
  FILE           *fp_11, *fp_init, *fp_13, *fp_21, *fp_22;
  double	  alat00, alng00, dept00, elat00, elng00, edpt00;
  int		  iyear   , imont, iday, ihour, iminu;
  double	  secc  , amag00;
  double	  alat0 , alng0, dept0;
  double	  va    , vb, vlct;
  int		  nn;
  double         *th, eot, ex1[3];
  static STRUCT	  strc, strc1;
  double	  zmin  , zmax;
  double	  zmin1 , zmax1;
  char		  vst     [4], svst[4];
  int		  na      , npd, nsd, nd;
  int		  iyr     , mnt, idy, ihr, min;
  static STATION *sta;
  static FOR_CALC *calc;
  double	  a     , b, c, sot, svr, fat, otm, pe2, se2, var, cot, ptrv, strv;
  double	  xm0    [3], ex0[3];
  int		  ll      , lm, jm, judg;
  double	  ccp   , al2, alp, vxm[3], xm1[3], as;
  int		  jj      , ln, ln1, np, np1;
  double         *ang, *trv, *bng;
  double         *ang1, *trv1, *bng1;
  double	  sn    , cn, vre, srb, src, wpt, wst, bcp;
  double	  xw     [3], rmx, rvx[3], aa, sra;	/* sra=0.0 need? */
  double	  zm1   , zm2, xmc[3] /* shokika? */ , acp /* shokika? */ ;
  double	  dp     [3][3], ds[3][3], bb[3][3], h[3][3], cc[3][3], dtb;
  double	  srt   , r1, r2, r3, rsl[3], xmz;
  double	  alati , alngi, alatf, alngf;
  static char    *diag[] = {"CONV", "AIRF", "DEEP", "NOCN"};
  int		  npr = 3;	/* Why? */
  int		  nsrp    , nsrs, nmag;
  double	  srp   , srs, amag;
  double	  hdist;
  double	  fmag;
#if CHK_RSLT
  int		  icheck;
#endif
  int		  i       , j, k, l;
  char		  txtbuf  [LINELEN];
  double	  xt    , xx, yy, rr;

  int		  cflag = 0, sflag = 0, smode = 0;
  char		  sstrname[1024], schname[1024];
  FILE           *fp_sstr, *fp_sch;
  int		  sstanum;
  char          **ssta;

#if (defined(__FreeBSD__) && (__FreeBSD__ < 4))
  /* allow divide by zero -- Inf */
  fpsetmask(fpgetmask() & ~(FP_X_DZ | FP_X_INV));
#endif

  /* get option */
  while ((i = getopt(argc, argv, "c:hs:")) != -1) {
    switch (i) {
    case 'c':			/* special channels table */
      fp_sch = fopen(optarg, "r");
      if (fp_sch == NULL) {
	fprintf(stderr, "Cannot open FILE : %s\n", optarg);
	usage();
	end_hypomhc(1);
      }
      sstanum = 0;
      while (!feof(fp_sch)) {
	fgets(txtbuf, LINELEN, fp_sch);
	if (txtbuf[0] == '#')
	  continue;
	sstanum++;
      }
      sstanum--;
      if (sstanum < 0)
	sstanum = 0;
      ssta = (char **)calloc(sstanum, sizeof(char *));
      for (j = 0; j < sstanum; ++j)
	ssta[j] = (char *)calloc(11, sizeof(char));
      rewind(fp_sch);
      j = 0;
      while (!feof(fp_sch)) {
	fgets(txtbuf, LINELEN, fp_sch);
	if (txtbuf[0] == '#')
	  continue;
	sscanf(txtbuf, "%10s", ssta[j]);
	ssta[j][10] = '\0';
	j++;
	if (j == sstanum)
	  break;
      }
#if DEBUGS
      printf("sstanum=%d\n", sstanum);
      for (j = 0; j < sstanum; ++j)
	printf("SSta %s.\n", ssta[j]);
#endif
      cflag = 1;
      strncpy(schname, optarg, sizeof(schname));
      break;
    case 's':			/* special structure */
      fp_sstr = fopen(optarg, "r");
      if (fp_sstr == NULL) {
	fprintf(stderr, "Cannot open FILE : %s\n", optarg);
	usage();
	end_hypomhc(1);
      }
      sflag = 1;
      strncpy(sstrname, optarg, sizeof(sstrname));
      break;
    case 'h':
    default:
      usage();
      end_hypomhc(0);
      /* NOTREACHED */
    }
  }
  argc -= (optind - 1);
  argv += (optind - 1);
  if ((cflag == 1) && (sflag == 1))
    smode = 1;

  sra = acp = xmc[0] = xmc[1] = xmc[2] = 0.0;

  printf("************ HYPOMH ***********\n");

  if (!(argc == 5 || argc == 6)) {
    printf("****** END OF HYPOMH ******\n");
    usage();
    end_hypomhc(0);
  }
  /***** Open files (Subroutine FOPEN) *****/
  if (NULL == (fp_11 = fopen(argv[1], "r"))) {
    fprintf(stderr, "Cannot open FILE : %s\n", argv[1]);
    usage();
    end_hypomhc(1);
  }
  if (NULL == (fp_13 = fopen(argv[2], "r"))) {
    fprintf(stderr, "Cannot open FILE : %s\n", argv[2]);
    usage();
    end_hypomhc(1);
  }
  if (NULL == (fp_22 = fopen(argv[3], "w"))) {
    fprintf(stderr, "Cannot open FILE : %s\n", argv[3]);
    usage();
    end_hypomhc(1);
  }
  if (NULL == (fp_21 = fopen(argv[4], "w"))) {
    fprintf(stderr, "Cannot open FILE : %s\n", argv[4]);
    usage();
    end_hypomhc(1);
  }
  iyear = -1;
  if (argc == 6) {
    if (NULL == (fp_init = fopen(argv[5], "r"))) {
      fprintf(stderr, "Cannot open FILE : %s\n", argv[5]);
      usage();
      end_hypomhc(1);
    }
    fgets(txtbuf, LINELEN, fp_init);
    sscanf(txtbuf, "%lf%lf%lf", &alat00, &alng00, &dept00);
    fgets(txtbuf, LINELEN, fp_init);
    sscanf(txtbuf, "%lf%lf%lf", &elat00, &elng00, &edpt00);
    /* If third line exists, enter 'travel-time calculation only' mode. */
    fgets(txtbuf, LINELEN, fp_init);
    if (sscanf(txtbuf, "%d%d%d%d%d%lf%lf%lf%lf%lf",
	       &iyear, &imont, &iday, &ihour, &iminu, &secc,
	       &alat00, &alng00, &dept00, &amag00) != 10)
      iyear = -1;
    fclose(fp_init);
#if DEBUG
    printf("alat00=%lf alng00=%lf dept00=%lf\n", alat00, alng00, dept00);
    printf("elat00=%lf elng00=%lf edpt00=%lf\n", elat00, elng00, edpt00);
    printf("iyear=%d\n", iyear);
#endif
  }
  fprintf(fp_21, " HYPOCENTER LOCATION USING A BAYESIAN APPROACH\n");
  fprintf(fp_21, " ---------------------------------------------\n");

  /****** Read Struct data *****/
  fgets(txtbuf, LINELEN, fp_11);
  sscanf(txtbuf, "%lf%lf%lf", &alat0, &alng0, &dept0);
  fgets(txtbuf, LINELEN, fp_11);
  sscanf(txtbuf, "%d%3s%lf%lf", &nn, vst, &va, &vb);
  vst[3] = '\0';
  strc.n1 = nn + 1;

  if (NULL == (strc.vr = (double *)malloc(sizeof(double) * (strc.n1 + 1))))
    memory_error();
  if (NULL == (strc.y = (double *)malloc(sizeof(double) * (strc.n1 + 1))))
    memory_error();
  if (NULL == (th = (double *)malloc(sizeof(double) * (strc.n1))))
    memory_error();
  if (NULL == (strc.vlg = (double *)malloc(sizeof(double) * (strc.n1))))
    memory_error();
  if (NULL == (strc.v = (double *)malloc(sizeof(double) * (strc.n1))))
    memory_error();
  /*-  This part is Original
     if(vb <= va)
       vlct = 0.0;
     else
       vlct = (vb-va)/log(vb/va);
  */
  vlct = 0.0;
  for (i = 0; i <= strc.n1; ++i)/* nn+2 */
    fscanf(fp_11, "%lf", strc.vr + i);	/* P-wave velocity of each layer */
  for (i = 0; i < strc.n1; ++i)	/* nn+1 */
    fscanf(fp_11, "%lf", th + i);	/* Thickness of each layer */
  fgets(txtbuf, LINELEN, fp_11);/* only for skip */
  fgets(txtbuf, LINELEN, fp_11);
  sscanf(txtbuf, "%lf%lf%lf%lf", &eot, ex1 + 1, ex1, ex1 + 2);
#if DEBUG
  printf("eot=%lf ex1[0]=%lf ex1[1]=%lf ex1[2]=%lf\n",
	 eot, ex1[0], ex1[1], ex1[2]);
#endif

  for (i = 0; i < strc.n1; ++i)
    strc.vlg[i] = (strc.vr[i + 1] - strc.vr[i]) / th[i];
  strc.y[0] = 0.0;
  for (i = 0; i < strc.n1; ++i)
    strc.y[i + 1] = strc.y[i] + th[i];
  strc.v[0] = strc.vr[0] / strc.vlg[0];
  for (i = 1; i < strc.n1; ++i)
    strc.v[i] =
      (strc.vlg[i - 1] * strc.v[i - 1] + (strc.vlg[i - 1] - strc.vlg[i]) * strc.y[i])
      / strc.vlg[i];
  zmin = -strc.vr[0] / strc.vlg[0] + 1.0e-1;
  zmax = strc.y[strc.n1] - 1.0e-1;
#if DEBUGS
  printf("%lf %lf\n", zmin, zmax);
#endif
  /* output report */
  fprintf(fp_21, " ***** VELOCITY STRUCTURE (%s) *****\n", vst);
  fprintf(fp_21, "         I    Y(I)      VR(I)     ALPHA(I)    VLG(I)\n");
  for (i = 0; i < strc.n1; ++i) {
    fprintf(fp_21, "     %5d%10.4lf%10.4lf\n", i, strc.y[i], strc.vr[i]);
    fprintf(fp_21, "                              %12.3lE%12.3lE\n",
	    strc.v[i], strc.vlg[i]);
  }
  fprintf(fp_21, "     %5d%10.4lf%10.4lf\n",
	  strc.n1, strc.y[strc.n1], strc.vr[strc.n1]);
  free(th);

  /****** Read Special Struct data *****/
  if (smode) {
    fgets(txtbuf, LINELEN, fp_sstr);
    /* sscanf(txtbuf, "%lf%lf%lf", &alat0,&alng0,&dept0); */
    fgets(txtbuf, LINELEN, fp_sstr);
    sscanf(txtbuf, "%d%s%lf%lf", &nn, svst, &va, &vb);
    svst[3] = '\0';
    strc1.n1 = nn + 1;

    if (NULL == (strc1.vr = (double *)malloc(sizeof(double) * (strc1.n1 + 1))))
      memory_error();
    if (NULL == (strc1.y = (double *)malloc(sizeof(double) * (strc1.n1 + 1))))
      memory_error();
    if (NULL == (th = (double *)malloc(sizeof(double) * (strc1.n1))))
      memory_error();
    if (NULL == (strc1.vlg = (double *)malloc(sizeof(double) * (strc1.n1))))
      memory_error();
    if (NULL == (strc1.v = (double *)malloc(sizeof(double) * (strc1.n1))))
      memory_error();
    /* vlct = 0.0; */
    for (i = 0; i <= strc1.n1; ++i)	/* nn+2 */
      fscanf(fp_sstr, "%lf", strc1.vr + i);	/* P-wave velocity of each
						 * layer */
    for (i = 0; i < strc1.n1; ++i)	/* nn+1 */
      fscanf(fp_sstr, "%lf", th + i);	/* Thickness of each layer */

    for (i = 0; i < strc1.n1; ++i)
      strc1.vlg[i] = (strc1.vr[i + 1] - strc1.vr[i]) / th[i];
    strc1.y[0] = 0.0;
    for (i = 0; i < strc1.n1; ++i)
      strc1.y[i + 1] = strc1.y[i] + th[i];
    strc1.v[0] = strc1.vr[0] / strc1.vlg[0];
    for (i = 1; i < strc1.n1; ++i)
      strc1.v[i] =
	(strc1.vlg[i - 1] * strc1.v[i - 1] + (strc1.vlg[i - 1] - strc1.vlg[i]) * strc1.y[i])
	/ strc1.vlg[i];
    zmin1 = -strc1.vr[0] / strc1.vlg[0] + 1.0e-1;
    zmax1 = strc1.y[strc1.n1] - 1.0e-1;
    zmin = zmin < zmin1 ? zmin1 : zmin;
    zmax = zmax > zmax1 ? zmax1 : zmax;
#if DEBUGS
    printf("%lf %lf\n", zmin1, zmax1);
#endif
    /* output report */
    fprintf(fp_21, " ***** SPECIAL VELOCITY STRUCTURE (%s) *****\n", svst);
    fprintf(fp_21, "         I    Y(I)      VR(I)     ALPHA(I)    VLG(I)\n");
    for (i = 0; i < strc1.n1; ++i) {
      fprintf(fp_21, "     %5d%10.4lf%10.4lf\n", i, strc1.y[i], strc1.vr[i]);
      fprintf(fp_21, "                              %12.3lE%12.3lE\n",
	      strc1.v[i], strc1.vlg[i]);
    }
    fprintf(fp_21, "     %5d%10.4lf%10.4lf\n",
	    strc1.n1, strc1.y[strc1.n1], strc1.vr[strc1.n1]);
    free(th);
  }				/* if (smode) */
#if DEBUGS
  printf("%lf %lf\n", zmin, zmax);
#endif

  /***** Read arrival time data (SUBROUTINE SDATA) *****/
  /*-
    C     READ ARRIVAL TIMES FROM FILE NO. 13 AND WRITE TO FILE NO. 21
    C     IF PTE(STE)=<0.0, NO INFORMATION ABOUT P(S) ARRIVAL TIME
    C
    C     NEQR     : ID NUMBER OF EARTHQUAKE
    C                NERQ=-1 MEANS END OF EARTHQUAKE IN FILE FT13FTXXX
    C     NDAT     : NUMBER OF DATA IN THE EARTHQUAKE
    C
    C     IYR1     : YEAR
    C     MNT1     : MONTH
    C     IDY1     : DAY
    C     IHR      : HOUR
    C     MIN      : MINUTE
    C
    C     SA1(I)   : STATION ABBREVIATION   (WITHIN 10 CHARACTERS)
    C     POLA1(I) : POLARITY OF THE FIRST MOTION
    C     PT1(I)   : P-PHASE ARRIVAL TIME             (IN SECOND)
    C     ST1(I)   : S-PHASE ARRIVAL TIME             (IN SECOND)
    C     PE1(I)   : STANDARD ERROR IN P-ARRIVAL DATA (IN SECOND)
    C     SE1(I)   : STANDARD ERROR IN S-ARRIVAL DATA (IN SECOND)
    C     FMP(I)   : F-P TIME DATA (IN SECOND, 0.0 FOR NO USE)
    C     AMP(I)   : MAXIMUM AMPLITUDE DATA (IN m/s, 0.0 FOR NO USE)
    C
    C     PE(I) AND SE(I) ARE RECALCULATED IN "START": 1 % OF TRAVEL TIMES
    C     ARE ADDED TO THE PE(I) AND SE(I).
  */
  na = 0;
  /* First, count number of stations */
  while (!feof(fp_13)) {
    fgets(txtbuf, LINELEN, fp_13);
    na++;
  }
#if DEBUG
  printf("na_dum = %d\n", na);
#endif
  if (na == 2)			/* if no data, exit. */
    goto HYPEND;
  rewind(fp_13);
  na -= 3;
  fgets(txtbuf, LINELEN, fp_13);
  sscanf(txtbuf, "%d/%d/%d %d:%d", &iyr, &mnt, &idy, &ihr, &min);
#if DEBUG
  printf("na = %d\n", na);
  printf("%02d %02d %02d %02d:%02d\n", iyr, mnt, idy, ihr, min);
#endif
  if (NULL == (sta = (STATION *) malloc(sizeof(STATION) * na)))
    memory_error();
  for (i = 0; i < na; ++i) {
    fgets(txtbuf, LINELEN, fp_13);
    sscanf(txtbuf, "%10s", sta[i].sa1);
    sta[i].sa1[10] = '\0';
    strncpy(sta[i].pola1, txtbuf + 11, 1);
    sta[i].pola1[1] = '\0';
    str2double(txtbuf, 12, 8, &sta[i].pt1);
    str2double(txtbuf, 20, 6, &sta[i].pe1);
    str2double(txtbuf, 26, 8, &sta[i].st1);
    str2double(txtbuf, 34, 6, &sta[i].se1);
    str2double(txtbuf, 40, 6, &xt);
    str2double(txtbuf, 46, 9, &sta[i].amp);
    str2double(txtbuf, 55, 11, &sta[i].alat);
    str2double(txtbuf, 66, 11, &sta[i].alng);
    str2double(txtbuf, 77, 7, &sta[i].ahgt);
    str2double(txtbuf, 84, 7, &sta[i].stcp);
    str2double(txtbuf, 91, 7, &sta[i].stcs);

    if (sta[i].stcp == 9999.0)
      sta[i].stcp = 0.0;
    if (sta[i].stcs == 9999.0)
      sta[i].stcs = 0.0;

    if (sta[i].pe1 > 0.0 && xt > sta[i].pt1)
      sta[i].fmp = xt - sta[i].pt1;
    else
      sta[i].fmp = 0.0;

    sta[i].ahgt *= 0.001;

    sta[i].flag = 0;
    if (smode) {
      for (j = 0; j < sstanum; ++j)
	if (strcmp(sta[i].sa1, ssta[j]) == 0)
	  sta[i].flag = 1;
    }
#if DEBUGS
    /* printf("%d\n", strlen(txtbuf)); */
    printf("%s %s %lf %lf %d\n",
	   sta[i].sa1, sta[i].pola1, sta[i].stcp, sta[i].stcs, sta[i].flag);
#endif
  }
  /* make data for calculate hypocenter */
  fprintf(fp_21, " ***** EARTHQUAKE %02d%02d%02d%02d%02d *****\n",
	  iyr, mnt, idy, ihr, min);
  k = npd = nsd = 0;
  /* if(NULL == (calc=(FOR_CALC *)malloc(sizeof(FOR_CALC)*na))) */
  if (NULL == (calc = (FOR_CALC *) calloc(na, sizeof(FOR_CALC))))
    memory_error();
  for (i = 0; i < na; ++i) {
    fprintf(fp_21,
	    "%-10s %s%8.3lf%6.3lf%8.3lf%6.3lf%6.1lf%10.3lE%11.5lf%11.5lf%7.3lf%7.3lf%7.3lf %d\n",
	    sta[i].sa1, sta[i].pola1, sta[i].pt1, sta[i].pe1,
	    sta[i].st1, sta[i].se1, sta[i].fmp, sta[i].amp,
	    sta[i].alat, sta[i].alng, sta[i].ahgt,
	    sta[i].stcp, sta[i].stcs, sta[i].flag);
    /* if(sta[i].alat==9999.0||sta[i].alng==9999.0||sta[i].ahgt==9.999){ */
    if ((sta[i].alat == 0.0) && (sta[i].alng == 0.0)) {
      fprintf(fp_21, "*** %s IS NOT CATALOGUED ***\n", sta[i].sa1);
      continue;
    }
    pltxy(alat0, alng0, &sta[i].alat, &sta[i].alng, &sta[i].xst, &sta[i].yst, 0);
    calc[k].org_num = i;
    strcpy(calc[k].sa, sta[i].sa1);
    calc[k].sc[0] = sta[i].xst;
    calc[k].sc[1] = -sta[i].yst;
    calc[k].sc[2] = -sta[i].ahgt - vlct * sta[i].stcp;
    if (sta[i].pe1 <= 0.0)
      calc[k].pt = 0.0;
    else {
      calc[k].pt = sta[i].pt1 + sta[i].stcp;
      npd++;
    }
    if (sta[i].se1 <= 0.0)
      calc[k].st = 0.0;
    else {
      calc[k].st = sta[i].st1 + sta[i].stcs;
      nsd++;
    }
    calc[k].pe = sta[i].pe1;
    calc[k].se = sta[i].se1;
    calc[k].apt = sta[i].pt1;
    strcpy(calc[k].pola, sta[i].pola1);
    calc[k].flag = sta[i].flag;
    k++;
  }
  nd = k;
#if DEBUG
  printf("npd=%d nsd=%d\n", npd, nsd);
#endif
  if (nd == 0)
    goto HYPEND;

  /***** START *****/
  /* a = 1.7320508; */
  a = VPVS;
  b = 1.0 / (a - 1.0);
  c = a * b;
  k = 0;
  sot = svr = 0.0;
  fat = 1.0e5;
  for (i = 0; i < nd; ++i) {
    if (calc[i].pe > 0.0)
      fat = MIN(fat, calc[i].pt);
    if (calc[i].se > 0.0)
      fat = MIN(fat, calc[i].st);
    if (calc[i].pe <= 0.0 || calc[i].se <= 0.0)
      continue;
    otm = b * (a * calc[i].pt - calc[i].st);
    pe2 = MULT2(calc[i].pe) + MULT2((calc[i].pt - otm) / 1.0e2);
    se2 = MULT2(calc[i].se) + MULT2((calc[i].st - otm) / 5.0e1);
    var = pe2 * c * c + se2 * b * b;
    sot += otm / var;
    svr += 1.0 / var;
    k++;
  }
  if (k == 0)
    cot = fat - 1.0;
  else
    cot = sot / svr;

  if (argc == 6) {
    pltxy(alat0, alng0, &alat00, &alng00, xm0, xm0 + 1, 0);
    xm0[1] = -xm0[1];
    xm0[2] = dept00;
    ex0[0] = elng00;
    ex0[1] = elat00;
    ex0[2] = edpt00;
  } else {			/* ie. argc==5 */
    xm0[0] = xm0[1] = 0.0;
    xm0[2] = dept0;
    for (i = 0; i < 3; ++i)
      ex0[i] = ex1[i];
  }
  fprintf(fp_21, " *** ADOPTED INITIAL VALUES *** X(KM)   Y(KM)  Z(KM)\n");
  fprintf(fp_21, "   0  %3d%3d%3d%3d%3d%8.3lf%8.3lf%8.3lf%8.3lf\n",
	  iyr, mnt, idy, ihr, min, cot, xm0[0], xm0[1], xm0[2]);
  fprintf(fp_21, "                             %8.3lf%8.3lf%8.3lf\n",
	  ex0[0], ex0[1], ex0[2]);
  /*-
    C     PE(I) AND SE(I) ARE RECALCULATED IN "START": 1 % OF TRAVEL TIMES
    C     ARE ADDED TO THE PE(I) AND SE(I).
  */
  for (i = 0; i < nd; ++i) {
    if (calc[i].pe > 0.0) {
      ptrv = (calc[i].pt - cot) / 1.0e2;
      /* calc[i].pe = sqrt(calc[i].pe*calc[i].pe+ptrv*ptrv); */
      calc[i].pe = hypot(calc[i].pe, ptrv);
    }
    if (calc[i].se > 0.0) {
      strv = (calc[i].st - cot) / 5.0e1;
      /* calc[i].se = sqrt(calc[i].se*calc[i].se+strv*strv); */
      calc[i].se = hypot(calc[i].se, strv);
    }
  }

  /***** NLINV *****/
  /*-
     judg=0 : NORMAL RETURN ( EQ.2-24 WAS SOLVED)
          1 : NOT SOLVED BECAUSE OF AIR-FOCUS
          2 : NOT SOLVED BECAUSE OF TOO DEEP FOCUS
          3 : NOT SOLVED; FAIL IN CONVERGENCE OF ITERATION
         10 : FATAL ERROR. CHECK INTERIM REPRT ON
              FT21F001. THIS MAY HAPPEN WHEN ERROR ON
              TRAVEL TIME CALCULATION BY "TRAVEL" WHICH
              IS USED IN "NLINV": I.E., "NO RAY PATH IS
              FOUND"
         11 : FATAL ERROR. ERROR IN "MXINV" : CHECK ELEMENTS
              OF MATRIX "bb"
  */
#if CHK_RSLT
line150:
#endif
  ll = 0;
  lm = 20;
  jm = 10;
  judg = 0;
  ccp = 1.0e-3;
  al2 = VPVS2;
  alp = VPVS;
  fprintf(fp_21, " ***** INVERSION OF ARRIVAL TIME DATA *****\n");
  fprintf(fp_21,
	  "  LL    SR                      CP                   JJ    X       Y       Z\n");
  for (i = 0; i < 3; ++i) {
    xm1[i] = xm0[i];
    vxm[i] = 1.0 / MULT2(ex0[i]);
  }
  as = 0.0;
  for (i = 0; i < nd; ++i) {
    if (calc[i].pe <= 0.0)
      calc[i].vpt = 0.0;
    else
      calc[i].vpt = 1.0 / MULT2(calc[i].pe);
    if (calc[i].se <= 0.0)
      calc[i].vst = 0.0;
    else
      calc[i].vst = 1.0 / MULT2(calc[i].se);
    calc[i].vps = calc[i].vpt + alp * calc[i].vst;
    as += calc[i].vpt + calc[i].vst;
  }
  for (i = 0; i < nd; ++i) {
    /* if(NULL == (calc[i].fp=(double *)malloc(sizeof(double)*nd))) */
    if (NULL == (calc[i].fp = (double *)calloc(nd, sizeof(double))))
      memory_error();
    /* if(NULL == (calc[i].fs=(double *)malloc(sizeof(double)*nd))) */
    if (NULL == (calc[i].fs = (double *)calloc(nd, sizeof(double))))
      memory_error();
  }
  for (i = 0; i < nd; ++i)
    for (j = 0; j < nd; ++j) {
      calc[i].fp[j] = -calc[i].vps / as;
      calc[i].fs[j] = calc[i].fp[j] / alp;
      if (i == j) {
	calc[i].fp[j] += 1.0;
	calc[i].fs[j] += 1.0;
      }
      calc[i].fp[j] *= calc[j].vpt;
      calc[i].fs[j] *= calc[j].vst;
    }
  if (NULL == (ang = (double *)malloc(sizeof(double) * (strc.n1 + 1))))
    memory_error();
  if (NULL == (trv = (double *)malloc(sizeof(double) * (strc.n1 + 1))))
    memory_error();
  if (NULL == (bng = (double *)malloc(sizeof(double) * (strc.n1 + 1))))
    memory_error();
  if (smode) {
    if (NULL == (ang1 = (double *)malloc(sizeof(double) * (strc1.n1 + 1))))
      memory_error();
    if (NULL == (trv1 = (double *)malloc(sizeof(double) * (strc1.n1 + 1))))
      memory_error();
    if (NULL == (bng1 = (double *)malloc(sizeof(double) * (strc1.n1 + 1))))
      memory_error();
  }
  while (1) {
    jj = 0;
line200:
    where(&strc, xm1[2], &ln);
    if (smode)
      where(&strc1, xm1[2], &ln1);
#if DEBUGS > 0
    fprintf(fp_21, "depth=%lf where=%d where1=%d\n", xm1[2], ln, ln1);
#endif
    for (i = 0; i < nd; ++i) {
      xx = xm1[0] - calc[i].sc[0];
      yy = xm1[1] - calc[i].sc[1];
      rr = hypot(xx, yy);	/* rr = sqrt(MULT2(xx)+MULT2(yy)); */
#if DEBUG
      printf("rr=%lf\n", rr);
      printf("xm1[0]=%lf  xm1[1]=%lf  xm1[2]= %lf\n", xm1[0], xm1[1], xm1[2]);
#endif
      /*
       * printf("xm1[0]=%lf xm1[1]=%lf xm1[2]=%lf calc[%d].sc[2]=%lf\n"
       * ,xm1[0],xm1[1],xm1[2],i,calc[i].sc[2]);
       */
      /*
       * printf("rr=%lf  xm1[2]=%lf  sc=%lf\n", rr,xm1[2],calc[i].sc[2]);
       */
      if (smode && calc[i].flag) {	/* special station */
	travel(rr, xm1[2], calc[i].sc[2], &np1, ang1, trv1, bng1, &strc1);
	if (np1 == 0) {
	  fprintf(fp_21,
		  " *** RAY PATH TO %d-STATION IS NOT FOUND ***\n", i);
	  judg = 10;
	  goto end_NLINV;
	}
      } else {
	travel(rr, xm1[2], calc[i].sc[2], &np, ang, trv, bng, &strc);
	if (np == 0) {
	  fprintf(fp_21,
		  " *** RAY PATH TO %d-STATION IS NOT FOUND ***\n", i);
	  judg = 10;
	  goto end_NLINV;
	}
      }
#if DEBUGS
      if (smode && calc[i].flag) {
	printf("np1=%d\n", np1);
	for (j = 0; j < np1; ++j)
	  printf("%d ang1=%lf trv1=%lf bng1=%lf\n",
		 j, ang1[j], trv1[j], bng1[j]);
      } else {
	printf("np=%d\n", np);
	for (j = 0; j < np; ++j)
	  printf("%d ang=%lf trv=%lf bng=%lf\n", j, ang[j], trv[j], bng[j]);
      }
#endif
      calc[i].tpt = 1.0e5;

      if (smode && calc[i].flag) {
	for (j = 0; j < np1; ++j) {
	  if (trv1[j] > calc[i].tpt)
	    continue;
	  else {
	    calc[i].tpt = trv1[j];
	    calc[i].tag = ang1[j];
	    calc[i].tbg = bng1[j];
	  }
	}
      } else {
	for (j = 0; j < np; ++j) {
	  if (trv[j] > calc[i].tpt)
	    continue;
	  else {
	    calc[i].tpt = trv[j];
	    calc[i].tag = ang[j];
	    calc[i].tbg = bng[j];
	  }
	}
      }

      sn = sin(calc[i].tag);
      cn = cos(calc[i].tag);
      if (smode && calc[i].flag)
	vre = strc1.vlg[ln1] * (xm1[2] + strc1.v[ln1]);
      else
	vre = strc.vlg[ln] * (xm1[2] + strc.v[ln]);
      /*
       * printf("tpt=%.11lf  tag=%.11lf  tbg=%.11lf  vre=%.13lf\n",
       * calc[i].tpt,calc[i].tag,calc[i].tbg,vre);
       */
      calc[i].a[0] = sn * xx / rr / vre;
      calc[i].a[1] = sn * yy / rr / vre;
      calc[i].a[2] = -cn / vre;
    }
    if (iyear >= 0) {
      for (i = 0; i < nd; ++i) {
	/* PT(I) and ST(I) contains STCP(I) and STCS(I), respectively. */
	calc[i].pe = calc[i].pt;
	calc[i].se = calc[i].st;
	calc[i].pt = secc + calc[i].tpt - calc[i].pt;
	calc[i].st = secc + alp * calc[i].tpt - calc[i].st;
	calc[i].rpt = 0.0;
	calc[i].rst = 0.0;
      }
      iyr = iyear;
      mnt = imont;
      idy = iday;
      ihr = ihour;
      min = iminu;
      cot = secc;
      judg = 0;
      goto end_NLINV;
    }				/* if (iyear >= 0) */
    srb = src = 0.0;
    for (i = 0; i < nd; ++i) {
      if (calc[i].pe <= 0.0)
	calc[i].rpt = 0.0;
      else
	calc[i].rpt = calc[i].pt - calc[i].tpt;
      if (calc[i].se <= 0.0)
	calc[i].rst = 0.0;
      else
	calc[i].rst = calc[i].st - alp * calc[i].tpt;
      wpt = calc[i].vpt * calc[i].rpt;
      wst = calc[i].vst * calc[i].rst;
      srb += wpt * calc[i].rpt + wst * calc[i].rst;
      /*
       * srb += calc[i].vpt*calc[i].rpt*calc[i].rpt
       * +calc[i].vst*calc[i].rst*calc[i].rst;
       */
      src += wpt + wst;
      /*
       * printf("as=%.11lf src=%.11lf srb=%.11lf rpt=%.11lf rst=%.11lf\n",
       * as,src,srb,calc[i].rpt,calc[i].rst);
       */
      /*
       * printf("%.14lf  %.14lf\n", (-4.2774906)*(-0.78770770),
       * (-4.2774906)*(-0.78770771));
       */
    }
    srb -= MULT2(src) / as;
    bcp = 0.0;
    /* printf("src=%lf  as=%lf  srb=%lf\n",src,as,srb);  as ok */
    /* printf("srb=%lf  bcp=%lf\n",srb,bcp); */
    for (i = 0; i < 3; ++i) {
      xw[i] = 0.0;
      for (j = 0; j < nd; ++j) {
	calc[j].tw = 0.0;
	for (k = 0; k < nd; ++k)
	  calc[j].tw
	    += calc[j].fp[k] * calc[k].rpt + alp * calc[j].fs[k] * calc[k].rst;
	xw[i] += calc[j].a[i] * calc[j].tw;
      }
      rmx = xm0[i] - xm1[i];
      rvx[i] = xw[i] + vxm[i] * rmx;
      srb += vxm[i] * MULT2(rmx);
      bcp += MULT2(rvx[i]);
    }
    bcp = sqrt(bcp / 3.0);
    if (ll != 0) {
      if (srb >= sra) {
	jj++;
	if (jj <= jm) {
	  aa = pow(0.5, (double)jj);
	  for (i = 0; i < 3; ++i)
	    xm1[i] -= aa * xmc[i];
	  goto line200;
	} else {		/* jj>jm */
	  judg = 3;
	  lm = ll;
	}
      } else {			/* srb<sra */
	zm1 = fabs(zmin - xm1[2]);
	zm2 = fabs(zmax - xm1[2]);
	if (zm1 > ccp && zm2 > ccp) {
	  if (bcp <= ccp)
	    lm = ll;
	} else if (zm1 <= ccp) {
	  judg = 1;
	  lm = ll;
	} else if (zm2 <= ccp) {
	  judg = 2;
	  lm = ll;
	}
      }
      fprintf(fp_21,
	      "%3d%12.5lE%12.5lE%12.5lE%12.5lE%3d%8.3lf%8.3lf%8.3lf\n",
	      ll, srb, sra, bcp, acp, jj, xm1[0], xm1[1], xm1[2]);
#if DEBUG
      fprintf(stdout,
	      "%3d%12.5lE%12.5lE%12.5lE%12.5lE%3d%8.3lf%8.3lf%8.3lf\n",
	      ll, srb, sra, bcp, acp, jj, xm1[0], xm1[1], xm1[2]);
#endif
    }
    for (i = 0; i < nd; ++i) {
      for (j = 0; j < 3; ++j) {
	calc[i].cp[j] = calc[i].cs[j] = 0.0;
	for (l = 0; l < nd; ++l) {
	  calc[i].cp[j] += calc[i].fp[l] * calc[l].a[j];
	  calc[i].cs[j] += calc[i].fs[l] * calc[l].a[j];
	}
      }
    }
    for (i = 0; i < 3; ++i) {
      for (j = 0; j < 3; ++j) {
	dp[i][j] = ds[i][j] = 0.0;
	for (l = 0; l < nd; ++l) {
	  dp[i][j] += calc[l].a[i] * calc[l].cp[j];
	  ds[i][j] += calc[l].a[i] * calc[l].cs[j];
	}
	bb[i][j] = dp[i][j] + al2 * ds[i][j];
	if (i == j)
	  bb[i][j] += vxm[i];
      }
    }
    /* MXINV */
    /* INVERSION OF A SYMMETRIC, POSITIVE-DEFINITE MATRIX */
    cc[0][0] = bb[1][1] * bb[2][2] - bb[1][2] * bb[1][2];
    cc[0][1] = bb[0][2] * bb[1][2] - bb[0][1] * bb[2][2];
    cc[0][2] = bb[0][1] * bb[1][2] - bb[0][2] * bb[1][1];
    cc[1][1] = bb[0][0] * bb[2][2] - bb[0][2] * bb[0][2];
    cc[1][2] = bb[0][1] * bb[0][2] - bb[0][0] * bb[1][2];
    cc[2][2] = bb[0][0] * bb[1][1] - bb[0][1] * bb[0][1];
    dtb = bb[0][0] * cc[0][0] + bb[0][1] * cc[0][1] + bb[0][2] * cc[0][2];
    if (dtb <= 0.0) {
      fprintf(fp_21, " *** THE MATRIX IS NOT POSITIVE-DEFINITE ***\n");
      judg = 11;
      goto end_NLINV;
    }
    for (i = 0; i < 3; ++i) {
      for (j = i; j < 3; ++j) {
	h[i][j] = cc[i][j] / dtb;
	if (i != j)
	  h[j][i] = h[i][j];
      }
    }				/* end MXINV */
    if (ll == lm) {
      srt = 0.0;
      for (i = 0; i < nd; ++i)
	srt += calc[i].rpt * calc[i].vpt + calc[i].rst * calc[i].vst;
      cot = srt / as;
      eot = 1.0 / as;
      for (i = 0; i < 3; ++i)
	ex1[i] = sqrt(h[i][i]);
      r1 = r2 = r3 = 0.0;
      for (i = 0; i < 3; ++i) {
	for (j = 0; j < 3; ++j) {
	  r1 += h[i][j] * dp[j][i];
	  r2 += h[i][j] * ds[j][i];
	}
	r3 += h[i][i] * vxm[i];
      }
      rsl[0] = 1.0e2 * r1 / 3.0;
      rsl[1] = 1.0e2 * r2 * al2 / 3.0;
      rsl[2] = 1.0e2 * r3 / 3.0;
      for (i = 0; i < nd; ++i) {
	if (calc[i].pe > 0.0)
	  calc[i].rpt -= cot;
	if (calc[i].se > 0.0)
	  calc[i].rst -= cot;
      }
      break;			/* exit while(1) loop */
    }
    /* i.e. ll != lm */
    for (i = 0; i < 3; ++i) {
      xmc[i] = 0.0;
      for (j = 0; j < 3; ++j)
	xmc[i] += h[i][j] * rvx[j];
      if (i == 2) {
	xmz = xm1[i] + xmc[i];
	if (xmz < zmin)
	  xmc[i] = zmin - xm1[i];
	if (xmz > zmax)
	  xmc[i] = zmax - xm1[i];
      }
      xm1[i] += xmc[i];
    }
    sra = srb;
    acp = bcp;
    ll++;
  }				/* while(1) */
end_NLINV:
  if (judg >= 10)
    goto HYPEND;

  /***** FINAL *****/
  /*-
       WRITE FINAL RESULTS TO FILE NO. 21 AND FILE NO. 22
       REVISED ON FEB-3-85  BY N.H.
       REVISED ON FEB-19-85 BY N.H.
       REVISED ON JAN- 9-91 BY T.U.
       RESULTS ARE CHECKED IN TERMS OF RESIDUALS
             IF A RESIDUAL IS GREATER THAN FACOR*(STANDARD DEVIATION)
                PUT JUDG=4 AND RE-CALCULATION: PE(I) OR SE(I)=-2.0
  */
  xm0[1] = -xm0[1];
  xm1[1] = -xm1[1];
  pltxy(alat0, alng0, &alati, &alngi, &xm0[0], &xm0[1], 1);
  pltxy(alat0, alng0, &alatf, &alngf, &xm1[0], &xm1[1], 1);
  xm0[1] = -xm0[1];
  xm1[1] = -xm1[1];		/* need? */
  nsrp = nsrs = nmag = 0;
  srp = srs = amag = 0.0;
  for (i = 0; i < nd; ++i) {
    xx = calc[i].sc[0] - xm1[0];
    yy = calc[i].sc[1] - xm1[1];
    calc[i].dl = hypot(xx, yy);	/* calc[i].dl=sqrt(MULT2(xx)+MULT2(yy)); */
    calc[i].az = PD * atan2(xx, -yy);
    if (calc[i].az < 0.0)
      calc[i].az += 360.0;
    calc[i].ta = PD * calc[i].tag;
    calc[i].tb = PD * calc[i].tbg;
    if (calc[i].pe > 0.0) {
      nsrp++;
      srp += MULT2(calc[i].rpt);
    }
    if (calc[i].se > 0.0) {
      nsrs++;
      srs += MULT2(calc[i].rst);
    }
    calc[i].bmag = 9.9;
    if (sta[calc[i].org_num].fmp == 0.0 && sta[calc[i].org_num].amp == 0.0)
      continue;
    /* hdist=sqrt(MULT2(calc[i].dl)+MULT2(xm1[2])); */
    hdist = hypot(calc[i].dl, xm1[2]);
    if (sta[calc[i].org_num].amp > 0.0) {
      /* ********** WATANABE'S FORMULA (Watanabe[1971]) ********** */
      /* AMP(I) is maximum deflection in m/s */
      if (hdist < 200.0)
	calc[i].bmag =
	  (log10(sta[calc[i].org_num].amp * 100.0) + 1.73 * log10(hdist)
	   + 2.50) / 0.85;
      else
	calc[i].bmag =
	  (log10(sta[calc[i].org_num].amp * 100.0) + 1.73 * log10(hdist)
	   + 2.50 + 0.0015 * (hdist - 200.0)) / 0.85;
    }
    /* ********************************************************** */
    else {
      /* ********** TSUMURA'S FORMULA (Tsumura[1967]) ********** */
      /* WE USE HYPOCENTRAL DISTANCE, INSTED OF EPICENTRAL DISTANCE  */
      if (hdist < 200.0)
	calc[i].bmag = -2.36 + 2.85 * log10(sta[calc[i].org_num].fmp);
      else
	calc[i].bmag = -2.53 + 2.58 * log10(sta[calc[i].org_num].fmp)
	  + 0.0014 * hdist;
      /* ********************************************************** */
    }
    nmag++;
    amag += calc[i].bmag;
  }
  if (nmag > 0)
    amag /= nmag;
  else
    amag = 9.9;
  /* this is to avoid trivial error by PLTXY() */
  if (iyear >= 0) {
    alatf = alat00;
    alngf = alng00;
    amag = amag00;
  }
  fprintf(fp_21, "***** FINAL RESULTS *****\n");
  fprintf(fp_21, "%3.2d%3.2d%3.2d   %3d%3d%8.3lf%11.5lf%11.5lf%8.3lf%6.1lf\n"
	  ,iyr, mnt, idy, ihr, min, cot, alatf, alngf, xm1[2], amag);
  fprintf(fp_22, "%3.2d%3.2d%3.2d   %3d%3d%8.3lf%11.5lf%11.5lf%8.3lf%6.1lf\n"
	  ,iyr, mnt, idy, ihr, min, cot, alatf, alngf, xm1[2], amag);
  fprintf(stdout, "%3.2d%3.2d%3.2d   %3d%3d%8.3lf%11.5lf%11.5lf%8.3lf%6.1lf\n"
	  ,iyr, mnt, idy, ihr, min, cot, alatf, alngf, xm1[2], amag);

  fprintf(fp_21, "   %s           %8.3lf%9.3lf  %9.3lf  %8.3lf\n",
	  diag[judg], eot, ex1[1], ex1[0], ex1[2]);
  fprintf(fp_22, "   %s           %8.3lf%9.3lf  %9.3lf  %8.3lf\n",
	  diag[judg], eot, ex1[1], ex1[0], ex1[2]);
  fprintf(stdout, "   %s           %8.3lf%9.3lf  %9.3lf  %8.3lf\n",
	  diag[judg], eot, ex1[1], ex1[0], ex1[2]);

  fprintf(fp_21, "%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf\n",
	  h[0][0], -h[0][1], h[0][2], h[1][1], -h[1][2], h[2][2]);
  fprintf(fp_22, "%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf\n",
	  h[0][0], -h[0][1], h[0][2], h[1][1], -h[1][2], h[2][2]);
  fprintf(stdout, "%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf\n",
	  h[0][0], -h[0][1], h[0][2], h[1][1], -h[1][2], h[2][2]);

  fprintf(fp_21, "            %7.3lf %5.1lf %7.3lf %5.1lf %7.3lf %5.1lf\n",
	  alati, ex0[1], alngi, ex0[0], xm0[2], ex0[2]);
  fprintf(fp_22, "            %7.3lf %5.1lf %7.3lf %5.1lf %7.3lf %5.1lf\n",
	  alati, ex0[1], alngi, ex0[0], xm0[2], ex0[2]);
  fprintf(stdout, "            %7.3lf %5.1lf %7.3lf %5.1lf %7.3lf %5.1lf\n",
	  alati, ex0[1], alngi, ex0[0], xm0[2], ex0[2]);

  fprintf(fp_21,
	  "  %3d %4s %3d (%5.1lf%% ) %3d (%5.1lf%% ) %3d (%5.1lf%% )\n",
	  nd, vst, npd, rsl[0], nsd, rsl[1], npr, rsl[2]);
  fprintf(fp_22,
	  "  %3d %4s %3d (%5.1lf%% ) %3d (%5.1lf%% ) %3d (%5.1lf%% )\n",
	  nd, vst, npd, rsl[0], nsd, rsl[1], npr, rsl[2]);
  fprintf(stdout,
	  "  %3d %4s %3d (%5.1lf%% ) %3d (%5.1lf%% ) %3d (%5.1lf%% )\n",
	  nd, vst, npd, rsl[0], nsd, rsl[1], npr, rsl[2]);

  for (i = 0; i < nd; ++i) {
    if (sta[calc[i].org_num].amp > 0.0)
      fmag = sta[calc[i].org_num].amp;
    else if (sta[calc[i].org_num].fmp > 0.0)
      fmag = sta[calc[i].org_num].fmp;
    else
      fmag = 0.0;

    fprintf(fp_21,
	    "%-10s %-2s%8.3lf%6.1lf%6.1lf%6.1lf%7.3lf%6.3lf%7.3lf%7.3lf%6.3lf%7.3lf%10.3lE%5.1lf\n",
	    calc[i].sa, calc[i].pola, calc[i].dl, calc[i].az,
	    calc[i].ta, calc[i].tb, calc[i].pt, calc[i].pe, calc[i].rpt,
	    calc[i].st, calc[i].se, calc[i].rst, fmag, calc[i].bmag);
    fprintf(fp_22,
	    "%-10s %-2s%8.3lf%6.1lf%6.1lf%6.1lf%7.3lf%6.3lf%7.3lf%7.3lf%6.3lf%7.3lf%10.3lE%5.1lf\n",
	    calc[i].sa, calc[i].pola, calc[i].dl, calc[i].az,
	    calc[i].ta, calc[i].tb, calc[i].pt, calc[i].pe, calc[i].rpt,
	    calc[i].st, calc[i].se, calc[i].rst, fmag, calc[i].bmag);
    fprintf(stdout,
	    "%-10s %-2s%8.3lf%6.1lf%6.1lf%6.1lf%7.3lf%6.3lf%7.3lf%7.3lf%6.3lf%7.3lf%10.3lE%5.1lf\n",
	    calc[i].sa, calc[i].pola, calc[i].dl, calc[i].az,
	    calc[i].ta, calc[i].tb, calc[i].pt, calc[i].pe, calc[i].rpt,
	    calc[i].st, calc[i].se, calc[i].rst, fmag, calc[i].bmag);

  }

  if (nsrp > 0)
    srp = sqrt(srp / (double)nsrp);
  if (nsrs > 0)
    srs = sqrt(srs / (double)nsrs);
  fprintf(fp_21,
	  "                                                    %7.3lf             %7.3lf\n",
	  srp, srs);
  fprintf(fp_22,
	  "                                                    %7.3lf             %7.3lf\n",
	  srp, srs);
  fprintf(stdout,
	  "                                                    %7.3lf             %7.3lf\n",
	  srp, srs);

#if CHK_RSLT
  /* check results */
  icheck = 0;
  for (i = 0; i < nd; ++i) {
    if (fabs(calc[i].rpt) > FACTOR * srp) {
      calc[i].pe = -2.0;
      icheck++;
      npd--;
    }
    if (fabs(calc[i].rst) > FACTOR * srs) {
      calc[i].se = -2.0;
      icheck++;
      nsd--;
    }
  }
  if (icheck > 0) {
    judg = 4;
    goto line150;
  }
#endif

  /***** HYPEND *****/
HYPEND:
  printf("****** END OF HYPOMH ******\n");
  fclose(fp_21);
  fclose(fp_22);
  end_hypomhc(0);
}
