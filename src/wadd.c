/* $Id: wadd.c,v 1.6.4.3.2.10 2011/01/12 15:51:41 uehira Exp $ */
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
  2010.9.16 64bit and high sampling rate compatibility (Uehira)
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
#define   NAMLEN    1024
#define   TEMPNAME  "wadd.tmp"

static const char  rcsid[] =
   "$Id: wadd.c,v 1.6.4.3.2.10 2011/01/12 15:51:41 uehira Exp $";

/* prototypes */
static int get_syschnum(uint8_w *, WIN_ch []);
static void make_skel(uint8_w *, uint8_w *);
static WIN_bs elim_ch(WIN_ch [], int, uint8_w *, uint8_w []);
static void werror(void);
static void bfov_error(void);
int main(int, char *[]);

static int
get_syschnum(uint8_w *buf, WIN_ch sys_ch[])
  {
  int i;
  uint32_w gsize;
  uint8_w *ptr,*ptr_lim;

  ptr_lim=buf+mkuint4(buf);
  ptr=buf+10;
  i=0;
  do
    {
    gsize = get_sysch(ptr, &sys_ch[i++]);
    ptr+=gsize;
    } while(ptr<ptr_lim);
  return (i);
  }

static void
make_skel(uint8_w *old_buf, uint8_w *new_buf)
  {
  int i;
  uint32_w  gsize;
  WIN_bs  new_size;
  uint8_w *ptr,*new_ptr,*ptr_lim;
  WIN_ch  chtmp;
  uint8_w  skelbuf[8];  /* dummy data buffer */

  /* 1Hz dummy data */
  skelbuf[2] = 0;
  skelbuf[3] = 1;
  /* Value of (first) sample = 0 */
  skelbuf[4] = skelbuf[5] = skelbuf[6] = skelbuf[7] = 0;

  ptr_lim=old_buf+mkuint4(old_buf);
  ptr=old_buf+4;
  new_ptr=new_buf+4;
  for(i=0;i<6;i++) *new_ptr++=(*ptr++);
  new_size=10;
  do
    {
    gsize = get_sysch(ptr, &chtmp);
    ptr+=gsize;
    /* channel ID */
    skelbuf[0] = (uint8_w)(chtmp >> 8);
    skelbuf[1] = (uint8_w)chtmp;
    (void)memcpy(new_ptr, skelbuf, sizeof(skelbuf));
    new_ptr += sizeof(skelbuf);
    new_size+=sizeof(skelbuf);
    } while(ptr<ptr_lim);
  new_buf[0]=new_size>>24;
  new_buf[1]=new_size>>16;
  new_buf[2]=new_size>>8;
  new_buf[3]=new_size;
  }

