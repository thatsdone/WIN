/* $Id: pltxy.c,v 1.2 2011/06/01 11:09:21 uehira Exp $ */

#include <math.h>

/*  this program was translated from HYPOMH(HIRATA and MATSU'URA) */
/* PLTXY TRANSFORMS (X,Y) TO (ALAT,ALONG) IF IND.EQ.1  */
/* PLTXY TRANSFORMS (ALAT,ALONG) TO (X,Y) IF IND.EQ.0  */
void
pltxy(double alt0, double alng0,
      double *alat, double *along, double *x, double *y, int ind)
{
  static double	 a = 6.378160e3, e2 = 6.6944541e-3, e12 = 6.7395719e-3,
    d = 5.72958e1;
  double         rd, rlat, slat, clat, v2, al, ph1, rph1, rph2, an, c1, c2,
    rlato, slato, tphi1, cphi1, r;

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
