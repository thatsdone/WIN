/* $Id: wadd.c,v 1.6.4.3.2.3 2009/08/25 04:00:16 uehira Exp $ */
/* program "wadd.c"
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
  2003.1.22 eliminate blank line from ch file
  2007.1.15 MAXSIZE 1M->5M
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>

#include "winlib.h"

/* #define   DEBUG   0 */
#define   MAXSIZE   5000000
#define   NAMLEN    256
#define   TEMPNAME  "wadd.tmp"

/* bcd_dec(dest,sour) */
/*   char *sour; */
/*   int *dest; */
/*   { */
/*   int cntr; */
/*   for(cntr=0;cntr<6;cntr++) */
/*     dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf); */
/*   } */

get_sysch(buf,sys_ch)
  unsigned char *buf;
  int *sys_ch;
  {
  int i,size,gsize,sr;
  unsigned char *ptr,*ptr_lim;
  unsigned int gh;
  size=mkuint4(buf);
  ptr_lim=buf+size;
  ptr=buf+10;
  i=0;
  do
    {
    gh=mkuint4(ptr);
    sys_ch[i++]=gh>>16;
    sr=gh&0xfff;
    if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8;
    else gsize=(sr>>1)+8;
#if DEBUG
    printf("gh=%08x sr=%d gs=%d\n",gh,sr,gsize); 
#endif
    ptr+=gsize;
    } while(ptr<ptr_lim);
  return i;
  }

make_skel(old_buf,new_buf)
  unsigned char *old_buf,*new_buf;
  {
  int i,size,gsize,new_size,sr;
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
    i=gh>>16;
    sr=gh&0xfff;
    if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8;
    else gsize=(sr>>1)+8;
#if DEBUG
    printf("gh=%08x sr=%d gs=%d\n",gh,sr,gsize); 
#endif
    ptr+=gsize;
    gh&=0xffff0fff;
    gsize=(sr>>1)+8;
#if DEBUG
    printf("gh=%08x sr=%d gs=%d\n",gh,sr,gsize); 
#endif
    *new_ptr++=gh>>24;
    *new_ptr++=gh>>16;
    *new_ptr++=gh>>8;
    *new_ptr++=gh;
    for(i=0;i<gsize-4;i++) *new_ptr++=0;
    new_size+=gsize;
    } while(ptr<ptr_lim);
  new_buf[0]=new_size>>24;
  new_buf[1]=new_size>>16;
  new_buf[2]=new_size>>8;
  new_buf[3]=new_size;
  }

