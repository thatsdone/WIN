/* $Id: vanpro2.c,v 1.4 2016/01/05 06:38:49 uehira Exp $ */
/* "vanpro2.c"   2012.2.10-4.18     urabe */
/* 2014.5.20-29 */
/* 64bit? */
/* 2015.12.25  Sample size 5 mode supported. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include <syslog.h>

#include "udpu.h"
#include "winlib.h"

#define DEBUG2    0
#define MAXMESG   2048

char *progname,*logfile;
int  syslog_mode, exit_status;

static int16_w mkshort(unsigned char *);
int main(int, char *[]);

static int16_w
mkshort(unsigned char *ptr)
{
  int16_w a;

  a = ((ptr[1] << 8) & 0xff00) + (ptr[0] & 0xff);
  return (a);
}

int
main(int argc, char *argv[])
  {
  time_t rt,rt_prev,nxt;
  key_t shm_key;
  uint32_w uni;
  WIN_ch sysch;
  uint8_w *ptr,*ptr_size,*p,host_name[256],tbuf[1024];
  int i,c,size,fromlen,sock_in,sock_out,itvl,len,tm[6],sbuf;
  struct sockaddr_in to_addr,from_addr;
  uint16_t my_port,host_port;
  int baro,t_in,t_out,w_dir,r_rate,r_day,batt,h_in,w_sp,w_av,h_out;
  int   ss_mode = SSIZE5_MODE, ssf_flag = 0;

  struct Shm *sh;
  char tb[256];
  char mcastgroup[256]; /* multicast address */
  char interface[256],sinterface[256]; /* interface */
  struct timeval timeout;
#if DEBUG
  int   j;
