/*  wxt.c              2009.8.5-6 urabe */
/*                     2014.5.13-29 urabe */
/* 64bit? */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
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

#include "udpu.h"
#include "winlib.h"

#define DEBUG     0
#define IOB1      "192.168.208.66"  /* Karacrix at KAE */
#define IOBPORT   20001  /* transparent asynchronous port */
#define WXT       0
#define RECV      1
#define SEND      2

static const char rcsid[] =
  "$Id: wxt.c,v 1.2.2.2 2014/05/30 02:42:27 uehira Exp $";

char *progname, *logfile;
int  syslog_mode, exit_status;

static void
write_data(fil,ptr)
  char *fil;
  char *ptr;
  {
  FILE *fp;
  if(*fil) fp=fopen(fil,"a");
  else fp=stdout;
  fprintf(fp,"%s",ptr);
  if(*fil) fclose(fp);
  }

static int
err_out(ptr)
  char *ptr;
  {
  perror(ptr);
  exit(1);
  }

int
main(int argc, char *argv[])
  {
  int tm[6],len,fromlen,sock,re,mode;
  struct sockaddr_in to_addr,from_addr;
  struct hostent *h;
  char host_name[256],inbuf[1024],outbuf[1024],file[256];
  uint16_t host_port;
  if((progname=strrchr(argv[0],'/'))) progname++;
  else progname=argv[0];

  if(strcmp(progname,"wxtrecv")==0) mode=RECV;
  else if(strcmp(progname,"wxtsend")==0) mode=SEND;
  else mode=WXT;

  *file=0;
  if(argc<2)
    {
    fprintf(stderr," usage : '%s [host] ([port] ([outfile]))'\n",progname);
    exit(1);
    }
  if(argc>3)
    {
    strcpy(file,argv[3]);
    } 
  if(argc>2) host_port=atoi(argv[2]);
  else host_port=IOBPORT;
  strcpy(host_name,argv[1]);

  /* my socket */
  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_out("socket");
  if(mode==SEND)
    {
    if(!(h=gethostbyname(host_name))) err_out("can't find host");
    memset((char *)&to_addr,0,sizeof(to_addr));
    to_addr.sin_family=AF_INET;
    memcpy((caddr_t)&to_addr.sin_addr,h->h_addr,h->h_length);
    to_addr.sin_port=htons(host_port);
    }
  else
    {
  /* bind my socket to a local port */
    memset((char *)&from_addr,0,sizeof(from_addr));
    from_addr.sin_family=AF_INET;
    from_addr.sin_addr.s_addr=htonl(INADDR_ANY);
/*  from_addr.sin_port=htons(0);*/
    from_addr.sin_port=htons(host_port);
    if(bind(sock,(struct sockaddr *)&from_addr,sizeof(from_addr))<0)
      err_out("bind");
    }

  switch(mode)
    {
    case RECV:
      while(1)
        {  
        fromlen=sizeof(from_addr);
        if((len=recvfrom(sock,inbuf,1024,0,
          (struct sockaddr *)&from_addr,&fromlen))<0) err_out("recvfrom");
        fwrite(inbuf,1,len,stdout);
        }
      break;
    case SEND:
      while(fgets(outbuf,1024,stdin))
        {
        outbuf[strlen(outbuf)-1]=0;
        strcat(outbuf,"\r\n");
        if((re=sendto(sock,outbuf,strlen(outbuf),0,
          (struct sockaddr *)&to_addr,sizeof(to_addr)))<0) err_out("sendto");
        printf("%d>%s:%d\n",re,host_name,host_port);
        }
      break;
    default:
      *outbuf=0;
      while(1)
        {
        fromlen=sizeof(from_addr);
        if((len=recvfrom(sock,inbuf,1024,0,
          (struct sockaddr *)&from_addr,&fromlen))<0) err_out("recvfrom");
        if(*outbuf==0)
          {
          get_time(tm);
/*      sprintf(outbuf,"%02d%02d%02d.%02d%02d%02d %s:%d ",
        tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],
        inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port));
*/      sprintf(outbuf,"%02d/%02d/%02d,%02d:%02d:%02d,",
        tm[0],tm[1],tm[2],tm[3],tm[4],tm[5]);
          }
        strncat(outbuf,inbuf,len);
        if(outbuf[strlen(outbuf)-1]==0x0a)
          {
          if(strncmp(outbuf+18,"0R0",3)==0) write_data(file,outbuf);
          *outbuf=0;
          }
        }
      break;
    }
  }
