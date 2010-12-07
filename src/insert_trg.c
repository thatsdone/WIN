/*
 * $Id: insert_trg.c,v 1.6.4.4.2.9 2010/12/07 04:58:33 uehira Exp $
 * Insert sorted timeout data to event data.
 *
 *------------ sample of parameter file ------------
 *|# This file is insert_trg's parameter file      |
 *|/dat/trg		# trg data dir             |
 *|/dat/raw_timeouts	# time-out data dir        |
 *|/dat/tmp		# temporary dir            |
 *|3                    # wait time[min].          |
 *|                     #   if blank, 0.           |
 *--------------------------------------------------
 *
 */

/*-
 * 2005/3/12   memory leak bug fixed.
 * 2007/9/12   bug fixed.
 * 2010/10/8   64bit check.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define DIRNAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define DIRNAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

#include "winlib.h"
/* #include "win_system.h" */

/*  #define DEBUG   0 */
#define DEBUG1  0

#define PRE_MIN  30  /* minute */
#define POST_MIN  2  /* minute */

#define TMP_ADD_NAME   "insert_trg.add"
#define DEFAULT_WAIT_MIN  POST_MIN   /* minute */

#define WIN_FILENAME_MAX 1024
#define BUF_SIZE 1024

static const char rcsid[]=
  "$Id: insert_trg.c,v 1.6.4.4.2.9 2010/12/07 04:58:33 uehira Exp $";

char *progname;

struct Cnt_file {
  char  trg_dir[WIN_FILENAME_MAX];    /* trg data directory */
  char  junk_dir[WIN_FILENAME_MAX];   /* time-out data directory */
  char  tmp_dir[WIN_FILENAME_MAX];    /* temporary directory */
  char  trg_latst[WIN_FILENAME_MAX];  /* trg LATEST */ 
  char  trg_oldst[WIN_FILENAME_MAX];  /* trg OLDEST */
  char  junk_latst[WIN_FILENAME_MAX]; /* time-out LATEST */
  char  junk_oldst[WIN_FILENAME_MAX]; /* time-out OLDEST */
  char  junk_used[WIN_FILENAME_MAX];  /* time-out USED */
  int   wait_min;       /* wait time[min.](>=POST_MIN) from timeout LATEST */
};

