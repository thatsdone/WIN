/* $Id: recvstatus.c,v 1.2.2.1 2001/11/02 11:43:38 uehira Exp $ */
/* "recvstatus.c"      5/24/95    urabe */
/* 97.7.17 two lines of "if() continue;" in the main loop */
/* 2000.4.24 strerror() */

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
#include <netdb.h>
#include <errno.h>

#include "subst_func.h"

#define MAXMESG   2048

int sock;     /* socket */
unsigned char rbuf[MAXMESG],stt[65536];
char tb[100],*progname,logfile[256];

get_time(rt)
  int *rt;
  {
  struct tm *nt;
  unsigned long ltime;
  time(&ltime);
  nt=localtime(&ltime);
  rt[0]=nt->tm_year%100;
  rt[1]=nt->tm_mon+1;
  rt[2]=nt->tm_mday;
  rt[3]=nt->tm_hour;
  rt[4]=nt->tm_min;
  rt[5]=nt->tm_sec;
  }

write_log(logfil,ptr)
  char *logfil;
  char *ptr;
  {
  FILE *fp;
  int tm[6];
  if(*logfil) fp=fopen(logfil,"a");
  else fp=stdout;
  get_time(tm);
  fprintf(fp,"%02d%02d%02d.%02d%02d%02d %s %s\n",
    tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
  if(*logfil) fclose(fp);
  }

ctrlc()
  {
  write_log(logfile,"end");
  close(sock);
  exit(0);
  }

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  write_log(logfile,ptr);
  if(strerror(errno)) write_log(strerror(errno));
  close(sock);
  ctrlc();
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  union {
    unsigned long i;
    unsigned short s;
    char c[4];
    } un;
  unsigned char *ptr,tm[6],*ptr_size;
  int i,j,k,size,fromlen,n,re;
  struct sockaddr_in to_addr,from_addr;
  unsigned short to_port;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<2)
    {
    fprintf(stderr,
      " usage : '%s [port] ([log file])'\n",progname);
    exit(1);
    }
  to_port=atoi(argv[1]);
  if(argc>2) strcpy(logfile,argv[2]);
  else *logfile=0;

  sprintf(tb,"%s started. port=%d file=%s",progname,to_port,logfile);
  write_log(logfile,tb);

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

  signal(SIGTERM,ctrlc);
  signal(SIGINT,ctrlc);
  signal(SIGPIPE,ctrlc);

  for(i=0;i<65535;i++) stt[i]=0xff;
  while(1)
    {
    fromlen=sizeof(from_addr);
    n=recvfrom(sock,rbuf,MAXMESG,0,&from_addr,&fromlen);
    if(rbuf[0]!=rbuf[8]) continue;
    if(rbuf[9]==stt[(rbuf[7]<<8)+rbuf[8]]) continue;
    sprintf(tb,"%s:%d %02X %02X%02X%02X %02X%02X%02X %02X%02X ... %02X",
      inet_ntoa(from_addr.sin_addr),from_addr.sin_port,
      rbuf[0],rbuf[1],rbuf[2],rbuf[3],rbuf[4],rbuf[5],rbuf[6],
      rbuf[7],rbuf[8],rbuf[9]);
    write_log(logfile,tb);
    stt[(rbuf[7]<<8)+rbuf[8]]=rbuf[9];
    }
  }
