/* $Id: wtime.c,v 1.3.2.3.2.5 2009/08/25 04:00:16 uehira Exp $ */

/*
  program "wtime.c"
  "wchch" shifts time of a win format data file
  2000.7.30   urabe
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <signal.h>
#include  <unistd.h>

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

#include  <math.h>

#include "winlib.h"

/* #define   DEBUG   0 */
#define   DEBUG1  0

unsigned char *rbuf,*wbuf;
int32_w *sbuf[WIN_CHMAX];
int s_add,ms_add;

wabort() {exit(0);}

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

shift_sec(tm_bcd,sec)
  unsigned char *tm_bcd;
  int sec;
  {
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
  nt=localtime((time_t *)&ltime);
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
  static int32_w fixbuf1[4096],fixbuf2[4096],ltime,ltime_ch[WIN_CHMAX];
  int i,size,new_size,sr_shift;
  WIN_ch  ch;
  WIN_sr  sr;
  unsigned char *ptr1,*ptr2,*ptr_lim;

  size=mkuint4(old_buf);
  ptr_lim=old_buf+size;
  ptr1=old_buf+4;
  ptr2=new_buf+4;
  for(i=0;i<6;i++) *ptr2++=(*ptr1++); /* time stamp */
  ltime=shift_sec(ptr2-6,s_add);      /* shift time */
  new_size=10;
  do
    {
    ptr1+=win2fix(ptr1,fixbuf1,&ch,&sr); /* returns group size in bytes */
    if(!sbuf[ch]) sbuf[ch]=(int32_w *)malloc(sizeof(int32_w)*sr);
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
  int c,hours,re;
  double fsec;
/*   extern int optind; */
/*   extern char *optarg; */

  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);
  hours=0;
  fsec=0.0;
  while((c=getopt(argc,argv,"ch:s:tu"))!=-1)
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
      if((re=fwrite(wbuf,1,mkuint4(wbuf),stdout))==0) exit(1);
#if DEBUG1      
    fprintf(stderr,"in:%d B out:%d B\n",mkuint4(rbuf),mkuint4(wbuf));
#endif
    }
#if DEBUG1
  fprintf(stderr," : done\n");
#endif
  exit(0);
  }
