/*  program "shmdump.c" 6/14/94 urabe */
/*  revised 5/29/96 */
/*  Little Endian (uehira) 8/27/96 */
/*  98.5.21 RT-TS */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#define DEBUG       0
#define MAXMESG     2000

/*****************************/
/* 8/20/96 for little-endian (uehira) */
#ifndef         LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234    /* LSB first: i386, vax */
#endif
#ifndef         BIG_ENDIAN
#define BIG_ENDIAN      4321    /* MSB first: 68000, ibm, net */
#endif
#ifndef  BYTE_ORDER
#define  BYTE_ORDER      BIG_ENDIAN
#endif

#define SWAPU  union { long l; float f; short s; char c[4];} swap
#define SWAPL(a) swap.l=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPF(a) swap.f=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPS(a) swap.s=(a); ((char *)&(a))[0]=swap.c[1];\
    ((char *)&(a))[1]=swap.c[0]
/*****************************/

extern const int sys_nerr;
extern const char *const sys_errlist[];
extern int errno;
 
char *progname;
struct Shm
  {
  unsigned long p;    /* write point */
  unsigned long pl;   /* write limit */
  unsigned long r;    /* latest */
  unsigned long c;    /* counter */
  unsigned char d[1];   /* data buffer */
  };

usleep(ms)  /* after T.I. UNIX MAGAZINE 1994.10 P.176 */
  unsigned int ms;
  {
  struct timeval tv;
  tv.tv_sec=ms/1000000;
  tv.tv_usec=ms%1000000;
  select(0,NULL,NULL,NULL,&tv);
  }

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned char *ptr1;
  unsigned long a;
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPU;
#endif
  ptr1=(unsigned char *)&a;
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1  =(*ptr);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(a);
#endif
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

time_dif(t1,t2) /* returns t1-t2(sec) */
  int *t1,*t2;
  {
  int i,*a,*b,soda,sodb,t1_is_lager,sdif,ok;
  for(i=0;i<6;i++) if(t1[i]!=t2[i]) break;
  if(i==6) return 0;
  else
    {
    if(t1[i]>t2[i]) {a=t2;b=t1;t1_is_lager=1;}
    else {a=t1;b=t2;t1_is_lager=0;}
    }
  /* always a<b */
  soda=a[5]+a[4]*60+a[3]*3600; /* sec of day */
  sodb=b[5]+b[4]*60+b[3]*3600; /* sec of day */
  sdif=sodb-soda;
  if(i>=3)
    {
    if(t1_is_lager) return sdif;
    else return (-sdif);
    }
  /* 'day' is different */
  ok=0;
  if(i==0 && a[0]+1==b[0] && a[1]==12 && b[1]==1 && a[2]==31 && b[2]==1) ok=1;
  else if(i==1 && a[1]+1==b[1] && b[2]==1) ok=1;
  else if(i==2 && a[2]+1==b[2]) ok=1;
  else
    {
    if(t1_is_lager) return 86400;
    else return -86400;
    }
  if(t1_is_lager) return 86400+sdif;
  else return -(86400+sdif);
  }

write_log(ptr)
  char *ptr;
  {
  int tm[6];
  get_time(tm);
  printf("%02d%02d%02d.%02d%02d%02d %s %s\n",
    tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
  fflush(stdout);
  }

ctrlc()
  {
  write_log("end");
  exit(0);
  }

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  write_log(ptr);
  if(errno<sys_nerr) write_log(sys_errlist[errno]);
  ctrlc();
  }

advance_s(shm,shp,c_save,size)
  struct Shm *shm;
  unsigned int *shp,*c_save,*size;
  {
  int shpp,tmp;
  shpp=(*shp);
  if(shm->c<*c_save || *size!=mklong(shm->d+(*shp))) return -1;
  if(shpp+(*size)>shm->pl) shpp=0; /* advance pointer */
  else shpp+=(*size);
  if(shm->p==shpp) return 0;
  *c_save=shm->c;
  *size=mklong(shm->d+(*shp=shpp));
  return 1;
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
  if(dest[0]<70) dest[0]+=100;
  return 1;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  key_t shm_key_in;
  union {
    unsigned long i;
    unsigned short s;
    char c[4];
    } un;
  int i,j,k,c_save_in,shp_in,size,shmid_in,size_in,shp,c_save,wtow;
  unsigned long tow;
  unsigned char *ptr,tbuf[100];
  struct Shm *shm_in;
  struct tm *nt;
  int rt[6],ts[6];

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];

  if(argc<2)
    {
    fprintf(stderr," usage : '%s [shm_key]'\n",progname);
    exit(0);
    }

  shm_key_in=atoi(argv[1]);

  /* shared memory */
  if((shmid_in=shmget(shm_key_in,0,0))<0) err_sys("shmget");
  if((shm_in=(struct Shm *)shmat(shmid_in,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");
  sprintf(tbuf,"in : shm_key_in=%d id=%d",shm_key_in,shmid_in);
  write_log(tbuf);

  signal(SIGPIPE,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGTERM,(void *)ctrlc);

reset:
  while(shm_in->r==(-1)) usleep(200000);
  c_save_in=shm_in->c;
  size_in=mklong(shm_in->d+(shp_in=shm_in->r));
  while(1)
    {
    i=advance_s(shm_in,&shp_in,&c_save_in,&size_in);
    if(i==0)
      {
      usleep(200000);
      continue;
      }
    else if(i<0) goto reset;
    printf("%4d : ",size_in);
    ptr=shm_in->d+shp_in+4;
    if(*ptr>0x20 && *ptr<0x90) /* with tow */
      {
      wtow=1;
      tow=mklong(ptr);
      nt=localtime(&tow);
/*      printf("%d ",tow);*/
      ptr+=4;
      rt[0]=nt->tm_year;
      rt[1]=nt->tm_mon+1;
      rt[2]=nt->tm_mday;
      rt[3]=nt->tm_hour;
      rt[4]=nt->tm_min;
      rt[5]=nt->tm_sec;
      printf("RT ",j);
      for(j=0;j<6;j++) printf("%02d",rt[j]);
      bcd_dec(ts,ptr);
      j=time_dif(rt,ts); /* returns t1-t2(sec) */
      printf(" %2d TS ",j);
      }
    else wtow=0;
    for(j=0;j<6;j++) printf("%02X",*ptr++);
    printf(" ");
    for(j=0;j<8;j++) printf("%02X",*ptr++);
    printf("...\n");
    }
  }
