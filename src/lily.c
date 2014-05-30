/* $Id $ */
/* "lily.c"      2010.10.19-20     urabe */
/*               modified from recvt.c */
/*               2011.4.15 urabe */
/*               2014.5.13-29 urabe */
/* 64bit? */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/param.h>
#include <sys/stat.h>

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

#include "udpu.h"
#include "winlib.h"

#define DEBUG     0
#define MAXMESG   2048

static const char rcsid[] =
  "$Id: lily.c,v 1.2.2.2 2014/05/30 02:42:25 uehira Exp $";

char *progname,*logfile;
int  syslog_mode, exit_status;

static int
make_time(rt,ltime)
  int *rt;
  time_t ltime;
  {
  struct tm *nt;
  nt=localtime(&ltime);
  rt[0]=nt->tm_year%100;
  rt[1]=nt->tm_mon+1;
  rt[2]=nt->tm_mday;
  rt[3]=nt->tm_hour;
  rt[4]=nt->tm_min;
  rt[5]=nt->tm_sec;
  }

int
main(int argc, char *argv[])
  {
  uint8_w rb[MAXMESG],rbuf[MAXMESG];
  key_t shm_key;
  int shmid;
  WIN_ch sysch,sysch2;
  uint32_w uni;  /*- 64bit ok -*/
  WIN_bs  uni2;  /*- 64bit ok -*/
  uint8_w *ptr,tm[6],*ptr_size,mdy[10],hms[10],serno[10],*p,*pn,*pw;
  int i,j,k,size,fromlen,n,nn,re,sock,c,pl,use_ts,t[6],itvl,diff,d,d_valid,
      sbuf,adiff,extout;
  struct sockaddr_in from_addr;
  uint16_t to_port,host_port;
  double tiltx,tilty,magnet,temp,volt;
  struct tm mt;
  int32_w x,y,mg,tp;

  struct Shm *sh;
  char tb[256];
  char mcastgroup[256]; /* multicast address */
  char interface[256]; /* multicast interface */
  time_t rt,ts,sec,nxt;
  struct hostent *h;
  struct timeval timeout;

  if((progname=strrchr(argv[0],'/'))) progname++;
  else progname=argv[0];

  snprintf(tb,sizeof(tb),
    " usage : '%s (-t) (-e [ch_base2]) (-g [mcast_group]) (-i [interface]) (-s itvl(m)) \\\n\
      [port] [ch_base] [shm_key] [shm_size(KB)] ([log file]))'",progname);
  
  use_ts=itvl=extout=0;
  *interface=(*mcastgroup)=0;
  sbuf=DEFAULT_RCVBUF;
  while((c=getopt(argc,argv,"e:i:g:s:t"))!=EOF)
    {
    switch(c)
      {
      case 'i':   /* interface (ordinary IP address) which receive data */
        strcpy(interface,optarg);
        break;
      case 'g':   /* multicast group (multicast IP address) */
        strcpy(mcastgroup,optarg);
        break;
      case 's':   /* status report to logfile every itvl min */
        itvl=atoi(optarg);
        break;
      case 'e':   /* extended output: magnetic compass heading and temperature */
        extout=1;
        sysch2=strtol(optarg,0,16);
        break;
      case 't':   /* use lily's time stamp */
        use_ts=1;
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
        exit(1);
      }
    }
  optind--;
  if(argc<5+optind)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }
  to_port=atoi(argv[1+optind]);
  sysch=strtol(argv[2+optind],0,16);
  shm_key=atoi(argv[3+optind]);
  size=atoi(argv[4+optind])*1000;
  if(argc>5+optind) logfile=argv[5+optind];
  else logfile=NULL;
  /* shared memory */
  write_log("start");
  sh = Shm_create(shm_key, size, "out");

  /* initialize buffer */
  Shm_init(sh, size);
  pl = sh->pl;

  if(*mcastgroup) *tb=0;
  else strcpy(tb,interface);
  sock = udp_accept4(to_port, sbuf, tb);
  if(*mcastgroup){
    mcast_join(sock, mcastgroup, interface);
  }

  snprintf(tb,sizeof(tb),"listen port=%d",to_port);
  write_log(tb);
  if(*mcastgroup) {
    snprintf(tb,sizeof(tb),"multicast group=%s",mcastgroup);
    write_log(tb);
    if(*interface) {
      snprintf(tb,sizeof(tb),"interface=%s",interface);
      write_log(tb);
    }
  } 

  if(use_ts) write_log("use lily's time stamp");
  else write_log("use system's time stamp");

  if(itvl) {
    snprintf(tb,sizeof(tb),"log status (i.e. raw data) every %d min",itvl);
    write_log(tb);
  }

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  ptr=ptr_size=sh->d+sh->p;
  nxt=0;
  d_valid=0;

  pw=rbuf;
  while(1)
    {
    k=1<<sock;
    timeout.tv_sec=0;
    timeout.tv_usec=500000;
    if(select(sock+1,(fd_set *)&k,NULL,NULL,&timeout)<=0) continue;
    fromlen=sizeof(from_addr);
    nn=recvfrom(sock,rb,MAXMESG,0,(struct sockaddr *)&from_addr,&fromlen);
    rb[nn]=0;
#if DEBUG
    printf("(%d)%s",nn,rb);
#endif
    p=rb;
    /* multiple lines and/or fraction of line may be in a packet */
    while((p<rb+nn) && (pn=strchr(p,'\n')))
      { /* loop for each line */
      *pn++=0;
      strcpy(pw,p);
      n=strlen(rbuf);
      p=pn;
      pw=rbuf;
#if DEBUG
      printf(" (%d)%s\n",n,rbuf);
#endif
      if(*rbuf!='$')
        {
        write_log(rbuf);
        continue;
        }
      time(&rt);
      if(nxt>=0 && rt>=nxt) /* write status (=raw data) to logfile */
        {
        write_log(rbuf);
        if(itvl) nxt=rt+itvl*60;
        else nxt=(-1);
        }
      for(i=0;i<n;i++)
        if(rbuf[i]=='$' || rbuf[i]==',' || rbuf[i]=='/' || rbuf[i]==':')
          rbuf[i]=' ';
      sscanf(rbuf,"%lf%lf%lf%lf%d%d%d%d%d%d%lf%s",
        &tiltx,&tilty,&magnet,&temp,t+1,t+2,t,t+3,t+4,t+5,&volt,serno);
      x=(int32_w)(tiltx*1000.0); /* in nano radian */
      y=(int32_w)(tilty*1000.0);
      if(extout)
        {
        mg=(int32_w)(magnet*100.0); /* 100 * 0 - 360 deg clkws from N */
        tp=(int32_w)(temp*100.0); /* 100 * deg C */
        }

#if DEBUG
      printf("%d %d %.2f %.2f %02d %02d %02d %02d %02d %02d %.2f %s\n",
        x,y,magnet,temp,t[0],t[1],t[2],t[3],t[4],t[5],volt,serno);
#endif
      if((mt.tm_year=t[0])<70) mt.tm_year+=100;
      mt.tm_mon=t[1]-1;
      mt.tm_mday=t[2];
      mt.tm_hour=t[3];
      mt.tm_min=t[4]; 
      mt.tm_sec=t[5]; 
      mt.tm_isdst=0;
      ts=mktime(&mt);
      diff=ts-rt;
      if(d_valid)
        {
        adiff=abs(diff-d);
        if(adiff>2) d=diff;
        else if(adiff==2) d=(diff+d)/2;
        }
      else
        {
        d=diff;
        d_valid=1;
        }
#if DEBUG
      printf("diff=%ds  d=%ds\n",diff,d);
#endif
      if(use_ts) sec=ts;
      else sec=ts-d;

      make_time(t,sec);
      for(i=0;i<6;i++) tm[i]=d2b[t[i]]; /* make TS */

      ptr_size=ptr;
      ptr+=4;   /* size */
      ptr+=4;   /* time of write */
      memcpy(ptr,tm,6);
      ptr+=6;
      ptr+=winform(&x,ptr,1,sysch);
      ptr+=winform(&y,ptr,1,sysch+1);
      if(extout)
        {
        ptr+=winform(&mg,ptr,1,sysch2);
        ptr+=winform(&tp,ptr,1,sysch2+1);
        }
      uni2=(WIN_bs)(ptr-ptr_size);
      ptr_size[0]=uni2>>24;  /* size (H) */
      ptr_size[1]=uni2>>16;
      ptr_size[2]=uni2>>8;
      ptr_size[3]=uni2;      /* size (L) */
      uni=(uint32_w)(time(NULL)-TIME_OFFSET);
      ptr_size[4]=uni>>24;  /* tow (H) */
      ptr_size[5]=uni>>16;
      ptr_size[6]=uni>>8;
      ptr_size[7]=uni;      /* tow (L) */
#if DEBUG
      printf("size=%d\n",ptr-ptr_size);
#endif
      sh->r=sh->p;      /* latest */
      if(ptr>sh->d+sh->pl) ptr=sh->d;
      sh->p=ptr-sh->d;
      sh->c++;
      }
    if(p<rb+nn)
      {
      strcpy(pw,p);
      pw+=strlen(pw);
      }
    }
  }
