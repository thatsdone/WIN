/* $Id: order.c,v 1.11 2004/10/26 14:42:01 uehira Exp $ */
/*  program "order.c" 1/26/94 - 2/7/94, 6/14/94 urabe */
/*                              1/6/95 bug in adj_time(tm[0]--) fixed */
/*                              3/17/95 write_log() */
/*                              5/24/96 check_time +/-30 min */
/*                              5/28/96 bcopy,usleep -> memcpy,sleep */
/*                              8/27/96 LITTLE ENDIAN (uehira) */
/*                              98.4.23 b2d[] etc. */
/*                              98.5.21 added passing etc. */
/*                              98.6.26 yo2000 */
/*                              99.4.19 byte-order-free */
/*                              2000.4.24/2001.11.14 strerror() */
/*                              2002.2.19-28 use mktime(), -l option after order2.c */
/*                              2002.3.1 option -a ; system clock mode */
/*                              2002.3.1 eobsize_in(auto), eobsize_out(-B) */
/*                              2002.5.2 i<1000 -> 1000000 */
/*                              2002.5.7 mktime2() */
/*                              2002.5.11 timezone for SVR4 */
/*                              2004.10.21 daemon mode (uehira) */

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

#include <errno.h>
#include <syslog.h>

#include "daemon_mode.h"
#include "subst_func.h"

#define SWAPL(a) a=(((a)<<24)|((a)<<8)&0xff0000|((a)>>8)&0xff00|((a)>>24)&0xff)
#define DEBUG     0
#define DEBUG1    0

#define NAMELEN  1025

char *progname,logfile[NAMELEN];
int  daemon_mode, syslog_mode;

struct Shm {
  unsigned long p;    /* write point */
  unsigned long pl;   /* write limit */
  unsigned long r;    /* latest */
  unsigned long c;    /* counter */
  unsigned char d[1]; /* data buffer */
};

