/* $Id: udp_dest.c,v 1.2 2004/11/11 11:36:52 uehira Exp $ */

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
	 struct sockaddr *saptr, socklen_t *lenp)
{
  int  sockfd, gai_error;
  struct addrinfo  hints, *res, *ai;
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
    return (-1);
  }
  
  memcpy(saptr, ai->ai_addr, ai->ai_addrlen);
  *lenp = ai->ai_addrlen;

  (void)getnameinfo(ai->ai_addr, ai->ai_addrlen, hbuf, sizeof(hbuf),
		    sbuf, sizeof(sbuf),
		    NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
  (void)snprintf(buf, sizeof(buf),
		 "dest: host=%s, serv=%s", hbuf, sbuf);
  write_log(buf);
  
  if (res != NULL)
    freeaddrinfo(res);

  return (sockfd);
}
#endif  /* INET6 */
