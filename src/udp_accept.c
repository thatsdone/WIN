/* $Id: udp_accept.c,v 1.5.2.4 2014/05/13 13:24:11 uehira Exp $ */

/*
 * Copyright (c) 2001-2014
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@bosai.go.jp
 *    National Research Institute for Earth Science and Disaster Prevention
 *
 *   2001-10-02   Initial version.
 *   2011-11-17  family type.
 *   2014-02-05  udp_accept4(): NIC for receive can be specified by 'interface'.
 *   2014-04-07  udp_accept(): NIC for receive can be specified by 'interface'.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>

#include <net/if.h>

#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <netdb.h>   /* struct addrinfo */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "winlib.h"
/* #include "udpu.h" */
/* #include "win_log.h" */

/*
 * Accept packets from "port".
 *  Return all available sockets or NULL pointer.
 *  listen IPv6 & IPv4 address.
 */
#ifdef INET6
struct conntable *
udp_accept(const char *port, int *maxsoc, int sockbuf, int family,
	   const char *interface)
{
  int  sockfd, gai_error;
  struct conntable  *ct_top = NULL, *ct, **ctp = &ct_top, *ct_next;
  struct addrinfo  hints, *res, *ai;
  int    sock_bufsiz;
  char  hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  char  buf[1024];
  int   j;
  char  *hnbuf;

  *maxsoc = -1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = family;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  if (*interface)
    hnbuf = (char *)interface;
  else
    hnbuf = NULL;
  if ((gai_error = getaddrinfo(hnbuf, port, &hints, &res)) != 0) {
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

    /* set socket option: recv. bufsize */
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

/*
 * Accept packets from "port".
 *  Return socket FD
 *  IPv4 only.
 */
int
udp_accept4(const uint16_t port, int sockbuf, const char *interface)
{
  int  sockfd;
  int  sock_bufsiz;
  struct sockaddr_in  to_addr;
  char  tb[1024];
  int  j;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    err_sys("socket");

  /* set socket option: recv. bufsize */
  for (j = sockbuf; j >= MIN_RECV_BUFSIZ; j -= 4) {
    sock_bufsiz = (j << 10);
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
		   &sock_bufsiz, sizeof(sock_bufsiz)) >= 0)
      break;
  }
  if (j < MIN_RECV_BUFSIZ) {
    (void)close(sockfd);
    err_sys("SO_RCVBUF setsockopt error");
  }
  (void)snprintf(tb, sizeof(tb), "RCVBUF size=%d", sock_bufsiz);
  write_log(tb);

  /* bind */
  memset(&to_addr, 0, sizeof(to_addr));
  to_addr.sin_family = AF_INET;
  if (*interface)
    to_addr.sin_addr.s_addr = inet_addr(interface);
  else
    to_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  to_addr.sin_port = htons(port);
  if (bind(sockfd, (struct sockaddr *)&to_addr, sizeof(to_addr)) < 0) {
    (void)close(sockfd);
    err_sys("bind");
  }

  return (sockfd);
}

/*
 * Join multicast group
 *   (IPv4 & IPv6 version)
 */
void
mcast_join(const int sockfd, const char *mcastgroup, const char *interface)
{
  struct ip_mreq  stMreq;
#ifdef INET6
  struct ipv6_mreq  stMreq6;
#endif
  int  status;
  char  tb[1024];

  switch (sockfd_to_family(sockfd)) {
  case AF_INET:
    status = inet_pton(AF_INET, mcastgroup, &stMreq.imr_multiaddr);
    (void)snprintf(tb, sizeof(tb), "mcast IPv4 inet_pton status = %d", status);
    write_log(tb);
    if (status == 0)  /* Invalid format */
      break;
    else if (status == -1)  /* error */
      err_sys("inet_pton");
    if(*interface)
      stMreq.imr_interface.s_addr = inet_addr(interface);
    else
      stMreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &stMreq,
		   sizeof(stMreq)) < 0)
      err_sys("IP_ADD_MEMBERSHIP setsockopt error");
    break;

#ifdef INET6
  case AF_INET6:
    status = inet_pton(AF_INET6, mcastgroup, &stMreq6.ipv6mr_multiaddr);
    (void)snprintf(tb, sizeof(tb), "mcast IPv6 inet_pton status = %d", status);
    write_log(tb);
    if (status == 0)  /* Invalid format */
      break;
    else if (status == -1)  /* error */
      err_sys("inet_pton");
    if(*interface) {
      stMreq6.ipv6mr_interface = if_nametoindex(interface);
      if (stMreq6.ipv6mr_interface == 0)
	err_sys("stMreq6.ipv6imr_interface");
    } else
      stMreq6.ipv6mr_interface = 0;
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &stMreq6,
		   sizeof(stMreq6)) < 0)
      err_sys("IPV6_JOIN_GROUP setsockopt error");
    break;
#endif  /* INET6 */

  default:
    /* errno = EPROTPNOSUPPORT; */
    err_sys("mcast_join");
  }
}
