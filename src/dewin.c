/* $Id: dewin.c,v 1.4.2.1 2009/01/05 14:55:54 uehira Exp $ */
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
#include  <signal.h>
#include  <math.h>

#include "subst_func.h"

#define   DEBUG   0

#define PI          3.141592654
#define HP          (PI/2.0)
#define LINELEN     1024
#define MAX_FILT    100
#define MAX_SR      20000
#define HSR         1

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

adj_time(tm)
  int *tm;
  {
  if(tm[5]==60)
    {
    tm[5]=0;
    if(++tm[4]==60)
      {
      tm[4]=0;
      if(++tm[3]==24)
        {
        tm[3]=0;
        tm[2]++;
        switch(tm[1])
          {
          case 2:
            if(tm[0]%4==0)
              {
              if(tm[2]==30)
                {
                tm[2]=1;
                tm[1]++;
                }
              break;
              }
            else
              {
              if(tm[2]==29)
                {
                tm[2]=1;
                tm[1]++;
                }
              break;
              }
          case 4:
          case 6:
          case 9:
          case 11:
            if(tm[2]==31)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
          default:
            if(tm[2]==32)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
          }
        if(tm[1]==13)
          {
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
          }
        }
      }
    }
  else if(tm[5]==-1)
    {
    tm[5]=59;
    if(--tm[4]==-1)
      {
      tm[4]=59;
      if(--tm[3]==-1)
        {
        tm[3]=23;
        if(--tm[2]==0)
          {
          switch(--tm[1])
            {
            case 2:
              if(tm[0]%4==0)
                tm[2]=29;else tm[2]=28;
              break;
            case 4:
            case 6:
            case 9:
            case 11:
              tm[2]=30;
              break;
            default:
              tm[2]=31;
              break;
            }
          if(tm[1]==0)
            {
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
            }
          }
        }
      }
    }
  }

adj_time_m(tm)
  int *tm;
  {
  if(tm[4]==60)
    {
    tm[4]=0;
    if(++tm[3]==24)
      {
      tm[3]=0;
      tm[2]++;
      switch(tm[1])
        {
        case 2:
          if(tm[0]%4==0)
            {
            if(tm[2]==30)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
            }
          else
            {
            if(tm[2]==29)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
            }
        case 4:
        case 6:
        case 9:
        case 11:
          if(tm[2]==31)
            {
            tm[2]=1;
            tm[1]++;
            }
          break;
        default:
          if(tm[2]==32)
            {
            tm[2]=1;
            tm[1]++;
            }
          break;
        }
      if(tm[1]==13)
        {
        tm[1]=1;
        if(++tm[0]==100) tm[0]=0;
        }
      }
    }
  else if(tm[4]==-1)
    {
    tm[4]=59;
    if(--tm[3]==-1)
      {
      tm[3]=23;
      if(--tm[2]==0)
        {
        switch(--tm[1])
          {
          case 2:
            if(tm[0]%4==0)
              tm[2]=29;else tm[2]=28;
            break;
          case 4:
          case 6:
          case 9:
          case 11:
            tm[2]=30;
            break;
          default:
            tm[2]=31;
            break;
          }
        if(tm[1]==0)
          {
          tm[1]=12;
          if(--tm[0]==-1) tm[0]=99;
          }
        }
      }
    }
  }

bcd_dec(dest,sour)
  char *sour;
  int *dest;
  {
  int cntr;
  for(cntr=0;cntr<6;cntr++)
    dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf);
  }

time_cmp(t1,t2,i)
  int *t1,*t2,i;  
  {
  int cntr;
  cntr=0;
  if(t1[cntr]<70 && t2[cntr]>70) return 1;
  if(t1[cntr]>70 && t2[cntr]<70) return -1;
  for(;cntr<i;cntr++)
    {
    if(t1[cntr]>t2[cntr]) return 1;
    if(t1[cntr]<t2[cntr]) return -1;
    } 
  return 0;  
  }

