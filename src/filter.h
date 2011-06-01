/* $Id: filter.h,v 1.2 2011/06/01 11:09:20 uehira Exp $ */

#ifndef _FILTER_H_
#define _FILTER_H_

/* filter.c */
int butlop(double *, int *, double *, int *, double, double, double, double);
int buthip(double *, int *, double *, int *, double, double, double, double);
int butpas(double *, int *, double *, int *, double, double, double, double, double);
/* static int recfil(double *, double *, int, double *, int, double *); */
int tandem(double *, double *, int, double *, int, int, double *);

#endif /* !_FILTER_H_*/
