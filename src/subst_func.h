/* $Id: subst_func.h,v 1.4 2011/06/01 11:09:22 uehira Exp $ */

/*
 * Copyright (c) 2001
 *   Urabe Taku & Uehira Kenji / All Rights Reserved.
 */

#ifndef _SUBST_FUNC_H_
#define _SUBST_FUNC_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/***** strerror(3) ******/
#ifndef HAVE_STRERROR
#define strerror(a) sys_errlist[a]
extern const char *const sys_errlist[];
#endif /* !HAVE_STRERROR */

/***** use timelocal(3) instead of mktime(3) in case of SunOS4 *****/
#if defined(SUNOS4) && defined(HAVE_TIMELOCAL)
#define mktime timelocal
#endif

/***** snprintf(3) ******/
#ifndef HAVE_SNPRINTF
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif /* HAVE_STDARG_H */
#ifdef HAVE_VARARGS_H   /* cf. SunOS 4 */
#include <varargs.h>
#endif /* HAVE_STDARG_H */
static int snprintf(char *, size_t, const char *, ...);

/*
 * The function below just acts like sprintf(); it is not safe, but it
 * tries to detect overflow.
 */
static int
snprintf(char *buf, size_t size, const char *fmt, ...)
{
  size_t   n;
  va_list  ap;

  va_start(ap, fmt);

  /* Sigh, some vsprintf's return ptr, not length */
  (void)vsprintf(buf, fmt, ap); 

  n = strlen(buf);
  va_end(ap);
  if (n >= size) {
    (void)fprintf(stderr, "snprintf: '%s' overflowed array", fmt);
    exit(1);
  }
  return((int)n);  /* warning: conversion from 'size_t' may lose accuracy */
}
#endif /* !HAVE_SNPRINTF */

#endif  /* !_SUBST_FUNC_H_ */
