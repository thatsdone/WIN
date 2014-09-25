/* $Id: recvstatus2.c,v 1.10.2.4 2014/09/25 14:29:02 uehira Exp $ */

/* modified from "recvstatus.c" */
/* 2002.6.19 recvstatus2 receive A8/A9 packets from Datamark LS-7000XT */
/* 2002.7.3 fixed a bug - 'ok' deleted */
/* 2002.7.11 DEBUG(2) */
/* 2010.9.30 daemon mode. 64bit check. */
/* 2014.4.10 update for udp_accept4() */
/* 2014.8.18 increased size of c[] in infoarray from 4000 to 5000 */
/* 2014.8.27 packet order tolerant */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>

#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define DIRNAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define DIRNAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

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

#include "daemon_mode.h"
#include "winlib.h"
#include "udpu.h"

#define NSMAX 100
#define MAXMESG   2048
#define MAXPACKETS 6
/*#define DEBUG   1*/

static const char rcsid[] =
  "$Id: recvstatus2.c,v 1.10.2.4 2014/09/25 14:29:02 uehira Exp $";

char *progname, *logfile = NULL;
int syslog_mode = 0, exit_status = EXIT_SUCCESS;

static int sock;     /* socket */
static int daemon_mode;

/* prototypes */
static void usage(void);
int main(int, char *[]);

static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  if (daemon_mode)
    (void)fprintf(stderr, " usage : '%s (-r) (-g [mcast_group]) (-i [interface]) [port] ([log dir])'\n", progname);
  else
    (void)fprintf(stderr, " usage : '%s (-Dr) (-g [mcast_group]) (-i [interface]) [port] ([log dir])'\n", progname);
}