print_usage()
  {
  fprintf(stderr,"usage: dewin (-camn) (-f [filter file]) [ch no.(in hex)] ([input file])\n");
  fprintf(stderr,"        -c  character output\n");
  fprintf(stderr,"        -a  audio format (u-law) output\n");
  fprintf(stderr,"        -m  minute block instead of second block\n");
  fprintf(stderr,"        -n  not fill absent part\n");
  fprintf(stderr,"        -f  [filter file] filter paramter file\n");
  }

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

read_data(ptr,fp)
     FILE *fp;
     unsigned char **ptr;
{
   static unsigned int size;
   unsigned char c[4];
   int i,re;
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
  register int i;
  register unsigned char *dp;
  unsigned char *ddp;
  unsigned long sys_ch;

  dp=ptr+10;
  ddp=ptr+mklong(ptr);
  while(1)
    {
    if((g_size=win2fix(dp,abuf,&sys_ch,&s_rate))==0) return 0;
    if(sys_ch==ch) return s_rate;
    if((dp+=g_size)>=ddp) return 0;
    }
  return 0;
  }

win2fix(ptr,abuf,sys_ch,sr) /* returns group size in bytes */
  unsigned char *ptr; /* input */
  register long *abuf;/* output */
  long *sys_ch;       /* sys_ch */
  long *sr;           /* sr */
  {
  int b_size,g_size;
  register int i,s_rate;
  register unsigned char *dp;
  unsigned int gh;
  short shreg;
  int inreg;

  dp=ptr;
#if HSR
  /* channel header = 4 byte */
  if((dp[2]&0x80)==0)
    {
    *sr=s_rate=dp[3]+(((long)(dp[2]&0x0f))<<8);
    if(b_size=(dp[2]>>4)&0x7) g_size=b_size*(s_rate-1)+8;
    else g_size=(s_rate>>1)+8;
    *sys_ch=dp[1]+(((long)dp[0])<<8);
    dp+=4;
    }
  /* channel header = 5 byte */
  else
    {
    *sr=s_rate=dp[4]+(((long)dp[3])<<8)+(((long)(dp[2]&0x0f))<<16);
    if(b_size=(dp[2]>>4)&0x7) g_size=b_size*(s_rate-1)+9;
    else g_size=(s_rate>>1)+9;
    *sys_ch=dp[1]+(((long)dp[0])<<8);
    dp+=5;
    }
#else
  gh=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  *sr=s_rate=gh&0xfff;
/*  if(s_rate>MAX_SR) return 0;*/
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16)&0xffff;
  dp+=4;
#endif

  /* read group */
  abuf[0]=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
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
        shreg=((dp[0]<<8)&0xff00)+(dp[1]&0xff);
        dp+=2;
        abuf[i]=abuf[i-1]+shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00);
        dp+=3;
        abuf[i]=abuf[i-1]+(inreg>>8);
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
        dp+=4;
        abuf[i]=abuf[i-1]+inreg;
        }
      break;
    default:
      return 0; /* bad header */
    }
  return g_size;  /* normal return */
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

