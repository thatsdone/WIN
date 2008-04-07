/*  program "shmdump.c" 6/14/94 urabe */
/*  revised 5/29/96 */
/*  Little Endian (uehira) 8/27/96 */
/*  98.5.21 RT-TS */
/*  99.4.20 byte-order-free */
/*  2000.3.13 packet_id format */
/*  2000.4.17 deleted definition of usleep() */
/*  2000.4.24 strerror() */
/*  2000.4.28 -aonwz -s [s] -f [chfile] options */
/*  2002.1.15 -m for MON data */
/*  2002.2.28 size at EOB in shm_in */
/*  2002.5.2 i<1000 -> 1000000 */
/*  2002.6.24 -t for text output, 6.26 fixed */
/*  2002.10.27 stdin input */
/*  2003.3.26,4.4 rawdump (-r) mode */
/*  2003.4.16 bug fix (last sec not outputted in stdin mode) */
/*  2003.5.14 fflush(fpout) inserted for rawdump */
/*  2003.5.31 filtering option -L -H -B -R (tsuru) */
/*  2003.6.1 bug fix (-f) mode */
/*  2003.6.4 output filtering info to stderr (tsuru) */
/*  2003.7.24 -x (hexadecimal dump) and -f (file output) */
/*  2003.11.3 splitted sprintf(tb,...) / use time_t */
/*  2004.10.14 XINETD compile option */
/*  2008.4.5 bug fix : unsigned long wsize -> long wsize */

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

#include <unistd.h>
#include <errno.h>

#include "winlib.h"
#include "subst_func.h"

#define DEBUG       1
#define MAXMESG     2000
#define XINETD      0

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

static
err_sys_local(ptr)
  char *ptr;
  {
#if XINETD
#else
  fprintf(stderr,"%s %s\n",ptr,strerror(errno));
#endif
  exit(0);
  }

advance_s(shm,shp,c_save,size)
  struct Shm *shm;
  unsigned int *shp,*c_save,*size;
  {
  int shpp,tmp,i;
  shpp=(*shp);
  i=shm->c-(*c_save);
  if(!(i<1000000 && i>=0) || *size!=mklong(shm->d+(*shp))) return -1;
  if(shpp+(*size)>shm->pl) shpp=0; /* advance pointer */
  else shpp+=(*size);
  if(shm->p==shpp) return 0;
  *c_save=shm->c;
  *size=mklong(shm->d+(*shp=shpp));
  return 1;
  }

bcd_dec2(dest,sour)
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

/* filter */
#define MAX_FILT    100
#define MAX_SR      20000

struct Filter
{
  char kind[12];
  double fl,fh,fp,fs,ap,as;
  int m_filt;                  /* order of filter */
  int n_filt;                  /* order of Butterworth function */
  double coef[MAX_FILT*4];     /* filter coefficients */
  double gn_filt;              /* gain factor of filter */
};

get_filter(sr,f)
     int    sr;
     struct Filter  *f;
{
   double   dt;

   dt=1.0/(double)sr;
   if(strcmp(f->kind,"LPF")==0)
     butlop(f->coef,&f->m_filt,&f->gn_filt,&f->n_filt,
	    f->fp*dt,f->fs*dt,f->ap,f->as);
   else if(strcmp(f->kind,"HPF")==0)
     buthip(f->coef,&f->m_filt,&f->gn_filt,&f->n_filt,
	    f->fp*dt,f->fs*dt,f->ap,f->as);
   else if(strcmp(f->kind,"BPF")==0)
     butpas(f->coef,&f->m_filt,&f->gn_filt,&f->n_filt,
	    f->fl*dt,f->fh*dt,f->fs*dt,f->ap,f->as);

   if(f->m_filt>MAX_FILT){
#if XINETD
#else
      fputs("filter order exceeded limit\n", stderr);
#endif
      exit(1);
   }
}

