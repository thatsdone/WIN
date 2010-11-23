/* $Id: filter.h,v 1.1.2.1.6.1 2010/11/23 05:25:17 uehira Exp $ */

#ifndef _FILTER_H_
#define _FILTER_H_

/* filter.c */
int butlop(double *, int *, double *, int *, double, double, double, double);
int buthip(double *, int *, double *, int *, double, double, double, double);
int butpas(double *, int *, double *, int *, double, double, double, double, double);
/* static int recfil(double *, double *, int, double *, int, double *); */
int tandem(double *, double *, int, double *, int, int, double *);

#endif /* !_FILTER_H_*/