/*
+   BUTTERWORTH LOW PASS FILTER COEFFICIENT
+
+   ARGUMENTS
+   H : FILTER COEFFICIENTS
+   M : ORDER OF FILTER  (M=(N+1)/2)
+   GN  : GAIN FACTOR
+   N : ORDER OF BUTTERWORTH FUNCTION
+   FP  : PASS BAND FREQUENCY  (NON-DIMENSIONAL)
+   FS  : STOP BAND FREQUENCY
+   AP  : MAX. ATTENUATION IN PASS BAND
+   AS  : MIN. ATTENUATION IN STOP BAND
+
+   M. SAITO  (17/XII/75)
*/
butlop(h,m,gn,n,fp,fs,ap,as)
     double *h,fp,fs,ap,as,*gn;
     int *m,*n;
{
   double wp,ws,tp,ts,pa,sa,cc,c,dp,g,fj,c2,sj,tj,a;
   int k,j;
   if(fabs(fp)<fabs(fs)) wp=fabs(fp)*PI;
   else wp=fabs(fs)*PI;
   if(fabs(fp)>fabs(fs)) ws=fabs(fp)*PI;
   else ws=fabs(fs)*PI;
   if(wp==0.0 || wp==ws || ws>=HP){
      fprintf(stderr,"? (butlop) invalid input : fp=%14.6e fs=%14.6e ?\n",
	      fp,fs);
      return 1;
   }
   /****  DETERMINE N & C */
   tp=tan(wp);
   ts=tan(ws);
   if(fabs(ap)<fabs(as)) pa=fabs(ap);
   else pa=fabs(as);
   if(fabs(ap)>fabs(as)) sa=fabs(ap);
   else sa=fabs(as);
   if(pa==0.0) pa=0.5;
   if(sa==0.0) sa=5.0;
   if((*n=(int)(fabs(log(pa/sa)/log(tp/ts))+0.5))<2) *n=2;
   cc=exp(log(pa*sa)/(double)(*n))/(tp*ts);
   c=sqrt(cc);
   
   dp=HP/(double)(*n);
   *m=(*n)/2;
   k=(*m)*4;
   g=fj=1.0;
   c2=2.0*(1.0-c)*(1.0+c);
   
   for(j=0;j<k;j+=4){
      sj=pow(cos(dp*fj),2.0);
      tj=sin(dp*fj);
      fj=fj+2.0;
      a=1.0/(pow(c+tj,2.0)+sj);
      g=g*a;
      h[j  ]=2.0;
      h[j+1]=1.0;
      h[j+2]=c2*a;
      h[j+3]=(pow(c-tj,2.0)+sj)*a;
   }
   /****  EXIT */
   *gn=g;
   if(*n%2==0) return 0;
   /****  FOR ODD N */
   *m=(*m)+1;
   *gn=g/(1.0+c);
   h[k  ]=1.0;
   h[k+1]=0.0;
   h[k+2]=(1.0-c)/(1.0+c);
   h[k+3]=0.0;
   return 0;
}

/*
+   BUTTERWORTH HIGH PASS FILTER COEFFICIENT
+
+   ARGUMENTS
+   H : FILTER COEFFICIENTS
+   M : ORDER OF FILTER  (M=(N+1)/2)
+   GN  : GAIN FACTOR
+   N : ORDER OF BUTTERWORTH FUNCTION
+   FP  : PASS BAND FREQUENCY  (NON-DIMENSIONAL)
+   FS  : STOP BAND FREQUENCY
+   AP  : MAX. ATTENUATION IN PASS BAND
+   AS  : MIN. ATTENUATION IN STOP BAND
+
+   M. SAITO  (7/I/76)
*/
buthip(h,m,gn,n,fp,fs,ap,as)
     double *h,fp,fs,ap,as,*gn;
     int *m,*n;
{
   double wp,ws,tp,ts,pa,sa,cc,c,dp,g,fj,c2,sj,tj,a;
   int k,j;
   if(fabs(fp)>fabs(fs)) wp=fabs(fp)*PI;
   else wp=fabs(fs)*PI;
   if(fabs(fp)<fabs(fs)) ws=fabs(fp)*PI;
   else ws=fabs(fs)*PI;
   if(wp==0.0 || wp==ws || wp>=HP){
      fprintf(stderr,"? (buthip) invalid input : fp=%14.6e fs=%14.6e ?\n",
	      fp,fs);
      return 1;
   }
   /****  DETERMINE N & C */
   tp=tan(wp);
   ts=tan(ws);
   if(fabs(ap)<fabs(as)) pa=fabs(ap);
   else pa=fabs(as);
   if(fabs(ap)>fabs(as)) sa=fabs(ap);
   else sa=fabs(as);
   if(pa==0.0) pa=0.5;
   if(sa==0.0) sa=5.0;
   if((*n=(int)(fabs(log(sa/pa)/log(tp/ts))+0.5))<2) *n=2;
   cc=exp(log(pa*sa)/(double)(*n))*(tp*ts);
   c=sqrt(cc);
   
   dp=HP/(double)(*n);
   *m=(*n)/2;
   k=(*m)*4;
   g=fj=1.0;
   c2=(-2.0)*(1.0-c)*(1.0+c);
   
   for(j=0;j<k;j+=4){
      sj=pow(cos(dp*fj),2.0);
      tj=sin(dp*fj);
      fj=fj+2.0;
      a=1.0/(pow(c+tj,2.0)+sj);
      g=g*a;
      h[j  ]=(-2.0);
      h[j+1]=1.0;
      h[j+2]=c2*a;
      h[j+3]=(pow(c-tj,2.0)+sj)*a;
   }
   /****  EXIT */
   *gn=g;
   if(*n%2==0) return 0;
   /****  FOR ODD N */
   *m=(*m)+1;
   *gn=g/(c+1.0);
   h[k  ]=(-1.0);
   h[k+1]=0.0;
   h[k+2]=(c-1.0)/(c+1.0);
   h[k+3]=0.0;
   return 0;
}