main(argc,argv)
  int argc;
  char *argv[];
  {
#define SR_MON 5
  key_t shm_key_in;
  union {
    unsigned long i;
    unsigned short s;
    char c[4];
    } un;
  int i,j,k,c_save_in,shp_in,shmid_in,size_in,shp,c_save,wtow,c,out,all,mon,
    ch,sr,gs,size,chsel,nch,search,seconds,tbufp,bufsize,zero,nsec,aa,bb,end,
    eobsize,eobsize_count,size2,tout,abuf[4096],bufsize_in,rawdump,quiet,
    hexdump;
  time_t tow,time_end,time_now;
  long wsize;
  unsigned int packet_id;
  unsigned char *ptr,tbuf[256],*ptr_lim,*buf,chlist[65536/8],*ptw,tms[6],
    tb[256],*ptr1,*ptr_save;
  struct Shm *shm_in;
  struct tm *nt;
  int rt[6],ts[6];
  extern int optind;
  extern char *optarg;
  FILE *fplist,*fp;
  char *tmpdir,fname[1024];
  static unsigned int mask[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
#define setch(a) (chlist[a>>3]|=mask[a&0x07])
#define testch(a) (chlist[a>>3]&mask[a&0x07])

  int filter_flag = 0;   /* filter var */
  int flt_kind;       
  int SR=0;
  struct Filter *flt;
  int chindex[65536];
  double (*uv)[MAX_FILT*4];
  double dbuf[MAX_SR];
  float flt_fl, flt_fh, flt_fp, flt_fs, flt_ap, flt_as;
  int chid;              /* filter var end */

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  sprintf(tb," usage : '%s (-amoqrtwxz) (-s [s]) (-f [chfile]/-) (-R [freq])\n", 
    progname);
  strcat(tb,
    "   (-L [fp]:[fs]:[ap]:[as] -H [fp]:[fs]:[ap]:[as] -B [fl]:[fh]:[fs]:[ap]:[as])\n");
  strcat(tb,"   [shm_key]/- ([ch] ...)'");
  search=out=all=seconds=win=zero=mon=tout=rawdump=quiet=hexdump=0;
  fplist=stdout;
  *fname=0;

  while((c=getopt(argc,argv,"amoqrs:twf:xzL:H:B:R:"))!=EOF)
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
        if((fpout=fopen(outfile,"w+"))==0) err_sys_local(outfile);
        win=out=1;
        break;
      case 'm':   /* MON format */
        mon=1;
        break;
      case 'a':   /* list all channels */
        all=1;
        break;
      case 'q':   /* supress listing */
        quiet=1;
        break;
      case 'r':   /* raw dump mode */
        rawdump=1;
        fpout=stdout;
        fplist=stderr;
        break;
      case 'x':   /* hexadecimal dump mode */
        hexdump=1;
        fpout=stdout;
        fplist=stderr;
        break;
      case 'z':   /* read from the beginning of the SHM buffer */
        zero=1;
        break;
      case 't':   /* text out */
        tout=out=1;
        fpout=stdout;
        fplist=stderr;
        break;
      case 's':   /* period in sec */
        seconds=atoi(optarg);
        break;
      case 'f':   /* file name */
        strcpy(fname,optarg);
        break;
      case 'L':   /* LowPass */
        filter_flag=1;
        flt_kind=0;
        sscanf(optarg,"%f:%f:%f:%f",&flt_fp,&flt_fs,&flt_ap,&flt_as);
#if XINETD
#else
        fprintf(stderr,"lpf fp=%g fs=%g ap=%g as=%g\n",flt_fp,flt_fs,flt_ap,flt_as);
#endif
        break;
      case 'H':   /* HighPass */
        filter_flag=1;
        flt_kind=1;
        sscanf(optarg,"%f:%f:%f:%f",&flt_fp,&flt_fs,&flt_ap,&flt_as);
#if XINETD
#else
        fprintf(stderr,"bpf fp=%g fs=%g ap=%g as=%g\n",flt_fp,flt_fs,flt_ap,flt_as);
#endif
        break;
      case 'B':   /* BandPass */
        filter_flag=1;
        flt_kind=2;
        sscanf(optarg,"%f:%f:%f:%f:%f",&flt_fl,&flt_fh,&flt_fs,&flt_ap,&flt_as);
#if XINETD
#else
        fprintf(stderr,"bpf fl=%g fh=%g fs=%g ap=%g as=%g\n",flt_fl,flt_fh,flt_fs,flt_ap,flt_as);
#endif
        break;
      case 'R':   /* resampling freq */
        SR=atoi(optarg);
        break;
      default:
#if XINETD
#else
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
#endif
        exit(1);
      }
    }
  optind--;
  if(argc<2+optind)
    {
#if XINETD
#else
    fprintf(stderr,"%s\n",tb);
#endif
    exit(1);
    }

  if(rawdump || hexdump) search=all=win=out=tout=mon=0;

  if(*fname)
    {
    if(!rawdump && !hexdump) /* channel list file */
      {
      if(*fname=='-') fp=stdin;
      else if((fp=fopen(fname,"r"))==0) err_sys_local(outfile);
      nch=0;
      for(i=0;i<65536/8;i++) chlist[i]=0;
      while(fgets(tbuf,100,fp))
        {
        if(*tbuf=='#' || sscanf(tbuf,"%x",&chsel)<0) continue;
        chsel&=0xffff;
        setch(chsel);
        chindex[chsel]=nch;
#if XINETD
#else
        fprintf(stderr,"%d: %04X\n", chindex[chsel], chsel);
#endif
        nch++;
        }
      fclose(fp);
      if(nch) search=1;
      }
    }

  if(quiet) fplist=fopen("/dev/null","a");

  if(strcmp(argv[1+optind],"-"))
    {
    shm_key_in=atoi(argv[1+optind]); /* shared memory */
    if((shmid_in=shmget(shm_key_in,0,0))<0) err_sys_local("shmget");
    if((shm_in=(struct Shm *)shmat(shmid_in,(char *)0,0))==(struct Shm *)-1)
      err_sys_local("shmat");
    /*fprintf(stderr,"in : shm_key_in=%d id=%d\n",shm_key_in,shmid_in);*/
    }
  else /* read data from stdin instead of Shm */
    {
    shm_key_in=0;
    bufsize_in=MAXMESG*100;
    if((shm_in=(struct Shm *)malloc(bufsize_in))==0) err_sys_local("malloc inbuf");
    zero=0;
    }

  if(argc>2+optind)
    {
    nch=0;
    for(i=0;i<65536/8;i++) chlist[i]=0;
    for(i=2+optind;i<argc;i++)
      {
      chsel=strtol(argv[i],0,16);
      setch(chsel);
      chindex[chsel]=nch; /* filter */
#if XINETD
#else
      fprintf(stderr,"%d: %04X\n", chindex[chsel], chsel);  /* filter */
#endif
      nch++;
      }
    if(nch) search=1;
    }

  /* allocate buf */
  if(out)
    {
    if(all) bufsize=MAXMESG*100;
    else bufsize=MAXMESG*nch;
    if((buf=(unsigned char *)malloc(bufsize))==0) err_sys_local("malloc outbuf");
    }

  /* alloc flt & uv */
  if((flt=malloc(nch*sizeof(struct Filter)))==0) err_sys_local("malloc filter");
  if((uv=malloc(nch*sizeof(double[MAX_FILT*4])))==0) err_sys_local("malloc uv");
  for(j=0;j<nch;j++)
    for(i=0;i<MAX_FILT*4;i++)
      uv[j][i]=0.0;

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
  if(shm_key_in)
    {
    while(shm_in->r==(-1)) usleep(200000);
    c_save_in=shm_in->c;
    if(zero) size_in=mklong(shm_in->d+(shp_in=0));
    else size_in=mklong(shm_in->d+(shp_in=shm_in->r));
    }
  wtow=0;
  ptw=buf+4;

  if(mklong(shm_in->d+shp_in+size_in-4)==size_in) eobsize=1;
  else eobsize=0;
  eobsize_count=eobsize;
  nch=end=0;

  while(1)
    {
    if(shm_key_in)
      {
      i=advance_s(shm_in,&shp_in,&c_save_in,&size_in);
      if(i==0)
        {
        usleep(100000);
        continue;
        }
      else if(i<0) goto reset;

      if(size_in==mklong(shm_in->d+shp_in+size_in-4)) eobsize_count++;
      else eobsize_count=0;
      if(eobsize && eobsize_count==0) goto reset;
      if(!eobsize && eobsize_count>3) goto reset;
      }
    else
      {
      if((i=fread(shm_in->d,1,4,stdin))==0) end=1;
      else
        {
        size_in=mklong(shm_in->d);
        if(sizeof(long)*4+size_in>bufsize_in)
          {
          bufsize_in=sizeof(long)*4+size_in+MAXMESG*100;
          if((shm_in=(struct Shm *)realloc(shm_in,bufsize_in))==0)
            {
#if XINETD
#else
            fprintf(stderr,"inbuf realloc failed !\n");
#endif
            exit(1);
            }
          }
        if(fread(shm_in->d+4,1,size_in-4,stdin)==0) end=1;
        }
      shp_in=0;
      }
    if(end)
      {
      if(out && (all || search)) goto last_out;
      else ctrlc();
      }
    if(eobsize) sprintf(tbuf,"%4d B ",size_in);
    else sprintf(tbuf,"%4d : ",size_in);
    ptr=ptr_save=shm_in->d+shp_in+4;
    ptr_lim=shm_in->d+shp_in+size_in;
    if(eobsize) ptr_lim-=4;
    if(*ptr>0x20 && *ptr<0x90) /* with tow */
      {
      wtow=1;
      tow=mklong(ptr);
      nt=localtime(&tow);
/*      printf("%d ",tow);*/
      ptr+=4;
      ptr_save+=4;
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
      bcd_dec2(ts,ptr);
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
        if(!mon)
          {
          sr=ptr[3]+(((long)(ptr[2]&0x0f))<<8);
          size=(ptr[2]>>4)&0x7;
          if(size) gs=size*(sr-1)+8;
          else gs=(sr>>1)+8;
          }
        else
          {
          ptr1=ptr;
          ptr1+=2;
          for(j=0;j<SR_MON;j++)
            {
            aa=(*(ptr1++));
            bb=aa&3;
            if(bb) for(k=0;k<bb*2;k++) ptr1++;
            else ptr1++;
            }
          gs=ptr1-ptr;
          }
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
              bufsize=ptw-buf+gs+MAXMESG*100;
              if((buf=(unsigned char *)realloc(buf,bufsize))==0)
                {
#if XINETD
#else
                fprintf(stderr,"buf realloc failed !\n");
#endif
                exit(1);
                }
              }
            if(memcmp(buf+4,tms,6)) /* new time */
              {
last_out:     if((wsize=ptw-buf)>10)
                {
                buf[0]=wsize>>24;  /* size (H) */
                buf[1]=wsize>>16;
                buf[2]=wsize>>8;
                buf[3]=wsize;      /* size (L) */
                if(tout) /* convert to text before output */
                  {
                  fprintf(fpout,"%02X %02X %02X %02X %02X %02X %d\n",
                    buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],nch);
                  ptw=buf+10;
                  while(ptw<buf+wsize)
                    {
                    ptw+=win2fix(ptw,(long *)abuf,(long *)&ch,(long *)&sr);
                    if ( SR > 0 ) fprintf(fpout,"%04X %d",ch,SR);
                    else          fprintf(fpout,"%04X %d",ch,sr);
                    if ( filter_flag ) {   /* filtering start */
                      chid=chindex[ch];
                      if ( flt_kind == 0 ) {
                        sprintf(flt[chid].kind,"LPF");
                      } else if ( flt_kind == 1 ) {
                        sprintf(flt[chid].kind,"HPF");
                      } else {
                        sprintf(flt[chid].kind,"BPF");
                      }
                      flt[chid].fp=(double)flt_fp; flt[chid].fs=(double)flt_fs;
                      flt[chid].fl=(double)flt_fl; flt[chid].fh=(double)flt_fh; 
                      flt[chid].ap=(double)flt_ap; flt[chid].as=(double)flt_as;
                      get_filter(sr,&flt[chid]);
                      for(i=0;i<sr;i++)
                        dbuf[i]=(double)abuf[i];
                      tandem(dbuf,dbuf,sr,flt[chid].coef,flt[chid].m_filt,1,uv[chid]);
                      for(i=0;i<sr;i++)
                        abuf[i]=(int)(dbuf[i]*flt[chid].gn_filt);
		    }                     /* filtering end */
                    if ( SR > 0 ) for(i=0;i<SR;i++) fprintf(fpout," %d",abuf[i*sr/SR]);
                    else          for(i=0;i<sr;i++) fprintf(fpout," %d",abuf[i]);
                    fprintf(fpout,"\n");
                    fflush(fpout);
                    }
                  }
                else
                  {
                  fwrite(buf,1,wsize,fpout);
                  fflush(fpout);
                  }
                nsec++;
                }
              if(end) ctrlc();
              ptw=buf+4;
              memcpy(ptw,tms,6); /* TS */
              ptw+=6;
              nch=0;
              }
            memcpy(ptw,ptr,gs);
            ptw+=gs;
            nch++;
            }
          }
        ptr+=gs;
        } while(ptr<ptr_lim);
      }
    else
      {
      for(j=0;j<8;j++) sprintf(tbuf+strlen(tbuf),"%02X",*ptr++);
      sprintf(tbuf+strlen(tbuf),"...\n");
      fputs(tbuf,fplist);
      }
    if(rawdump)
      {
      if(*fname) fpout=fopen(fname,"a");
      fwrite(ptr_save,1,ptr_lim-ptr_save,fpout);
      if(*fname) fclose(fpout);
      else fflush(fpout);
      }
    else if(hexdump)
      {
      if(*fname) fpout=fopen(fname,"a");
      for(ptr=ptr_save;ptr<ptr_lim;ptr++) fprintf(fpout,"%02X",*ptr);
      fprintf(fpout,"\n");
      if(*fname) fclose(fpout);
      }
    time(&time_now);
/*    if(end || (seconds && (time_now>time_end || nsec>=seconds))) ctrlc();*/
    if(end || (seconds && (time_now>time_end))) ctrlc(); /* 071220 urabe */
    }
  }
