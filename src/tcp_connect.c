/* $Id: tcp_connect.c,v 1.2 2011/06/01 11:09:22 uehira Exp $ */

/*
 * Copyright (c) 2006
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *   2006-05-02   Initial version.
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

#include "tcpu.h"
#include "win_log.h"


#ifdef INET6
/*
 * Connection host/port (IPv6 & IPv4 support)
 *  Return Socket FD or -1
 */
int
tcp_connect(const char *hostname, const char *port,
	 struct sockaddr *saptr, socklen_t *lenp)
{
  int  sockfd, gai_error;
  struct addrinfo  hints, *res, *ai;
  char  hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  char buf[1024];

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  if ((gai_error = getaddrinfo(hostname, port, &hints, &res)) != 0) {
    (void)snprintf(buf, sizeof(buf), 
		   "tcp_connect: getaddrinfo : %s", gai_strerror(gai_error));
    write_log(buf);
    return (-1);
  }
  
  /* search destination address */
  for (ai = res; ai != NULL; ai = ai->ai_next) {
    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0)
      continue;

    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == 0)
      break;   /* success */
    
    (void)close(sockfd);  /* ignore */
  }  /* for (ai = res; ai != NULL; ai = ai->ai_next) */

  if (res != NULL)
    freeaddrinfo(res);

  if (ai == NULL) {
    (void)snprintf(buf, sizeof(buf), "%s: tcp_connect error for %s:%s",
		   strerror(errno), hostname, port);
    write_log(buf);
    return (-1);
  }
  
  memcpy(saptr, ai->ai_addr, ai->ai_addrlen);
  *lenp = ai->ai_addrlen;

  /* print info */
  (void)getnameinfo(ai->ai_addr, ai->ai_addrlen, hbuf, sizeof(hbuf),
		    sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
  (void)snprintf(buf, sizeof(buf),
		 "tcp connected to host=%s, serv=%s", hbuf, sbuf);
  write_log(buf);
  
  return (sockfd);
}
#endif  /* INET6 */
