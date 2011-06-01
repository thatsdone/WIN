/* $Id: udpu.h,v 1.2.2.2 2011/06/01 12:14:54 uehira Exp $ */

/*
 * Copyright (c) 2001-2011
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 */

#ifndef _UDPU_H_
#define _UDPU_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MIN_RECV_BUFSIZ  16   /* min. bufsize for recipt in KB */
#define MIN_SEND_BUFSIZ  16   /* min. bufsize for transmit in KB */

struct conntable {
  struct conntable  *next;
  int  soc;
  int  sockbuf;
};

#ifdef INET6
int udp_dest(const char *, const char *, struct sockaddr *, socklen_t *,
	     const char *);
struct conntable * udp_accept(const char *, int *, int );
#endif  /* INET6 */

/* IPv4 only version */
int udp_dest4(const char *, const uint16_t, struct sockaddr_in *,
	      int, const uint16_t);
int udp_accept4(const uint16_t, int);

/* Multicast functions (IPv4 & IPv6) */
void mcast_join(const int, const char *, const char *);
void mcast_set_outopt(const int, const char *, const int);

#endif  /* !_UDPU_H_ */
