/* $Id: wch.c,v 1.2.2.1 2001/11/02 11:43:39 uehira Exp $ */
/*
program "wch.c"
"wch" edits a win format data file by channles
1997.6.17   urabe
1997.8.5 fgets/sscanf
1997.10.1 FreeBSD
1999.4.20 byte-order free
2000.4.17   wabort
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <signal.h>

#include "subst_func.h"

#define   DEBUG   0
#define SWAPL(a) a=(((a)<<24)|((a)<<8)&0xff0000|((a)>>8)&0xff00|((a)>>24)&0xff)

unsigned char *buf,*outbuf;
unsigned char ch_table[65536];

wabort() {exit();}

read_chfile(chfile)
  char *chfile;
  {
  FILE *fp;
  int i,j,k;
  char tbuf[1024];
  if((fp=fopen(chfile,"r"))!=NULL)
    {
#if DEBUG
    fprintf(stderr,"ch_file=%s\n",chfile);
#endif
    for(i=0;i<65536;i++) ch_table[i]=0;
    i=j=0;
    while(fgets(tbuf,1024,fp))
      {
      if(*tbuf=='#' || sscanf(tbuf,"%x",&k)<0) continue;
      k&=0xffff;
#if DEBUG
      fprintf(stderr," %04X",k);
#endif
      if(ch_table[k]==0)
        {
        ch_table[k]=1;
        j++;
        }
      i++;
      }
#if DEBUG
    fprintf(stderr,"\n",k);
#endif
    }
  else
    {
    fprintf(stderr,"ch_file '%s' not open\n",chfile);
    return 0;
    }
  return j;
  }

get_one_record()
  {
  int i,re;
  while(read_data()>0)
    {
    /* read one sec */
    if(select_ch(ch_table,buf,outbuf)>10)
      /* write one sec */
      if((re=fwrite(outbuf,1,mklong(outbuf),stdout))==0) exit(1);
#if DEBUG       
    fprintf(stderr,"in:%d B out:%d B\n",mklong(buf),mklong(outbuf));
#endif
    }
#if DEBUG
  fprintf(stderr," : done\n");
#endif
  }

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
  if(buf==0)
    {
    buf=(unsigned char *)malloc(size=re*2);
    outbuf=(unsigned char *)malloc(size=re*2);
    }
  else if(re>size)
    {
    buf=(unsigned char *)realloc(buf,size=re*2);
    outbuf=(unsigned char *)realloc(outbuf,size=re*2);
    }
  buf[0]=re>>24;
  buf[1]=re>>16;
  buf[2]=re>>8;
  buf[3]=re;
  re=fread(buf+4,1,re-4,stdin);
  return re;
  }

select_ch(table,old_buf,new_buf)
  unsigned char *old_buf,*new_buf,*table;
  {
  int i,j,size,gsize,new_size,sr;
  unsigned char *ptr,*new_ptr,*ptr_lim;
  unsigned int gh;
  size=mklong(old_buf);
  ptr_lim=old_buf+size;
  ptr=old_buf+4;
  new_ptr=new_buf+4;
  for(i=0;i<6;i++) *new_ptr++=(*ptr++);
  new_size=10;
  do
    {
    gh=mklong(ptr);
    i=gh>>16;
    sr=gh&0xfff;
    if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8;
    else gsize=(sr>>1)+8;
    if(table[i])
      {
      new_size+=gsize;
      while(gsize-->0) *new_ptr++=(*ptr++);
      }
    else ptr+=gsize;
    } while(ptr<ptr_lim);
  new_buf[0]=new_size>>24;
  new_buf[1]=new_size>>16;
  new_buf[2]=new_size>>8;
  new_buf[3]=new_size;
  return new_size;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);

  if(argc<2)
    {
    fprintf(stderr," usage of 'wch' :\n");
    fprintf(stderr,"   'wch [ch file] <[in_file] >[out_file]'\n");
    exit(0);
    }

  if(read_chfile(argv[1])>0) get_one_record();
  exit(0);
  }
