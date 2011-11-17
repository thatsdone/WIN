/* $Id: tcpu.h,v 1.3 2011/11/17 03:58:41 uehira Exp $ */

/*
 * Copyright (c) 2006-2011
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 */

#ifndef _TCPU_H_
#define _TCPU_H_

#include <sys/types.h>
#include <sys/socket.h>

#ifdef INET6
int tcp_connect(const char *, const char *, struct sockaddr *, socklen_t *,
		int);
#endif  /* INET6 */

#endif  /* !_TCPU_H_ */
