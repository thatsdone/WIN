/* "raw_100.c"    97.6.23 - 6.30 urabe */
/*                  modified from raw_raw.c */
/*                  97.8.4 bug fixed (output empty block) */
/*                  97.8.5 fgets/sscanf */
/*                  99.2.4 moved signal(HUP) to read_chfile() by urabe */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>

#define DEBUG       0
#define BELL        0
#define MAX_SR   4095
#define SR_LOWER   50
#define SR        100

extern const int sys_nerr;
extern const char *const sys_errlist[];
extern int errno;

long buf_raw[MAX_SR];
unsigned char ch_table[65536];
char *progname,logfile[256],chfile[256];
int n_ch,negate_channel;

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned char *ptr1;
  unsigned long a;
  ptr1=(unsigned char *)&a;
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1  =(*ptr);
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
  signal(SIGHUP,read_chfile);
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

  dp=ptr;
  pts=(unsigned char *)&gh;
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts  =(*dp++);
  *sr=s_rate=gh&0xfff;
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+4;
  else g_size=(s_rate>>1)+4;
  *sys_ch=(gh>>16);

  /* read group */
  pts=(unsigned char *)&abuf[0];
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts  =(*dp++);
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
        abuf[i]=abuf[i-1]+inreg;
        }
      break;
    default:
      return g_size; /* bad header */
    }
  return g_size;  /* normal return */
  }

