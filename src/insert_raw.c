/* $Id: insert_raw.c,v 1.6.4.2.2.3 2010/02/03 01:39:55 uehira Exp $ */

/*
 * Insert sorted timeout data to raw data.
 *
 *------------ sample of parameter file ------------
 *|# This file is insert_raw's parameter file      |
 *|./raw		# raw data dir             |
 *|./raw_timeouts	# time-out data dir        |
 *|/dat/tmp		# temporary dir            |
 *|3                    # wait time[min].          |
 *|                     #   if blank, 0.           |
 *--------------------------------------------------
 *
 */

/*-
 * 2005/3/12   memory leak bug fixed.
 * 2010/2/2    64bit check
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

#include "winlib.h"
#include "subst_func.h"
/* #include "win_system.h" */

/* #define DEBUG  0 */

#define TMP_ADD_NAME   "insert_raw.add"
#define DEFAULT_WAIT_MIN   0  /* minute */

#define WIN_FILENAME_MAX 1024
#define BUF_SIZE 1024

static char rcsid[] =
  "$Id: insert_raw.c,v 1.6.4.2.2.3 2010/02/03 01:39:55 uehira Exp $";

char *progname;

struct Cnt_file {
  char  raw_dir[WIN_FILENAME_MAX];    /* raw data directory */
  char  junk_dir[WIN_FILENAME_MAX];   /* time-out data directory */
  char  tmp_dir[WIN_FILENAME_MAX];    /* temporary directory */
  char  raw_latst[WIN_FILENAME_MAX];  /* raw LATEST */ 
  char  raw_oldst[WIN_FILENAME_MAX];  /* raw OLDEST */
  char  junk_latst[WIN_FILENAME_MAX]; /* time-out LATEST */
  char  junk_oldst[WIN_FILENAME_MAX]; /* time-out OLDEST */
  char  junk_used[WIN_FILENAME_MAX];  /* time-out USED */
  int   wait_min;                 /* wait time(min.) from raw LATEST */
};

/* prototypes */
static void end_prog(int);
static void memory_error(void);
static void print_usage(void);
static int read_param(char [], struct Cnt_file *);
static void do_insert(int [], struct Cnt_file *);
int main(int, char *[]);

/* exit program with status */
static void
end_prog(int status)
{

  printf("*****  %s end  *****\n",progname);
  exit(status);
}

static void
memory_error()
{

  fprintf(stderr,"'%s': cannot allocate memory.\n",progname);
  end_prog(-3);
}

/* print usage */
static void
print_usage()
{

  WIN_version();
  fprintf(stderr,"%s\n",rcsid);
  fprintf(stderr,"Usage of %s :\n",progname);
  fprintf(stderr,"\t%s [param file] ([YYMMDDhh.mm(1)] [YYMMDDhh.mm(2)])\n",
	  progname);
}

/* 
   read_param(char filename[], struct Cnt_file *Cnt_file)
      read parameter file and write to "struct Cnt_file".
      Input: char filename[]
      Output: struct Cnt_file *Cnt_file
      Return value: 
       -1: cannot open parameter file.
        0: normal end.
	1: parameter values are not enough. */
static int
read_param(char filename[], struct Cnt_file *Cnt_file)
{
  FILE  *fp;
  char  buf[BUF_SIZE],out[BUF_SIZE];
  int   status,count;

  if(NULL==(fp=fopen(filename,"r"))){
    fprintf(stderr,"Cannot open parameter file: %s\n",filename);
    return(-1);
  }
  count=0;
  status=1;
  Cnt_file->wait_min=DEFAULT_WAIT_MIN;
  while(fgets(buf,sizeof(buf),fp)!=NULL){
    if(buf[0]=='#') continue;  /* skip comment line */
    if(sscanf(buf,"%s",out)<1) break;
    if(count==0) strcpy(Cnt_file->raw_dir,out);
    else if(count==1) strcpy(Cnt_file->junk_dir,out);
    else if(count==2){
      strcpy(Cnt_file->tmp_dir,out);
      status=0;
    }
    else if(count==3){
      Cnt_file->wait_min=atoi(out);
      break;
    }
    count++;
  }
  fclose(fp);
  if(status)
    fprintf(stderr,"Parameter is not enough. Please check %s\n",filename);

  return(status);
}

