/* "raw_mon.c"      7/2/93,6/17/94,6/28/94    urabe */
/*                  3/17/95 write_log(), 4/17/95 MAX_SR safety */
/*                  usleep -> sleep */
/*                  1/13/97 don't abort loop when SR exceeds MAXSR(=1000Hz) */
/*                  97.8.5 fgets/sscanf */
/*                  97.9.23 FreeBSD  urabe, 97.10.3 */
/*                  98.4.13 MAX_SR : 300 -> 4096 */
/*                  98.4.14 usleep from U.M. */
/*                  99.2.4  moved signal(HUP) to read_chfile() by urabe */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>

/*#define DEBUG     0*/
#define BELL        0
#define MAX_SR      4096
#define SR_MON      5

/*****************************/
/* 8/21/96 for little-endian (uehira) */
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

long buf_raw[MAX_SR],buf_mon[SR_MON][2];
unsigned char ch_table[65536];
char *progname,logfile[256],chfile[256];
int n_ch,negate_channel;

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
  exit(0);
  }

get_mon(gm_sr,gm_raw,gm_mon)
  int gm_sr,*gm_raw,(*gm_mon)[2];
  {
  int gm_i,gm_j,gm_subr;
  switch(gm_sr)
    {
    case 100:
      gm_subr=100/SR_MON;
      break;
    case 20:
      gm_subr=20/SR_MON;
      break;
    case 120:
      gm_subr=120/SR_MON;
      break;
    default:
      gm_subr=gm_sr/SR_MON;
      break;
    }
  for(gm_i=0;gm_i<SR_MON;gm_i++)
    {
    gm_mon[gm_i][0]=gm_mon[gm_i][1]=(*gm_raw);
    for(gm_j=0;gm_j<gm_subr;gm_j++)
      {
      if(*gm_raw<gm_mon[gm_i][0]) gm_mon[gm_i][0]=(*gm_raw);
      else if(*gm_raw>gm_mon[gm_i][1]) gm_mon[gm_i][1]=(*gm_raw);
      gm_raw++;
      }
    }
  }

unsigned char *compress_mon(peaks,ptr)
  int *peaks;
  unsigned char *ptr;
  {
  /* data compression */
  if(((peaks[0]&0xffffffc0)==0xffffffc0 || (peaks[0]&0xffffffc0)==0) &&
    ((peaks[1]&0xffffffc0)==0xffffffc0 ||(peaks[1]&0xffffffc0)==0))
    {
    *ptr++=((peaks[0]&0x70)<<1)|((peaks[1]&0x70)>>2);
    *ptr++=((peaks[0]&0xf)<<4)|(peaks[1]&0xf);
    }
  else
  if(((peaks[0]&0xfffffc00)==0xfffffc00 || (peaks[0]&0xfffffc00)==0) &&
    ((peaks[1]&0xfffffc00)==0xfffffc00 ||(peaks[1]&0xfffffc00)==0))
    {
    *ptr++=((peaks[0]&0x700)>>3)|((peaks[1]&0x700)>>6)|1;
    *ptr++=peaks[0];
    *ptr++=peaks[1];
    }
  else
  if(((peaks[0]&0xfffc0000)==0xfffc0000 ||(peaks[0]&0xfffc0000)==0) &&
    ((peaks[1]&0xfffc0000)==0xfffc0000 ||(peaks[1]&0xfffc0000)==0))
    {
    *ptr++=((peaks[0]&0x70000)>>11)|((peaks[1]&0x70000)>>14)|2;
    *ptr++=peaks[0];
    *ptr++=peaks[0]>>8;
    *ptr++=peaks[1];
    *ptr++=peaks[1]>>8;
    }
  else
  if(((peaks[0]&0xfc000000)==0xfc000000 || (peaks[0]&0xfc000000)==0) &&
    ((peaks[1]&0xfc000000)==0xfc000000 ||(peaks[1]&0xfc000000)==0))
    {
    *ptr++=((peaks[0]&0x7000000)>>19)|((peaks[1]&0x7000000)>>22)|3;
    *ptr++=peaks[0];
    *ptr++=peaks[0]>>8;
    *ptr++=peaks[0]>>16;
    *ptr++=peaks[1];
    *ptr++=peaks[1]>>8;
    *ptr++=peaks[1]>>16;
    }
  else 
    {
    *ptr++=0;
    *ptr++=0;
    }
  return ptr;
  }

