/* $Id: send_raw_old.c,v 1.8 2004/11/26 14:09:45 uehira Exp $ */
/*
    program "send_raw_old/send_mon_old.c"   1/24/94 - 1/25/94,5/25/94 urabe
                                    6/15/94 - 6/16/94
            "sendt_raw_old/sendt_mon_old.c" 12/6/94 - 12/6/94
                                    3/15/95-3/26/95 write_log(), read_chfile()
                                    2/29/96  exit if chfile does not open
                                    97.8.5 fgets/sscanf
                                    99.2.4 moved signal(HUP) to read_chfile()
                                    99.9.10 byte-order-free
                                  2000.4.17 deleted definition of usleep()
                                  2000.4.24 strerror()
                          2000.9.7 multiple resend-request by MEISEI < 1/30/97
                                  2001.11.14 strerror(),ntohs()
				  2004.11.26 some systems (exp. Linux), select(2) changes timeout value
*/

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
#include <unistd.h>
#include <errno.h>

#include "subst_func.h"

#define DEBUG     0
#define MAXMESG   1024
#define SR_MON    5
#define BUFNO     128

int sock,raw,mon,tow,psize[BUFNO],n_ch;
unsigned char sbuf[BUFNO][MAXMESG],ch_table[65536],rbuf[MAXMESG];
char *progname,logfile[256],chfile[256];

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

mkshort(ptr)
  unsigned char *ptr;
  {
  unsigned short a;
  a=((ptr[0]<<8)&0xff00)+(ptr[1]&0xff);
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
  close(sock);
  exit(0);
  }

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  write_log(logfile,ptr);
  if(strerror(errno)) write_log(logfile,strerror(errno));
  write_log(logfile,"end");
  close(sock);
  exit(1);
  }

get_packet(bufno,no)
  int bufno;  /* present(next) bufno */
  unsigned char no; /* packet no. to find */
  {
  int i;
  if((i=bufno-1)<0) i=BUFNO-1;
  while(i!=bufno && psize[i]>0)
    {
    if(sbuf[i][0]==no) return i;
    if(--i<0) i=BUFNO-1;
    }
  return -1;  /* not found */
  }

