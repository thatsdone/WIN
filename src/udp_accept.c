/* $Id: udp_accept.c,v 1.3 2004/11/26 13:55:38 uehira Exp $ */

/*
 * Copyright (c) 2001-2004
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *   2001-10-2   Initial version.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>

#include <netinet/in.h>

#include <netdb.h>   /* struct addrinfo */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "udpu.h"
#include "win_log.h"

#define MIN_RECV_BUFSIZ  16   /* min. bufsize in KB */


#ifdef INET6
/*
 * Accept packets from "port".
 *  Return all available sockets or NULL pointer.
 *  listen IPv6 & IPv4 address.
 */
struct conntable *
udp_accept(const char *port, int *maxsoc, int sockbuf)
{
  int  sockfd, gai_error;
  struct conntable  *ct_top = NULL, *ct, **ctp = &ct_top, *ct_next;
  struct addrinfo  hints, *res, *ai;
  int    sock_bufsiz;
  char  hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  char  buf[1024];
  int   j;

  *maxsoc = -1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  if ((gai_error = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    (void)snprintf(buf, sizeof(buf), "udp_accept: getaddrinfo : %s",
		   gai_strerror(gai_error));
    write_log(buf);
    return (NULL);
  }

  /* search connectable address(es) */
  for (ai = res; ai != NULL; ai = ai->ai_next) {
    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0)
      continue;

    /* set socket opttion: recv. bufsize */
    for (j = sockbuf; j >= MIN_RECV_BUFSIZ; j -= 4) {
      sock_bufsiz = (j << 10);
      if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
		     &sock_bufsiz, sizeof(sock_bufsiz)) >= 0)
	break;
    }
    if (j < MIN_RECV_BUFSIZ) {
      (void)close(sockfd);
      continue;
    }

    if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
      (void)close(sockfd);
      continue;
    }

    /* save connectable socks */
    if ((ct = (struct conntable *)malloc(sizeof(*ct))) == NULL) {
      (void)close(sockfd);
      continue;
    }
    memset(ct, 0, sizeof(*ct));
    ct->soc = sockfd;
    ct->sockbuf = sock_bufsiz;
    *ctp = ct;
    ctp = &ct->next;

    /* save max socket number */
    if (sockfd > *maxsoc)
      *maxsoc = sockfd;

    (void)getnameinfo(ai->ai_addr, ai->ai_addrlen, hbuf, sizeof(hbuf),
		      sbuf, sizeof(sbuf),
		      NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
    (void)snprintf(buf, sizeof(buf),
		   "listen: host=%s, serv=%s, RCVBUF size=%d",
		   hbuf, sbuf, ct->sockbuf);
    write_log(buf);
  }  /* for (ai = res; ai != NULL; ai = ai->next) */

  if (*maxsoc < 0) {
    for (ct = ct_top; ct != NULL; ct = ct_next) {
      ct_next = ct->next;
      (void)close(ct->soc);
      free(ct);
    }
    ct_top = NULL; 
  }

  if (res != NULL)
    freeaddrinfo(res);

  return (ct_top);
}
#endif  /* INET6 */