static WIN_bs
elim_ch(WIN_ch sys_ch[], int n_ch, uint8_w *old_buf, uint8_w new_buf[])
  {
  int i,j;
  uint32_w  gsize;
  WIN_bs  new_size;
  uint8_w *ptr,*new_ptr,*ptr_lim;
  WIN_ch  chtmp;

  ptr_lim=old_buf+mkuint4(old_buf);
  ptr=old_buf+4;
  new_ptr=new_buf+4;
  for(i=0;i<6;i++) *new_ptr++=(*ptr++);
  new_size=10;
  do
    {
    gsize = get_sysch(ptr, &chtmp);
    for(j=0;j<n_ch;j++) if(chtmp==sys_ch[j]) break;
    if(n_ch==0 || j==n_ch)
      {
      new_size+=gsize;
      if (new_size >= MAXSIZE) {
	(void)fprintf(stderr, "buffer max. size exceeded! new_size=%d > %d\n",
		      new_size, MAXSIZE);
	exit(1);
      }
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
  return (new_size);
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

  (void)fprintf(stderr,"wadd : Buffer overrun!\n");
  exit(1);
}

int
main(int argc, char *argv[])
  {
  int i,re,init,mainend,subend,nch,
    dec_start[6],dec_now[6];
  WIN_bs  size,mainsize,subsize;
  FILE *f_main,*f_sub,*f_out,*fp;
  char *ptr;
  static uint8_w subbuf[MAXSIZE];
  static char  tmpfile1[NAMLEN],tmpfile2[NAMLEN], tmpfile3[NAMLEN],
    textbuf[NAMLEN],new_file[NAMLEN],chfile1[NAMLEN],chfile2[NAMLEN];
  static  uint8_w *mainbuf=NULL,*selbuf=NULL;
  size_t  mainbuf_size, selbuf_size;
  static WIN_ch sysch[WIN_CHMAX];
  int  slen;

  if(argc<3)
    {
    WIN_version();
    fprintf(stderr, "%s\n", rcsid);
    fprintf(stderr," usage of 'wadd' :\n");
    fprintf(stderr,"   'wadd [main file] [sub file] ([output directory])\n");
    fprintf(stderr,"   output file has the same name as 'main file'\n");
    exit(0);
    }

  if((f_main=fopen(argv[1],"r"))==NULL)
    {
    perror(argv[1]);
    exit(1);
    }
  if (snprintf(chfile1,sizeof(chfile1),"%s.ch",argv[1]) >= sizeof(chfile1))
    bfov_error();

  if((fp=fopen(chfile1,"r"))==NULL) *chfile1=0;
  else fclose(fp);

  if((f_sub=fopen(argv[2],"r"))==NULL)
    {
    perror(argv[2]);
    exit(1);
    }
  if (snprintf(chfile2,sizeof(chfile2),"%s.ch",argv[2]) >= sizeof(chfile2))
    bfov_error();
  if((fp=fopen(chfile2,"r"))==NULL) *chfile2=0;
  else fclose(fp);

  if(argc>3)
    {
    if (snprintf(tmpfile1,sizeof(tmpfile1),
		 "%s/%s.%d",argv[3],TEMPNAME,getpid()) >= sizeof(tmpfile1))
      bfov_error();
    if (snprintf(tmpfile2,sizeof(tmpfile2),
		 "%s/%s_ch.%d",argv[3],TEMPNAME,getpid()) >= sizeof(tmpfile2))
      bfov_error();
    if (snprintf(tmpfile3,sizeof(tmpfile3),
		 "%s/%s_chs.%d",argv[3],TEMPNAME,getpid()) >= sizeof(tmpfile3))
      bfov_error();
    }
  else
    {
    if (snprintf(tmpfile1,sizeof(tmpfile1),
		 "%s.%d",TEMPNAME,getpid()) >= sizeof(tmpfile1))
      bfov_error();
    if (snprintf(tmpfile2,sizeof(tmpfile2),
		 "%s_ch.%d",TEMPNAME,getpid()) >= sizeof(tmpfile2))
      bfov_error();
    if (snprintf(tmpfile3,sizeof(tmpfile3),
		 "%s_chs.%d",TEMPNAME,getpid()) >= sizeof(tmpfile3))
      bfov_error();
    }

  if((f_out=fopen(tmpfile1,"w+"))==NULL)
    {
    perror("fopen");
    exit(1);
    }

  init=1;
  mainend=subend=0;
  if((mainsize=read_onesec_win(f_main,&mainbuf,&mainbuf_size))==0)
    {
    mainend=1;
    nch=0;
    }
  else
    {
    bcd_dec(dec_start,mainbuf+4);
    nch=get_syschnum(mainbuf,sysch);
#if DEBUG
    printf("nch=%d\n",nch);
#endif
    }
  if((subsize=read_onesec_win(f_sub,&selbuf,&selbuf_size))==0) subend=1;
  else
    {
    if((subsize=elim_ch(sysch,nch,selbuf,subbuf))<=10) subend=1;
    else bcd_dec(dec_now,subbuf+4);
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
          i=1;if(*(char *)&i) SWAP32(size);
          if((re=fwrite(&size,4,1,f_out))==0) werror();
          if((re=fwrite(mainbuf+4,1,mainsize-4,f_out))==0) werror();
          if((re=fwrite(selbuf+10,1,mkuint4(selbuf)-10,f_out))==0) werror();
          }
        }
      else
      if((re=fwrite(mainbuf,1,mainsize,f_out))==0) werror();
      init=0;
      if((mainsize=read_onesec_win(f_main,&mainbuf,&mainbuf_size))==0)
	mainend=1;
      else bcd_dec(dec_start,mainbuf+4);
      }     
    else if(mainend || re==1) /* skip sub until main */
      {
      if(init==0)
        {
        if((re=fwrite(subbuf,1,subsize,f_out))==0) werror();
        }
      if((subsize=read_onesec_win(f_sub,&selbuf,&selbuf_size))==0)
	subend=1;
      else
        {
        if((subsize=elim_ch(sysch,nch,selbuf,subbuf))<=10) subend=1;
        else bcd_dec(dec_now,subbuf+4);
        }
      }
    else             /* start together */
      {
      size=mainsize+subsize-10;
      i=1;if(*(char *)&i) SWAP32(size);
      if((re=fwrite(&size,4,1,f_out))==0) werror();
      if((re=fwrite(mainbuf+4,1,mainsize-4,f_out))==0) werror();
      if((re=fwrite(subbuf+10,1,subsize-10,f_out))==0) werror();
      init=0;
      if((mainsize=read_onesec_win(f_main,&mainbuf,&mainbuf_size))==0)
	mainend=1;
      else bcd_dec(dec_start,mainbuf+4);
      if((subsize=read_onesec_win(f_sub,&selbuf,&selbuf_size))<=10)
	subend=1;
      else
        {
        if((subsize=elim_ch(sysch,nch,selbuf,subbuf))<=10) subend=1;
        else bcd_dec(dec_now,subbuf+4);
        }
      }
#if DEBUG
    printf("%02x%02x%02x%02x%02x%02x\n",mainbuf[4],mainbuf[5],mainbuf[6],
      mainbuf[7],mainbuf[8],mainbuf[9]);
    printf("%02x%02x%02x%02x%02x%02x\n",subbuf[4],subbuf[5],subbuf[6],
      subbuf[7],subbuf[8],subbuf[9]);
#endif
    }  /* while(mainend==0 || subend==0) */

  fclose(f_out);
  if((ptr=strrchr(argv[1],'/'))==NULL) ptr=argv[1];
  else ptr++;
  if(argc>3)
    slen = snprintf(new_file,sizeof(new_file),"%s/%s",argv[3],ptr);
  else
    slen = snprintf(new_file,sizeof(new_file),"%s",ptr);
  if (slen >= sizeof(new_file))
    bfov_error();
  if (snprintf(textbuf,sizeof(textbuf),"%s.sv",new_file) >= sizeof(textbuf))
    bfov_error();
  unlink(textbuf);  /* remove bitmap file if exists */
  if(strcmp(argv[1],new_file)) rename(tmpfile1,new_file);
  else
    {
    if (snprintf(textbuf,sizeof(textbuf),
		 "cp %s %s",tmpfile1,argv[1]) >= sizeof(textbuf))
      bfov_error();
    system(textbuf);
    unlink(tmpfile1);
    }

  if(*chfile1==0 && *chfile2==0) exit(0);
  else if(*chfile1)
    {
    if (snprintf(textbuf,sizeof(textbuf),
		 "cp %s %s",chfile1,tmpfile2) >= sizeof(textbuf))
      bfov_error();
    system(textbuf);
    if(*chfile2)
      {
      if (snprintf(textbuf,sizeof(textbuf),
		   "egrep -v '^#|^$' %s|awk '{print \"^\" $1}'>%s",
		   tmpfile2,tmpfile3) >= sizeof(textbuf))
	bfov_error();
      system(textbuf);
      if (snprintf(textbuf,sizeof(textbuf),
		   "egrep -f %s -v %s >> %s",
		   tmpfile3,chfile2,tmpfile2) >= sizeof(textbuf))
	bfov_error();
      system(textbuf);
      }
    }
  else if(*chfile2)
    {
    if (snprintf(textbuf,sizeof(textbuf),
		 "cp %s %s",chfile2,tmpfile2) >= sizeof(textbuf))
      bfov_error();
    system(textbuf);
    }
  if(strcmp(argv[1],new_file))
    {
    /* strcat(new_file,".ch"); */
    if (snprintf(new_file, sizeof(new_file), "%s.ch", new_file)
	>= sizeof(new_file))
      bfov_error();
    rename(tmpfile2,new_file);
    }
  else
    {
    if (snprintf(textbuf,sizeof(textbuf),
		 "cp %s %s",tmpfile2,chfile1) >= sizeof(textbuf))
      bfov_error();
    system(textbuf);
    unlink(tmpfile2);
    }
  unlink(tmpfile3);
  exit(0);
  }
