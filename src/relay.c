/* $Id: relay.c,v 1.5 2002/01/13 06:57:51 uehira Exp $ */
/* "relay.c"      5/23/94-5/25/94,6/15/94-6/16/94,6/23/94,3/16/95 urabe */
/*                3/26/95 check_packet_no; port# */
/*                5/24/96 added processing of "host table full" */
/*                5/28/96 bzero -> memset */
/*                5/31/96 bug fix */
/*                6/11/96 discard duplicated resent packets */ 
/*                6/23/97 SNDBUF/RCVBUF=65535 */ 
/*                97.9.3               &50000 */ 
/*                98.6.30 FreeBSD */ 
/*                2000.4.24/2001.11.14 strerror() */
/*                2001.11.14 ntohs() */

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

#define DEBUG     0
#define DEBUG1    0
#define DEBUG2    1
#define BELL      0
#define MAXMESG   2048
#define N_PACKET  64    /* N of old packets to be requested */  
#define BUFNO     128

int sock_in,sock_out;   /* socket */
unsigned char sbuf[BUFNO][MAXMESG],sbuf_in[MAXMESG];
int psize[BUFNO],psize_in;
char tb[100],tb1[100],*progname,host_name[100],logfile[256];

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned char *ptr1;
  unsigned long a;
  ptr1=(unsigned char *)&a;
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1  =(*ptr);
  return a;
  }

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
  close(sock_in);
  close(sock_out);
  exit(0);
  }

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  write_log(logfile,ptr);
  if(strerror(errno)) write_log(logfile,strerror(errno));
  close(sock_in);
  close(sock_out);
  ctrlc();
  }

