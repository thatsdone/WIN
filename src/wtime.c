/* $Id: wtime.c,v 1.3.2.3.2.11.2.1 2010/12/22 14:39:58 uehira Exp $ */

/*
  program "wtime.c"
  "wchch" shifts time of a win format data file
  2000.7.30   urabe
  2010.9.17   replace read_data() with read_onesec_win2().
              64bit clean. (Uehira)
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

static const char  rcsid[] =
   "$Id: wtime.c,v 1.3.2.3.2.11.2.1 2010/12/22 14:39:58 uehira Exp $";

static uint8_w *rbuf=NULL,*wbuf;
static int32_w *sbuf[WIN_CHMAX];
static int s_add,ms_add;

/* prototypes */
static void wabort(void);
static WIN_bs chloop(uint8_w *, uint8_w *);
static void print_usage(void);
int main(int, char *[]);

static void
wabort() {exit(0);}

static WIN_bs
chloop(uint8_w *old_buf, uint8_w *new_buf)
  {
  static int32_w fixbuf1[HEADER_5B],fixbuf2[HEADER_5B];
  static time_t  ltime,ltime_ch[WIN_CHMAX];
  int i,sr_shift;
  WIN_bs  new_size;
  WIN_ch  ch;
  WIN_sr  sr;
  uint8_w *ptr1,*ptr2,*ptr_lim;

  ptr_lim=old_buf+mkuint4(old_buf);
  ptr1=old_buf+4;
  ptr2=new_buf+4;
  for(i=0;i<6;i++) *ptr2++=(*ptr1++); /* time stamp */
  ltime=shift_sec(ptr2-6,s_add);      /* shift time */
  new_size=10;
  do
    {
    ptr1+=win2fix(ptr1,fixbuf1,&ch,&sr); /* returns group size in bytes */
    if(!sbuf[ch]) sbuf[ch]=MALLOC(int32_w, sr);
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
  return (new_size);
  }

static void
print_usage()
  {

  WIN_version();
  fprintf(stderr,"%s\n", rcsid);
  fprintf(stderr," usage of 'wtime' :\n");
  fprintf(stderr,"   'wtime (-h [hrs]) (-s [sec in float]) <[in_file] >[out_file]'\n");
  }

int
main(int argc, char *argv[])
  {
  int c,hours,re;
  double fsec;

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

  while(read_onesec_win2(stdin,&rbuf,&wbuf)>0)
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