mklong(ptr)
  unsigned char *ptr;
  {   
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
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

bcd_dec(dest,sour)
  unsigned char *sour;
  int *dest;
  {
  static int b2d[]={
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,  /* 0x00 - 0x0F */
    10,11,12,13,14,15,16,17,18,19,-1,-1,-1,-1,-1,-1,
    20,21,22,23,24,25,26,27,28,29,-1,-1,-1,-1,-1,-1,
    30,31,32,33,34,35,36,37,38,39,-1,-1,-1,-1,-1,-1,
    40,41,42,43,44,45,46,47,48,49,-1,-1,-1,-1,-1,-1,
    50,51,52,53,54,55,56,57,58,59,-1,-1,-1,-1,-1,-1,
    60,61,62,63,64,65,66,67,68,69,-1,-1,-1,-1,-1,-1,
    70,71,72,73,74,75,76,77,78,79,-1,-1,-1,-1,-1,-1,
    80,81,82,83,84,85,86,87,88,89,-1,-1,-1,-1,-1,-1,
    90,91,92,93,94,95,96,97,98,99,-1,-1,-1,-1,-1,-1,  /* 0x90 - 0x9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  int i;
  i=b2d[sour[0]];
  if(i>=0 && i<=99) dest[0]=i; else return 0;
  i=b2d[sour[1]];
  if(i>=1 && i<=12) dest[1]=i; else return 0;
  i=b2d[sour[2]];
  if(i>=1 && i<=31) dest[2]=i; else return 0;
  i=b2d[sour[3]];
  if(i>=0 && i<=23) dest[3]=i; else return 0;
  i=b2d[sour[4]];
  if(i>=0 && i<=59) dest[4]=i; else return 0;
  i=b2d[sour[5]];
  if(i>=0 && i<=60) dest[5]=i; else return 0;
  return 1;
  }

t_bcd(t,ptr)
     time_t t;
     unsigned char *ptr;
{
static unsigned char d2b[]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,   
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99};
  struct tm *nt;
  nt=localtime(&t);
  ptr[0]=d2b[nt->tm_year%100];
  ptr[1]=d2b[nt->tm_mon+1];
  ptr[2]=d2b[nt->tm_mday];
  ptr[3]=d2b[nt->tm_hour];
  ptr[4]=d2b[nt->tm_min];
  ptr[5]=d2b[nt->tm_sec];
}

time_t mktime2(struct tm *mt) /* high-speed version of mktime() */
  {
  static struct tm *m;
  time_t t;
  register int i,j,ye;
  static int dm[]={31,28,31,30,31,30,31,31,30,31,30,31};
  static int dy[]={365,365,366,365,365,365,366,365,365,365,366,365, /* 1970-81 */
                   365,365,366,365,365,365,366,365,365,365,366,365, /* 1982-93 */
                   365,365,366,365,365,365,366,365,365,365,366,365, /* 1994-2005 */
                   365,365,366,365,365,365,366,365,365,365,366,365, /* 2006-17 */
                   365,365,366,365,365,365,366,365,365,365,366,365, /* 2018-2029 */
                   365,365,366,365,365,365,366,365,365,365,366,365};/* 2030-2041 */
#if defined(__SVR4)
  extern time_t timezone;
#endif
  if(m==NULL) m=localtime(&t);
  ye=mt->tm_year-70;
  j=0;
  for(i=0;i<ye;i++) j+=dy[i]; /* days till the previous year */
  for(i=0;i<mt->tm_mon;i++) j+=dm[i]; /* days till the previous month */
  if(!(mt->tm_year&0x3) && mt->tm_mon>1) j++;  /* in a leap year */
  j+=mt->tm_mday-1; /* days */
#if defined(__SVR4)
  return j*86400+mt->tm_hour*3600+mt->tm_min*60+mt->tm_sec+timezone;
#endif
#if defined(HAVE_STRUCT_TM_GMTOFF)
  return j*86400+mt->tm_hour*3600+mt->tm_min*60+mt->tm_sec-m->tm_gmtoff;
#endif
#if defined(__CYGWIN__)
  tzset();
  return j*86400+mt->tm_hour*3600+mt->tm_min*60+mt->tm_sec+_timezone;
#endif
  }

time_t bcd_t(ptr)
  unsigned char *ptr;
  {
  int tm[6];
  time_t ts;
  struct tm mt;
  if(!bcd_dec(tm,ptr)) return 0; /* out of range */
  memset((char *)&mt,0,sizeof(mt));
  if((mt.tm_year=tm[0])<50) mt.tm_year+=100;
  mt.tm_mon=tm[1]-1;
  mt.tm_mday=tm[2];
  mt.tm_hour=tm[3];
  mt.tm_min=tm[4];
  mt.tm_sec=tm[5];
  mt.tm_isdst=0;
#if defined(__SVR4) || defined(HAVE_STRUCT_TM_GMTOFF) || defined(__CYGWIN__)
  ts=mktime2(&mt);
#else
  ts=mktime(&mt);
#endif
  return ts;
  }

check_time(ptr)
  char *ptr;
  {
  time_t ts,rt;
  if(!(ts=bcd_t(ptr))) return 1;
  rt=time(0);
  if(abs(rt-ts)>60*30) return 1; /* within 30 minutes ? */
  return 0;
  }

write_log(logfil,ptr)
  char *logfil;
  char *ptr;
{
  FILE *fp;
  int tm[6];

  if (syslog_mode)
    syslog(LOG_NOTICE, "%s", ptr);
  else
    {
      if(*logfil) fp=fopen(logfil,"a");
      else fp=stdout;
      get_time(tm);
      fprintf(fp,"%02d%02d%02d.%02d%02d%02d %s %s\n",
	      tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
      if(*logfil) fclose(fp);
    }
}

ctrlc()
{
  write_log(logfile,"end");
  if (syslog_mode)
    closelog();
  exit(0);
}

err_sys(ptr)
  char *ptr;
{
  if (syslog_mode)
    syslog(LOG_NOTICE, "%s", ptr);
  else
    {
      perror(ptr);
      write_log(logfile,ptr);
    }
  if(strerror(errno)) write_log(logfile,strerror(errno));
  ctrlc();
}

advance(shm,shp,c_save,size,t,shp_busy_save)
  struct Shm *shm;
  unsigned int *shp,*size,*c_save,*shp_busy_save;
  time_t *t;
{
  int shpp,i;

  shpp=(*shp); /* copy read pointer */
  i=shm->c-(*c_save);
  if(!(i<1000000 && i>=0) || *size!=mklong(shm->d+(*shp))) return -2;
  if((shpp+=(*size))>shm->pl) shpp=0; /* advance read pointer by one block */
  if(shm->p==shpp) {  /* block is still busy */
    if(shp_busy_save) *shp_busy_save=shpp;
    return 0;
  }
  *c_save=shm->c;
  *size=mklong(shm->d+(*shp=shpp)); /* size of next block */
  if(!(*t=bcd_t(shm->d+shpp+8))) return -1;
  return mklong(shm->d+shpp+4); /* return value = TOW */
}


main(argc,argv)
  int argc;
  char *argv[];
{
  key_t shm_key_in,shm_key_out,shm_key_late;
  unsigned long uni;
  time_t t,t_out,t_bottom,tow,ts,rt,rt_next;
  int tm_out[6],c_save_in,shp_in,sec,sec_1,size,sizej,shmid_in,late,c,pl_out,
    shmid_out,shmid_late,size_in,shp,c_save,n_sec,no_data,sysclk,sysclk_org,
    timeout_flag,size_next,eobsize_in,eobsize_out,i,eobsize_in_count,size2;
  unsigned char *ptr,*ptr_save,*ptw,*ptw_save,*ptw_late,
    tbuf[NAMELEN],tb[NAMELEN],*ptr_prev;
  struct Shm *shm_in,*shm_out,*shm_late;
  unsigned int shp_busy_save;
  extern int optind;
  extern char *optarg;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  if(strcmp(progname,"orderd")==0) daemon_mode=1;

  if(daemon_mode)
    sprintf(tb,
	    " usage : '%s (-aB) (-l [shm_key_late]:[shm_size_late(KB)]) [shm_key_in] \\\n\
           [shm_key_out] [shm_size(KB)] [limit_sec] ([log file])'", progname);
  else
    sprintf(tb,
	    " usage : '%s (-aBD) (-l [shm_key_late]:[shm_size_late(KB)]) [shm_key_in] \\\n\
           [shm_key_out] [shm_size(KB)] [limit_sec] ([log file])'", progname);


  sysclk_org=late=eobsize_in=eobsize_out=0;
  while((c=getopt(argc,argv,"aBDl:"))!=EOF)
    {
    switch(c)
      {
      case 'a':   /* reference to absolute time (i.e. the system clock) */
        sysclk_org=1;
        break;
      case 'B':   /* write blksiz at EOB in shm_out */
        eobsize_out=1;
        break;
      case 'D':
	daemon_mode = 1;  /* daemon mode */
	break;   
      case 'l':   /* output late packets (i.e. order2 mode) */
        strcpy(tbuf,optarg);
        if((ptr=strchr(tbuf,':'))==0)
          {
          fprintf(stderr,"-l option requires [shm_key_late]:[shm_size_late(KB)]\n");
          exit(1);
          }
        else
          {
          *ptr=0;
          shm_key_late=atoi(tbuf);
          sizej=atoi(ptr+1)*1000;
          late=1;
          }
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
   
  shm_key_in=atoi(argv[1+optind]);
  shm_key_out=atoi(argv[2+optind]);
  size=atoi(argv[3+optind])*1000;
  n_sec=atoi(argv[4+optind]);
  if(argc>5+optind) strcpy(logfile,argv[5+optind]);
  else
    {
      *logfile=0;
      if (daemon_mode)
	syslog_mode = 1;
    }

   /* daemon mode */
   if (daemon_mode) {
     daemon_init(progname, LOG_USER, syslog_mode);
     umask(022);
   }
   
  /* shared memory */
  if((shmid_in=shmget(shm_key_in,0,0))<0) err_sys("shmget");
  if((shm_in=(struct Shm *)shmat(shmid_in,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");
  sprintf(tbuf,"in : shm_key_in=%d id=%d",shm_key_in,shmid_in);
  write_log(logfile,tbuf);
   
  if((shmid_out=shmget(shm_key_out,size,IPC_CREAT|0666))<0) err_sys("shmget");
  if((shm_out=(struct Shm *)shmat(shmid_out,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");
  sprintf(tbuf,"out: shm_key_out=%d id=%d size=%d",shm_key_out,shmid_out,size);
  write_log(logfile,tbuf);

  /* initialize buffer */

  shm_out->p=shm_out->c=0;
  shm_out->pl=pl_out=(size-sizeof(*shm_out))/10*9;
  shm_out->r=(-1);

  if(late) {
    if((shmid_late=shmget(shm_key_late,sizej,IPC_CREAT|0666))<0)
      err_sys("shmget");
    if((shm_late=(struct Shm *)shmat(shmid_late,(char *)0,0))==(struct Shm *)-1)
      err_sys("shmat");
    sprintf(tbuf,"late: shm_key_late=%d id=%d size=%d",
          shm_key_late,shmid_late,sizej);
    write_log(logfile,tbuf);
    shm_late->p=shm_late->c=0;
    shm_late->pl=(sizej-sizeof(*shm_late))/10*9;
    shm_late->r=(-1);
  }
 
  signal(SIGPIPE,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGTERM,(void *)ctrlc);

reset:
  while(shm_in->r==(-1)) sleep(1);
  c_save_in=shm_in->c;
  size_in=mklong(ptr_save=shm_in->d+(shp_in=shm_in->r));
  if(check_time(ptr_save+8)){
     sleep(1);
     goto reset;
  }
  sec_1=mklong(ptr_save+4); /* TOW */
  t_out=t_bottom=bcd_t(ptr_save+8); /* TS */
  if(late) {
    shp_busy_save=(-1);
    timeout_flag=0;
  }
  sysclk=sysclk_org;
  if(mklong(ptr_save+size_in-4)==size_in) eobsize_in=1;
  else eobsize_in=sysclk=0;
  eobsize_in_count=eobsize_in;
  sprintf(tbuf,"eobsize_in=%d, eobsize_out=%d, sysclk=%d sysclk_org=%d",
    eobsize_in,eobsize_out,sysclk,sysclk_org);
  write_log(logfile,tbuf);

  if(sysclk) /* system clock mode */
    {
    rt_next=time(0)-1;
    ts=t_out;
    if(late) ptw_late=shm_late->d+shm_late->p;
    while(1)
      {
      tow=sec_1;
#if DEBUG1
      printf("tow=%d ts=%d shp=%d\n",sec_1,ts,shp_in);
#endif
      /* new TS */
      if(tow>ts+n_sec) /* output as a late packet */
        {
#if DEBUG
        printf("  late !\n");
#endif
        if(late)
          {
          /* output late data */
          ptr=shm_in->d+shp_in;
          size2=size_in-4;
          memcpy(ptw_late,ptr,size2);
          ptw_late[0]=size2>>24;
          ptw_late[1]=size2>>16;
          ptw_late[2]=size2>>8;
          ptw_late[3]=size2;
          uni=time(NULL);
          ptw_late[4]=uni>>24;
          ptw_late[5]=uni>>16;
          ptw_late[6]=uni>>8;
          ptw_late[7]=uni;
          ptw_late+=size2;
          shm_late->r=shm_late->p;
          if(ptw_late>shm_late->d+shm_late->pl) ptw_late=shm_late->d;
          shm_late->p=ptw_late-shm_late->d;
          shm_late->c++;
          }
        }
      while((sec_1=advance(shm_in,&shp_in,&c_save_in,&size_in,&ts,&shp_busy_save))<=0)
        {
#if DEBUG1
        printf("sec_1=%d\n",sec_1);
#endif
        if(sec_1==(-2)) /* shm corrupted */
          {
          ptr=shm_in->d+shp+8;
          sprintf(tbuf,"reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
            ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
          write_log(logfile,tbuf);
          sleep(1);
          goto reset;
          }
        else if(sec_1==0) /* waiting */
          { /* sweep output data if RT advanced */
          if((rt=time(0))<rt_next) {usleep(10000);continue;}
          ptr_save=shm_in->d+shp_in;
          for(t_out=rt_next-n_sec;t_out<=rt-n_sec;t_out++)
            {
            ptr=ptr_save;
            ptw_save=ptw=shm_out->d+shm_out->p;
            ptw+=4; /* block size */
            t_bcd(t_out,ptw); /* write TS */
            ptw+=6; /* TS */
            i=0;
            while(1) /* sweep to output data at ts==rt-n_sec */
              {
              size=mklong(ptr);
              tow=mklong(ptr+4);
              ts=bcd_t(ptr+8);
              if(tow<rt-n_sec-1) /* quit loop - TOW too old */
                {
#if DEBUG
                printf("\nend>ptr=%d size=%d tow=%d ts=%d rt=%d\n",
                  ptr-shm_in->d,size,tow,ts,rt);
#endif
                break;
                }
              if(ts==rt-n_sec)
                {
#if DEBUG
                printf("out %d(%d)>%d ",ptr-shm_in->d,size-(4+4+6+4),
                  ptw-shm_out->d);
#endif
                memcpy(ptw,ptr+(4+4+6),size-(4+4+6+4));
                ptw+=size-(4+4+6+4);
                }
              ptr_prev=ptr;
              if(ptr-shm_in->d>0) /* go back one packet */
                {
                size_next=mklong(ptr-4);
                ptr-=size_next;
                }
              else if(ptr==shm_in->d) /* return to the tail of SHM */
                {
                size_next=mklong(shm_in->d+shm_in->pl);
                ptr=shm_in->d+shm_in->pl+4-size_next;
                }
              else break;
              if(ptr<shm_in->d) break;
              if(size_next!=mklong(ptr))
                {
                if(i) break;
                else goto reset;
                }
              i++;
              }

           if((uni=ptw-ptw_save)>10)
             {
             uni=ptw-ptw_save;
             if(eobsize_out) uni+=4;
             ptw_save[0]=uni>>24;
             ptw_save[1]=uni>>16;
             ptw_save[2]=uni>>8;
             ptw_save[3]=uni;
             if(eobsize_out)
               {
               ptw[0]=uni>>24;
               ptw[1]=uni>>16;
               ptw[2]=uni>>8;
               ptw[3]=uni;
               ptw+=4;
               }
             shm_out->r=shm_out->p;
             if(eobsize_out && ptw>shm_out->d+pl_out)
               {shm_out->pl=ptw-shm_out->d-4;ptw=shm_out->d;}
             if(!eobsize_out && ptw>shm_out->d+shm_out->pl) ptw=shm_out->d;
             shm_out->p=ptw-shm_out->d;
             shm_out->c++;
#if DEBUG
             printf("eob %d B > %d next=%d\n",uni,shm_out->r,shm_out->p);
#endif
             }
            }
          rt_next=t_out+n_sec;
          }
        else if(sec_1==(-1)) usleep(100000);
        }
      }
    }

  else /* conventional mode */
  while(1){
    no_data=1;
    while(sec_1+n_sec>time(0)) sleep(1);
    shp=shp_in;
    c_save=c_save_in;
    size=size_in;
    ptw_save=ptw=shm_out->d+shm_out->p;
    ptw+=4;
    if(late) ptw_late=shm_late->d+shm_late->p;
    t=t_bottom;

    do{
      if(size==mklong(shm_in->d+shp+size-4)) eobsize_in_count++;
      else eobsize_in_count=0;
      if(eobsize_in && eobsize_in_count==0) goto reset;
      if(!eobsize_in && eobsize_in_count>3) goto reset;

      if(eobsize_in) size2=size-4;
      else size2=size;

      if(t==t_out){
        ptr=shm_in->d+shp+8; /* points to YY-ss */
        if(no_data){
          memcpy(ptw,ptr,size2-8);
          ptw+=size2-8;
          no_data=0;
        }
        else{
          memcpy(ptw,ptr+6,size2-14); /* skip YY-ss */
          ptw+=size2-14;
        }
      }

      if(late && shp==shp_busy_save) timeout_flag=1;
      if(late && timeout_flag){
        if(t<t_out){
          /* output late data */
          ptr=shm_in->d+shp;
          memcpy(ptw_late,ptr,size2);
          ptw_late[0]=size2>>24;
          ptw_late[1]=size2>>16;
          ptw_late[2]=size2>>8;
          ptw_late[3]=size2;
          uni=time(NULL);
          ptw_late[4]=uni>>24;
          ptw_late[5]=uni>>16;
          ptw_late[6]=uni>>8;
          ptw_late[7]=uni;
          ptw_late+=size2;
          shm_late->r=shm_late->p;
          if(ptw_late>shm_late->d+shm_late->pl) ptw_late=shm_late->d;
          shm_late->p=ptw_late-shm_late->d;
          shm_late->c++;
        }
      }

      while((sec=advance(shm_in,&shp,&c_save,&size,&t,&shp_busy_save))==(-1));
      if(sec==(-2)){
        ptr=shm_in->d+shp+8;
        sprintf(tbuf,"reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
        write_log(logfile,tbuf);
        goto reset;
      }
    } while(sec>0);

    if(late) timeout_flag=0;

    if((uni=ptw-ptw_save)>10){
      if(eobsize_out) uni+=4;
      ptw_save[0]=uni>>24;
      ptw_save[1]=uni>>16;
      ptw_save[2]=uni>>8;
      ptw_save[3]=uni;
      if(eobsize_out)
        {
        ptw[0]=uni>>24;
        ptw[1]=uni>>16;
        ptw[2]=uni>>8;
        ptw[3]=uni;
        ptw+=4;
        }
      shm_out->r=shm_out->p;
      if(eobsize_out && ptw>shm_out->d+pl_out)
        {shm_out->pl=ptw-shm_out->d-4;ptw=shm_out->d;}
      if(!eobsize_out && ptw>shm_out->d+shm_out->pl) ptw=shm_out->d;
      shm_out->p=ptw-shm_out->d;
      shm_out->c++;
    }

    t_out++;
    while(t_bottom!=t_out){
      if(sec_1+n_sec+4>time(0) && t_bottom>t_out) break;
      if(t_bottom>t_out){
        ptr=shm_in->d+shp_in+8;
        t_bcd(t_out,tm_out);
        sprintf(tbuf,
"passing %02X%02X%02X.%02X%02X%02X:%02X%02X(out=%02d%02d%02d.%02d%02d%02d)",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
          tm_out[0],tm_out[1],tm_out[2],tm_out[3],tm_out[4],tm_out[5]);
        write_log(logfile,tbuf);
      }
      while((sec_1=advance(shm_in,&shp_in,&c_save_in,&size_in,&t_bottom,NULL))<=0){
        if(late && shp_in==shp_busy_save) shp_busy_save=(-1);
        if(sec_1==(-1)) continue;
        else if(sec_1==0){
          sleep(1);
          continue;
        }
        else if(sec_1==(-2)){
          ptr=shm_in->d+shp_in+8;
          sprintf(tbuf,"reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
            ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
          write_log(logfile,tbuf);
          goto reset;
        }
      }
    }
  }
}