win2fix(ptr,abuf,sys_ch,sr) /* returns group size in bytes */
  unsigned char *ptr; /* input */
  register long *abuf;/* output */
  long *sys_ch;       /* sys_ch */
  long *sr;           /* sr */
  {
  int b_size,g_size;
  register int i,s_rate;
  register unsigned char *dp,*pts;
  unsigned int gh;
  short shreg;
  int inreg;
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPU;
#endif

  dp=ptr;
  pts=(unsigned char *)&gh;
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(gh);
#endif
  *sr=s_rate=gh&0xfff;
  if(s_rate>MAX_SR) return 0;
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16);

  /* read group */
  pts=(unsigned char *)&abuf[0];
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(abuf[0]);
#endif
  if(s_rate==1) return g_size;  /* normal return */
  switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        abuf[i]=abuf[i-1]+((*(char *)dp)>>4);
        abuf[i+1]=abuf[i]+(((char)(*(dp++)<<4))>>4);
        }
      break;
    case 1:
      for(i=1;i<s_rate;i++)
        abuf[i]=abuf[i-1]+(*(char *)(dp++));
      break;
    case 2:
      for(i=1;i<s_rate;i++)
        {
        pts=(unsigned char *)&shreg;
        *pts++=(*dp++);
        *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
        SWAPS(shreg);
#endif
        abuf[i]=abuf[i-1]+shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        pts=(unsigned char *)&inreg;
        *pts++=(*dp++);
        *pts++=(*dp++);
        *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
        SWAPL(inreg);
#endif
        abuf[i]=abuf[i-1]+(inreg>>8);
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        pts=(unsigned char *)&inreg;
        *pts++=(*dp++);
        *pts++=(*dp++);
        *pts++=(*dp++);
        *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
        SWAPL(inreg);
#endif
        abuf[i]=abuf[i-1]+inreg;
        }
      break;
    default:
      return 0; /* bad header */
    }
  return g_size;  /* normal return */
  }

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  write_log(logfile,ptr);
  if(errno<sys_nerr) write_log(logfile,sys_errlist[errno]);
  ctrlc();
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
      if(negate_channel) for(i=0;i<65536;i++) ch_table[i]=1;
      else for(i=0;i<65536;i++) ch_table[i]=0;
      i=j=0;
      while(fgets(tbuf,1024,fp))
        {
        if(*tbuf=='#' || sscanf(tbuf,"%x",&k)<0) continue;
        k&=0xffff;
#if DEBUG
        fprintf(stderr," %04X",k);
#endif
        if(negate_channel)
          {
          if(ch_table[k]==1)
            {
            ch_table[k]=0;
            j++;
            }
          }
        else
          {
          if(ch_table[k]==0)
            {
            ch_table[k]=1;
            j++;
            }
          }
        i++;
        }
#if DEBUG
      fprintf(stderr,"\n",k);
