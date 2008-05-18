/* $Id: dewin.c,v 1.4.4.3 2008/05/18 07:43:44 uehira Exp $ */
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <signal.h>
#include  <unistd.h>

#include  <math.h>

#include "winlib.h"

#define   DEBUG   0

#define LINELEN     1024
#define MAX_FILT    100
#define MAX_SR      20000

  int buf[MAX_SR];
  double dbuf[MAX_SR];
  long au_header[]={0x2e736e64,0x00000020,0xffffffff,0x00000001,
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

wabort() {exit(0);}

/* bcd_dec(dest,sour) */
/*   char *sour; */
/*   int *dest; */
/*   { */
/*   int cntr; */
/*   for(cntr=0;cntr<6;cntr++) */
/*     dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf); */
/*   } */

print_usage()
  {
  fprintf(stderr,"usage: dewin (-camn) (-f [filter file]) [ch no.(in hex)] ([input file])\n");
  fprintf(stderr,"        -c  character output\n");
  fprintf(stderr,"        -a  audio format (u-law) output\n");
  fprintf(stderr,"        -m  minute block instead of second block\n");
  fprintf(stderr,"        -n  not fill absent part\n");
  fprintf(stderr,"        -f  [filter file] filter paramter file\n");
  }

read_data(ptr,fp)
     FILE *fp;
     unsigned char **ptr;
{
   static unsigned int size;
   unsigned char c[4];
   int re;
   if(fread(c,1,4,fp)==0) return 0;
   re=(c[0]<<24)+(c[1]<<16)+(c[2]<<8)+c[3];
   if(*ptr==0) *ptr=(unsigned char *)malloc(size=re*2);
   else if(re>size) *ptr=(unsigned char *)realloc(*ptr,size=re*2);
   (*ptr)[0]=c[0];
   (*ptr)[1]=c[1];
   (*ptr)[2]=c[2];
   (*ptr)[3]=c[3];
   if(fread(*ptr+4,1,re-4,fp)==0) return 0;
#if DEBUG > 2
   fprintf(stderr,"%02x%02x%02x%02x%02x%02x %d\n",(*ptr)[4],(*ptr)[5],(*ptr)[6],
	   (*ptr)[7],(*ptr)[8],(*ptr)[9],re);
#endif
   return re;
}

read_one_sec(ptr,ch,abuf)
  unsigned char *ptr; /* input */
  unsigned long ch; /* input */
  register long *abuf;/* output */
  {
  int g_size,s_rate;
  register unsigned char *dp;
  unsigned char *ddp;
  unsigned long sys_ch;

  dp=ptr+10;
  ddp=ptr+mklong(ptr);
  while(1)
    {
    if((g_size=win2fix(dp,abuf,&sys_ch,(long *)&s_rate))==0) return 0;
    if(sys_ch==ch) return s_rate;
    if((dp+=g_size)>=ddp) return 0;
    }
  return 0;
  }

/************************************************************************/
/*      Copyright 1989 by Rich Gopstein and Harris Corporation          */
/*                                                                      */
/*      Permission to use, copy, modify, and distribute this software   */
/*      and its documentation for any purpose and without fee is        */
/*      hereby granted, provided that the above copyright notice        */
/*      appears in all copies and that both that copyright notice and   */
/*      this permission notice appear in supporting documentation, and  */
/*      that the name of Rich Gopstein and Harris Corporation not be    */
/*      used in advertising or publicity pertaining to distribution     */
/*      of the software without specific, written prior permission.     */
/*      Rich Gopstein and Harris Corporation make no representations    */
/*      about the suitability of this software for any purpose.  It     */
/*      provided "as is" without express or implied warranty.           */
/************************************************************************/

/************************************************************************/
/* sound2sun.c - Convert sampled audio files into uLAW format for the   */
/*               Sparcstation 1.                                        */
/*               Send comments to ..!rutgers!soleil!gopstein            */
/************************************************************************/
/*									*/
/*  Modified November 27, 1989 to convert to 8000 samples/sec           */
/*   (contrary to man page)                                             */
/*  Modified December 13, 1992 to write standard Sun .au header with	*/
/*   unspecified length.  Also made miscellaneous changes for 		*/
/*   VMS port.  (K. S. Kubo, ken@hmcvax.claremont.edu)			*/
/*  Fixed Bug with converting slow sample speeds			*/
/*									*/
/************************************************************************/

/* convert two's complement ch into uLAW format */

unsigned int
cvt(ch)
     int ch;
{

  int mask;

  if (ch < 0) {
    ch = -ch;
    mask = 0x7f;
  } else {
    mask = 0xff;
  }

  if (ch < 32) {
    ch = 0xF0 | 15 - (ch / 2);
  } else if (ch < 96) {
    ch = 0xE0 | 15 - (ch - 32) / 4;
  } else if (ch < 224) {
    ch = 0xD0 | 15 - (ch - 96) / 8;
  } else if (ch < 480) {
    ch = 0xC0 | 15 - (ch - 224) / 16;
  } else if (ch < 992) {
    ch = 0xB0 | 15 - (ch - 480) / 32;
  } else if (ch < 2016) {
    ch = 0xA0 | 15 - (ch - 992) / 64;
  } else if (ch < 4064) {
    ch = 0x90 | 15 - (ch - 2016) / 128;
  } else if (ch < 8160) {
    ch = 0x80 | 15 - (ch - 4064) /  256;
  } else {
    ch = 0x80;
  }
return (mask & ch);
}
    /*ulaw = cvt(chr * 16);*/

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
      fputs("filter order exceeded limit\n", stderr);
      exit(1);
   }
}

main(argc,argv)
  int argc;
  char *argv[];
  {
  int i,j,k,sec,mainsize,sr,sr_save,c,form,nofill,zero,minblock,
    time1[6],time2[6],time3[6];
  unsigned long sysch;
  static unsigned char *mainbuf;
  FILE *f_main,*f_filter;
  unsigned char cc;
  extern int optind;
  extern char *optarg;
  char  txtbuf[LINELEN];
  int   filter_flag;
  struct Filter flt;
  double uv[MAX_FILT*4];

  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);
  form=nofill=filter_flag=minblock=0;
  for(i=0;i<MAX_FILT*4;++i)
    uv[i]=0.0;

  while((c=getopt(argc,argv,"chmnaf:"))!=EOF){
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
    }  /*  End of "while((c=getopt(argc,argv,"chmnaf:"))!=EOF){" */

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

  sysch=strtol(argv[optind+1],0,16);

  if(argc<3+optind) f_main=stdin;
  else if((f_main=fopen(argv[2+optind],"r"))==NULL){
     perror("dewin");
     exit(1);
  }

  sec=sr_save=i=0;
  while(mainsize=read_data(&mainbuf,f_main)){
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
	   cc=cvt(buf[j]*256);
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
  fprintf(stderr,"%02d%02d%02d.%02d%02d%02d (%d[%d] blks)\n",
	  time3[0],time3[1],time3[2],time3[3],time3[4],time3[5],i,sec);
  exit(0);
}
