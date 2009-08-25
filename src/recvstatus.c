/* $Id: recvstatus.c,v 1.6.8.3 2009/08/25 04:00:15 uehira Exp $ */

/* "recvstatus.c"      5/24/95    urabe */
/* 97.7.17 two lines of "if() continue;" in the main loop */
/* 2000.4.24/2001.11.14 strerror() */
/* 2001.11.14 ntohs() */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else  /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif  /* !TIME_WITH_SYS_TIME */

#include <sys/socket.h>
#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <errno.h>

#include "winlib.h"

#define MAXMESG   2048

static int sock;     /* socket */
static unsigned char rbuf[MAXMESG],stt[WIN_CHMAX];
static char tb[100];

char *progname,*logfile;
int  syslog_mode = 0, exit_status = EXIT_SUCCESS;

main(argc,argv)
  int argc;
  char *argv[];
  {
  int i,fromlen,n;
  struct sockaddr_in to_addr,from_addr;
  unsigned short to_port;

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];
  if(argc<2)
    {
    fprintf(stderr,
      " usage : '%s [port] ([log file])'\n",progname);
    exit(1);
    }
  to_port=atoi(argv[1]);
  if(argc>2) logfile=argv[2];
  else logfile=NULL;

  sprintf(tb,"%s started. port=%d file=%s",progname,to_port,logfile);
  write_log(tb);

  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");
  i=32768;
  if(setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))<0)
    err_sys("SO_RCVBUF setsockopt error\n");

  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  to_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  to_addr.sin_port=htons(to_port);

  if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0)
    err_sys("bind");

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  for(i=0;i<WIN_CHMAX;i++) stt[i]=0xff;
  while(1)
    {
    fromlen=sizeof(from_addr);
    n=recvfrom(sock,rbuf,MAXMESG,0,(struct sockaddr *)&from_addr,&fromlen);
    if(rbuf[0]!=rbuf[8]) continue;
    if(rbuf[9]==stt[(rbuf[7]<<8)+rbuf[8]]) continue;
    sprintf(tb,"%s:%d %02X %02X%02X%02X %02X%02X%02X %02X%02X ... %02X",
      inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),
      rbuf[0],rbuf[1],rbuf[2],rbuf[3],rbuf[4],rbuf[5],rbuf[6],
      rbuf[7],rbuf[8],rbuf[9]);
    write_log(tb);
    stt[(rbuf[7]<<8)+rbuf[8]]=rbuf[9];
    }
  }
