/* $Id: wchch.c,v 1.5.4.2.2.6 2010/09/17 10:33:42 uehira Exp $ */

/*
program "wchch.c"
"wchch" changes channel no. in a win format data file
1997.11.22   urabe
1999.4.20    byte-order free
2000.4.17   wabort
2003.10.29 exit()->exit(0)
2005.2.20 added fclose() in read_chfile()
2009.7.31  64bit clean. High sampling clean(?) (Uehira)
2010.9.17 replace read_data() with read_onesec_win2() (Uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <signal.h>

#include "winlib.h"

/* #define   DEBUG   0 */
#define   DEBUG1  0

static char rcsid[] =
  "$Id: wchch.c,v 1.5.4.2.2.6 2010/09/17 10:33:42 uehira Exp $";

static uint8_w *buf=NULL,*outbuf;
static WIN_ch ch_table[WIN_CHMAX];

/* prototypes */
static void wabort(void);
static int read_chfile(char *);
static void get_one_record(void);
static WIN_bs select_ch(WIN_ch *, uint8_w *, uint8_w *);
static void usage(void);
int main(int, char *[]);

static void
wabort() {exit(0);}

static int
read_chfile(char *chfile)
  {
  FILE *fp;
  int i,j,k;
  char tbuf[1024];

  if((fp=fopen(chfile,"r"))!=NULL)
    {
#if DEBUG
    fprintf(stderr,"ch_file=%s\n",chfile);
#endif
    for(i=0;i<WIN_CHMAX;i++) ch_table[i]=(WIN_ch)i;
    /* i=0; */   /* what for? */
    while(fgets(tbuf,sizeof(tbuf),fp) != NULL)
      {
      if(*tbuf=='#' || sscanf(tbuf,"%x%x",&k,&j)<0) continue;
      k&=0xffff;  
      j&=0xffff;
#if DEBUG
      fprintf(stderr," %04X->%04X",k,j);
#endif
      if(k!=j && ch_table[k]==k)
        {
        ch_table[k]=(WIN_ch)j;
        /* i++; */   /* what for? */
        }
      }
#if DEBUG
    fprintf(stderr,"\n");
#endif
    fclose(fp);
    }
  else
    {
    fprintf(stderr,"ch_file '%s' not open\n",chfile);
    return (0);
    }
  return (1);
  }

static void
get_one_record()
  {

  while(read_onesec_win2(stdin,&buf,&outbuf)>0)
    {
    /* read one sec */
    if(select_ch(ch_table,buf,outbuf)>10)
      /* write one sec */
      if(fwrite(outbuf,1,mkuint4(outbuf),stdout)==0) exit(1);
#if DEBUG1      
    fprintf(stderr,"in:%d B out:%d B\n",mkuint4(buf),mkuint4(outbuf));
#endif
    }
#if DEBUG1
  fprintf(stderr," : done\n");
#endif
  }

static WIN_bs
select_ch(WIN_ch *table, uint8_w *old_buf, uint8_w *new_buf)
  {
  int i,ss;
  WIN_bs size, new_size;
  uint8_w *ptr,*new_ptr,*ptr_lim;
  uint32_w gsize;
  WIN_ch ch;
  WIN_sr sr;

  size=mkuint4(old_buf);
  ptr_lim=old_buf+size;
  ptr=old_buf+WIN_BSLEN;
  new_ptr=new_buf+WIN_BSLEN;
  for(i=0;i<6;i++) *new_ptr++=(*ptr++);
  new_size=10;
  do
    {
    gsize=win_chheader_info(ptr,&ch,&sr,&ss);
/*     gh=mkuint4(ptr); */
/*     ch=gh>>16; */
/*     sr=gh&0xfff; */
/*     if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8; */
/*     else gsize=(sr>>1)+8; */
    *new_ptr++=table[ch]>>8;
    *new_ptr++=table[ch];
    if (sr < HEADER_4B)
      ptr+=2;
    else if (sr < HEADER_5B)
      ptr+=3;
    else {
      fprintf(stderr, "Invalid sampling rate.\n");
      exit(1);
    }
    new_size+=gsize;
    if (sr < HEADER_4B)
      gsize-=2;
    else if (sr < HEADER_5B)
      gsize-=3;
    else {
      fprintf(stderr, "Invalid sampling rate.\n");
      exit(1);
    }
    while(gsize-->0) *new_ptr++=(*ptr++);
    } while(ptr<ptr_lim);
  new_buf[0]=new_size>>24;
  new_buf[1]=new_size>>16;
  new_buf[2]=new_size>>8;
  new_buf[3]=new_size;
  return (new_size);
  }

static void
usage()
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr," usage of 'wchch' :\n");
  fprintf(stderr,"   'wchch [ch conv table] <[in_file] >[out_file]'\n");
}

int
main(int argc, char *argv[])
  {

  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);

  if(argc<2)
    {
    usage();
    exit(1);
    }

  if(!read_chfile(argv[1])) exit(1);
  get_one_record();
  exit(0);
  }
