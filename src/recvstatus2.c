/* $Id: recvstatus2.c,v 1.1 2002/06/19 02:31:58 urabe Exp $ */
/* "recvstatus.c"      5/24/95    urabe */
/* 97.7.17 two lines of "if() continue;" in the main loop */
/* 2000.4.24/2001.11.14 strerror() */
/* 2001.11.14 ntohs() */
/* 2002.6.19 receive A8/A9 packets from Datamark LS-7000XT */

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
#include <dirent.h>

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

ctrlc()
  {
  close(sock);
  exit(0);
  }

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  close(sock);
  ctrlc();
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
#define NSMAX 100
  unsigned char rbuf[MAXMESG];
  char tb[100],*progname,logdir[256],logfile[256];
  int i,j,k,fromlen,n,re,ns,ok,c,rcs;
  extern int optind;
  extern char *optarg;
  struct sockaddr_in to_addr,from_addr;
  unsigned short to_port;
  struct {
    int adrs;
    int port;
    int id;
    char t[6];
    int ch;
    int seq;
    char c[4000];
    int len;
    } **s;
  DIR *dir_ptr;
  FILE *fp;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  sprintf(tb," usage : '%s (-r) [port] ([log dir])'",progname);
  rcs=0;
  while((c=getopt(argc,argv,"r"))!=EOF)
    {
    switch(c)
      {
      case 'r':   /* do rcs check-in */
        rcs=1;
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
        exit(1);
      }
    }
  optind--;
  if(argc<2+optind)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }

  to_port=atoi(argv[1+optind]);
  if(argc>2+optind)
    {
    strcpy(logdir,argv[2+optind]);
    if((dir_ptr=opendir(logdir))==NULL) err_sys("opendir");
    }
  else *logdir=0;

  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");

  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  to_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  to_addr.sin_port=htons(to_port);

  if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0) err_sys("bind");

  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGPIPE,(void *)ctrlc);

  ns=0;
  while(1)
    {
    fromlen=sizeof(from_addr);
    n=recvfrom(sock,rbuf,MAXMESG,0,(struct sockaddr *)&from_addr,&fromlen);
    if(rbuf[2]==0xA8 || rbuf[2]==0xA9)
      {
      for(i=0;i<ns;i++)
        if(s[i]->adrs==from_addr.sin_addr.s_addr &&
          s[i]->port==from_addr.sin_port) break;
      if(i==ns)
        {
        if(ns==NSMAX)
          {
          for(i=0;i<ns;i++) free(s[i]);
          i=ns=0;
          } 
        s[i]=malloc(sizeof(*s));
        s[i]->adrs=from_addr.sin_addr.s_addr; 
        s[i]->port=from_addr.sin_port; 
        s[i]->seq=1;
        s[i]->len=0;
        ns++;
        }
      if(rbuf[14]==1)
        {
        memcpy(s[i]->t,rbuf+5,6);
        s[i]->id=rbuf[2];
        s[i]->ch=(rbuf[11]<<8)+rbuf[12];
        s[i]->len=0;
        ok=1;
        }
      else if(s[i]->id==rbuf[2] && s[i]->seq==rbuf[14])
        {
        ok=1;
        }
      if(!ok) continue;
      memcpy(s[i]->c+s[i]->len,rbuf+15,(rbuf[3]<<8)+rbuf[4]-12);
      s[i]->len+=(rbuf[3]<<8)+rbuf[4]-12;
      if(rbuf[13]==rbuf[14])
        {
        s[i]->seq=1;
        s[i]->c[s[i]->len]='\n';
        if(*logdir)
          {
          if(rbuf[2]==0xA8) sprintf(logfile,"%s/S%04X.xml",logdir,s[i]->ch);
          else sprintf(logfile,"%s/M%04X.xml",logdir,s[i]->ch);
          if((fp=fopen(logfile,"w+"))==NULL)
            {
            fprintf(stderr,"file '%s' not open !\n",tb);
            ctrlc();
            }
          fwrite(s[i]->c,1,s[i]->len+1,fp);
          fclose(fp);
          if(rcs)
            {
            sprintf(tb,"ci -l -q %s</dev/null",logfile);
            system(tb);
            }
          }
        else fwrite(s[i]->c,1,s[i]->len+1,stdout);
        }
      else s[i]->seq=rbuf[14]+1;
      }
    }
  }
