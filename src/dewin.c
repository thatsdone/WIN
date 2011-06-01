/* $Id: dewin.c,v 1.6 2011/06/01 11:09:20 uehira Exp $ */

/* program dewin  1994.4.11-4.20  urabe */
/*                1996.2.23 added -n option */
/*                1996.9.12 added -8 option */
/*                1998.5.16 LITTLE ENDIAN and High Sampling Rate (uehira) */
/*                1998.5.18 add -f [filter file] option (uehira) */
/*                1998.6.26 yo2000 urabe */
/*                1999.7.19 endian-free */
/*                2000.3.10 abort->wabort */
/*                2000.3.10 added -m option */
/*                2003.10.29 exit()->exit(0) */
/*                2010.04.03 64bit? */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <signal.h>
#include  <unistd.h>
#include  <limits.h>

#include  <math.h>

#include "winlib.h"


/* #define   DEBUG   0 */

#define LINELEN     1024
#define MAX_FILT    100
/* #define MAX_SR      20000 */
#define MAX_SR      HEADER_5B

static const char  rcsid[] =
   "$Id: dewin.c,v 1.6 2011/06/01 11:09:20 uehira Exp $";

static int32_w buf[MAX_SR];
static double dbuf[MAX_SR];
static uint32_w au_header[8]={0x2e736e64,0x00000020,0xffffffff,0x00000001,
		  0x00001f40,0x00000001,0x00000000,0x00000000};

struct Filter
{
   char kind[12];
   double fl,fh,fp,fs,ap,as;
   int m_filt;       /* order of filter */
   int n_filt;       /* order of Butterworth function */
   double coef[MAX_FILT*4]; /* filter coefficients */
   double gn_filt;     /* gain factor of filter */ 
};

/* prototypes */
static void wabort(void);
static void print_usage(void);
static WIN_sr read_one_sec(uint8_w *, WIN_ch, register int32_w *);
static void get_filter(WIN_sr, struct Filter *);
int main(int, char *[]);

static void
wabort(void) {exit(0);}

/* bcd_dec(dest,sour) */
/*   char *sour; */
/*   int *dest; */
/*   { */
/*   int cntr; */
/*   for(cntr=0;cntr<6;cntr++) */
/*     dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf); */
/*   } */

static void
print_usage(void)
  {

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr,"usage: dewin (-camn) (-f [filter file]) [ch no.(in hex)] ([input file])\n");
  fprintf(stderr,"        -c  character output\n");
  fprintf(stderr,"        -a  audio format (u-law) output\n");
  fprintf(stderr,"        -m  minute block instead of second block\n");
  fprintf(stderr,"        -n  not fill absent part\n");
  fprintf(stderr,"        -f  [filter file] filter paramter file\n");
  }

static WIN_sr
read_one_sec(uint8_w *ptr, WIN_ch ch, register int32_w *abuf)
  /* uint8_w            *ptr; /\* input *\/ */
  /* WIN_ch              ch; /\* input *\/ */
  /* register int32_w  *abuf; /\* output *\/ */
  {
  uint32_w g_size;
  WIN_sr   s_rate;
  register uint8_w *dp;
  uint8_w *ddp;
  WIN_ch   sys_ch;

  dp=ptr+10;
  ddp=ptr+mkuint4(ptr);
  for(;;)
    {
    if((g_size=win2fix(dp,abuf,&sys_ch,&s_rate))==0) return (0);
    if(sys_ch==ch) return (s_rate);
    if((dp+=g_size)>=ddp) return (0);
    }
  return (0);
  }

static void
get_filter(WIN_sr sr, struct Filter *f)
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
      fputs("filter order exceeded limit\n", stderr);
      exit(1);
   }
}