static void
do_insert(int tim[], struct Cnt_file *cnt)
{
  FILE  *fp,*fpraw,*fpadd;
  long  fpt;  /* 64bit ok */
  WIN_bs  a;
  WIN_bs  size,size_save;
  WIN_bs  sizer;
  WIN_bs  sizem,sizew;
  WIN_bs  data_num,data_num_save;
  WIN_bs  array_size_of_data;
  uint8_w  *data,*datam,*datar,*tmpbuf;
  uint8_w  size_arr[WIN_BLOCKSIZE_LEN];
  uint8_w  *ptrd;
  int  dtime[WIN_TIME_LEN],drtime[WIN_TIME_LEN],dtime_tmp[WIN_TIME_LEN];
  int  tim_raw_latest[5],tim_raw_oldest[5];
  char  data_name[WIN_FILENAME_MAX];
  char  outname[WIN_FILENAME_MAX];
  char  addname[WIN_FILENAME_MAX];
  char  cmdbuf[WIN_FILENAME_MAX];
  int  wait_flag;
  int  i=0,j;

  if (snprintf(data_name,sizeof(data_name)-1,"%s/%02d%02d%02d%02d.%02d",
	       cnt->junk_dir,tim[0],tim[1],tim[2],tim[3],tim[4])
      > sizeof(data_name)-1) {
    fprintf(stderr, "Error : data is too large!¥ü\n");
    exit(1);
  }
  if((fp=fopen(data_name,"r"))==NULL) return;

  while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN){  /*(1)*/
    /*** copy same minute data to data[] ***/
    data_num_save=data_num=size=(WIN_bs)mkuint4((uint8_w *)&a);
    array_size_of_data = data_num << 2;
    if((data=MALLOC(uint8_w,array_size_of_data))==NULL) memory_error();
    memcpy(data,&a,WIN_BLOCKSIZE_LEN);
    size-=WIN_BLOCKSIZE_LEN;
    if(fread(data+WIN_BLOCKSIZE_LEN,1,size,fp)!=size){
      FREE(data);
      break; /* exit do_insert() in case of timeout file broken */
    }
    if(!bcd_dec(dtime,data+WIN_BLOCKSIZE_LEN)){
      FREE(data);
      continue; /* skip in case of strange time stamp */
    }
    fpt=ftell(fp);
    while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN){  /*(2)*/
      size_save=size=(WIN_bs)mkuint4((uint8_w *)&a);
      if((tmpbuf=MALLOC(uint8_w,size))==NULL) memory_error();
      memcpy(tmpbuf,&a,WIN_BLOCKSIZE_LEN);
      size-=WIN_BLOCKSIZE_LEN;
      if(fread(tmpbuf+WIN_BLOCKSIZE_LEN,1,size,fp)!=size){
	FREE(tmpbuf); FREE(data);
	goto insert_end; /* exit do_insert() in case of timeout file broken */
      }
      if(!bcd_dec(dtime_tmp,tmpbuf+WIN_BLOCKSIZE_LEN)){
	FREE(tmpbuf);
	continue; /* skip in case of strange time stamp */
      }
      /* if next minutes, exit this loop */
      if(time_cmp(dtime,dtime_tmp,5)){
	FREE(tmpbuf);
	fseek(fp,fpt,0);
	break;
      }
      data_num+=size_save;
      if (array_size_of_data < data_num) {
	array_size_of_data = data_num << 1;
	if((data=REALLOC(uint8_w,data,array_size_of_data))==NULL)
	  memory_error();
      }
      memcpy(data+data_num_save,tmpbuf,size_save);
      data_num_save=data_num;
      fpt=ftell(fp);
      FREE(tmpbuf);
    } /* while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN) (2) */

    /* compare with oldest raw data */
    rmemo5(cnt->raw_oldst,tim_raw_oldest);
    if(time_cmp(dtime,tim_raw_oldest,5)<0){
      FREE(data);
      continue;  /* skip data which is older than OLDEST */
    }
    /* compare with latest raw data */
    do{
      rmemo5(cnt->raw_latst,tim_raw_latest);
#if DEBUG>5
      fprintf(stderr,"raw LATEST: %02d%02d%02d%02d.%02d",
	      tim_raw_latest[0],tim_raw_latest[1],tim_raw_latest[2],
	      tim_raw_latest[3],tim_raw_latest[4]);
#endif
      for(j=0;j<cnt->wait_min;++j){
	tim_raw_latest[4]--;
	adj_time_m(tim_raw_latest);
      }
#if DEBUG>5
      fprintf(stderr,"   %02d%02d%02d%02d.%02d\n",
	      tim_raw_latest[0],tim_raw_latest[1],tim_raw_latest[2],
	      tim_raw_latest[3],tim_raw_latest[4]);
      fflush(stderr);
#endif
      if(time_cmp(dtime,tim_raw_latest,5)<=0) wait_flag=0;
      else{
	wait_flag=1;
#if DEBUG
	fprintf(stderr,"Waiting raw LATEST: %02d%02d%02d%02d.%02d + %d\n",
		dtime[0],dtime[1],dtime[2],dtime[3],dtime[4],cnt->wait_min);
	fflush(stderr);
#endif
	sleep(5);  /* wait new LATEST */
      }
    } while(wait_flag);

    sprintf(outname,"%s/%02d%02d%02d%02d.%02d",cnt->raw_dir,
	    dtime[0],dtime[1],dtime[2],dtime[3],dtime[4]);
    sprintf(addname,"%s/%s.%d.%d",cnt->tmp_dir,TMP_ADD_NAME,i,getpid());
