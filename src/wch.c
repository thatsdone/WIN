/*
program "wch.c"
"wch" edits a win format data file by channles
1997.6.17   urabe
1997.8.5 fgets/sscanf
1997.10.1 FreeBSD
*/

#include  <stdio.h>
#include  <signal.h>

#define   DEBUG   0

unsigned char *buf,*outbuf;
unsigned char ch_table[65536];

/*****************************/
/* 8/5/96 for little-endian (uehira) */
#ifndef         LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234    /* LSB first: i386, vax */
#endif
#ifndef         BIG_ENDIAN
#define BIG_ENDIAN      4321    /* MSB first: 68000, ibm, net */
#endif
#ifndef  BYTE_ORDER
#define  BYTE_ORDER      BIG_ENDIAN
#endif

#define SWAPU  union { long l; float f; short s; char c[4];} swap
#define SWAPL(a) swap.l=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPF(a) swap.f=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPS(a) swap.s=(a); ((char *)&(a))[0]=swap.c[1];\
    ((char *)&(a))[1]=swap.c[0]
/*****************************/


abort() {exit();}

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
  unsigned char *ptr1;
  unsigned long a;
#if    BYTE_ORDER == LITTLE_ENDIAN
  SWAPU;
#endif
  ptr1=(unsigned char *)&a;
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1  =(*ptr);
#if    BYTE_ORDER == LITTLE_ENDIAN
  SWAPL(a);
#endif
  return a;
  }

read_data()
  {
  static unsigned int size;
  int re;
#if BYTE_ORDER == LITTLE_ENDIAN
  int re1;
  SWAPU;
#endif
  if(fread(&re,1,4,stdin)==0) return 0;
#if BYTE_ORDER == LITTLE_ENDIAN
  SWAPL(re);
#endif
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
#if BYTE_ORDER == LITTLE_ENDIAN
  re1=re;
  SWAPL(re1);
  *(int *)buf=re1;
#endif
#if BYTE_ORDER == BIG_ENDIAN
  *(int *)buf=re;
#endif
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
  ptr=(unsigned char *)&new_size;
#if BYTE_ORDER == BIG_ENDIAN
  new_buf[0]=(*ptr++);
  new_buf[1]=(*ptr++);
  new_buf[2]=(*ptr++);
  new_buf[3]=(*ptr++);
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
  new_buf[3]=(*ptr++);
  new_buf[2]=(*ptr++);
  new_buf[1]=(*ptr++);
  new_buf[0]=(*ptr++);
#endif
  return new_size;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  signal(SIGINT,(void *)abort);
  signal(SIGTERM,(void *)abort);

  if(argc<2)
    {
    fprintf(stderr," usage of 'wch' :\n");
    fprintf(stderr,"   'wch [ch file] <[in_file] >[out_file]'\n");
    exit(0);
    }

  if(read_chfile(argv[1])>0) get_one_record();
  exit(0);
  }
