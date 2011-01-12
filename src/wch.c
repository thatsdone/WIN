/* $Id: wch.c,v 1.8.4.2.2.7 2011/01/12 15:44:31 uehira Exp $ */
/*
program "wch.c"
"wch" edits a win format data file by channles
1997.6.17   urabe
1997.8.5 fgets/sscanf
1997.10.1 FreeBSD
1999.4.20 byte-order free
2000.4.17 wabort
2002.2.18 delete duplicated data & negate_channel
2003.10.29 exit()->exit(0)
2005.2.20 added fclose() in read_chfile()
2009.7.31  64bit clean. (Uehira)
2010.9.17 replace read_data() with read_onesec_win2() (Uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <signal.h>

#include "winlib.h"

/* #define   DEBUG   0 */

static const char rcsid[] =
  "$Id: wch.c,v 1.8.4.2.2.7 2011/01/12 15:44:31 uehira Exp $";

static uint8_w *buf=NULL,*outbuf;
static size_t  cbufsiz; /* buffer size of buf[] & outbuf[] */
static uint8_w ch_table[WIN_CHMAX];
static int negate_channel;

/* prototypes */
static void wabort(void);
static int read_chfile(char *);
static WIN_bs select_ch(uint8_w *, uint8_w *, uint8_w *);
static void get_one_record(void);
static void usage(void);
int main(int, char *[]);

static void
wabort(void) {exit(0);}

static int
read_chfile(char *chfile)
  {
  FILE *fp;
  int i,j,k;
  char tbuf[1024];

  if(chfile != NULL)
    {
    if((fp=fopen(chfile,"r"))!=NULL)
      {
      if(negate_channel) for(i=0;i<WIN_CHMAX;i++) ch_table[i]=1;
      else for(i=0;i<WIN_CHMAX;i++) ch_table[i]=0;
      i=j=0;
      while(fgets(tbuf,sizeof(tbuf),fp) != NULL)
        {
        if(*tbuf=='#' || sscanf(tbuf,"%x",&k)<0) continue;
        k&=0xffff;
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
      fclose(fp);
      if(negate_channel) fprintf(stderr,"-%d channels\n",j);
      else fprintf(stderr,"%d channels\n",j);
      return (j);
      }
    else
      {
      fprintf(stderr,"ch_file '%s' not open\n",chfile);
      return (0);
      }
    }
  else
    {
    for(i=0;i<WIN_CHMAX;i++) ch_table[i]=1;
    fprintf(stderr,"all channels\n");
    return (i);
    }
  }

static WIN_bs 
select_ch(uint8_w *table, uint8_w *old_buf, uint8_w *new_buf)
  {
  int i,ss;
  WIN_bs size,new_size;
  uint8_w *ptr,*new_ptr,*ptr_lim;
  unsigned int chtbl[WIN_CHMAX];
  uint32_w gsize;
  WIN_ch ch;
  WIN_sr sr;
  /* unsigned int gh; */

  for(i=0;i<WIN_CHMAX;i++) chtbl[i]=0;
  size=mkuint4(old_buf);
  ptr_lim=old_buf+size;
  ptr=old_buf+WIN_BSLEN;
  new_ptr=new_buf+WIN_BSLEN;
  for(i=0;i<6;i++) *new_ptr++=(*ptr++);
  new_size=10;
  do
    {
    gsize = win_chheader_info(ptr,&ch,&sr,&ss);
/*     gh=mkuint4(ptr); */
/*     i=gh>>16; */
/*     sr=gh&0xfff; */
/*     if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8; */
/*     else gsize=(sr>>1)+8; */
    if(table[ch] && chtbl[ch]==0)
      {
      new_size+=gsize;
      while(gsize-->0) *new_ptr++=(*ptr++);
      chtbl[ch]=1;
      }
    else ptr+=gsize;
    } while(ptr<ptr_lim);
  new_buf[0]=new_size>>24;
  new_buf[1]=new_size>>16;
  new_buf[2]=new_size>>8;
  new_buf[3]=new_size;
  return (new_size);
  }

static void
get_one_record(void)
  {

  while(read_onesec_win2(stdin,&buf,&outbuf,&cbufsiz)>0)
    {
    /* read one sec */
    if(select_ch(ch_table,buf,outbuf)>10)
      /* write one sec */
      if(fwrite(outbuf,1,mkuint4(outbuf),stdout)==0) exit(1);
#if DEBUG
    fprintf(stderr,"in:%d B out:%d B\n",mkuint4(buf),mkuint4(outbuf));
#endif
    }
#if DEBUG
  fprintf(stderr," : done\n");
#endif
  }

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr," usage of 'wch' :\n");
  fprintf(stderr,"   'wch -/[ch file]/-[ch file] <[in_file] >[out_file]'\n");
}

int
main(int argc, char *argv[])
  {
  char *chfile;

  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);

  if(argc<2)
    {
    usage();
    exit(1);
    }
  if(strcmp("-",argv[1])==0) chfile=NULL;
  else
    {
    if(argv[1][0]=='-')
      {
      chfile=argv[1]+1;
      negate_channel=1;
      }
    else
      {
      chfile=argv[1];
      negate_channel=0;
      }
    }

  if(!read_chfile(chfile)) exit(1);
  get_one_record();
  exit(0);
  }