elim_ch(sys_ch,n_ch,old_buf,new_buf)
  unsigned char *old_buf,*new_buf;
  int *sys_ch,n_ch;
  {
  int i,j,size,gsize,new_size,sr;
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
    i=gh>>16;
    sr=gh&0xfff;
    if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8;
    else gsize=(sr>>1)+8;
    for(j=0;j<n_ch;j++) if(i==sys_ch[j]) break;
    if(n_ch==0 || j==n_ch)
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
#if DEBUG
  printf("new size=%d\n",new_size); 
#endif
  return new_size;
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
  int i,re,size,mainsize,subsize,init,mainend,subend,nch,
    dec_start[6],dec_now[6];
  FILE *f_main,*f_sub,*f_out,*fp;
  char *ptr;
  static unsigned char subbuf[MAXSIZE],tmpfile1[NAMLEN],
    textbuf[NAMLEN],new_file[NAMLEN],tmpfile3[NAMLEN],
    chfile1[NAMLEN],chfile2[NAMLEN],tmpfile2[NAMLEN];
  static  unsigned char *mainbuf=NULL,*selbuf=NULL;
  static int sysch[65536];

  if(argc<3)
    {
    fprintf(stderr," usage of 'wadd' :\n");
    fprintf(stderr,"   'wadd [main file] [sub file] ([output directory])\n");
    fprintf(stderr,"   output file has the same name as 'main file'\n");
    exit(0);
    }

  if((f_main=fopen(argv[1],"r"))==NULL)
    {
    perror("fopen");
    exit(1);
    }
  sprintf(chfile1,"%s.ch",argv[1]);
  if((fp=fopen(chfile1,"r"))==NULL) *chfile1=0;
  else fclose(fp);

  if((f_sub=fopen(argv[2],"r"))==NULL)
    {
    perror("fopen");
    exit(1);
    }
  sprintf(chfile2,"%s.ch",argv[2]);
  if((fp=fopen(chfile2,"r"))==NULL) *chfile2=0;
  else fclose(fp);

  if(argc>3)
    {
    sprintf(tmpfile1,"%s/%s.%d",argv[3],TEMPNAME,getpid());
    sprintf(tmpfile2,"%s/%s_ch.%d",argv[3],TEMPNAME,getpid());
    sprintf(tmpfile3,"%s/%s_chs.%d",argv[3],TEMPNAME,getpid());
    }
  else
    {
    sprintf(tmpfile1,"%s.%d",TEMPNAME,getpid());
    sprintf(tmpfile2,"%s_ch.%d",TEMPNAME,getpid());
    sprintf(tmpfile3,"%s_chs.%d",TEMPNAME,getpid());
    }

  if((f_out=fopen(tmpfile1,"w+"))==NULL)
    {
    perror("fopen");
    exit(1);
    }

  init=1;
  mainend=subend=0;
  if((mainsize=read_onesec_win(f_main,&mainbuf))==0)
    {
    mainend=1;
    nch=0;
    }
  else
    {
    bcd_dec(dec_start,(char *)mainbuf+4);
    nch=get_sysch(mainbuf,sysch);
#if DEBUG
    printf("nch=%d\n",nch);
#endif
    }
  if((subsize=read_onesec_win(f_sub,&selbuf))==0) subend=1;
  else
    {
    if((subsize=elim_ch(sysch,nch,selbuf,subbuf))<=10) subend=1;
    else bcd_dec(dec_now,(char *)subbuf+4);
    }
#if DEBUG
  printf("%02x%02x%02x%02x%02x%02x\n",mainbuf[4],mainbuf[5],mainbuf[6],
    mainbuf[7],mainbuf[8],mainbuf[9]);
  printf("%02x%02x%02x%02x%02x%02x\n",subbuf[4],subbuf[5],subbuf[6],
    subbuf[7],subbuf[8],subbuf[9]);
#endif
  while(mainend==0 || subend==0)
    {
    if(mainend==0 && subend==0) re=time_cmp(dec_start,dec_now,6);
    else re=2;
#if DEBUG
    printf("main=%d sub=%d re=%d\n",mainend,subend,re);
#endif
    if(subend || re==(-1))      /* write main until sub comes */
      {
      if(init)
        {
        if(subend)
          {
          if((re=fwrite(mainbuf,1,mainsize,f_out))==0) werror();
          }
        else
          {
          make_skel(subbuf,selbuf);
          size=mainsize+mkuint4(selbuf)-10;
          i=1;if(*(char *)&i) SWAPL(size);
          if((re=fwrite(&size,4,1,f_out))==0) werror();
          if((re=fwrite(mainbuf+4,1,mainsize-4,f_out))==0) werror();
          if((re=fwrite(selbuf+10,1,mkuint4(selbuf)-10,f_out))==0) werror();
          }
        }
      else
      if((re=fwrite(mainbuf,1,mainsize,f_out))==0) werror();
      init=0;
      if((mainsize=read_onesec_win(f_main,&mainbuf))==0) mainend=1;
      else bcd_dec(dec_start,(char *)mainbuf+4);
      }     
    else if(mainend || re==1) /* skip sub until main */
      {
      if(init==0)
        {
        if((re=fwrite(subbuf,1,subsize,f_out))==0) werror();
        }
      if((subsize=read_onesec_win(f_sub,&selbuf))==0) subend=1;
      else
        {
        if((subsize=elim_ch(sysch,nch,selbuf,subbuf))<=10) subend=1;
        else bcd_dec(dec_now,(char *)subbuf+4);
        }
      }
    else             /* start together */
      {
      size=mainsize+subsize-10;
      i=1;if(*(char *)&i) SWAPL(size);
      if((re=fwrite(&size,4,1,f_out))==0) werror();
      if((re=fwrite(mainbuf+4,1,mainsize-4,f_out))==0) werror();
      if((re=fwrite(subbuf+10,1,subsize-10,f_out))==0) werror();
      init=0;
      if((mainsize=read_onesec_win(f_main,&mainbuf))==0) mainend=1;
      else bcd_dec(dec_start,(char *)mainbuf+4);
      if((subsize=read_onesec_win(f_sub,&selbuf))<=10) subend=1;
      else
        {
        if((subsize=elim_ch(sysch,nch,selbuf,subbuf))<=10) subend=1;
        else bcd_dec(dec_now,(char *)subbuf+4);
        }
      }
#if DEBUG
    printf("%02x%02x%02x%02x%02x%02x\n",mainbuf[4],mainbuf[5],mainbuf[6],
      mainbuf[7],mainbuf[8],mainbuf[9]);
    printf("%02x%02x%02x%02x%02x%02x\n",subbuf[4],subbuf[5],subbuf[6],
      subbuf[7],subbuf[8],subbuf[9]);
#endif
    }

  fclose(f_out);
  if((ptr=strrchr(argv[1],'/'))==NULL) ptr=argv[1];
  else ptr++;
  if(argc>3) sprintf(new_file,"%s/%s",argv[3],ptr);
  else strcpy(new_file,ptr);
  sprintf(textbuf,"%s.sv",new_file);
  unlink(textbuf);  /* remove bitmap file if exists */
  if(strcmp(argv[1],new_file)) rename(tmpfile1,new_file);
  else
    {
    sprintf(textbuf,"cp %s %s",tmpfile1,argv[1]);
    system(textbuf);
    unlink(tmpfile1);
    }

  if(*chfile1==0 && *chfile2==0) exit(0);
  else if(*chfile1)
    {
    sprintf(textbuf,"cp %s %s",chfile1,tmpfile2);
    system(textbuf);
    if(*chfile2)
      {
      sprintf(textbuf,"egrep -v '^#|^$' %s|awk '{print \"^\" $1}'>%s",
        tmpfile2,tmpfile3);
      system(textbuf);
      sprintf(textbuf,"egrep -f %s -v %s >> %s",tmpfile3,chfile2,tmpfile2);
      system(textbuf);
      }
    }
  else if(*chfile2)
    {
    sprintf(textbuf,"cp %s %s",chfile2,tmpfile2);
    system(textbuf);
    }
  if(strcmp(argv[1],new_file))
    {
    strcat(new_file,".ch");
    rename(tmpfile2,new_file);
    }
  else
    {
    sprintf(textbuf,"cp %s %s",tmpfile2,chfile1);
    system(textbuf);
    unlink(tmpfile2);
    }
  unlink(tmpfile3);
  exit(0);
  }
