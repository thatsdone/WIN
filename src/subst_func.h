/* $Id: subst_func.h,v 1.2 2002/01/13 06:57:51 uehira Exp $ */

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

#endif  /* !_SUBST_FUNC_H_ */
