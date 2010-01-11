/* $Id: win_system.c,v 1.10.4.1.4.4 2010/01/11 07:07:27 uehira Exp $ */
/* win system utility functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

#include "winlib.h"
#include "win_system.h"
#include "subst_func.h"

#define BUF_SIZE 1024


void
rmemo5(char f[], int c[])
{
  FILE *fp;
  int  bad_flag;

  do{
    while((fp=fopen(f,"r"))==NULL){
      fprintf(stderr,"file '%s' not found\007\n",f);
      sleep(1);
    }
    if(fscanf(fp,"%02d%02d%02d%02d.%02d",&c[0],&c[1],&c[2],&c[3],&c[4])>=5)
      bad_flag=0; /* OK */
    else{
      bad_flag=1;
      fprintf(stderr,"'%s' illegal. Waiting ...\n",f);
      sleep(1);
    }
    fclose(fp);
  } while(bad_flag);
}

void
rmemo6(char f[], int c[])
{
  FILE *fp;
  int  bad_flag;

  do{
    while((fp=fopen(f,"r"))==NULL){
      fprintf(stderr,"file '%s' not found\007\n",f);
      sleep(1);
    }
    if(fscanf(fp,"%02d%02d%02d.%02d%02d%02d",
	      &c[0],&c[1],&c[2],&c[3],&c[4],&c[5])>=6)
      bad_flag=0; /* OK */
    else{
      bad_flag=1;
      fprintf(stderr,"'%s' illegal. Waiting ...\n",f);
      sleep(1);
    }
    fclose(fp);
  } while(bad_flag);
}

int
wmemo5(char name[], int tm[])
{
  FILE  *fp;

  if((fp=fopen(name,"w+"))==NULL){
    fprintf(stderr,"Done file cannot open: %s\n",name);
    return(-2);
  }
  fprintf(fp,"%02d%02d%02d%02d.%02d\n",tm[0],tm[1],tm[2],tm[3],tm[4]);
  fclose(fp);
  return(0);
}

/* get channel list from buffer */
WIN_ch
get_sysch_list(unsigned char *buf, WIN_blocksize bufnum, WIN_ch sysch[])
{
  WIN_ch  num,chtmp;
  unsigned char  *ptr,*ptr_lim;
  WIN_blocksize  gsize;
  int  i;

  num=0;
  ptr_lim=buf+bufnum;
  ptr=buf+WIN_TIME_LEN;
  do{
    gsize=get_sysch(ptr,&chtmp);
    for(i=0;i<num;++i)
      if(sysch[i]==chtmp) break;
    if(i==num) sysch[num++]=chtmp;
    ptr+=gsize;
  } while(ptr<ptr_lim);

  return(num);
}

/* get channel list from channel file */
WIN_ch
get_chlist_chfile(FILE *fp, WIN_ch sysch[])
{
  WIN_ch chnum;
  char tbuf[BUF_SIZE];
  int  i;

  chnum=0;
  while(fgets(tbuf,BUF_SIZE,fp)!=NULL){
    if(tbuf[0]=='#') continue;  /* skip comment line */
    if(sscanf(tbuf,"%x",&i)<1) continue;  /* skip blank line */
    sysch[chnum++]=(WIN_ch)i;
  }
  return(chnum);
}

WIN_blocksize
get_merge_data(unsigned char *mergebuf,
	     unsigned char *mainbuf, WIN_blocksize *main_num,
	     unsigned char *subbuf,  WIN_blocksize *sub_num)
{
  WIN_ch  main_chnum,i;
  static WIN_ch  main_ch[WIN_CH_MAX_NUM],sub_ch;
  unsigned char  *ptr,*ptr_lim,*ptw;
  WIN_blocksize  gsize,size;

  main_chnum=get_sysch_list(mainbuf,*main_num,main_ch);
  ptr_lim=subbuf+(*sub_num);
  ptr=subbuf+WIN_TIME_LEN;  /* skip time stamp */
  ptw=mergebuf;
  size=0;
  do{
    gsize=get_sysch(ptr,&sub_ch);
    for(i=0;i<main_chnum;++i)
      if(main_ch[i]==sub_ch) break;
    if(i==main_chnum){
      main_ch[main_chnum++]=sub_ch;
      size+=gsize;
      while((gsize--)>0) *ptw++=(*ptr++);
    }
    else ptr+=gsize;
  } while(ptr<ptr_lim);

  return(size);
}


WIN_blocksize
get_select_data(unsigned char *selectbuf,
		WIN_ch chlist[], WIN_ch ch_num,
		unsigned char *buf,  WIN_blocksize buf_num)
{
  WIN_blocksize  size,gsize;
  static WIN_ch  buf_ch[WIN_CH_MAX_NUM],sel_ch;
  WIN_ch  buf_chnum;
  unsigned char  *ptr,*ptw;
  int  i;

  buf_chnum=get_sysch_list(buf,buf_num,buf_ch);
  ptr=buf+WIN_TIME_LEN;  /* skip time stamp */
  ptw=selectbuf;
  size=0;
  do{
    gsize=get_sysch(ptr,&sel_ch);
    for(i=0;i<ch_num;++i)
      if(chlist[i]==sel_ch) break;
    if(i<ch_num){
      size+=gsize;
      while((gsize--)>0) *ptw++=(*ptr++);
    }
    else ptr+=gsize;
  } while(ptr<buf+buf_num);
  
  return(size);
}


