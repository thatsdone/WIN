/* $Id: win_system.c,v 1.8 2003/11/03 10:08:02 uehira Exp $ */
/* win system utility functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "win_system.h"
#include "subst_func.h"

#define BUF_SIZE 1024

unsigned long
mklong(unsigned char *ptr)
{   
  unsigned long a;
  
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return(a);
}

void
adj_time(int tm[])
{
  if(tm[5]==60){
    tm[5]=0;
    if(++tm[4]==60){
      tm[4]=0;
      if(++tm[3]==24){
        tm[3]=0;
        tm[2]++;
        switch(tm[1]){
	  case 2:
	    if(tm[0]%4==0){
	      if(tm[2]==30){
                tm[2]=1;
                tm[1]++;
              }
              break;
            }
            else{
              if(tm[2]==29){
                tm[2]=1;
                tm[1]++;
              }
              break;
            }
          case 4:
          case 6:
          case 9:
          case 11:
            if(tm[2]==31){
              tm[2]=1;
              tm[1]++;
            }
            break;
          default:
            if(tm[2]==32){
              tm[2]=1;
              tm[1]++;
            }
            break;
        }
        if(tm[1]==13){
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
        }
      }
    }
  }
  else if(tm[5]==-1){
    tm[5]=59;
    if(--tm[4]==-1){
      tm[4]=59;
      if(--tm[3]==-1){
        tm[3]=23;
        if(--tm[2]==0){
          switch(--tm[1]){
            case 2:
              if(tm[0]%4==0) tm[2]=29;
              else tm[2]=28;
              break;
            case 4:
            case 6:
            case 9:
            case 11:
              tm[2]=30;
              break;
            default:
              tm[2]=31;
              break;
          }
          if(tm[1]==0){
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
          }
        }
      }
    }
  }
}

void
adj_time_m(int tm[])
{
  if(tm[4]==60){
    tm[4]=0;
    if(++tm[3]==24){
      tm[3]=0;
      tm[2]++;
      switch(tm[1]){
        case 2:
          if(tm[0]%4==0){
            if(tm[2]==30){
              tm[2]=1;
              tm[1]++;
            }
            break;
          }
          else{
            if(tm[2]==29){
              tm[2]=1;
              tm[1]++;
            }
            break;
          }
        case 4:
        case 6:
        case 9:
        case 11:
          if(tm[2]==31){
            tm[2]=1;
            tm[1]++;
          }
          break;
        default:
          if(tm[2]==32){
            tm[2]=1;
            tm[1]++;
          }
          break;
      }
      if(tm[1]==13){
        tm[1]=1;
        if(++tm[0]==100) tm[0]=0;
      }
    }
  }
  else if(tm[4]==-1){
    tm[4]=59;
    if(--tm[3]==-1){
      tm[3]=23;
      if(--tm[2]==0){
        switch(--tm[1]){
          case 2:
            if(tm[0]%4==0) tm[2]=29;
            else tm[2]=28;
            break;
          case 4:
          case 6:
          case 9:
          case 11:
            tm[2]=30;
            break;
          default:
            tm[2]=31;
            break;
        }
        if(tm[1]==0){
          tm[1]=12;
          if(--tm[0]==-1) tm[0]=99;
        }
      }
    }
  }
}

int
bcd_dec(int dest[], unsigned char *sour)
{
  static int b2d[]={
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,  /* 0x00 - 0x0F */
    10,11,12,13,14,15,16,17,18,19,-1,-1,-1,-1,-1,-1,
    20,21,22,23,24,25,26,27,28,29,-1,-1,-1,-1,-1,-1,
    30,31,32,33,34,35,36,37,38,39,-1,-1,-1,-1,-1,-1,
    40,41,42,43,44,45,46,47,48,49,-1,-1,-1,-1,-1,-1,
    50,51,52,53,54,55,56,57,58,59,-1,-1,-1,-1,-1,-1,
    60,61,62,63,64,65,66,67,68,69,-1,-1,-1,-1,-1,-1,
    70,71,72,73,74,75,76,77,78,79,-1,-1,-1,-1,-1,-1,
    80,81,82,83,84,85,86,87,88,89,-1,-1,-1,-1,-1,-1,
    90,91,92,93,94,95,96,97,98,99,-1,-1,-1,-1,-1,-1,  /* 0x90 - 0x9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  int i;
  
  i=b2d[sour[0]];
  if(i>=0 && i<=99) dest[0]=i; else return(0);
  i=b2d[sour[1]];
  if(i>=1 && i<=12) dest[1]=i; else return(0);
  i=b2d[sour[2]];
  if(i>=1 && i<=31) dest[2]=i; else return(0);
  i=b2d[sour[3]];
  if(i>=0 && i<=23) dest[3]=i; else return(0);
  i=b2d[sour[4]];
  if(i>=0 && i<=59) dest[4]=i; else return(0);
  i=b2d[sour[5]];
  if(i>=0 && i<=60) dest[5]=i; else return(0);
  return(1);
}

int
time_cmp(int *t1, int *t2, int i)
{
  int cntr;

  cntr=0;
  if(t1[cntr]<70 && t2[cntr]>70) return(1);
  if(t1[cntr]>70 && t2[cntr]<70) return(-1);
  for(;cntr<i;cntr++){
    if(t1[cntr]>t2[cntr]) return(1);
    if(t1[cntr]<t2[cntr]) return(-1);
  } 
  return(0);
}
 
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

static WIN_blocksize
get_sysch(unsigned char *buf, WIN_ch *ch)
{
  unsigned char  gh[WIN_CHHEADER_LEN];
  WIN_sr  sr;
  WIN_blocksize  gsize;
  int  i;
  
  for(i=0;i<WIN_CHHEADER_LEN;++i) gh[i]=buf[i];
  /* channel number */
  *ch=(((WIN_ch)gh[0])<<8)+(WIN_ch)gh[1];
  /* sampling rate */
  sr=(((WIN_sr)(gh[2]&0x0f))<<8)+(WIN_sr)gh[3];
  /* sample size */
  if((gh[2]>>4)&0x7) gsize=((gh[2]>>4)&0x7)*(sr-1)+8;
  else gsize=(sr>>1)+8;

  return(gsize);
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
    size=(WIN_blocksize)mklong((unsigned char *)&a)-WIN_BLOCKSIZE_LEN;
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

WIN_blocksize
read_onesec_win(FILE *fp, unsigned char **rbuf)
{
  unsigned char  sz[WIN_BLOCKSIZE_LEN];
  WIN_blocksize  size;
  static WIN_blocksize  sizesave;

  /* printf("%d\n", sizesave); */
  if (fread(sz, 1, WIN_BLOCKSIZE_LEN, fp) != WIN_BLOCKSIZE_LEN)
    return (0);
  size = mklong(sz);

  if (*rbuf == NULL)
    sizesave = 0;
  if (size > sizesave) {
    sizesave = (size << 1);
    *rbuf = realloc(*rbuf, sizesave);
    if (*rbuf == NULL) {
      (void)fprintf(stderr, "%s\n", strerror(errno));
      exit(1);
    }
  }

  memcpy(*rbuf, sz, WIN_BLOCKSIZE_LEN);
  if (fread(*rbuf + WIN_BLOCKSIZE_LEN, 1, size - WIN_BLOCKSIZE_LEN, fp) !=
      size - WIN_BLOCKSIZE_LEN)
    return (0);

  return (size);
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