/* winform.c  4/30/91   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */
winform(inbuf,outbuf,sr,sys_ch)
  long *inbuf;      /* input data array for one sec*/
  unsigned char *outbuf;  /* output data array for one sec */
  int sr;         /* n of data (i.e. sampling rate) */
  unsigned short sys_ch;  /* 16 bit long channel ID number */
  {
  int dmin,dmax,aa,bb,br,i,byte_leng;
  long *ptr;
  unsigned char *buf,*bf;

  /* differentiate and obtain min and max */
  ptr=inbuf;
  bb=(*ptr++);
  dmax=dmin=0;
  for(i=1;i<sr;i++)
    {
    aa=(*ptr);
    *ptr++=br=aa-bb;
    bb=aa;
    if(br>dmax) dmax=br;
    else if(br<dmin) dmin=br;
    }

  /* determine sample size */
  if(((dmin&0xfffffff8)==0xfffffff8 || (dmin&0xfffffff8)==0) &&
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0))
    byte_leng=0;
  else
  if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0))
    byte_leng=1;
  else
  if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0))
    byte_leng=2;
  else
  if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0))
    byte_leng=3;
  else byte_leng=4;

  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  bf=(unsigned char *)inbuf;
  *buf++=(*bf++);
  *buf++=(*bf++);
  *buf++=(*bf++);
  *buf++=(*bf++);

  /* second and after */
  switch(byte_leng)
    {
    case 0:
      for(i=1;i<sr-1;i+=2)
        *buf++=(inbuf[i]<<4)|(inbuf[i+1]&0xf);
      if(i==sr-1) *buf++=(inbuf[i]<<4);
      break;
    case 1:
      for(i=1;i<sr;i++)
        *buf++=inbuf[i];
      break;
    case 2:
      for(i=1;i<sr;i++)
        {
        bf=(unsigned char *)&inbuf[i]+2;
        *buf++=(*bf++);
        *buf++=(*bf++);
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        bf=(unsigned char *)&inbuf[i]+1;
        *buf++=(*bf++);
        *buf++=(*bf++);
        *buf++=(*bf++);
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        bf=(unsigned char *)&inbuf[i];
        *buf++=(*bf++);
        *buf++=(*bf++);
        *buf++=(*bf++);
        *buf++=(*bf++);
        }
      break;
    }
  return (int)(buf-outbuf);
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
  int sr,i,j,k,size,n,size_shm,ch1,sr1,tow,rest;
  unsigned long c_save;
  unsigned short ch;
  int gs,gh;
  static int buf1[MAX_SR],buf2[SR];
  float t,ds,dt;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<4)
    {
    fprintf(stderr,
      " usage : '%s [in_key] [out_key] [shm_size(KB)] \\\n",
      progname);
    fprintf(stderr,
      "                       (-/[ch_file]/-[ch_file]/+[ch_file] ([log file]))'\n",
      progname);
    exit(1);
    }
  rawkey=atoi(argv[1]);
  monkey=atoi(argv[2]);
  size_shm=atoi(argv[3])*1000;
  *chfile=(*logfile)=0;
  rest=1;
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
      else if(argv[4][0]=='+')
        {
        strcpy(chfile,argv[4]+1);
        negate_channel=0;
        }
      else
        {
        strcpy(chfile,argv[4]);
        rest=negate_channel=0;
        }
      }
    }    
  if(argc>5) strcpy(logfile,argv[5]);
    
  read_chfile();

  /* in shared memory */
  if((shmid_raw=shmget(rawkey,0,0))<0) err_sys("shmget in");
  if((shr=(struct Shm *)shmat(shmid_raw,(char *)0,0))==
      (struct Shm *)-1) err_sys("shmat in");

  /* out shared memory */
  if((shmid_mon=shmget(monkey,size_shm,IPC_CREAT|0666))<0) err_sys("shmget out");
  if((shm=(struct Shm *)shmat(shmid_mon,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat out");

  sprintf(tb,"start in_key=%d id=%d out_key=%d id=%d size=%d",
    rawkey,shmid_raw,monkey,shmid_mon,size_shm);
  write_log(logfile,tb);

  signal(SIGTERM,ctrlc);
  signal(SIGINT,ctrlc);

reset:
  /* initialize buffer */
  shm->p=shm->c=0;
  shm->pl=(size_shm-sizeof(*shm))/10*9;
  shm->r=(-1);
  ptr=shr->d;
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;
  tow=(-1);

  while(1)
    {
    ptr_lim=ptr+(size=mklong(ptr_save=ptr));
    c_save=shm->c;
    ptr+=4;
#if DEBUG
    for(i=0;i<6;i++) printf("%02X",ptr[i]);
    printf(" : %d R\n",size);
#endif
  /* make output data */
    ptw=shm->d+shm->p;
    ptw+=4;               /* size (4) */
    un.i=time(0);
    i=un.i-mklong(ptr);
    if(i>=0 && i<1440)   /* with tow */
      {
      if(tow!=1)
        {
        sprintf(tb,"with TOW (diff=%ds)",i);
        write_log(logfile,tb);
        if(tow==0)
          {
          write_log(logfile,"reset");
          goto reset;
          }
        tow=1;
        }
      ptr+=4;
      *ptw++=un.c[0];  /* tow (H) */
      *ptw++=un.c[1];
      *ptw++=un.c[2];
      *ptw++=un.c[3];  /* tow (L) */
      }
    else if(tow!=0)
      {
      write_log(logfile,"without TOW");
      if(tow==1)
        {
        write_log(logfile,"reset");
        goto reset;
        }
      tow=0;
      }
    for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */

    do    /* loop for ch's */
      {
      gh=mklong(ptr);
      ch=(*(unsigned short *)&gh);
      sr=gh&0xfff;
      if((gh>>12)&0xf) gs=((gh>>12)&0xf)*(sr-1)+8;
      else gs=(sr>>1)+8;
      if(ch_table[ch])
        {
#if DEBUG
        fprintf(stderr,"%5d",gs);
#endif
        if(sr!=SR && sr>=SR_LOWER) 
          {
          win2fix(ptr,buf1,&ch1,&sr1);
          ds=(float)sr/(float)SR;
          t=0.0;
          buf1[sr]=buf1[sr+1];
          for(i=0;i<SR;i++)
            {
            j=(int)t;
            dt=t-(float)j;
            buf2[i]=buf1[j]+(int)(dt*(float)(buf1[j+1]-buf1[j]));
            t+=ds;
            }
          ptw+=winform(buf2,ptw,SR,ch);
          }
        else
          {
          memcpy(ptw,ptr,gs);
          ptw+=gs;
          }
        }
      else if(rest==1)
        {
        memcpy(ptw,ptr,gs);
        ptw+=gs;
        }
      ptr+=gs;
      } while(ptr<ptr_lim);
    if(tow) i=14;
    else i=10;
    if((un.i=ptw-(shm->d+shm->p))>i)
      {
      un.i=ptw-(shm->d+shm->p);
      shm->d[shm->p  ]=un.c[0]; /* size (H) */
      shm->d[shm->p+1]=un.c[1];
      shm->d[shm->p+2]=un.c[2];
      shm->d[shm->p+3]=un.c[3]; /* size (L) */

#if DEBUG
      for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+4+i]);
      printf(" : %d M\n",un.i);
#endif

      shm->r=shm->p;
      if(ptw>shm->d+shm->pl) ptw=shm->d;
      shm->p=ptw-shm->d;
      shm->c++;
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    if((ptr=ptr_lim)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) sleep(1);
    if(shr->c<c_save || mklong(ptr_save)!=size)
      {
      write_log(logfile,"reset");
      goto reset;
      }
    }
  }
