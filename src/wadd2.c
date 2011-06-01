/* $Id: wadd2.c,v 1.4.2.3 2011/06/01 12:14:54 uehira Exp $ */
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
  2002.5.11 debugged
  2003.7.12 mv -> cp;rm  for cygwin
  2007.1.15 MAXSIZE 1M->5M
  2010.9.17 64bit and high sampling rate compatibility (Uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>

#include "winlib.h"

#define   MAXSIZE   5000000
#define   NAMLEN    1024
#define   TEMPNAME  "wadd2.tmp"

static const char  rcsid[] =
   "$Id: wadd2.c,v 1.4.2.3 2011/06/01 12:14:54 uehira Exp $";


/* prototypes */
static WIN_bs copy_ch(int, WIN_ch [], uint8_w *, int, uint8_w *);
static void werror(void);
static void bfov_error(void);
int main(int, char *[]);

static WIN_bs
copy_ch(int makelist, WIN_ch sys_ch[],
	uint8_w *inbuf, int insize, uint8_w *outbuf)
  {
  int i,j,k;
  uint32_w  gsize;
  uint8_w *ptr,*ptw,*ptr_lim;
  WIN_ch  chtmp;

  ptr_lim=inbuf+insize;
  ptr=inbuf;
  ptw=outbuf;
  if(makelist)
    {
    for(i=0;i<6;i++) *ptw++=(*ptr++);
    for(j=0;j<WIN_CHMAX;j++) sys_ch[j]=0;
    }
  else ptr+=6;
  k=0;
  do
    {
    gsize = get_sysch(ptr, &chtmp);
    if(makelist)
      {
      sys_ch[chtmp]=1;
      memcpy(ptw,ptr,gsize);
      ptw+=gsize;
      k++;
      }
    else
      {      
      if(sys_ch[chtmp]==0)
        {
        memcpy(ptw,ptr,gsize);
        ptw+=gsize;
        k++;
        }
      }
    ptr+=gsize;
    } while(ptr<ptr_lim);
#if DEBUG
  printf("makelist=%d insize=%d insize2=%d outsize=%d ch=%d\n",
    makelist,insize,ptr-inbuf,ptw-outbuf,k); 
#endif
  return (ptw-outbuf);
  }

static void
werror(void)
  {

  perror("fwrite");
  exit(1);
  }

static void
bfov_error(void)
{

  (void)fprintf(stderr,"wadd2 : Buffer overrun!\n");
  exit(1);
}

int
main(int argc, char *argv[])
  {
  int re,mainend,subend,dec_sub[6],dec_main[6];
  WIN_bs  size, mainsize, subsize;
  FILE *f_main,*f_sub,*f_out;
  uint8_w *ptr;
  char  *ptrs;
  static char tmpfile1[NAMLEN],textbuf[NAMLEN],new_file[NAMLEN];
  static uint8_w  selbuf[MAXSIZE];
  static uint8_w *mainbuf=NULL,*subbuf=NULL;
  size_t  mainbuf_siz, subbuf_siz;
  static WIN_ch sysch[WIN_CHMAX];

  if(argc<3)
    {
    WIN_version();
    fprintf(stderr, "%s\n", rcsid);
    fprintf(stderr," usage of 'wadd2' :\n");
    fprintf(stderr,"   'wadd2 [mainfile] [subfile] (-/[outdir])\n");
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
    else
      {
	if (snprintf(tmpfile1,sizeof(tmpfile1),
		     "%s/%s.%d",argv[3],TEMPNAME,getpid()) >= sizeof(tmpfile1))
	  bfov_error();
      }
    }
  else
    {
      if (snprintf(tmpfile1,sizeof(tmpfile1),
		   "%s.%d",TEMPNAME,getpid()) >= sizeof(tmpfile1))
	bfov_error();
    }

  if(*tmpfile1 && (f_out=fopen(tmpfile1,"w+"))==NULL)
    {
    perror("fopen");
    exit(1);
    }

  mainend=subend=0;

  if((mainsize=read_onesec_win(f_main,&mainbuf,&mainbuf_siz))==0) mainend=1;
  else bcd_dec(dec_main,mainbuf+4);

  if((subsize=read_onesec_win(f_sub,&subbuf,&subbuf_siz))==0) subend=1;
  else bcd_dec(dec_sub,subbuf+4);

  while(mainend==0 || subend==0) /* MAIN LOOP */
    {
    if(mainend==1) re=1; /* main end. out sub only */
    else if(subend==1) re=(-1); /* sub end. out main only */
    else re=time_cmp(dec_main,dec_sub,6);
             /* if main<sub then re==-1 and out main only */
             /* if main>sub then re==1  and out sub only */
#if DEBUG
    printf("main=%d sub=%d re=%d\n",mainend,subend,re);
    printf("m:%02x%02x%02x%02x%02x%02x\n",mainbuf[4],mainbuf[5],mainbuf[6],
      mainbuf[7],mainbuf[8],mainbuf[9]);
    printf("s:%02x%02x%02x%02x%02x%02x\n",subbuf[4],subbuf[5],subbuf[6],
      subbuf[7],subbuf[8],subbuf[9]);
#endif

    ptr=selbuf;
    ptr+=4;

    if(re<=0) /* output main */
      {
      ptr+=copy_ch(1,sysch,mainbuf+4,mainsize-4,ptr);
      if((mainsize=read_onesec_win(f_main,&mainbuf,&mainbuf_siz))==0) mainend=1;
      else bcd_dec(dec_main,mainbuf+4);
      }
    if(re>=0) /* output sub */
      {
      if(re>0) ptr+=copy_ch(1,sysch,subbuf+4,subsize-4,ptr);
      else ptr+=copy_ch(0,sysch,subbuf+4,subsize-4,ptr);
      if((subsize=read_onesec_win(f_sub,&subbuf,&subbuf_siz))==0) subend=1;
      else bcd_dec(dec_sub,subbuf+4);
      }

    if ((size=ptr-selbuf) > sizeof(selbuf)) {
      (void)fprintf(stderr,
		    "Buffer overrun! Already overwritten illegal adress!!\n");
      exit(1);
    }
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

  if(f_out!=stdout)
    {
    fclose(f_out);
    if((ptrs=strrchr(argv[1],'/'))==NULL) ptrs=argv[1];
    else ptrs++;
    /* ptrs points to basename of outfile */
    if(argc>3)
      {
	if (snprintf(new_file,sizeof(new_file),"%s/%s",argv[3],ptrs)
	    >= sizeof(new_file))
	  bfov_error();
      }
    else
      {
	/* strcpy(new_file,argv[1]); */
	if (snprintf(new_file,sizeof(new_file),"%s",argv[1])
	    >= sizeof(new_file))
	  bfov_error();
      }
    /* newfile holds path of outfile */
/*    sprintf(textbuf,"mv %s %s",tmpfile1,new_file);*/
    if (snprintf(textbuf,sizeof(textbuf),"cp %s %s;rm %s",
		 tmpfile1,new_file,tmpfile1) >= sizeof(textbuf))
      bfov_error();
    system(textbuf);
    }

  exit(0);
  }