int
main(int argc, char *argv[])
  {
  int j,k,c,form,nofill,zero=0,minblock,
    time1[6],time2[6],time3[6];
  unsigned long  i, sec;   /* 64bit ok */
  WIN_bs  mainsize;
  WIN_sr  sr, sr_save;
  WIN_ch sysch;
  static uint8_w *mainbuf=NULL;
  size_t  mainbuf_siz;
  FILE *f_main,*f_filter;
  uint8_w cc;
  char  txtbuf[LINELEN];
  int   filter_flag;
  struct Filter flt;
  double uv[MAX_FILT*4];

  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);
  form=nofill=filter_flag=minblock=0;
  for(i=0;i<MAX_FILT*4;++i)
    uv[i]=0.0;

  while((c=getopt(argc,argv,"chmnaf:"))!=-1){
     switch(c){
      case 'a':
        form=8;   /* 8bit format output*/
        break;
      case 'c':
        form=1;   /* numerical character output*/
        break;
      case 'm':
        minblock=1; /* minute block */
        break;
      case 'n':
        nofill=1; /* don't fill absent seconds */
        break;
      case 'f':  /* filter output */
	if(NULL==(f_filter=fopen(optarg,"r"))){
	   fprintf(stderr,"Cannot open filter parameter file: %s\n",optarg);
	   exit(1);
	}
	while(!feof(f_filter)){
	   if(fgets(txtbuf,LINELEN,f_filter)==NULL){
	      fputs("No filter parameter.\n",stderr);
	      exit(1);
	   }
	   if(*txtbuf=='#') continue;
	   sscanf(txtbuf,"%3s",flt.kind);
	   /*fprintf(stderr,"%s\n",flt.kind);*/
	   if(strcmp(flt.kind,"LPF")==0 || strcmp(flt.kind,"lpf")==0)
	     strcpy(flt.kind,"LPF");
	   else if(strcmp(flt.kind,"HPF")==0 || strcmp(flt.kind,"hpf")==0)
	     strcpy(flt.kind,"HPF");
	   else if(strcmp(flt.kind,"BPF")==0 || strcmp(flt.kind,"bpf")==0)
	     strcpy(flt.kind,"BPF");
	   else{
	      fprintf(stderr,"bad filter name '%s'\n",flt.kind);
	      exit(1);
	   }
	   for(j=0;j<strlen(txtbuf)-3;j++){
	      if(strncmp(txtbuf+j,"fl=",3)==0)
		sscanf(txtbuf+j+3,"%lf",&flt.fl);
	      else if(strncmp(txtbuf+j,"fh=",3)==0)
		sscanf(txtbuf+j+3,"%lf",&flt.fh);
	      else if(strncmp(txtbuf+j,"fp=",3)==0)
		sscanf(txtbuf+j+3,"%lf",&flt.fp);
	      else if(strncmp(txtbuf+j,"fs=",3)==0)
		sscanf(txtbuf+j+3,"%lf",&flt.fs);
	      else if(strncmp(txtbuf+j,"ap=",3)==0)
		sscanf(txtbuf+j+3,"%lf",&flt.ap);
	      else if(strncmp(txtbuf+j,"as=",3)==0)
		sscanf(txtbuf+j+3,"%lf",&flt.as);
	   }
	   if(!(strcmp(flt.kind,"LPF")==0 && flt.fp<flt.fs) &&
	      !(strcmp(flt.kind,"HPF")==0 && flt.fs<flt.fp) &&
	      !(strcmp(flt.kind,"BPF")==0 && flt.fl<flt.fh && flt.fh<flt.fs)){
	      fprintf(stderr,"%s %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f",
		      flt.kind,flt.fl,flt.fh,flt.fp,flt.fs,flt.ap,flt.as);
	      fprintf(stderr," : illegal filter\n");
	      exit(1);
	   }
	   break;
	}
	/*close(f_filter);*/
	/*fprintf(stderr,"filter=%s\n",optarg);*/
	filter_flag=1;
	break;
      case 'h':
      default:
        print_usage();
        exit(0);
      }
    }  /*  End of "while((c=getopt(argc,argv,"chmnaf:"))!=-1){" */

