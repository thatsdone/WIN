/* $Id: udpu.h,v 1.2 2004/11/26 13:55:39 uehira Exp $ */

/*
 * Copyright (c) 2001-2004
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 */

#ifndef _UDPU_H_
#define _UDPU_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>

struct conntable {
  struct conntable  *next;
  int  soc;
  int  sockbuf;
};

#ifdef INET6
int udp_dest(const char *, const char *, struct sockaddr *, socklen_t *);
struct conntable * udp_accept(const char *, int *, int );
#endif  /* INET6 */

#endif  /* !_UDPU_H_ */
