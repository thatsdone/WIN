/* $Log: wtime.c,v $
/* Revision 1.1  2000/08/10 07:40:57  urabe
/* find_picks : pick file searching server for win
/* wtime      : time shift tool for WIN format file
/*
program "wtime.c"
"wchch" shifts time of a win format data file
2000.7.30   urabe
*/

#include  <stdio.h>
#include  <stdlib.h>
#include  <signal.h>
#include  <time.h>
#include  <math.h>

#define   DEBUG   0
#define   DEBUG1  0
#define SWAPL(a) a=(((a)<<24)|((a)<<8)&0xff0000|((a)>>8)&0xff00|((a)>>24)&0xff)

unsigned char *rbuf,*wbuf;
long *sbuf[65536];
int s_add,ms_add;
static int b2d[]={
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,0,0,0,0,0,0,  /* 0x00 - 0x0F */
    10,11,12,13,14,15,16,17,18,19,0,0,0,0,0,0,
    20,21,22,23,24,25,26,27,28,29,0,0,0,0,0,0,
    30,31,32,33,34,35,36,37,38,39,0,0,0,0,0,0,
    40,41,42,43,44,45,46,47,48,49,0,0,0,0,0,0,
    50,51,52,53,54,55,56,57,58,59,0,0,0,0,0,0,
    60,61,62,63,64,65,66,67,68,69,0,0,0,0,0,0,
    70,71,72,73,74,75,76,77,78,79,0,0,0,0,0,0,
    80,81,82,83,84,85,86,87,88,89,0,0,0,0,0,0,
    90,91,92,93,94,95,96,97,98,99,0,0,0,0,0,0}; /* 0x90 - 0x9F */
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

wabort() {exit(0);}

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

read_data()
  {
  static unsigned int size;
  int re,i;
  if(fread(&re,1,4,stdin)==0) return 0;
  i=1;if(*(char *)&i) SWAPL(re);
  if(rbuf==0)
    {
    rbuf=(unsigned char *)malloc(size=re*2);
    wbuf=(unsigned char *)malloc(size=re*2);
    }
  else if(re>size)
    {
    rbuf=(unsigned char *)realloc(rbuf,size=re*2);
    wbuf=(unsigned char *)realloc(wbuf,size=re*2);
    }
  rbuf[0]=re>>24;
  rbuf[1]=re>>16;
  rbuf[2]=re>>8;
  rbuf[3]=re;
  re=fread(rbuf+4,1,re-4,stdin);
  return re;
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
  gh=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  *sr=s_rate=gh&0xfff;
/*  if(s_rate>MAX_SR) return 0;*/
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16)&0xffff;

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

/* winform.c  4/30/91,99.4.19   urabe */
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
  unsigned char *buf;

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
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0)) byte_leng=0;
  else if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0)) byte_leng=1;
  else if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0)) byte_leng=2;
  else if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0)) byte_leng=3;
  else byte_leng=4;
  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  *buf++=inbuf[0]>>24;
  *buf++=inbuf[0]>>16;
  *buf++=inbuf[0]>>8;
  *buf++=inbuf[0];
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
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>24;
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    }
  return (int)(buf-outbuf);
  }

shift_sec(tm_bcd,sec)
  unsigned char *tm_bcd;
  int sec;
  {
  int tm[6];
  struct tm *nt,mt;
  unsigned long ltime;
  memset((char *)&mt,0,sizeof(mt));
  if((mt.tm_year=b2d[tm_bcd[0]])<50) mt.tm_year+=100;
  mt.tm_mon=b2d[tm_bcd[1]]-1;
  mt.tm_mday=b2d[tm_bcd[2]];
  mt.tm_hour=b2d[tm_bcd[3]];
  mt.tm_min=b2d[tm_bcd[4]];
  mt.tm_sec=b2d[tm_bcd[5]];
  mt.tm_isdst=0;
  ltime=mktime(&mt);
  if(sec) ltime+=sec;
  else return ltime;
  nt=localtime(&ltime);
  tm_bcd[0]=d2b[nt->tm_year%100];
  tm_bcd[1]=d2b[nt->tm_mon+1];
  tm_bcd[2]=d2b[nt->tm_mday];
  tm_bcd[3]=d2b[nt->tm_hour];
  tm_bcd[4]=d2b[nt->tm_min];
  tm_bcd[5]=d2b[nt->tm_sec];
  return ltime;
  }