read_chfile()
  {
  FILE *fp;
  int i,j,k;
  char tbuf[1024];
  if(*chfile)
    {
    if((fp=fopen(chfile,"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile);
#endif
      for(i=0;i<65536;i++) ch_table[i]=0;
      i=j=0;
      while(fgets(tbuf,1024,fp))
        {
        if(*tbuf=='#' || sscanf(tbuf,"%x",&k)<0) continue;
        k&=0xffff;
#if DEBUG
        fprintf(stderr," %04X",k);
#endif
        if(ch_table[k]==0)
          {
          ch_table[k]=1;
          j++;
          }
        i++;
        }
#if DEBUG
      fprintf(stderr,"\n",k);
#endif
      n_ch=j;
      sprintf(tbuf,"%d channels",n_ch);
      write_log(logfile,tbuf);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile);
#endif
      sprintf(tbuf,"channel list file '%s' not open",chfile);
      write_log(logfile,tbuf);
      write_log(logfile,"end");
      close(sock);
      exit(1);
      }
    }
  else
    {
    for(i=0;i<65536;i++) ch_table[i]=1;
    n_ch=i;
    write_log(logfile,"all channels");
    }
  signal(SIGHUP,(void *)read_chfile);
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  FILE *fp;
  key_t shm_key;
  union {
    unsigned long i;
    unsigned short s;
    char c[4];
    } un;
  struct timeval timeout;
  int i,j,k,c_save,shp,aa,bb,ii,bufno,bufno_f,fromlen;
  struct sockaddr_in to_addr,from_addr;
  struct hostent *h;
  unsigned short host_port,ch;
  int size,gs,gh,sr,re,shmid;
  unsigned char *ptr,*ptr1,*ptr_save,*ptr_lim,*ptw,*ptw_save,no,no_f,
    host_name[100],tbuf[100];
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *shm;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];

  raw=mon=tow=0;
  if(strcmp(progname,"send_raw_old")==0) raw=1;
  else if(strcmp(progname,"send_mon_old")==0) mon=1;
  else if(strcmp(progname,"sendt_raw_old")==0) {raw=1;tow=1;}
  else if(strcmp(progname,"sendt_mon_old")==0) {mon=1;tow=1;}
  else exit(1);

  if(argc<4)
    {
    fprintf(stderr,
      " usage : '%s [shm_key] [dest] [port] ([ch_file]/- ([log file]))'\n",
      progname);
    exit(0);
    }

  shm_key=atoi(argv[1]);
  strcpy(host_name,argv[2]);
  host_port=atoi(argv[3]);
  *chfile=(*logfile)=0;
  if(argc>4) strcpy(chfile,argv[4]);
  if(strcmp("-",chfile)==0) *chfile=0;
  if(argc>5) strcpy(logfile,argv[5]);
    
  read_chfile();

  /* shared memory */
  if((shmid=shmget(shm_key,0,0))<0) err_sys("shmget");
  if((shm=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");

  sprintf(tbuf,"start shm_key=%d id=%d",shm_key,shmid);
  write_log(logfile,tbuf);

  /* destination host/port */
  if(!(h=gethostbyname(host_name))) err_sys("can't find host");
  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  memcpy((caddr_t)&to_addr.sin_addr,h->h_addr,h->h_length);
  to_addr.sin_port=htons(host_port);

  /* my socket */
  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");
  i=32768;
  if(setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0)
                err_sys("SO_SNDBUF setsockopt error\n");

  /* bind my socket to a local port */
  memset((char *)&from_addr,0,sizeof(from_addr));
  from_addr.sin_family=AF_INET;
  from_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  from_addr.sin_port=htons(0);
  if(bind(sock,(struct sockaddr *)&from_addr,sizeof(from_addr))<0)
    err_sys("bind");

  signal(SIGPIPE,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGTERM,(void *)ctrlc);
  for(i=0;i<BUFNO;i++) psize[i]=(-1);
  no=bufno=0;

reset:
  while(shm->r==(-1)) sleep(1);
  c_save=shm->c;
  size=mklong(ptr_save=shm->d+(shp=shm->r));

  while(1)
    {
    if(shp+size>shm->pl) shp=0; /* advance pointer */
    else shp+=size;

    while(shm->p==shp) usleep(200000);
    if(shm->c<c_save || mklong(ptr_save)!=size)
      {   /* previous block has been destroyed */
      write_log(logfile,"reset");
      goto reset;
      }
    c_save=shm->c;
    size=mklong(ptr_save=ptr=shm->d+shp);

    ptr_lim=ptr+size;
    ptr+=4;
    if(tow) ptr+=4;
    ptw=ptw_save=sbuf[bufno];
    *ptw++=no;  /* packet no. */
    *ptw++=no;  /* packet no.(2) */
    for(i=0;i<6;i++) *ptw++=(*ptr++);
#if DEBUG
    for(i=0;i<8;i++) fprintf(stderr,"%02X",ptw_save[i]);
    fprintf(stderr," : %d\n",size);
#endif
    /* send data packets */
    if(raw)
      {
      i=j=re=0;
      while(ptr<ptr_lim)
        {
        gh=mklong(ptr);
        ch=(gh>>16);
        sr=gh&0xfff;
        if((gh>>12)&0xf) gs=((gh>>12)&0xf)*(sr-1)+8;
        else gs=(sr>>1)+8;
        if(ch_table[ch] && gs+6<=MAXMESG)
          {
          if(ptw+gs-ptw_save>MAXMESG)
            {
            re=sendto(sock,ptw_save,psize[bufno]=ptw-ptw_save,
              0,(const struct sockaddr *)&to_addr,sizeof(to_addr));
#if DEBUG
            fprintf(stderr,"%5d",re);
#endif
            if(re==(-1)) break; /* abort the second */
            else re=0;
            if(++bufno==BUFNO) bufno=0;
            no++;
            for(i=2;i<8;i++) sbuf[bufno][i]=ptw_save[i];
            ptw_save=sbuf[bufno];
            ptw_save[0]=no; /* packet no. */
            ptw_save[1]=no; /* packet no.(2) */
            ptw=ptw_save+8;
            }
          memcpy(ptw,ptr,gs);
          ptw+=gs;
          j++;
          }
        ptr+=gs;
        i++;
        }
      if(ptw>ptw_save+8 && re==0)
        {
        re=sendto(sock,ptw_save,psize[bufno]=ptw-ptw_save,
          0,(const struct sockaddr *)&to_addr,sizeof(to_addr));
#if DEBUG
        fprintf(stderr,"%5d",re);
#endif
        if(++bufno==BUFNO) bufno=0;
        no++;
        }
#if DEBUG
      fprintf(stderr,"\007");
      fprintf(stderr," %d/%d\n",j,i); /* nch_sent/nch */
#endif
      }
    else if(mon)
      {
      i=j=re=0;
      while(ptr<ptr_lim)
        {
        ch=mkshort(ptr1=ptr);
        ptr+=2;
        for(ii=0;ii<SR_MON;ii++)
          {
          aa=(*(ptr++));
          bb=aa&3;
          if(bb) for(k=0;k<bb*2;k++) ptr++;
          else ptr++;
          }
        gs=ptr-ptr1;
        if(ch_table[ch] && gs+6<=MAXMESG)
          {
          if(ptw+gs-ptw_save>MAXMESG)
            {
            re=sendto(sock,ptw_save,psize[bufno]=ptw-ptw_save,
              0,(const struct sockaddr *)&to_addr,sizeof(to_addr));
#if DEBUG
            fprintf(stderr,"%5d",re);
#endif
            if(re==(-1)) break; /* abort the second */
            else re=0;
            if(++bufno==BUFNO) bufno=0;
            no++;
            for(i=2;i<8;i++) sbuf[bufno][i]=ptw_save[i];
            ptw_save=sbuf[bufno];
            ptw_save[0]=no; /* packet no. */
            ptw_save[1]=no; /* packet no.(2) */
            ptw=ptw_save+8;
            }
          memcpy(ptw,ptr1,gs);
          ptw+=gs;
          j++;
          }
        i++;
        }
      if(ptw>ptw_save+8 && re==0)
        {
        re=sendto(sock,ptw_save,psize[bufno]=ptw-ptw_save,
          0,(const struct sockaddr *)&to_addr,sizeof(to_addr));
#if DEBUG
        fprintf(stderr,"%5d",re);
#endif
        if(++bufno==BUFNO) bufno=0;
        no++;
        }
#if DEBUG
      fprintf(stderr,"\007");
      fprintf(stderr," %d/%d\n",j,i);
#endif
      }
    i=1<<sock;
    timeout.tv_sec=timeout.tv_usec=0;
    while(select(sock+1,(fd_set *)&i,NULL,NULL,&timeout)>0)
      {
      j=recvfrom(sock,rbuf,MAXMESG,0,(struct sockaddr *)&from_addr,&fromlen);
      if(j>0 && j<BUFNO) for(k=0;k<j;k++)
        {
        if((bufno_f=get_packet(bufno,no_f=(rbuf[k])))>=0)
          {
          memcpy(sbuf[bufno],sbuf[bufno_f],psize[bufno]=psize[bufno_f]);
          sbuf[bufno][0]=no;    /* packet no. */
          sbuf[bufno][1]=no_f;  /* old packet no. */
          re=sendto(sock,sbuf[bufno],psize[bufno],0,
		    (const struct sockaddr *)&to_addr,sizeof(to_addr));
          sprintf(tbuf,"resend to %s:%d #%d as #%d, %d B",
            inet_ntoa(to_addr.sin_addr),ntohs(to_addr.sin_port),no_f,no,re);
          write_log(logfile,tbuf);
          if(++bufno==BUFNO) bufno=0;
          no++;
          }
        }
      timeout.tv_sec=timeout.tv_usec=0;
      }
    }
  }