check_pno(from_addr,pn,pn_f,sock,fromlen) /* returns -1 if duplicated */
  struct sockaddr_in *from_addr;  /* sender address */
  unsigned int pn,pn_f;           /* present and former packet Nos. */
  int sock;                       /* socket */
  int fromlen;                    /* length of from_addr */
  {
#define N_HOST  100
  static int host[N_HOST],no[N_HOST];
  static struct {
    int host;
    int port;
    int no;
    unsigned char nos[256/8];
    } h[N_HOST];
  int i,j;
  int host_;  /* 32-bit-long host address */
  int port_;  /* port No. */
  unsigned int pn_1;
  static unsigned int mask[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
  unsigned char pnc;

  j=(-1);
  host_=from_addr->sin_addr.s_addr;
  port_=from_addr->sin_port;
  for(i=0;i<N_HOST;i++)
    {
    if(h[i].host==0) break;
    if(h[i].host==host_ && h[i].port==port_)
      {
      j=h[i].no;
      h[i].no=pn;
      h[i].nos[pn>>3]|=mask[pn&0x03]; /* set bit for the packet no */
      }
    }
  if(i==N_HOST)   /* table is full */
    {
    for(i=0;i<N_HOST;i++) h[i].host=0;
    write_log(logfile,"host table full - flushed.");
    i=0;
    }
  if(j<0)
    {
    h[i].host=host_;
    h[i].port=port_;
    h[i].no=pn;
    h[i].nos[pn>>3]|=mask[pn&0x03]; /* set bit for the packet no */
    sprintf(tb,"registered host %d.%d.%d.%d:%d (%d)",
      ((unsigned char *)&host_)[0],((unsigned char *)&host_)[1],
      ((unsigned char *)&host_)[2],((unsigned char *)&host_)[3],port_,i);
    write_log(logfile,tb);
    }
  else /* check packet no */
    {
    pn_1=(j+1)&0xff;
    if(pn!=pn_1 && ((pn-pn_1)&0xff)<N_PACKET) do
      { /* send request-resend packet(s) */
      pnc=pn_1;
      sendto(sock,&pnc,1,0,from_addr,fromlen);
      sprintf(tb,"request resend %s:%d #%d",
        inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1);
      write_log(logfile,tb);
#if DEBUG1
      printf("<%d ",pn_1);
#endif
      h[i].nos[pn_1>>3]&=~mask[pn_1&0x03]; /* reset bit for the packet no */
      } while((pn_1=(++pn_1&0xff))!=pn);
    }
  if(pn!=pn_f && h[i].nos[pn_f>>3]&mask[pn_f&0x03]) return -1;
     /* if the resent packet is duplicated, return with -1 */
  return 0;
  }

get_packet(bufno,no)
  int bufno;  /* present(next) bufno */
  unsigned char no; /* packet no. to find */
  {
  int i;
  if((i=bufno-1)<0) i=BUFNO-1;
  while(i!=bufno && psize[i]>2)
    {
    if(sbuf[i][0]==no) return i;
    if(--i<0) i=BUFNO-1;
    }
  return -1;  /* not found */
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  struct timeval timeout;
  int i,re,fromlen,n,bufno,bufno_f,pts;
  struct sockaddr_in to_addr,from_addr;
  unsigned short to_port;
  struct hostent *h;
  unsigned short host_port,ch;
  unsigned char no,no_f;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<4)
    {
    fprintf(stderr," usage : '%s [in_port] [host] [host_port] ([log file])'\n",
      progname);
    exit(1);
    }
  to_port=atoi(argv[1]);
  strcpy(host_name,argv[2]);
  host_port=atoi(argv[3]);
  if(argc>4) strcpy(logfile,argv[4]);
  else *logfile=0;

  sprintf(tb,"start in_port=%d to host '%s' port=%d",
    to_port,host_name,host_port);
  write_log(logfile,tb);

  /* 'in' port */
  if((sock_in=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("sock_in");
  i=65535;
  if(setsockopt(sock_in,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))<0)
    {
    i=50000;
    if(setsockopt(sock_in,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))<0)
      err_sys("SO_RCVBUF setsockopt error\n");
    }
  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  to_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  to_addr.sin_port=htons(to_port);
  if(bind(sock_in,(struct sockaddr *)&to_addr,sizeof(to_addr))<0)
    err_sys("bind_in");

  /* destination host/port */
  if(!(h=gethostbyname(host_name))) err_sys("can't find host");
  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
/*  to_addr.sin_addr.s_addr=inet_addr(inet_ntoa(h->h_addr));*/
  to_addr.sin_addr.s_addr=mklong(h->h_addr);
  to_addr.sin_port=htons(host_port);
  if((sock_out=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("sock_out");
  i=1;
  if(setsockopt(sock_out,SOL_SOCKET,SO_BROADCAST,(char *)&i,sizeof(i))<0)
    err_sys("SO_BROADCAST setsockopt error\n");
  i=65535;
  if(setsockopt(sock_out,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0)
    {
    i=50000;
    if(setsockopt(sock_out,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0)
      err_sys("SO_SNDBUF setsockopt error\n");
    }
  /* bind my socket to a local port */
  memset((char *)&from_addr,0,sizeof(from_addr));
  from_addr.sin_family=AF_INET;
  from_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  from_addr.sin_port=htons(0);
  if(bind(sock_out,(struct sockaddr *)&from_addr,sizeof(from_addr))<0)
    err_sys("bind_out");

  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  no=0;
  for(i=0;i<BUFNO;i++) psize[i]=2;
  bufno=0;
  timeout.tv_sec=timeout.tv_usec=0;

  while(1)
    {
    fromlen=sizeof(from_addr);
    psize[bufno]=recvfrom(sock_in,sbuf[bufno],MAXMESG,0,&from_addr,&fromlen);
#if DEBUG1
    if(sbuf[bufno][0]==sbuf[bufno][1]) printf("%d ",sbuf[bufno][0]);
    else printf("%d(%d) ",sbuf[bufno][0],sbuf[bufno][1]);
#endif

    if(check_pno(&from_addr,sbuf[bufno][0],sbuf[bufno][1],sock_in,fromlen)<0)
      {
#if DEBUG2
      sprintf(tb,"discard duplicated resent packet #%d as #%d",
        sbuf[bufno][0],sbuf[bufno][1]);
      write_log(logfile,tb);
#endif
      continue;
      }

    sbuf[bufno][0]=sbuf[bufno][1]=no;
    re=sendto(sock_out,sbuf[bufno],psize[bufno],0,&to_addr,sizeof(to_addr));
#if DEBUG
    for(i=0;i<8;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
    fprintf(stderr," : %d > %d\n",psize[bufno],re);
#endif
#if DEBUG1
    printf(">%d ",no);
#endif
    if(++bufno==BUFNO) bufno=0;
    no++;

    while(1)
      {
      i=1<<sock_out;
      if(select(sock_out+1,&i,NULL,NULL,&timeout)>0)
        {
        fromlen=sizeof(from_addr);
        if(recvfrom(sock_out,sbuf[bufno],MAXMESG,0,&from_addr,&fromlen)==1 &&
            (bufno_f=get_packet(bufno,no_f=sbuf[bufno][0]))>=0)
          {
          memcpy(sbuf[bufno],sbuf[bufno_f],psize[bufno]=psize[bufno_f]);
          sbuf[bufno][0]=no;    /* packet no. */
          sbuf[bufno][1]=no_f;  /* old packet no. */
          re=sendto(sock_out,sbuf[bufno],psize[bufno],0,&to_addr,
            sizeof(to_addr));
          sprintf(tb,"resend to %s:%d #%d as #%d, %d B",
            inet_ntoa(to_addr.sin_addr),ntohs(to_addr.sin_port),no_f,no,re);
          write_log(logfile,tb);
#if DEBUG
          for(i=0;i<8;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
          fprintf(stderr," : %d > %d\n",psize[bufno],re);
#endif
#if DEBUG1
          printf(">%d ",no);
#endif
          if(++bufno==BUFNO) bufno=0;
          no++;
          }
        }
      else break;
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    }
  }