#endif
      n_ch=j;
      if(negate_channel) sprintf(tbuf,"-%d channels",n_ch);
      else sprintf(tbuf,"%d channels",n_ch);
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
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *shr,*shm;
  key_t rawkey,monkey;
  int shmid_raw,shmid_mon;
  union {
    unsigned long i;
    unsigned short s;
    char c[4];
    } un;
  char tb[100];
  unsigned char *ptr,*ptw,tm[6],*ptr_lim,*ptr_save;
  int ch,sys,sr,i,j,k,size,n,size_shm,re;
  unsigned long c_save;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<4)
    {
    fprintf(stderr,
      " usage : '%s [raw_key] [mon_key] [shm_size(KB)] ([ch_file]/- ([log file]))'\n",
      progname);
    exit(1);
    }
  rawkey=atoi(argv[1]);
  monkey=atoi(argv[2]);
  size_shm=atoi(argv[3])*1000;
  *chfile=(*logfile)=0;
  if(argc>4)
    {
    if(strcmp("-",argv[4])==0) *chfile=0;
    else
      {
      if(argv[4][0]=='-')
        {
        strcpy(chfile,argv[4]+1);
        negate_channel=1;
        }
      else
        {
        strcpy(chfile,argv[4]);
        negate_channel=0;
        }
      }
    }    
  if(argc>5) strcpy(logfile,argv[5]);
    
  read_chfile();

  /* raw shared memory */
  if((shmid_raw=shmget(rawkey,0,0))<0) err_sys("shmget raw");
  if((shr=(struct Shm *)shmat(shmid_raw,(char *)0,0))==
      (struct Shm *)-1) err_sys("shmat raw");

  /* mon shared memory */
  if((shmid_mon=shmget(monkey,size_shm,IPC_CREAT|0666))<0) err_sys("shmget mon");
  if((shm=(struct Shm *)shmat(shmid_mon,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat mon");

  sprintf(tb,"start raw_key=%d id=%d mon_key=%d id=%d size=%d",
    rawkey,shmid_raw,monkey,shmid_mon,size_shm);
  write_log(logfile,tb);

  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);

reset:
  /* initialize buffer */
  shm->p=shm->c=0;
  shm->pl=(size_shm-sizeof(*shm))/10*9;
  shm->r=(-1);

  ptr=shr->d;
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;

  while(1)
    {
    ptr_lim=ptr+(size=mklong(ptr_save=ptr));
    c_save=shm->c;
    ptr+=4;
#if DEBUG
    for(i=0;i<6;i++) printf("%02X",ptr[i]);
    printf(" : %d R\n",size);
#endif
  /* make mon data */
    ptw=shm->d+shm->p;
    ptw+=4;               /* size (4) */
    for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */

    do    /* loop for ch's */
      {
      if((re=win2fix(ptr,buf_raw,&ch,&sr))==0)
        {
        sprintf(tb,"%02X%02X%02X%02X%02X%02X%02X%02X",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
        strcat(tb," ?");
        write_log(logfile,tb);
        break;
        }
      if(ch_table[ch])
        {
        *ptw++=ch>>8;
        *ptw++=ch;
        get_mon(sr,buf_raw,buf_mon);  /* get mon data from raw */
        for(i=0;i<SR_MON;i++) ptw=compress_mon(buf_mon[i],ptw);
        }
      } while((ptr+=re)<ptr_lim);

    un.i=ptw-(shm->d+shm->p);
#if BYTE_ORDER ==  BIG_ENDIAN
    shm->d[shm->p  ]=un.c[0]; /* size (H) */
    shm->d[shm->p+1]=un.c[1];
    shm->d[shm->p+2]=un.c[2];
    shm->d[shm->p+3]=un.c[3]; /* size (L) */
#endif
#if BYTE_ORDER ==  LITTLE_ENDIAN
    shm->d[shm->p  ]=un.c[3]; /* size (H) */
    shm->d[shm->p+1]=un.c[2];
    shm->d[shm->p+2]=un.c[1];
    shm->d[shm->p+3]=un.c[0]; /* size (L) */
#endif

#if DEBUG
    for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+4+i]);
    printf(" : %d M\n",un.i);
#endif

    shm->r=shm->p;
    if(ptw>shm->d+shm->pl) ptw=shm->d;
    shm->p=ptw-shm->d;
    shm->c++;
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    if((ptr=ptr_lim)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) usleep(100000);
    if(shr->c<c_save || mklong(ptr_save)!=size)
      {
      write_log(logfile,"reset");
      goto reset;
      }
    }
  }
