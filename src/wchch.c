/* $Id: wchch.c,v 1.5.4.2.2.1 2008/11/11 15:19:48 uehira Exp $ */
/*
program "wchch.c"
"wchch" changes channel no. in a win format data file
1997.11.22   urabe
1999.4.20    byte-order free
2000.4.17   wabort
2003.10.29 exit()->exit(0)
2005.2.20 added fclose() in read_chfile()
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <signal.h>

#include "winlib.h"

#define   DEBUG   0
#define   DEBUG1  0

unsigned char *buf,*outbuf;
unsigned short ch_table[65536];

wabort() {exit(0);}

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
    for(i=0;i<65536;i++) ch_table[i]=i;
    i=0;
    while(fgets(tbuf,1024,fp))
      {
      if(*tbuf=='#' || sscanf(tbuf,"%x%x",&k,&j)<0) continue;
      k&=0xffff;  
      j&=0xffff;
#if DEBUG
      fprintf(stderr," %04X->%04X",k,j);
#endif
      if(k!=j && ch_table[k]==k)
        {
        ch_table[k]=j;
        i++;
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
    return 0;
    }
  return 1;
  }

get_one_record()
  {
  int i,re;
  while(read_data()>0)
    {
    /* read one sec */
    if(select_ch(ch_table,buf,outbuf)>10)
      /* write one sec */
      if((re=fwrite(outbuf,1,mkuint4(outbuf),stdout))==0) exit(1);
#if DEBUG1      
    fprintf(stderr,"in:%d B out:%d B\n",mkuint4(buf),mkuint4(outbuf));
#endif
    }
#if DEBUG1
  fprintf(stderr," : done\n");
#endif
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
  unsigned char *old_buf,*new_buf;
  unsigned short *table;
  {
  int ch,i,j,size,gsize,new_size,sr;
  unsigned char *ptr,*new_ptr,*ptr_lim;
  unsigned int gh;

  size=mkuint4(old_buf);
  ptr_lim=old_buf+size;
  ptr=old_buf+4;
  new_ptr=new_buf+4;
  for(i=0;i<6;i++) *new_ptr++=(*ptr++);
  new_size=10;
  do
    {
    gh=mkuint4(ptr);
    ch=gh>>16;
    sr=gh&0xfff;
    if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8;
    else gsize=(sr>>1)+8;
    *new_ptr++=table[ch]>>8;
    *new_ptr++=table[ch];
    ptr+=2;
    new_size+=gsize;
    gsize-=2;
    while(gsize-->0) *new_ptr++=(*ptr++);
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
    fprintf(stderr," usage of 'wchch' :\n");
    fprintf(stderr,"   'wchch [ch conv table] <[in_file] >[out_file]'\n");
    exit(0);
    }

  if(read_chfile(argv[1])>0) get_one_record();
  exit(0);
  }