/*
+   BUTTERWORTH BAND PASS FILTER COEFFICIENT
+
+   ARGUMENTS
+   H : FILTER COEFFICIENTS
+   M : ORDER OF FILTER
+   GN  : GAIN FACTOR
+   N : ORDER OF BUTTERWORTH FUNCTION
+   FL  : LOW  FREQUENCY CUT-OFF  (NON-DIMENSIONAL)
+   FH  : HIGH FREQUENCY CUT-OFF
+   FS  : STOP BAND FREQUENCY
+   AP  : MAX. ATTENUATION IN PASS BAND
+   AS  : MIN. ATTENUATION IN STOP BAND
+
+   M. SAITO  (7/I/76)
*/
butpas(h,m,gn,n,fl,fh,fs,ap,as)
     double *h,fl,fh,fs,ap,as,*gn;
     int *m,*n;
{
   double wl,wh,ws,clh,op,ww,ts,os,pa,sa,cc,c,dp,g,fj,rr,tt,
   re,ri,a,wpc,wmc;
   int k,l,j,i;
   struct {
      double r;
      double c;
   } oj,aa,cq,r[2];
   if(fabs(fl)<fabs(fh)) wl=fabs(fl)*PI;
   else wl=fabs(fh)*PI;
   if(fabs(fl)>fabs(fh)) wh=fabs(fl)*PI;
   else wh=fabs(fh)*PI;
   ws=fabs(fs)*PI;
   if(wl==0.0 || wl==wh || wh>=HP || ws==0.0 || ws>=HP ||(ws-wl)*(ws-wh)<=0.0){
      fprintf(stderr,
	      "? (butpas) invalid input : fl=%14.6e fh=%14.6e fs=%14.6e ?\n",
	      fl,fh,fs);
      *m=0;
      *gn=1.0;
      return 1;
   }
   /****  DETERMINE N & C */
   clh=1.0/(cos(wl)*cos(wh));
   op=sin(wh-wl)*clh;
   ww=tan(wl)*tan(wh);
   ts=tan(ws);
   os=fabs(ts-ww/ts);
   if(fabs(ap)<fabs(as)) pa=fabs(ap);
   else pa=fabs(as);
   if(fabs(ap)>fabs(as)) sa=fabs(ap);
   else sa=fabs(as);
   if(pa==0.0) pa=0.5;
   if(sa==0.0) sa=5.0;
   if((*n=(int)(fabs(log(pa/sa)/log(op/os))+0.5))<2) *n=2;
   cc=exp(log(pa*sa)/(double)(*n))/(op*os);
   c=sqrt(cc);
   ww=ww*cc;
   
   dp=HP/(double)(*n);
   k=(*n)/2;
   *m=k*2;
   l=0;
   g=fj=1.0;
   
   for(j=0;j<k;j++){
      oj.r=cos(dp*fj)*0.5;
      oj.c=sin(dp*fj)*0.5;
      fj=fj+2.0;
      aa.r=oj.r*oj.r-oj.c*oj.c+ww;
      aa.c=2.0*oj.r*oj.c;
      rr=sqrt(aa.r*aa.r+aa.c*aa.c);
      tt=atan(aa.c/aa.r);
      cq.r=sqrt(rr)*cos(tt/2.0);
      cq.c=sqrt(rr)*sin(tt/2.0);
      r[0].r=oj.r+cq.r;
      r[0].c=oj.c+cq.c;
      r[1].r=oj.r-cq.r;
      r[1].c=oj.c-cq.c;
      g=g*cc;
      
      for(i=0;i<2;i++){
	 re=r[i].r*r[i].r;
	 ri=r[i].c;
	 a=1.0/((c+ri)*(c+ri)+re);
	 g=g*a;
	 h[l  ]=0.0;
	 h[l+1]=(-1.0);
	 h[l+2]=2.0*((ri-c)*(ri+c)+re)*a;
	 h[l+3]=((ri-c)*(ri-c)+re)*a;
	 l=l+4;
      }
   }
   /****  EXIT */
   *gn=g;
   if(*n==(*m)) return 0;
   /****  FOR ODD N */
   *m=(*m)+1;
   wpc=  cc *cos(wh-wl)*clh;
   wmc=(-cc)*cos(wh+wl)*clh;
   a=1.0/(wpc+c);
   *gn=g*c*a;
   h[l  ]=0.0;
   h[l+1]=(-1.0);
   h[l+2]=2.0*wmc*a;
   h[l+3]=(wpc-c)*a;
   return 0;
}