#if DEBUG
    fprintf(stderr,"outname:%s  addname:%s\n",outname,addname);
    fflush(stderr);
#endif
    if((fpadd=fopen(addname,"w"))==NULL){
      fprintf(stderr,"Cannot create file: '%s'\n",addname);
      end_prog(-4);
    }

    ptrd=data;
    /* output only timeout data if there is no raw file. */
    if((fpraw=fopen(outname,"r"))==NULL) fwrite(data,1,data_num,fpadd);
    /* read raw data file */
    else{
      while(fread(&a,1,WIN_BLOCKSIZE_LEN,fpraw)==WIN_BLOCKSIZE_LEN){
	sizer=(WIN_bs)mkuint4((uint8_w *)&a);
	sizer-=WIN_BLOCKSIZE_LEN;
	if((datar=MALLOC(uint8_w,sizer))==NULL){
	  memory_error();
	}
	if(fread(datar,1,sizer,fpraw)!=sizer){
	  FREE(datar);
	  fwrite(ptrd,1,data_num-(WIN_bs)(ptrd-data),fpadd);
	  break; /* exit loop in case of raw file broken */
	}
	if(!bcd_dec(drtime,datar)){
	  FREE(datar);
	  continue; /* skip in case of strange time stamp in raw file */
	}
	/* output only raw data, if time stamp differ */
	if(time_cmp(dtime,drtime,6)){
	  fwrite(&a,1,WIN_BLOCKSIZE_LEN,fpadd);
	  fwrite(datar,1,sizer,fpadd);
	}
	/* In case of time stamp same */
	else{
	  size=(WIN_bs)mkuint4(ptrd)-WIN_BLOCKSIZE_LEN;
	  ptrd+=WIN_BLOCKSIZE_LEN;
	  if((datam=MALLOC(uint8_w,size))==NULL) memory_error();
	  sizem=get_merge_data(datam,datar,&sizer,ptrd,&size);
	  sizew=WIN_BLOCKSIZE_LEN+sizer+sizem;
	  size_arr[0]=sizew>>24;
	  size_arr[1]=sizew>>16;
	  size_arr[2]=sizew>>8;
	  size_arr[3]=sizew;
	  fwrite(size_arr,1,WIN_BLOCKSIZE_LEN,fpadd); /* write block_size */
	  fwrite(datar,1,sizer,fpadd); /* write raw data part */
	  fwrite(datam,1,sizem,fpadd); /* write add data part */
	  FREE(datam);
	  ptrd+=size;
	  if(ptrd<data+data_num)
	    bcd_dec(dtime,ptrd+WIN_BLOCKSIZE_LEN);
	  else
	    dtime[0]=-1;
#if DEBUG
	  fprintf(stderr,"dtime=%02d%02d%02d.%02d%02d%02d\n",
		  dtime[0],dtime[1],dtime[2],dtime[3],dtime[4],dtime[5]);
	  fflush(stderr);
#endif
	} /* if(time_cmp(dtime,drtime,6)) */
	FREE(datar);
      } /* while(fread(&a,1,WIN_BLOCKSIZE_LEN,fpraw)==WIN_BLOCKSIZE_LEN) */
      fclose(fpraw);
    } /* if((fpraw=fopen(outname,"r"))==NULL) */

    fclose(fpadd);
    sprintf(cmdbuf,"cp %s %s",addname,outname);
    system(cmdbuf);
    unlink(addname);
    FREE(data);
    i++;
  } /* while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN) (1) */

insert_end:
  fclose(fp);
}