/* prototypes */
static void end_prog(int);
static void bfov_error(void);
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
bfov_error()
{

  fprintf(stderr,"'%s': Buffer overrun!\n",progname);
  end_prog(1);
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
  while(fgets(buf,BUF_SIZE,fp)!=NULL){
    if(buf[0]=='#') continue;  /* skip comment line */
    if(sscanf(buf,"%s",out)<1) break;
    if(count==0) strcpy(Cnt_file->trg_dir,out);
    else if(count==1) strcpy(Cnt_file->junk_dir,out);
    else if(count==2){
      strcpy(Cnt_file->tmp_dir,out);
      status=0;
    }
    else if(count==3){
      Cnt_file->wait_min=atoi(out);
      if(Cnt_file->wait_min<DEFAULT_WAIT_MIN){
	fprintf(stderr,"Warrning: default wait time set to %d minutes.\n",
		DEFAULT_WAIT_MIN);
	Cnt_file->wait_min=DEFAULT_WAIT_MIN;
      }
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
  FILE  *fp,*fptrg,*fpadd,*fpch;
  long  fpt;
  struct dirent  *dir_ent;
  DIR  *dir_ptr;
  static WIN_ch  trg_ch[WIN_CH_MAX_NUM],trg_chnum;
  WIN_bs  a;
  WIN_bs  size,size_save;
  WIN_bs  sizet;
  WIN_bs  sizem,sizew,datas_num;
  WIN_bs  sizes;
  WIN_bs  data_num,data_num_save;
  WIN_bs  array_size_of_data;
  uint8_w  *data,*datam,*datat,*tmpbuf,*datatmp;
  uint8_w  *datas;
  uint8_w  size_arr[WIN_BLOCKSIZE_LEN];
  uint8_w  *ptrd,*ptw,*ptrs;
  int  dtime[WIN_TIME_LEN],dtime_start[WIN_TIME_LEN],dtime_end[WIN_TIME_LEN];
  int  dttime[WIN_TIME_LEN],dstime[WIN_TIME_LEN];
  int  dtime_tmp[WIN_TIME_LEN];
  int  scan_tim_old[WIN_TIME_LEN],scan_tim_yng[WIN_TIME_LEN];
  int  /* tim_trg_oldest[WIN_TIME_LEN], */tim_trg[WIN_TIME_LEN];
  int  tim_trg_start[WIN_TIME_LEN],tim_trg_end[WIN_TIME_LEN];
  char  data_name[WIN_FILENAME_MAX];
  char  outname[WIN_FILENAME_MAX],chname[WIN_FILENAME_MAX];
  char  addname[WIN_FILENAME_MAX];
  char  cmdbuf[WIN_FILENAME_MAX];
  int  j;

  if (snprintf(data_name,sizeof(data_name),
	       "%s/%02d%02d%02d%02d.%02d", cnt->junk_dir,tim[0],
	       tim[1],tim[2],tim[3],tim[4]) >= sizeof(data_name))
    bfov_error();
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
      data = NULL;
      break; /* exit do_insert() in case of timeout file broken */
    }
    if(!bcd_dec(dtime,data+WIN_BLOCKSIZE_LEN)){
      FREE(data);
      data = NULL;
      continue; /* skip in case of strange time stamp */
    }
    for(j=0;j<WIN_TIME_LEN;++j) dtime_start[j]=dtime_end[j]=dtime[j];
    fpt=ftell(fp);
    while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN){  /*(2)*/
      size_save=size=(WIN_bs)mkuint4((uint8_w *)&a);
      if((tmpbuf=MALLOC(uint8_w,size))==NULL) memory_error();
      memcpy(tmpbuf,&a,WIN_BLOCKSIZE_LEN);
      size-=WIN_BLOCKSIZE_LEN;
      if(fread(tmpbuf+WIN_BLOCKSIZE_LEN,1,size,fp)!=size){
	FREE(tmpbuf); FREE(data);
	tmpbuf = data = NULL;
	goto insert_end; /* exit do_insert() in case of timeout file broken */
      }
      if(!bcd_dec(dtime_tmp,tmpbuf+WIN_BLOCKSIZE_LEN)){
	FREE(tmpbuf);
	tmpbuf = NULL;
	continue; /* skip in case of strange time stamp */
      }
      /* if next minutes, exit this loop */
      if(time_cmp(dtime,dtime_tmp,5)){
	FREE(tmpbuf);
	tmpbuf = NULL;
	fseek(fp,fpt,0);
	break;
      }
      data_num+=size_save;
      if (array_size_of_data < data_num) {
	array_size_of_data = data_num << 1;
	if((datatmp=REALLOC(uint8_w,data,array_size_of_data))==NULL)
	  memory_error();
	data=datatmp;
      }
      memcpy(data+data_num_save,tmpbuf,size_save);
      data_num_save=data_num;
      fpt=ftell(fp);
      if(time_cmp(dtime_tmp,dtime_end,WIN_TIME_LEN)>0)
	for(j=0;j<WIN_TIME_LEN;++j) dtime_end[j]=dtime_tmp[j];
      FREE(tmpbuf);
      tmpbuf = NULL;
    } /* while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN) (2) */

    for(j=0;j<5;++j) scan_tim_old[j]=scan_tim_yng[j]=dtime[j];
    scan_tim_old[5]=scan_tim_yng[5]=0;
    for(j=0;j<PRE_MIN;++j){
      scan_tim_old[4]--;
      adj_time_m(scan_tim_old);
    }
    for(j=0;j<POST_MIN;++j){
      scan_tim_yng[4]++;
      adj_time_m(scan_tim_yng);
    }
#if DEBUG1
    fprintf(stderr,
	    "packet %02d%02d%02d.%02d%02d%02d-%02d%02d%02d.%02d%02d%02d:  ",
	    dtime[0],dtime[1],dtime[2],dtime[3],dtime[4],dtime[5],
	    dtime_end[0],dtime_end[1],dtime_end[2],
	    dtime_end[3],dtime_end[4],dtime_end[5]);
    fprintf(stderr,"%02d%02d%02d.%02d%02d%02d <---> ",
	    scan_tim_old[0],scan_tim_old[1],scan_tim_old[2],
	    scan_tim_old[3],scan_tim_old[4],scan_tim_old[5]);
    fprintf(stderr,"%02d%02d%02d.%02d%02d%02d\n",
	    scan_tim_yng[0],scan_tim_yng[1],scan_tim_yng[2],
	    scan_tim_yng[3],scan_tim_yng[4],scan_tim_yng[5]);
    fflush(stderr);
#endif

    /* compare with oldest trg data */
    /* skip data which is older than OLDEST */
    /* rmemo6(cnt->trg_oldst,tim_trg_oldest);
    if(time_cmp(scan_tim_yng,tim_trg_oldest,WIN_TIME_LEN)<0){
      FREE(data);
      continue;
    } */

    /***** open trg dir & sweep all trg files *****/
    if((dir_ptr=opendir(cnt->trg_dir))==NULL){
      fprintf(stderr,"Cannot access trg directory: '%s'\n",cnt->trg_dir);
      end_prog(-1);
    }
    while((dir_ent=readdir(dir_ptr))!=NULL){
      if(DIRNAMLEN(dir_ent)!=13) continue;
      if(dir_ent->d_name[0]=='.') continue;
      if(!isdigit(dir_ent->d_name[0])) continue;
      /* skip *.ch file */
      if(!isdigit(dir_ent->d_name[strlen(dir_ent->d_name)-1])) continue;
      if(sscanf(dir_ent->d_name,"%2d%2d%2d.%2d%2d%2d",&tim_trg[0],
		&tim_trg[1],&tim_trg[2],&tim_trg[3],
		&tim_trg[4],&tim_trg[5])<WIN_TIME_LEN)
	continue;
      if(time_cmp(tim_trg,scan_tim_old,WIN_TIME_LEN)<0) continue;
      if(time_cmp(tim_trg,scan_tim_yng,WIN_TIME_LEN)>0) continue;
#if DEBUG1>3
      /* fprintf(stderr,"%s\n",dir_ent->d_name); */
#endif

      /* get start_time and end_time of 1 trg file. */
      if (snprintf(outname, sizeof(outname), "%s/%s",
		   cnt->trg_dir,dir_ent->d_name) >= sizeof(outname))
	bfov_error();
      if(WIN_time_hani(outname,tim_trg_start,tim_trg_end)) continue;
      if(time_cmp(dtime_end,tim_trg_start,WIN_TIME_LEN)<0) continue;
      if(time_cmp(dtime_start,tim_trg_end,WIN_TIME_LEN)>0) continue;
#if DEBUG1
      fprintf(stderr,
	      "  %s: %02d%02d%02d.%02d%02d%02d--%02d%02d%02d.%02d%02d%02d\n",
	      outname,tim_trg_start[0],tim_trg_start[1],tim_trg_start[2],
	      tim_trg_start[3],tim_trg_start[4],tim_trg_start[5],
	      tim_trg_end[0],tim_trg_end[1],tim_trg_end[2],
	      tim_trg_end[3],tim_trg_end[4],tim_trg_end[5]);
      fflush(stderr);
#endif

      /** read .ch file or trg file(first sec.) to get channel list **/
      if (snprintf(chname,sizeof(chname),"%s/%s%s",cnt->trg_dir,
		   dir_ent->d_name,TRG_CHFILE_SUFIX) >= sizeof(chname))
	bfov_error();
      trg_chnum=0;
      if((fpch=fopen(chname,"r"))==NULL){
	if((fptrg=fopen(outname,"r"))!=NULL){
	  if(fread(&a,1,WIN_BLOCKSIZE_LEN,fptrg)==WIN_BLOCKSIZE_LEN){
	    sizet=(WIN_bs)mkuint4((uint8_w *)&a)-WIN_BLOCKSIZE_LEN;
	    if((tmpbuf=MALLOC(uint8_w,sizet))==NULL) memory_error();
	    if(fread(tmpbuf,1,sizet,fptrg)==sizet){
	      trg_chnum=get_sysch_list(tmpbuf,sizet,trg_ch);
	    }
	    FREE(tmpbuf);
	    tmpbuf = NULL;
	  }
	  fclose(fptrg);
	}
      }
      else{
	trg_chnum=get_chlist_chfile(fpch,trg_ch);
	fclose(fpch);
      }
#if DEBUG1
      fprintf(stderr,"\ttrg_chnum=%d  [%X,%X]\n",trg_chnum,
	      trg_ch[trg_chnum-2],
	      trg_ch[trg_chnum-1]);
#endif
      if(trg_chnum==0) continue;

      /** read data[](timeout data) and select add data to datas[] **/
      ptrd=data;
      for(j=0;j<WIN_TIME_LEN;++j) dtime[j]=dtime_start[j];
      while(time_cmp(dtime,tim_trg_start,WIN_TIME_LEN)<0){  /* skip */
	size=(WIN_bs)mkuint4((uint8_w *)ptrd);
	ptrd+=size;
	bcd_dec(dtime,ptrd+WIN_BLOCKSIZE_LEN);
      }
      if(time_cmp(dtime,tim_trg_end,WIN_TIME_LEN)>0) continue;
      if((datas=MALLOC(uint8_w,data_num))==NULL) memory_error();
      ptw=datas;
      datas_num=0;
      while(time_cmp(dtime,tim_trg_end,WIN_TIME_LEN)<=0 && ptrd<data+data_num){
	size=(WIN_bs)mkuint4((uint8_w *)ptrd)-WIN_BLOCKSIZE_LEN;
	ptrd+=WIN_BLOCKSIZE_LEN;
#if DEBUG1>5
	fprintf(stderr,"size=%ld dtime=%02d%02d%02d.%02d%02d%02d\n",
		size,dtime[0],dtime[1],dtime[2],dtime[3],dtime[4],dtime[5]);
	fflush(stderr);
#endif
	sizes=get_select_data(ptw+WIN_BLOCKSIZE_LEN+WIN_TIME_LEN,
				trg_ch,trg_chnum,ptrd,size);
	if(sizes){
	  sizes+=WIN_BLOCKSIZE_LEN+WIN_TIME_LEN;
	  ptw[0]=sizes>>24;
	  ptw[1]=sizes>>16;
	  ptw[2]=sizes>>8;
	  ptw[3]=sizes;
	  memcpy(ptw+WIN_BLOCKSIZE_LEN,ptrd,WIN_TIME_LEN);
	  datas_num+=sizes;
	  ptw+=sizes;
	}
	ptrd+=size;
	if (ptrd>=data+data_num)  /****** double check ? *****/
	  break;
	bcd_dec(dtime,ptrd+WIN_BLOCKSIZE_LEN);
      } /* while(time_cmp(dtime,tim_trg_end,WIN_TIME_LEN)<=0 && ptrd<data+data_num) */
      if(datas_num==0){
	FREE(datas);
	datas = NULL;
	continue;  /* There is no selected data, go to next trg file */
      }
#if DEBUG1
      fprintf(stderr,"\t\t datas_num=%ld\n",datas_num);
      fflush(stderr);
#endif

      if((fptrg=fopen(outname,"r"))==NULL) continue;
      if (snprintf(addname, sizeof(addname),"%s/%s.%d",cnt->tmp_dir,
		   TMP_ADD_NAME,getpid()) >= sizeof(addname))
	bfov_error();
      if((fpadd=fopen(addname,"w"))==NULL){
	fprintf(stderr,"Cannot create file: '%s'\n",addname);
	end_prog(-4);
      }
      ptrs=datas;
      for(j=0;j<WIN_TIME_LEN;++j) dttime[j]=tim_trg_start[j];
      bcd_dec(dstime,datas+WIN_BLOCKSIZE_LEN);
#if DEBUG1>5
      fprintf(stderr,"aa dstime=%02d%02d%02d.%02d%02d%02d dttime=%02d%02d%02d.%02d%02d%02d\n",dstime[0],dstime[1],dstime[2],dstime[3],dstime[4],dstime[5],
dttime[0],dttime[1],dttime[2],dttime[3],dttime[4],dttime[5]);
      fflush(stderr);
#endif
      /* merge data */
      while(fread(&a,1,WIN_BLOCKSIZE_LEN,fptrg)==WIN_BLOCKSIZE_LEN){
	sizet=(WIN_bs)mkuint4((uint8_w *)&a)-WIN_BLOCKSIZE_LEN;
	if((datat=MALLOC(uint8_w,sizet))==NULL) memory_error();
	if(fread(datat,1,sizet,fptrg)!=sizet){
	  FREE(datat);
	  datat = NULL;
	  fwrite(ptrs,1,datas_num-(WIN_bs)(ptrs-datas),fpadd);
	  break;
	}
	if(!bcd_dec(dttime,datat)){
	  FREE(datat);
	  datat = NULL;
	  continue; /* skip in case of strange time stamp in trg file */
	}
	/* output only trg data, if time stamp differ */
#if DEBUG1>10
	fprintf(stderr,"dstime=%02d%02d%02d.%02d%02d%02d dttime=%02d%02d%02d.%02d%02d%02d\n",dstime[0],dstime[1],dstime[2],dstime[3],dstime[4],dstime[5],
dttime[0],dttime[1],dttime[2],dttime[3],dttime[4],dttime[5]);
	fflush(stderr);
#endif
	if(time_cmp(dstime,dttime,WIN_TIME_LEN)){
	  fwrite(&a,1,WIN_BLOCKSIZE_LEN,fpadd);
	  fwrite(datat,1,sizet,fpadd);
	}
	/* In case of time stamp same */
	else{
	  size=(WIN_bs)mkuint4(ptrs)-WIN_BLOCKSIZE_LEN;
	  ptrs+=WIN_BLOCKSIZE_LEN;
	  if((datam=MALLOC(uint8_w,size))==NULL) memory_error();
	  sizem=get_merge_data(datam,datat,&sizet,ptrs,&size);
#if DEBUG1>5
	  fprintf(stderr,"sizem=%ld  ",sizem);
	  fprintf(stderr,"dstime=%02d%02d%02d.%02d%02d%02d dttime=%02d%02d%02d.%02d%02d%02d\n",dstime[0],dstime[1],dstime[2],dstime[3],dstime[4],dstime[5],
dttime[0],dttime[1],dttime[2],dttime[3],dttime[4],dttime[5]);
	  fflush(stderr);
#endif
	  sizew=WIN_BLOCKSIZE_LEN+sizet+sizem;
	  size_arr[0]=sizew>>24;
	  size_arr[1]=sizew>>16;
	  size_arr[2]=sizew>>8;
	  size_arr[3]=sizew;
	  fwrite(size_arr,1,WIN_BLOCKSIZE_LEN,fpadd); /* write block_size */
	  fwrite(datat,1,sizet,fpadd); /* write trg data part */
	  fwrite(datam,1,sizem,fpadd); /* write add data part */
	  FREE(datam);
	  datam = NULL;	  
	  ptrs+=size;
	  if(ptrs<datas+datas_num)
	    bcd_dec(dstime,ptrs+WIN_BLOCKSIZE_LEN);
	  else
	    dstime[0]=-1;
	} /* if(time_cmp(dstime,dttime,WIN_TIME_LEN)) */
	FREE(datat);
	datat = NULL;
      } /* while(fread(&a,1,WIN_BLOCKSIZE_LEN,fptrg)==WIN_BLOCKSIZE_LEN) */
      fclose(fpadd);
      if (snprintf(cmdbuf, sizeof(cmdbuf),
		   "cp %s %s", addname, outname) >= sizeof(cmdbuf))
	bfov_error();
      system(cmdbuf);
      unlink(addname);
      fclose(fptrg);
      FREE(datas);
      datas = NULL;
    } /* while((dir_ent=readdir(dir_ptr))!=NULL) */
    closedir(dir_ptr);
    FREE(data);
    data = NULL;
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
  fprintf(stderr,"trg:  %s\njunk: %s\ntmp:  %s\n",
	  cnt.trg_dir,cnt.junk_dir,cnt.tmp_dir);
  fflush(stderr);
#endif

  /** set names of control files **/
  if (snprintf(cnt.junk_used, sizeof(cnt.junk_used), "%s/%s",
	       cnt.junk_dir, INSERT_TRG_USED) >= sizeof(cnt.junk_used))
    bfov_error();
  if (snprintf(cnt.junk_latst, sizeof(cnt.junk_latst), "%s/%s",
	       cnt.junk_dir, WDISKT_LATEST) >= sizeof(cnt.junk_latst))
    bfov_error();
  if (snprintf(cnt.junk_oldst, sizeof(cnt.junk_oldst), "%s/%s",
	       cnt.junk_dir, WDISKT_OLDEST) >= sizeof(cnt.junk_oldst))
    bfov_error();
  if (snprintf(cnt.trg_latst, sizeof(cnt.trg_latst), "%s/%s",
	       cnt.trg_dir, EVENTS_LATEST) >= sizeof(cnt.junk_oldst))
    bfov_error();
  if (snprintf(cnt.trg_oldst, sizeof(cnt.trg_oldst), "%s/%s",
	       cnt.trg_dir, EVENTS_OLDEST) >= sizeof(cnt.junk_oldst))
    bfov_error();
#if DEBUG
  fprintf(stderr,"used: %s\nlatst: %s\n",cnt.junk_used,cnt.junk_latst);
  fprintf(stderr,"oldest: %s\n",cnt.junk_oldst);
  fprintf(stderr,"out latest: %s\nout oldest: %s\n",
	  cnt.trg_latst,cnt.trg_oldst);
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

  for(;;){
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
      for(i=0;i<cnt.wait_min;++i){ /* set wait time */
	tim_junk_latest[4]--;
	adj_time_m(tim_junk_latest);
      }
      if(time_cmp(tim,tim_junk_latest,5)<=0) wait_flag=0;
      else{
	wait_flag=1;
#if DEBUG
	fprintf(stderr,"Waiting junk LATEST:%02d%02d%02d%02d.%02d + %d\n",
		tim[0],tim[1],tim[2],tim[3],tim[4],cnt.wait_min);
	fflush(stderr);
#endif
	sleep(5); /* wait junk new LATEST */
      }
    } while(wait_flag);
    
    do_insert(tim,&cnt);

    if(!time_flag && (status=wmemo5(cnt.junk_used,tim)))
      end_prog(status);

#ifdef GC_MEMORY_LEAK_TEST
    CHECK_LEAKS();
#endif
  }/* for(;;) */
}
