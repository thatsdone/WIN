/* $Id: udp_dest.c,v 1.3.2.1.4.1.2.1 2011/05/05 04:15:57 uehira Exp $ */

/*
 * Copyright (c) 2001-2011
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
#include <errno.h>

#include "udpu.h"
#include "win_log.h"

#define  SOCKET_SND_BUFSIZ   65535


#ifdef INET6
/*
 * Destination host/port (IPv6 & IPv4 support)
 *  Return Socket FD or -1
 */
int
udp_dest(const char *hostname, const char *port,
	  struct sockaddr *saptr, socklen_t *lenp, const char *src_port)
{
  int  sockfd, gai_error;
  struct addrinfo  hints, *res, *ai, *ai_src;
  int  sock_bufsiz;
  char  hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  char buf[1024];

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  if ((gai_error = getaddrinfo(hostname, port, &hints, &res)) != 0) {
    (void)snprintf(buf, sizeof(buf), 
		   "udp_dest: getaddrinfo : %s", gai_strerror(gai_error));
    write_log(buf);
    return (-1);
  }
  
  sock_bufsiz = SOCKET_SND_BUFSIZ;

  /* search destination address */
  for (ai = res; ai != NULL; ai = ai->ai_next) {
    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0)
      continue;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF,
		   &sock_bufsiz, sizeof(sock_bufsiz)) < 0) {
      (void)close(sockfd);
      continue;
    }

    break;   /* success */
  }  /* for (ai = res; ai != NULL; ai = ai->ai_next) */

  if (ai == NULL) {
    (void)snprintf(buf, sizeof(buf), "%s: udp_dest error for %s:%s",
		   strerror(errno), hostname, port);
    write_log(buf);
    sockfd = -1;
  } else {
    memcpy(saptr, ai->ai_addr, ai->ai_addrlen);
    *lenp = ai->ai_addrlen;
  
    gai_error = getnameinfo(ai->ai_addr, ai->ai_addrlen, hbuf, sizeof(hbuf),
			    sbuf, sizeof(sbuf),
			    NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
    if (gai_error)
      (void)snprintf(buf, sizeof(buf), 
		     "udp_dest: getnameinfo : %s", gai_strerror(gai_error));
    else 
      (void)snprintf(buf, sizeof(buf),
		     "dest: host=%s, serv=%s", hbuf, sbuf);
    write_log(buf);
  }
  
  if (res != NULL)
    freeaddrinfo(res);

  /* src port part */
  if (src_port != NULL && sockfd != -1) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    if ((gai_error = getaddrinfo(NULL, src_port, &hints, &res)) != 0) {
      (void)snprintf(buf, sizeof(buf), 
		     "udp_dest: getaddrinfo2 : %s", gai_strerror(gai_error));
      write_log(buf);
      return (-1);
    }
    
    for (ai_src = res; ai_src != NULL; ai_src = ai_src->ai_next) {
      /* printf("family %d\n", ai_src->ai_family); */
      if (bind(sockfd, ai_src->ai_addr, ai_src->ai_addrlen) < 0) {
	/* write_log("skip"); */
	continue;
      }
      break;  /* success */
    }
    if (ai_src == NULL) {
      close(sockfd);
      write_log("error in bind");
      sockfd = -1;
    } else {
      (void)snprintf(buf, sizeof(buf), "src_port=%s", src_port);
      write_log(buf);
    }

    if (res != NULL)
      freeaddrinfo(res);
  }  /* if (src_port != NULL) */

  return (sockfd);
}
#endif  /* INET6 */

/*
 * Destination host/port (IPv4 only)
 *  Return Socket FD
 */
int
udp_dest4(const char *hostname, const uint16_t port,
	  struct sockaddr_in *saptr, int sockbuf, const uint16_t src_port)
{
  int  sockfd;
  struct hostent  *h;
  struct sockaddr_in  src_addr;
  int  sock_bufsiz;
  int  bflag;
  char  tbuf[1024];
  int  j;

  /* set destination host:port */
  if ((h = gethostbyname(hostname)) == NULL)
    err_sys("can't find host");
  memset(saptr, 0, sizeof(*saptr));
  saptr->sin_family = AF_INET;
  memcpy(&saptr->sin_addr, h->h_addr, h->h_length);
  saptr->sin_port = htons(port);

  /* open socket */
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    err_sys("socket");

  /* set socket option: send bufsize */
  for (j = sockbuf; j >= MIN_SEND_BUFSIZ; j -= 4) {
    sock_bufsiz = (j << 10);
    if (setsockopt(sockfd, SOL_SOCKET,  SO_SNDBUF,
		   &sock_bufsiz, sizeof(sock_bufsiz)) >= 0)
      break;
  }
  if (j < MIN_SEND_BUFSIZ) {
    (void)close(sockfd);
    err_sys("SO_SNDBUF setsockopt error");
  }
  (void)snprintf(tbuf, sizeof(tbuf), "SNDBUF size=%d", sock_bufsiz);
  write_log(tbuf);

  /* broadcast */
  bflag = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &bflag, sizeof(bflag)) < 0) {
    (void)close(sockfd);
    err_sys("SO_BROADCAST setsockopt error");
  }
  
  /* bind my socket to a local port */
  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.sin_family = AF_INET;
  src_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  src_addr.sin_port = htons(src_port);
  if (src_port) {
    (void)snprintf(tbuf, sizeof(tbuf), "src_port=%d", src_port);
    write_log(tbuf);
  }
  if (bind(sockfd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
    (void)close(sockfd);
    err_sys("bind");
  }

  return (sockfd);
}

/*
 * set multicast options for output.
 *  interface & TTL (or HOPS).
 *  (IPv4 & IPv6)
 */
void
mcast_set_outopt(const int sockfd, const char *interface, const int ttl)
{
  in_addr_t  mif; /* multicast interface address */
  u_char     no;
#ifdef INET6
  unsigned int  mif6;
  int   hops;
#endif

  switch (sockfd_to_family(sockfd)) {
  case AF_INET:
    /* set interface */
    if(*interface) {
      mif = inet_addr(interface);
      if (setsockopt(sockfd, IPPROTO_IP,
		     IP_MULTICAST_IF, &mif, sizeof(mif)) < 0)
	err_sys("IP_MULTICAST_IF setsockopt error");
    }

    /* set TTL */
    if (ttl > 1) {
      no = (u_char)ttl;
      if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &no, sizeof(no)) < 0)
	err_sys("IP_MULTICAST_TTL setsockopt error");
    }
    break;

#ifdef INET6
  case AF_INET6:
    /* set interface */
    if(*interface) {
      mif6 = if_nametoindex(interface);
      if (mif6 == 0)
	err_sys("mif6");
      if (setsockopt(sockfd, IPPROTO_IPV6,
		     IPV6_MULTICAST_IF, &mif6, sizeof(mif6)) < 0)
	err_sys("IPV6_MULTICAST_IF setsockopt error");
    }

    /* set HOPS */
    if (ttl > 1) {
      hops = ttl;
      if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		     &hops, sizeof(hops)) < 0)
	err_sys("IPV6_MULTICAST_HOPS setsockopt error");
    }
    break;
#endif  /* INET6 */

  default:
    /* errno = EPROTPNOSUPPORT; */
    err_sys("mcast_set_outopt");
  }
}