chloop(old_buf,new_buf)
  unsigned char *old_buf,*new_buf;
  {
  static long fixbuf1[4096],fixbuf2[4096],ltime,ltime_ch[65536];
  int ch,i,j,size,gsize,new_size,sr,sr_shift;
  unsigned char *ptr1,*ptr2,*ptr_lim;

  size=mklong(old_buf);
  ptr_lim=old_buf+size;
  ptr1=old_buf+4;
  ptr2=new_buf+4;
  for(i=0;i<6;i++) *ptr2++=(*ptr1++); /* time stamp */
  ltime=shift_sec(ptr2-6,s_add);      /* shift time */
  new_size=10;
  do
    {
    ptr1+=win2fix(ptr1,fixbuf1,&ch,&sr); /* returns group size in bytes */
    if(!sbuf[ch]) sbuf[ch]=(long *)malloc(4*sr);
    if(ltime!=ltime_ch[ch]+1) for(i=0;i<sr;i++) sbuf[ch][i]=fixbuf1[0]; 
    sr_shift=(ms_add*sr+500)/1000;
    for(i=0;i<sr_shift;i++) fixbuf2[i]=sbuf[ch][sr-sr_shift+i];
    for(i=sr_shift;i<sr;i++) fixbuf2[i]=fixbuf1[i-sr_shift];
    for(i=0;i<sr;i++) sbuf[ch][i]=fixbuf1[i];
    ltime_ch[ch]=ltime;
    ptr2+=winform(fixbuf2,ptr2,sr,ch);
    } while(ptr1<ptr_lim);
  new_size=ptr2-new_buf;
  new_buf[0]=new_size>>24;
  new_buf[1]=new_size>>16;
  new_buf[2]=new_size>>8;
  new_buf[3]=new_size;
  return new_size;
  }

print_usage()
  {
  fprintf(stderr," usage of 'wtime' :\n");
  fprintf(stderr,"   'wtime (-h [hrs]) (-s [sec in float]) <[in_file] >[out_file]'\n");
  exit(0);
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  int c,hours,i,re;
  double fsec;
  extern int optind;
  extern char *optarg;

  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);
  hours=0;
  fsec=0.0;
  while((c=getopt(argc,argv,"ch:s:tu"))!=EOF)
    {
    switch(c)
      {
      case 'h':   /* hours */
        hours=atoi(optarg);
        break;
      case 's':   /* sec */
        fsec=atof(optarg);
        break;
      case 'u':   /* usage */
      default:
        print_usage();
        exit(0);
      }
    }
  if(hours==0 && fsec==0.0)
    {
    print_usage();
    exit(0);
    }

  ms_add=1000*(fsec-floor(fsec));
  s_add=hours*3600+(int)floor(fsec);
#if DEBUG
  fprintf(stderr,"s=%d, ms=%d\n",s_add,ms_add);
#endif

  while((re=read_data())>0)
    {
    /* read one sec */
    re=chloop(rbuf,wbuf);
    if(re>10)      /* write one sec */
      if((re=fwrite(wbuf,1,mklong(wbuf),stdout))==0) exit(1);
#if DEBUG1      
    fprintf(stderr,"in:%d B out:%d B\n",mklong(rbuf),mklong(wbuf));
#endif
    }
#if DEBUG1
  fprintf(stderr," : done\n");
#endif
  exit(0);
  }