/*
+   RECURSIVE FILTERING : F(Z) = (1+A*Z+AA*Z**2)/(1+B*Z+BB*Z**2)
+
+   ARGUMENTS
+   X : INPUT TIME SERIES
+   Y : OUTPUT TIME SERIES  (MAY BE EQUIVALENT TO X)
+   N : LENGTH OF X & Y
+   H : FILTER COEFFICIENTS ; H(1)=A, H(2)=AA, H(3)=B, H(4)=BB
+   NML : >0 ; FOR NORMAL  DIRECTION FILTERING
+       <0 ; FOR REVERSE DIRECTION FILTERING
+   uv  : past data and results saved
+
+   M. SAITO  (6/XII/75)
*/
recfil(x,y,n,h,nml,uv)
     int n,nml;
     double *x,*y,*h,*uv;
{
   int i,j,jd;
   double a,aa,b,bb,u1,u2,u3,v1,v2,v3;
   if(n<=0){
      fprintf(stderr,"? (recfil) invalid input : n=%d ?\n",n);
      return 1;
   }
   if(nml>=0){
      j=0;
      jd=1;
   }
   else{
      j=n-1;
      jd=(-1);
   }
   a =h[0];
   aa=h[1];
   b =h[2];
   bb=h[3];
   u1=uv[0];
   u2=uv[1];
   v1=uv[2];
   v2=uv[3];
   /****  FILTERING */
   for(i=0;i<n;i++){
      u3=u2;
      u2=u1;
      u1=x[j];
      v3=v2;
      v2=v1;
      v1=u1+a*u2+aa*u3-b*v2-bb*v3;
      y[j]=v1;
      j+=jd;
   }
   uv[0]=u1;
   uv[1]=u2;
   uv[2]=v1;
   uv[3]=v2;
   return 0;
}

/*
+   RECURSIVE FILTERING IN SERIES
+
+   ARGUMENTS
+   X : INPUT TIME SERIES
+   Y : OUTPUT TIME SERIES  (MAY BE EQUIVALENT TO X)
+   N : LENGTH OF X & Y
+   H : COEFFICIENTS OF FILTER
+   M : ORDER OF FILTER
+   NML : >0 ; FOR NORMAL  DIRECTION FILTERING
+       <0 ;   REVERSE DIRECTION FILTERING
+   uv  : past data and results saved
+
+   SUBROUTINE REQUIRED : RECFIL
+
+   M. SAITO  (6/XII/75)
*/
tandem(x,y,n,h,m,nml,uv)
     double *x,*y,*h,*uv;
     int n,m,nml;
{
   int i;
   if(n<=0 || m<=0){
      fprintf(stderr,"? (tandem) invalid input : n=%d m=%d ?\n",n,m);
      return 1;
   }
   /****  1-ST CALL */
   recfil(x,y,n,h,nml,uv);
   /****  2-ND AND AFTER */
   if(m>1) for(i=1;i<m;i++) recfil(y,y,n,&h[i*4],nml,&uv[i*4]);
   return 0;
}

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
  int i,j,k,sec,re,mainsize,sr,sr_save,c,form,nofill,zero,minblock,
    time1[6],time2[6],time3[6];
  unsigned long sysch;
  static unsigned char *mainbuf;
  FILE *f_main,*f_filter;
  char *ptr;
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
