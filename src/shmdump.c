/* $Id: shmdump.c,v 1.3 2000/05/01 04:19:43 urabe Exp $ */
/*  program "shmdump.c" 6/14/94 urabe */
/*  revised 5/29/96 */
/*  Little Endian (uehira) 8/27/96 */
/*  98.5.21 RT-TS */
/*  99.4.20 byte-order-free */
/*  2000.3.13 packet_id format */
/*  2000.4.17 deleted definition of usleep() */
/*  2000.4.24 strerror() */
/*  2000.4.28 -aonwz -s [s] -f [chfile] options */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#define DEBUG       0
#define MAXMESG     2000

char *progname,outfile[256];
int win;
FILE *fpout;

struct Shm
  {
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

err_sys(ptr)
  char *ptr;
  {
  fprintf(stderr,"%s %s\n",ptr,strerror(errno));
  exit(0);
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

ctrlc()
  {
  char tb[100];
  if(win) /* invoke "win" */
    {
    fclose(fpout);
    sprintf(tb,"win %s",outfile);
    system(tb);
    unlink(outfile);
    }
  exit(0);
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
  int i,j,k,c_save_in,shp_in,shmid_in,size_in,shp,c_save,wtow,c,out,all,
    ch,sr,gs,size,chsel,nch,search,seconds,tbufp,bufsize,zero,nsec;
  unsigned long tow,wsize,time_end,time_now;
  unsigned int packet_id;
  unsigned char *ptr,tbuf[100],*ptr_lim,*buf,chlist[65536/8],*ptw,tms[6];
  struct Shm *shm_in;
  struct tm *nt;
  int rt[6],ts[6];
  extern int optind;
  extern char *optarg;
  FILE *fplist,*fp;
  char *tmpdir;
  static unsigned int mask[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
#define setch(a) (chlist[a>>3]|=mask[a&0x07])
#define testch(a) (chlist[a>>3]&mask[a&0x07])

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  search=out=all=seconds=win=zero=0;
  fplist=stdout;

  while((c=getopt(argc,argv,"aoqs:wf:z"))!=EOF)
    {
    switch(c)
      {
      case 'o':   /* data output */
        out=1;
        fpout=stdout;
        fplist=stderr;
        break;
      case 'w':   /* invoke win */
        if((tmpdir=getenv("TMP")) || (tmpdir=getenv("TEMP")))
          sprintf(outfile,"%s/%s.%d",tmpdir,progname,getpid()); 
        else sprintf(outfile,"%s.%d",progname,getpid());
        if((fpout=fopen(outfile,"w+"))==0) err_sys(outfile);
        win=out=1;
        break;
      case 'a':   /* list all channels */
        all=1;
        break;
      case 'q':   /* supress listing */
        fplist=fopen("/dev/null","a");
        break;
      case 'z':   /* read from the beginning of the SHM buffer */
        zero=1;
        break;
      case 's':   /* period in sec */
        seconds=atoi(optarg);
        break;
      case 'f':   /* channel file */
        if(*optarg='-') fp=stdin;
        else if((fp=fopen(optarg,"r"))==0) err_sys(outfile);
        nch=0;
        for(i=0;i<65536/8;i++) chlist[i]=0;
        while(fgets(tbuf,100,fp))
          {
          if(*tbuf=='#' || sscanf(tbuf,"%x",&chsel)<0) continue;
          chsel&=0xffff;
          setch(chsel);
          nch++;
          }
        if(nch) search=1;
        break;
      default:
        fprintf(stderr," usage : '%s (-aoqwz) (-s [s]) (-f [chfile]/-) [shm_key] ([ch] ...)'\n",progname);
        exit(1);
      }
    }
  optind--;
  if(argc<2+optind)
    {
    fprintf(stderr," usage : '%s (-aoqwz) (-s [s]) (-f [chfile]/-) [shm_key] ([ch] ...)'\n",progname);
    exit(0);
    }

  shm_key_in=atoi(argv[1+optind]);

  if(argc>2+optind)
    {
    nch=0;
    for(i=0;i<65536/8;i++) chlist[i]=0;
    for(i=2+optind;i<argc;i++)
      {
      chsel=strtol(argv[i],0,16);
      setch(chsel);
      nch++;
      }
    if(nch) search=1;
    }

  /* allocate buf */
  if(out)
    {
    if(all) bufsize=MAXMESG*100;
    else bufsize=MAXMESG*nch;
    if((buf=(unsigned char *)malloc(bufsize))==0) err_sys("malloc");
    }

  /* shared memory */
  if((shmid_in=shmget(shm_key_in,0,0))<0) err_sys("shmget");
  if((shm_in=(struct Shm *)shmat(shmid_in,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");
/*fprintf(stderr,"in : shm_key_in=%d id=%d\n",shm_key_in,shmid_in);*/

  signal(SIGPIPE,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGTERM,(void *)ctrlc);
  if(seconds)
    {
    time(&time_end);
    time_end+=seconds;
    }

  nsec=0;
reset:
  while(shm_in->r==(-1)) usleep(200000);
  c_save_in=shm_in->c;
  if(zero) size_in=mklong(shm_in->d+(shp_in=0));
  else size_in=mklong(shm_in->d+(shp_in=shm_in->r));
  wtow=0;
  ptw=buf+4;
  while(1)
    {
    i=advance_s(shm_in,&shp_in,&c_save_in,&size_in);
    if(i==0)
      {
      usleep(200000);
      continue;
      }
    else if(i<0) goto reset;
    sprintf(tbuf,"%4d : ",size_in);
    ptr=shm_in->d+shp_in+4;
    ptr_lim=shm_in->d+shp_in+size_in;
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
      sprintf(tbuf+strlen(tbuf),"RT ");
      sprintf(tbuf+strlen(tbuf),"%02d",rt[0]%100);
      for(j=1;j<6;j++) sprintf(tbuf+strlen(tbuf),"%02d",rt[j]);
      if(*ptr>=0xA0) packet_id=(*ptr++);
      else packet_id=0;
      bcd_dec(ts,ptr);
      j=time_dif(rt,ts); /* returns t1-t2(sec) */
      sprintf(tbuf+strlen(tbuf)," %2d TS ",j);
      }
    else wtow=0;
    if(wtow && packet_id>=0xA0)
      {
      sprintf(tbuf+strlen(tbuf),"%02X:",packet_id);
      search=all=out=0;
      }
    if(out) memcpy(tms,ptr,6); /* save TS */
    for(j=0;j<6;j++) sprintf(tbuf+strlen(tbuf),"%02X",*ptr++);
    sprintf(tbuf+strlen(tbuf)," ");
    tbufp=strlen(tbuf);
    if(search || all)
      {
      i=0;
      do
        {
        tbuf[tbufp]=0;
        ch=ptr[1]+(((long)ptr[0])<<8);
        sr=ptr[3]+(((long)(ptr[2]&0x0f))<<8);
        size=(ptr[2]>>4)&0x7;
        if(size) gs=size*(sr-1)+8;
        else gs=(sr>>1)+8;
        if(all ||(search && testch(ch)))
          {
          for(j=0;j<8;j++) sprintf(tbuf+strlen(tbuf),"%02X",ptr[j]);
          sprintf(tbuf+strlen(tbuf),"(%d)",gs);
          fputs(tbuf,fplist);
          fputs("\n",fplist);
          if(out)
            {
            if(ptw-buf+gs>bufsize)
              {
              j=ptw-buf;
              if((buf=(unsigned char *)realloc(buf,bufsize+MAXMESG*100))==0)
                {
                fprintf(stderr,"buf realloc failed !\n");
                break;
                }
              else
                {
                bufsize+=MAXMESG*100;
                ptw=buf+j;
                }
              }
            if(memcmp(buf+4,tms,6)) /* new time */
              {
              if((wsize=ptw-buf)>10)
                {
                buf[0]=wsize>>24;  /* size (H) */
                buf[1]=wsize>>16;
                buf[2]=wsize>>8;
                buf[3]=wsize;      /* size (L) */
                fwrite(buf,1,wsize,fpout);
                fflush(fpout);
                nsec++;
                }
              ptw=buf+4;
              memcpy(ptw,tms,6); /* TS */
              ptw+=6;
              }
            memcpy(ptw,ptr,gs);
            ptw+=gs;
            }
          i=1;
          }
        ptr+=gs;
        nch++;
        } while(ptr<ptr_lim);
      }
    else
      {
      for(j=0;j<8;j++) sprintf(tbuf+strlen(tbuf),"%02X",*ptr++);
      sprintf(tbuf+strlen(tbuf),"...\n");
      fputs(tbuf,fplist);
      }
    time(&time_now);
    if(seconds && (time_now>time_end || nsec>=seconds)) ctrlc();
    }
  }
