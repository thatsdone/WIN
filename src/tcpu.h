/* $Id: tcpu.h,v 1.2 2011/06/01 11:09:22 uehira Exp $ */

/*
 * Copyright (c) 2006
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 */

#ifndef _TCPU_H_
#define _TCPU_H_

#include <sys/types.h>
#include <sys/socket.h>

#ifdef INET6
int tcp_connect(const char *, const char *, struct sockaddr *, socklen_t *);
#endif  /* INET6 */

#endif  /* !_TCPU_H_ */
