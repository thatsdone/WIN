/* $Id: filter.h,v 1.1.2.1 2007/06/19 11:19:40 uehira Exp $ */

#ifndef _FILTER_H_
#define _FILTER_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* filter.c */
int butlop(double *, int *, double *, int *, double, double, double, double);
int buthip(double *, int *, double *, int *, double, double, double, double);
int butpas(double *, int *, double *, int *, double, double, double, double, double);
/* static int recfil(double *, double *, int, double *, int, double *); */
int tandem(double *, double *, int, double *, int, int, double *);

#endif /* !_FILTER_H_*/
