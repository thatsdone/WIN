/* $Id: wadd2.c,v 1.2 2002/05/07 14:00:31 urabe Exp $ */
/* program "wadd2.c"
  "wadd" puts two win data files together
  7/24/91 - 7/25/91, 4/20/94,6/27/94-6/28/94,7/12/94   urabe
        rindex -> strrchr 5/29/96
        97.8.14 remove duplicated channels from joined ch file 
  98.6.26 yo2000
  98.7.1 FreeBSD
  99.4.20 byte-order-free
  2000.4.24 permit blank lines in egrep pattern file (Uehira)
  2002.1.7  fix bug in pattern file (Uehira)
  2002.4.30 MAXSIZE 300K->1M
  2002.5.7  wadd -> wadd2
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <string.h>

#include "subst_func.h"

#define   DEBUG   0
#define   MAXSIZE   1000000
#define   NAMLEN    1024
#define   TEMPNAME  "wadd2.tmp"

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
  unsigned char *ptr;
  {
  int re;
  if(fread(ptr,1,4,fp)==0) return 0;
  re=mklong(ptr);
  if(fread(ptr+4,1,re-4,fp)==0) return 0;
#if DEBUG
  printf("%02x%02x%02x%02x%02x%02x %d\n",ptr[4],ptr[5],ptr[6],
    ptr[7],ptr[8],ptr[9],re);
#endif
  return re;
  }

copy_ch(makelist,sys_ch,inbuf,insize,outbuf)
  int makelist;
  unsigned char *inbuf,*outbuf;
  int *sys_ch,insize;
  {
  int i,j,size,gsize,new_size,sr;
  unsigned char *ptr,*ptw,*ptr_lim;
  unsigned int gh;
  ptr_lim=inbuf+insize;
  ptr=inbuf;
  ptw=outbuf;
  if(makelist) for(j=0;j<65536;j++) sys_ch[j]=0;
  do
    {
    gh=mklong(ptr);
    i=gh>>16;
    sr=gh&0xfff;
    if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8;
    else gsize=(sr>>1)+8;
    if(makelist)
      {
      sys_ch[i]=1;
      memcpy(ptw,ptr,gsize);
      ptw+=gsize;
      }
    else
      {      
      if(sys_ch[i]==0)
        {
        memcpy(ptw,ptr,gsize);
        ptw+=gsize;
        }
      }
    ptr+=gsize;
    } while(ptr<ptr_lim);
#if DEBUG
  printf("out size=%d\n",ptw-outbuf); 
#endif
  return ptw-outbuf;
  }

werror()
  {
  perror("fwrite");
  exit(1);
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  int i,re,size,mainsize,subsize,mainend,subend,dec_sub[6],dec_main[6];
  FILE *f_main,*f_sub,*f_out;
  unsigned char *ptr;
  static unsigned char mainbuf[MAXSIZE],subbuf[MAXSIZE],tmpfile1[NAMLEN],
    textbuf[NAMLEN],new_file[NAMLEN],selbuf[MAXSIZE];
  static int sysch[65536];

  if(argc<3)
    {
    fprintf(stderr," usage of 'wadd2' :\n");
    fprintf(stderr,"   'wadd2 [infile1] [infile2] (-/[outdir])\n");
    exit(0);
    }

  if((f_main=fopen(argv[1],"r"))==NULL)
    {
    perror("fopen");
    exit(1);
    }

  if((f_sub=fopen(argv[2],"r"))==NULL)
    {
    perror("fopen");
    exit(1);
    }

  if(argc>3)
    {
    if(strcmp(argv[3],"-")==0)
      {
      *tmpfile1=0;
      f_out=stdout;
      }
    else sprintf(tmpfile1,"%s/%s.%d",argv[3],TEMPNAME,getpid());
    }
  else sprintf(tmpfile1,"%s.%d",TEMPNAME,getpid());

  if(*tmpfile1 && (f_out=fopen(tmpfile1,"w+"))==NULL)
    {
    perror("fopen");
    exit(1);
    }

  mainend=subend=0;

  if((mainsize=read_data(mainbuf,f_main))==0) mainend=1;
  else bcd_dec(dec_main,(char *)mainbuf+4);

  if((subsize=read_data(subbuf,f_sub))==0) subend=1;
  else bcd_dec(dec_sub,(char *)subbuf+4);

  while(mainend==0 || subend==0) /* MAIN LOOP */
    {
    if(mainend==1) re=(-1);
    else if(subend==1) re=1;
    else re=time_cmp(dec_main,dec_sub,6);
#if DEBUG
    printf("main=%d sub=%d re=%d\n",mainend,subend,re);
    printf("m:%02x%02x%02x%02x%02x%02x\n",mainbuf[4],mainbuf[5],mainbuf[6],
      mainbuf[7],mainbuf[8],mainbuf[9]);
    printf("s:%02x%02x%02x%02x%02x%02x\n",subbuf[4],subbuf[5],subbuf[6],
      subbuf[7],subbuf[8],subbuf[9]);
#endif

    ptr=selbuf;
    ptr+=4;

    if(re>=0) /* output main */
      {
      for(i=0;i<6;i++) *ptr++=mainbuf[4+i];
      ptr+=copy_ch(1,sysch,mainbuf+10,mainsize-10,ptr);
      if((mainsize=read_data(mainbuf,f_main))==0) mainend=1;
      else bcd_dec(dec_main,(char *)mainbuf+4);
      }
    if(re<=0) /* output sub */
      {
      if(re<0) for(i=0;i<6;i++) *ptr++=subbuf[4+i];
      ptr+=copy_ch(0,sysch,subbuf+10,subsize-10,ptr);
      if((subsize=read_data(subbuf,f_sub))==0) subend=1;
      else bcd_dec(dec_sub,(char *)subbuf+4);
      }

    size=ptr-selbuf;
    selbuf[0]=size>>24;
    selbuf[1]=size>>16;
    selbuf[2]=size>>8;
    selbuf[3]=size;
    if((re=fwrite(selbuf,1,size,f_out))==0) werror();

#if DEBUG
    printf("out:%02x%02x%02x%02x%02x%02x %d\n",selbuf[4],selbuf[5],selbuf[6],
      selbuf[7],selbuf[8],selbuf[9],size);
#endif
    } /* END OF MAIN LOOP */

  fclose(f_out);

  if(f_out!=stdout)
    {
    if((ptr=strrchr(argv[1],'/'))==NULL) ptr=argv[1];
    else ptr++;
    if(argc>3) sprintf(new_file,"%s/%s",argv[3],ptr);
    else strcpy(new_file,ptr);
    if(strcmp(argv[1],new_file)) rename(tmpfile1,new_file);
    else
      {
      sprintf(textbuf,"cp %s %s",tmpfile1,argv[1]);
      system(textbuf);
      unlink(tmpfile1);
      }
    }

  exit(0);
  }