#if DEBUG
  fprintf(stderr,"filter_flag = %d\n",filter_flag);
#endif
  if(filter_flag){
     fputs("Type  Low  High  Pass  Stop    AP    AS\n",stderr);
     fprintf(stderr,"%s %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f\n",
	     flt.kind,flt.fl,flt.fh,flt.fp,flt.fs,flt.ap,flt.as);
  }

  optind--;
  if(argc<optind+2){
     print_usage();
     exit(1);
  }

  sysch=(WIN_ch)strtol(argv[optind+1],0,16);

  if(argc<3+optind) f_main=stdin;
  else if((f_main=fopen(argv[2+optind],"r"))==NULL){
     perror("dewin");
     exit(1);
  }

  sr_save=0;
  sec=i=0;
  while((mainsize=read_onesec_win(f_main,&mainbuf,&mainbuf_siz))){
     if((sr=read_one_sec(mainbuf,sysch,buf))==0) continue;
     bcd_dec(time3,mainbuf+4);
     if(sr_save==0){
	if(minblock) fprintf(stderr,"%04X  %d samples/min  ",sysch,sr);
	else fprintf(stderr,"%04X  %d Hz  ",sysch,sr);
	bcd_dec(time1,mainbuf+4);
	fprintf(stderr,"%02d%02d%02d.%02d%02d%02d -> ",
		time1[0],time1[1],time1[2],time1[3],time1[4],time1[5]);
	if(form==8) fwrite(au_header,sizeof(au_header),1,stdout);
     }
     else{
        if(minblock)
          {
	  time2[4]++;
	  adj_time_m(time2);
          }
        else
          {
	  time2[5]++;
	  adj_time(time2);
          }
	while(time_cmp(time2,time3,6)<0){  /* fill absent data */
	   if(nofill==0){
	      k=0;
	      cc=128;
	      if(form==1)
		for(j=0;j<sr_save;j++)
		  printf("0\n");
	      else if(form==8)
		for(j=0;j<sr_save;j++)
		  fwrite(&cc,1,1,stdout);
	      else
		for(j=0;j<sr_save;j++)
		  fwrite(&k,4,1,stdout);
	      i++;
	   }
          if(minblock)
            {
	    time2[4]++;
	    adj_time_m(time2);
            }
          else
            {
	    time2[5]++;
	    adj_time(time2);
            }
        }
     }
     if(filter_flag){
	get_filter(sr,&flt);
	/* fprintf(stderr,"m_filt=%d\n",flt.m_filt);*/
	for(j=0;j<sr;++j)
	  dbuf[j]=(double)buf[j];
	tandem(dbuf,dbuf,sr,flt.coef,flt.m_filt,1,uv);
	for(j=0;j<sr;++j)
	  buf[j]=(int)(dbuf[j]*flt.gn_filt);
     }

     if(form==1)
       for(j=0;j<sr;j++)
	 printf("%d\n",buf[j]);
     else if(form==8){
	if(sr_save==0){
	   zero=0;
	   for(j=0;j<sr;j++) zero+=buf[j];
	   zero/=sr;
        }
	for(j=0;j<sr;j++){
	   buf[j]-=zero;
	   cc=(uint8_w)ulaw(buf[j]*256);
	   /*        fprintf(stderr,"%d %d\n",buf[j],cc);*/
	   fwrite(&cc,1,1,stdout);
        }
     }
     else
       fwrite(buf,4,sr,stdout);
     i++;
     sr_save=sr;
     bcd_dec(time2,mainbuf+4);
     sec++;
  }
  fprintf(stderr,"%02d%02d%02d.%02d%02d%02d (%lu[%lu] blks)\n",
	  time3[0],time3[1],time3[2],time3[3],time3[4],time3[5],i,sec);
  exit(0);
}