int
main(int argc, char *argv[])
{
  FILE  *fp;
  struct Cnt_file  cnt;
  int  status;
  int  time_flag,bad_flag,wait_flag;
  int  tim[5],tim_end[5];
  int  tim_junk_oldest[5],tim_junk_latest[5];
  int  i;

  if((progname=strrchr(argv[0],'/'))) progname++;
  else progname=argv[0];

  if(argc<2){
    print_usage();
    exit(0);
  }
  if(argc>3) time_flag=1;
  else time_flag=0;

  printf("*****  %s start  *****\n",progname);

  /** read parameter file **/
  if((status=read_param(argv[1],&cnt))) end_prog(status);
#if DEBUG
  fprintf(stderr,"raw:  %s\njunk: %s\ntmp:  %s\n",
	  cnt.raw_dir,cnt.junk_dir,cnt.tmp_dir);
  fflush(stderr);
#endif

  /** set names of control files **/
  sprintf(cnt.junk_used,"%s/%s",cnt.junk_dir,INSERT_RAW_USED);
  sprintf(cnt.junk_latst,"%s/%s",cnt.junk_dir,WDISKT_LATEST);
  sprintf(cnt.junk_oldst,"%s/%s",cnt.junk_dir,WDISKT_OLDEST);
  sprintf(cnt.raw_latst,"%s/%s",cnt.raw_dir,WDISK_LATEST);
  sprintf(cnt.raw_oldst,"%s/%s",cnt.raw_dir,WDISK_OLDEST);
#if DEBUG
  fprintf(stderr,"used: %s\nlatst: %s\n",cnt.junk_used,cnt.junk_latst);
  fprintf(stderr,"oldest: %s\n",cnt.junk_oldst);
  fprintf(stderr,"out latest: %s\nout oldest: %s\n",
	  cnt.raw_latst,cnt.raw_oldst);
  fprintf(stderr,"wait time: %d [min.]\n",cnt.wait_min);
  fflush(stderr);
#endif

  /** set begin time **/
  if(time_flag){
    if(sscanf(argv[2],
	      "%2d%2d%2d%2d.%2d",&tim[0],&tim[1],&tim[2],&tim[3],&tim[4])<5){
      fprintf(stderr,"'%s' illegal time.\n",argv[2]);
      end_prog(-1);
    }
    if(sscanf(argv[3],"%2d%2d%2d%2d.%2d",
	      &tim_end[0],&tim_end[1],&tim_end[2],&tim_end[3],&tim_end[4])<5){
      fprintf(stderr,"'%s' illegal time.\n",argv[3]);
      end_prog(-1);
    }
    tim[4]--;
    /* adj_time_m(tim); */
    cnt.wait_min=0;
  }
  else{ /* if(time_flag) */
    do{
      /* read USED or LATEST file to get start time */
      if((fp=fopen(cnt.junk_used,"r"))==NULL){
	while((fp=fopen(cnt.junk_latst,"r"))==NULL){
	  fprintf(stderr,"'%s' not found. Waiting ...\n",cnt.junk_latst);
	  sleep(30);
	}
      }
      if(fscanf(fp,"%2d%2d%2d%2d.%2d",
		&tim[0],&tim[1],&tim[2],&tim[3],&tim[4])>=5) bad_flag=0;
      else{
	bad_flag=1;
	fprintf(stderr,"'%s' illegal. Waiting ...\n",cnt.junk_dir);
	sleep(30);
      }
      fclose(fp);
    } while(bad_flag);
  } /* if(time_flag) */
#if DEBUG
  fprintf(stderr,"begin: %02d%02d%02d%02d.%02d\n",
	  tim[0],tim[1],tim[2],tim[3],tim[4]);
  fflush(stderr);
#endif
  
  signal(SIGINT,end_prog);
  signal(SIGTERM,end_prog);

  while(1){
    tim[4]++;
    adj_time_m(tim);
#if DEBUG
    /*sleep(1);*/
    fprintf(stderr,"timeout data: %02d%02d%02d%02d.%02d\n",
	    tim[0],tim[1],tim[2],tim[3],tim[4]);
    fflush(stderr);
#endif
    if(time_flag){
      if(time_cmp(tim,tim_end,5)==1) end_prog(0);
    }
    /* compare with oldest */
    rmemo5(cnt.junk_oldst,tim_junk_oldest);
    if(time_cmp(tim,tim_junk_oldest,5)<0)
      for(i=0;i<5;++i) tim[i]=tim_junk_oldest[i];
    /* compare with latest */
    do{
      rmemo5(cnt.junk_latst,tim_junk_latest);
      /*fprintf(stderr,"%02d%02d%02d%02d.%02d-%02d%02d%02d%02d.%02d\n",
	      tim[0],tim[1],tim[2],tim[3],tim[4],
	      tim_junk_latest[0],tim_junk_latest[1],
	      tim_junk_latest[2],tim_junk_latest[3],tim_junk_latest[4]);*/
      if(time_cmp(tim,tim_junk_latest,5)<=0) wait_flag=0;
      else{
	wait_flag=1;
#if DEBUG
	fprintf(stderr,"Waiting junk LATEST:%02d%02d%02d%02d.%02d\n",
		tim[0],tim[1],tim[2],tim[3],tim[4]);
	fflush(stderr);
#endif
	sleep(5); /* wait new LATEST */
      }
    } while(wait_flag);
    
    do_insert(tim,&cnt);

    if(!time_flag && (status=wmemo5(cnt.junk_used,tim)))
      end_prog(status);

#ifdef GC_MEMORY_LEAK_TEST
    CHECK_LEAKS();
#endif
  }/* while(1) */
}
