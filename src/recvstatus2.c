/* $Id: recvstatus2.c,v 1.6.8.8.2.2 2011/01/12 16:57:06 uehira Exp $ */

/* modified from "recvstatus.c" */
/* 2002.6.19 recvstatus2 receive A8/A9 packets from Datamark LS-7000XT */
/* 2002.7.3 fixed a bug - 'ok' deleted */
/* 2002.7.11 DEBUG(2) */
/* 2010.9.30 daemon mode. 64bit check. */

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
/* #define DEBUG   0 */

static const char rcsid[] =
  "$Id: recvstatus2.c,v 1.6.8.8.2.2 2011/01/12 16:57:06 uehira Exp $";

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
    (void)fprintf(stderr, " usage : '%s (-r) [port] ([log dir])'\n", progname);
  else
    (void)fprintf(stderr, " usage : '%s (-Dr) [port] ([log dir])'\n", progname);
}

int
main(int argc, char *argv[])
  {
  uint8_w rbuf[MAXMESG];
  char tb[100],*logdir,logxml[256];
  int i,ns,c,rcs;
  ssize_t  n;
  socklen_t  fromlen;
  struct sockaddr_in  from_addr;
  uint16_t to_port;
  struct infoarray {
    in_addr_t adrs;
    in_port_t port;
    int id;
    char t[6];
    int ch;
    int seq;
    char c[4000];
    int len;
    } *s[NSMAX];
  DIR *dir_ptr;
  FILE *fp;
  int  chtmp;

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];

  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;
  if (strcmp(progname, "recvstatus2d") == 0)
    daemon_mode = 1;

  rcs=0;
  while((c=getopt(argc,argv,"Dr"))!=-1)
    {
    switch(c)
      {
      case 'D':
	daemon_mode = 1;  /* daemon mode */
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

  sock = udp_accept4(to_port, DEFAULT_RCVBUF);
  /* if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket"); */

  /* memset((char *)&to_addr,0,sizeof(to_addr)); */
  /* to_addr.sin_family=AF_INET; */
  /* to_addr.sin_addr.s_addr=htonl(INADDR_ANY); */
  /* to_addr.sin_port=htons(to_port); */

  /* if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0) err_sys("bind"); */

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
	if (s[i] == NULL)
	  err_sys("malloc");
        s[i]->adrs=from_addr.sin_addr.s_addr; 
        s[i]->port=from_addr.sin_port; 
        s[i]->seq=1;
        s[i]->len=0;
        ns++;
        }
#if DEBUG
printf("ns=%d i=%d s[i]->seq=%d s[i]->len=%d\n",ns,i,s[i]->seq,s[i]->len);
#endif
      if(rbuf[14]==1)
        {
        memcpy(s[i]->t,rbuf+5,6);
        s[i]->id=rbuf[2];
        s[i]->ch=(rbuf[11]<<8)+rbuf[12];
        s[i]->len=0;
        }
      else if(!(s[i]->id==rbuf[2] && s[i]->seq==rbuf[14])) continue;

      memcpy(s[i]->c+s[i]->len,rbuf+15,(rbuf[3]<<8)+rbuf[4]-12);
      s[i]->len+=(rbuf[3]<<8)+rbuf[4]-12;
#if DEBUG
printf("s[i]->seq=%d s[i]->len=%d\n",s[i]->seq,s[i]->len);
#endif
      if(rbuf[13]==rbuf[14])
        {
        s[i]->seq=1;
        s[i]->c[s[i]->len]='\n';
#if DEBUG
printf("%s",s[i]->c);
#endif
        if(logdir != NULL)
          {
	  if(rbuf[2]==0xA8) {
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
          fwrite(s[i]->c,1,s[i]->len+1,fp);
          fclose(fp);
          if(rcs)
            {
	    if (snprintf(tb,sizeof(tb),
			 "ci -l -q %s</dev/null",logxml) >= sizeof(tb))
	      err_sys("snprintf");
            system(tb);
            }
          }
        else fwrite(s[i]->c,1,s[i]->len+1,stdout);
        }
      else s[i]->seq=rbuf[14]+1;
      }
    }
  }