#endif

  if((progname=strrchr(argv[0],'/'))) progname++;
  else progname=argv[0];

  snprintf(tb,sizeof(tb),
    " usage : '%s (-b [4|5|5f]) (-i [interface]) (-g [mcast_group]) (-s itvl(m)) \\\n\
      [remIP:remport] [myport] [ch_base] [shm_key] [shm_size(KB)] ([log file]))'",progname);
  
  itvl=0;
  sbuf=DEFAULT_RCVBUF;
  *interface=(*mcastgroup)=(*sinterface)=0;
  while((c=getopt(argc,argv,"b:i:g:s:"))!=EOF)
    {
    switch(c)
      {
      case 'b':
	if (strcmp(optarg, "4") == 0)
	  ss_mode = 0;
	else if (strcmp(optarg, "5") == 0) {
	  ss_mode = 1;
	  ssf_flag = 0;
	} else if (strcmp(optarg, "5f") == 0) {
	  ss_mode = 1;
	  ssf_flag = 1;
	} else {
	  fprintf(stderr, "Invalid option: -%c\n", c);
	  fprintf(stderr,"%s\n",tb);
	  exit(1);
	}
	break;
      case 'i':   /* interface (ordinary IP address) for receive */
        strcpy(interface,optarg);
        break;
      case 'g':   /* multicast group (multicast IP address) */
        strcpy(mcastgroup,optarg);
        break;
      case 's':   /* status report to logfile every itvl min */
        itvl=atoi(optarg);
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
        exit(1);
      }
    }
  optind--;
  if(argc<6+optind)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }

  strcpy(host_name,argv[1+optind]);
  if((ptr=strchr(host_name,':'))==0)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }
  *ptr=0;
  host_port=atoi(ptr+1);
  my_port=atoi(argv[2+optind]);
  sysch=strtol(argv[3+optind],0,16);
  shm_key=atoi(argv[4+optind]);
  size=atoi(argv[5+optind])*1000;
  if(argc>6+optind) logfile=argv[6+optind];
  else logfile=NULL;

  /* shared memory */
  write_log("start");
  sh = Shm_create(shm_key, size, "out");

  /* initialize buffer */
  Shm_init(sh, size);

  /* send socket */
  if (*sinterface)
    sock_out = udp_dest4(host_name, host_port, &to_addr, 64, 0, sinterface);
  else
    sock_out = udp_dest4(host_name, host_port, &to_addr, 64, 0, NULL);

  /* receive socket */
  if(*mcastgroup)
    sock_in = udp_accept4(my_port, sbuf, NULL);
  else {
    if (*interface)
      sock_in = udp_accept4(my_port, sbuf, interface);
    else
      sock_in = udp_accept4(my_port, sbuf, NULL);
  }
  if(*mcastgroup){
    if (*interface)
      mcast_join(sock_in, mcastgroup, interface);
    else
      mcast_join(sock_in, mcastgroup, NULL);
  }

  snprintf(tb,sizeof(tb),"peer=%s:%d, listen port=%d",host_name,host_port,my_port);
  write_log(tb);

  snprintf(tb,sizeof(tb),"base ch=%04X",sysch);
  write_log(tb);
  snprintf(tb,sizeof(tb),"  baro[%04X] t_in[%04X] h_in[%04X] t_out[%04X] w_sp[%04X] w_av[%04X]",
    sysch,sysch+1,sysch+2,sysch+3,sysch+4,sysch+5);
  write_log(tb);
  snprintf(tb,sizeof(tb),"  w_dir[%04X] h_out[%04X] r_rate[%04X] r_day[%04X] batt[%04X]",
    sysch+6,sysch+7,sysch+8,sysch+9,sysch+10);
  write_log(tb);

  if(itvl) {
    snprintf(tb,sizeof(tb),"log status (i.e. raw data) every %d min",itvl);
    write_log(tb);
  }

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  ptr=ptr_size=sh->d+sh->p;
  nxt=0;

  strcpy(tbuf,"\n");
  sendto(sock_out,tbuf,strlen(tbuf),0,(struct sockaddr *)&to_addr,sizeof(to_addr));
  rt_prev=get_time(tm);

  while(1)
    {
    if((rt=get_time(tm))!=rt_prev)
      {
      rt_prev=rt;
      strcpy(tbuf,"LOOP 1\n");
      sendto(sock_out,tbuf,strlen(tbuf),0,(struct sockaddr *)&to_addr,sizeof(to_addr));
      tbuf[strlen(tbuf)-1]=' ';
#if DEBUG
      printf("%s>%s:%d\n",tbuf,host_name,host_port);
#endif
      }
    i=1<<sock_in;
    timeout.tv_sec=timeout.tv_usec=0;
    if(select(sock_in+1,(fd_set *)&i,NULL,NULL,&timeout)>0)
      {
      fromlen=sizeof(from_addr);
      if((len=recvfrom(sock_in,tbuf,1024,0,(struct sockaddr *)&from_addr,&fromlen))<0)
        err_sys("recvfrom");

#if DEBUG
      printf("%s:%d(%d) ",inet_ntoa(from_addr.sin_addr),
        ntohs(from_addr.sin_port),len);
      if(len>30) j=30;
      else j=len;
      p=tbuf;
      for(i=0;i<j;i++) printf("%02X",*p++);
      printf("...\n");
#endif
      p=strchr(tbuf,'L');
      if(p==NULL) continue;
      baro=(double)mkshort(p+7)*0.001*25.4*1013.25/760.0; /* hPa */
      t_in=((double)mkshort(p+9)*0.1-32.0)*5.0*10.0/9.0; /* 0.1 deg C */
      h_in=p[11];  /* % */
      t_out=((double)mkshort(p+12)*0.1-32.0)*5.0*10.0/9.0; /* 0.1 deg C */
      w_sp=((double)p[14]*1.6093*1000.0*10/3600.0);  /* 0.1m/s */
      w_av=((double)p[15]*1.6093*1000.0*10/3600.0);  /* 0.1m/s */
      w_dir=mkshort(p+16); /* deg from N cw */
      h_out=p[33];  /* % */
      r_rate=mkshort(p+41)*2; /* 0.1mm/hour */
      r_day=mkshort(p+50)*2;  /* 0.1mm */
      batt=mkshort(p+87)*300/512; /* 0.01V */

#if DEBUG2
      printf("%02d%02d%02d.%02d%02d%02d ",tm[0],tm[1],tm[2],tm[3],tm[4],tm[5]);
      printf("baro=%d t_in=%d h_in=%d t_out=%d w_sp=%d w_av=%d w_dir=%d h_out=%d r_rate=%d r_day=%d batt=%d\n",
        baro,t_in,h_in,t_out,w_sp,w_av,w_dir,h_out,r_rate,r_day,batt);
#endif
      time(&rt);
      if(nxt>=0 && rt>=nxt) /* write status (i.e. data) to logfile */
        {
        snprintf(tb,sizeof(tb),
          "baro=%d t_in=%d h_in=%d t_out=%d w_sp=%d w_av=%d w_dir=%d h_out=%d r_rate=%d r_day=%d batt=%d",
          baro,t_in,h_in,t_out,w_sp,w_av,w_dir,h_out,r_rate,r_day,batt);
        write_log(tb);
        if(itvl) nxt=rt+itvl*60;
        else nxt=(-1);
        }

      ptr_size=ptr;
      ptr+=4;   /* size */
      ptr+=4;   /* time of write */
      for(i=0;i<6;i++) *ptr++=d2b[tm[i]]; /* make TS */
      i=sysch;
      /* ptr+=winform(&baro,ptr,1,i++); */
      /* ptr+=winform(&t_in,ptr,1,i++); */
      /* ptr+=winform(&h_in,ptr,1,i++); */
      /* ptr+=winform(&t_out,ptr,1,i++); */
      /* ptr+=winform(&w_sp,ptr,1,i++); */
      /* ptr+=winform(&w_av,ptr,1,i++); */
      /* ptr+=winform(&w_dir,ptr,1,i++); */
      /* ptr+=winform(&h_out,ptr,1,i++); */
      /* ptr+=winform(&r_rate,ptr,1,i++); */
      /* ptr+=winform(&r_day,ptr,1,i++); */
      /* ptr+=winform(&batt,ptr,1,i++); */
      ptr += mk_windata(&baro, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&t_in, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&h_in, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&t_out, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&w_sp, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&w_av, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&w_dir, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&h_out, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&r_rate, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&r_day, ptr, 1, i++, ss_mode, ssf_flag);
      ptr += mk_windata(&batt, ptr, 1, i++, ss_mode, ssf_flag);
      uni=ptr-ptr_size;
      ptr_size[0]=uni>>24;  /* size (H) */
      ptr_size[1]=uni>>16;
      ptr_size[2]=uni>>8;
      ptr_size[3]=uni;      /* size (L) */
      uni=time(0);
      ptr_size[4]=uni>>24;  /* tow (H) */
      ptr_size[5]=uni>>16;
      ptr_size[6]=uni>>8;
      ptr_size[7]=uni;      /* tow (L) */
#if DEBUG
      printf("size=%td : ",ptr-ptr_size);
      p=ptr_size;
      if((j=ptr-ptr_size)>30) j=30;
      for(i=0;i<j;i++) printf("%02X",*p++);
      printf("... > %ld\n",shm_key);
#endif
      sh->r=sh->p;      /* latest */
      if(ptr>sh->d+sh->pl) ptr=sh->d;
      sh->p=ptr-sh->d;
      sh->c++;
      }
    else usleep(100000);
    }
  }