int
main(int argc, char *argv[])
  {
  uint8_w rbuf[MAXMESG];
  char tb[100],*logdir,logxml[256];
  int i,j,ns,c,rcs,seq,nseq,len;
  ssize_t  n;
  socklen_t  fromlen;
  struct sockaddr_in  from_addr;
  uint16_t to_port;
  struct infoarray {
    int id;
    int ch;
    int seq;
    char c[MAXPACKETS][MAXMESG];
    int len[MAXPACKETS];
    } *s[NSMAX];
  DIR *dir_ptr;
  FILE *fp;
  int  chtmp;
  char mcastgroup[256]; /* multicast address */
  char interface[256]; /* multicast interface */

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];

  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;
  if (strcmp(progname, "recvstatus2d") == 0)
    daemon_mode = 1;

  *interface=(*mcastgroup)=0;
  rcs=0;
  while((c=getopt(argc,argv,"Dg:i:r"))!=-1)
    {
    switch(c)
      {
      case 'D':
	daemon_mode = 1;  /* daemon mode */
	break;
      case 'g':   /* multicast group (multicast IP address) */
	if (snprintf(mcastgroup, sizeof(mcastgroup), "%s", optarg)
	    >= sizeof(mcastgroup)) {
	  fprintf(stderr,"'%s': -g option : Buffer overrun!\n",progname);
	  exit(1);
	}
        break;
      case 'i':   /* interface (ordinary IP address) which receive mcast */
	if (snprintf(interface, sizeof(interface), "%s", optarg)
	    >= sizeof(interface)) {
	  fprintf(stderr,"'%s': -i option : Buffer overrun!\n",progname);
	  exit(1);
	}
        break;
      case 'r':   /* do rcs check-in */
        rcs=1;
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
	usage();
        exit(1);
      }
    }

  optind--;
  if(argc<2+optind)
    {
    usage();
    exit(1);
    }

  to_port=atoi(argv[1+optind]);
  if(argc>2+optind)
    {
    logdir = argv[2+optind];
    if((dir_ptr=opendir(logdir))==NULL) err_sys("opendir");
    if (closedir(dir_ptr)) err_sys("closedir");
    }
  else logdir=NULL;

  if (daemon_mode) {
    syslog_mode = 1;
    daemon_init(progname, LOG_USER, syslog_mode);
    umask(022);
  }

  snprintf(tb,sizeof(tb),"started. port=%d logdir=%s",to_port,logdir);
  write_log(tb);

  if (*mcastgroup)
    sock = udp_accept4(to_port, DEFAULT_RCVBUF, NULL);
  else {
    if (*interface)
      sock = udp_accept4(to_port, DEFAULT_RCVBUF, interface);
    else
      sock = udp_accept4(to_port, DEFAULT_RCVBUF, NULL);
  }
  /* if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket"); */

  /* memset((char *)&to_addr,0,sizeof(to_addr)); */
  /* to_addr.sin_family=AF_INET; */
  /* to_addr.sin_addr.s_addr=htonl(INADDR_ANY); */
  /* to_addr.sin_port=htons(to_port); */

  /* if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0) err_sys("bind"); */

  /* multicast */
  if(*mcastgroup) {
    if (*interface)
      mcast_join(sock, mcastgroup, interface);
    else
      mcast_join(sock, mcastgroup, NULL);
  }

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  ns=0;
  for(;;)
    {
    fromlen=sizeof(from_addr);
    n=recvfrom(sock,rbuf,MAXMESG,0,(struct sockaddr *)&from_addr,&fromlen);
#if DEBUG
printf("n=%zd from %s:%d\n",n,inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port));
for(i=0;i<25;i++) printf(" %02X",rbuf[i]);
printf("\n");
#endif
    if(rbuf[2]==0xA8 || rbuf[2]==0xA9)
      {
      chtmp = (rbuf[11]<<8)+rbuf[12];
      for(i=0;i<ns;i++)
        if(s[i]->ch==chtmp && s[i]->id==rbuf[2])
	  break;
#if DEBUG
printf("ns=%d i=%d\n",ns,i);
#endif
      if(i==ns)
        {
        if(ns==NSMAX)
          {
          for(i=0;i<ns;i++) FREE(s[i]);
          i=ns=0;
          } 
        /* s[i]=(struct infoarray *)malloc(sizeof(struct infoarray)); */
	s[i]=MALLOC(struct infoarray, 1);
	if (s[i] == NULL) err_sys("malloc");
        s[i]->id=rbuf[2];
        s[i]->ch=chtmp;
        for(j=0;j<MAXPACKETS;j++) s[i]->len[j]=0;
        ns++;
        }

      seq=rbuf[14]-1; /* from 0 to nseq-1 */
      nseq=rbuf[13];
      len=(rbuf[3]<<8)+rbuf[4]-12;
      if(nseq>MAXPACKETS || seq>nseq-1 || len>MAXMESG) break;

#if DEBUG
printf("ns=%d i=%d seq=%d/%d len=%d\n",ns,i,seq+1,nseq,len);
#endif

      memcpy(s[i]->c[seq],rbuf+15,s[i]->len[seq]=len);

#if DEBUG
for(j=0;j<MAXPACKETS;j++) printf("seq=%d/%d len=%d\n",j+1,nseq,s[i]->len[j]);
#endif

      for(j=0;j<nseq;j++) if(s[i]->len[j]==0) break;
      if(j==nseq) /* filled up */
        {
        if(logdir != NULL)
          {
          if(s[i]->id==0xA8) {
            if (snprintf(logxml,sizeof(logxml),
                         "%s/S%04X.xml",logdir,s[i]->ch) >= sizeof(logxml))
              err_sys("snprintf");
          }
          else {
            if (snprintf(logxml,sizeof(logxml),
                         "%s/M%04X.xml",logdir,s[i]->ch) >= sizeof(logxml))
              err_sys("snprintf");
          }
          if((fp=fopen(logxml,"w+"))==NULL)
            err_sys(logxml);

          for(j=0;j<nseq;j++) fwrite(s[i]->c[j],1,s[i]->len[j],fp);
          fwrite("\n",1,1,fp);
          fclose(fp);
          if(rcs)
            {
            if (snprintf(tb,sizeof(tb),
                       "ci -l -q %s</dev/null",logxml) >= sizeof(tb))
              err_sys("snprintf");
            system(tb);
            }
          }
        else
          {
          for(j=0;j<nseq;j++) fwrite(s[i]->c[j],1,s[i]->len[j],stdout);
          fwrite("\n",1,1,stdout);
          }
        for(j=0;j<MAXPACKETS;j++) s[i]->len[j]=0;
        }
      }
    }
  }