int
WIN_time_hani(char fname[], int start[], int end[])
{
  FILE  *fp;
  WIN_blocksize  a,size;
  unsigned char  *tbuf;
  int  status,init_flag,i;
  int  dtime[WIN_TIME_LEN];

  if((fp=fopen(fname,"r"))==NULL) return(-1);
  status=0;
  init_flag=1;
  while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN){
    size=(WIN_blocksize)mkuint4((unsigned char *)&a)-WIN_BLOCKSIZE_LEN;
    if((tbuf=MALLOC(unsigned char,size))==NULL){
      status=1;
      break;
    }
    if(fread(tbuf,1,size,fp)!=size){
      status=1;
      FREE(tbuf);
      break;
    }
    if(!bcd_dec(dtime,tbuf)){
      FREE(tbuf);
      continue;
    }
    if(init_flag){
      for(i=0;i<WIN_TIME_LEN;++i) start[i]=end[i]=dtime[i];
      init_flag=0;
    }
    else{
      if(time_cmp(start,dtime,6)>0) 
	for(i=0;i<WIN_TIME_LEN;++i) start[i]=dtime[i];
      if(time_cmp(end,dtime,6)<0)
	for(i=0;i<WIN_TIME_LEN;++i) end[i]=dtime[i];
    }
    FREE(tbuf);
  } /* while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN) */
  fclose(fp);

  return(status);
}

int
read_channel_file(FILE *fp, struct channel_tbl tbl[], int arrynum)
{
  char  tbuf[BUF_SIZE], format[BUF_SIZE];
  int   i, ich;

  i = 0;
  if (snprintf(format, sizeof(format),
	       "%%x%%d%%d%%%ds%%%ds%%d%%255s%%lf%%255s%%lf%%lf%%lf%%lf%%lf%%lf%%lf%%lf%%lf",
	       WIN_STANAME_LEN - 1, WIN_STACOMP_LEN - 1) >= sizeof(format)) {
    (void)fprintf(stderr, "buffer overtun!\n");
    exit(1);
  }
  /* printf("%s\n", format); */

  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')   /* skip comment line */
      continue;
    if (sscanf(tbuf, format,
	       &ich, &tbl[i].flag, &tbl[i].delay, tbl[i].name, tbl[i].comp,
	       &tbl[i].scale, tbl[i].bit, &tbl[i].sens, tbl[i].unit,
	       &tbl[i].t0, &tbl[i].h, &tbl[i].gain, &tbl[i].adc, &tbl[i].lat,
	       &tbl[i].lng, &tbl[i].higt, &tbl[i].stcp, &tbl[i].stcs) < 1)
      continue;   /* skip blank line */
    tbl[i++].sysch = (WIN_ch)ich;
    if (i == arrynum)
      break;
  }

  return (i);
}

/*** make matrix: m[nrow][ncol] ***/
int **
imatrix(int nrow, int ncol)
{
  int  **m;
  int  i, j;

  if (NULL == (m = (int **)malloc((size_t)(sizeof(int *) * nrow)))) {
    (void)fprintf(stderr, "malloc error\n");
    exit(1);
  }
  if (NULL == (m[0] = (int *)malloc((size_t)(sizeof(int) * nrow * ncol)))) {
    (void)fprintf(stderr, "malloc error\n");
    exit(1);
  }
  for (i = 1; i < nrow; ++i)
    m[i] = m[i - 1] + ncol;

  /* initialize */
  for (i = 0; i < nrow; ++i)
    for (j = 0; j < ncol; ++j)
      m[i][j] = 0;

  return (m);
}

/* High sampling rate version */
WIN_blocksize
win_get_chhdr(unsigned char *ptr, WIN_ch *chnum, WIN_sr *sr)
{
  WIN_blocksize  gsize;

  /* channel number */
  *chnum = (((WIN_ch)ptr[0]) << 8) + (WIN_ch)ptr[1];

  /* sampling rate */
  if ((ptr[2] & 0x80) == 0x0) /* channel header = 4 byte */
    *sr = (WIN_sr)ptr[3] + (((WIN_sr)(ptr[2] & 0x0f)) << 8);
  else                        /* channel header = 5 byte */
    *sr = (WIN_sr)ptr[4] + (((WIN_sr)ptr[3]) << 8)
      + (((WIN_sr)(ptr[2] & 0x0f)) << 16);

  /* size */
  if ((ptr[2] >> 4) & 0x7)
    gsize = ((ptr[2] >> 4) & 0x7) * (*sr - 1) + 8;
  else
    gsize = (*sr >> 1) + 8;
  if (ptr[2] & 0x80)
    gsize++;

  return (gsize);
}

