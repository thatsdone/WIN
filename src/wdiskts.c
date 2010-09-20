/* $Id: wdiskts.c,v 1.6.2.5.2.8 2010/09/20 03:33:28 uehira Exp $ */

/*-
  2005.8.10 urabe bug in strcmp2() fixed : 0-6 > 7-9 
  2009.1.4  64bit clean? (Uehira)
  2010/1/12 bcd2time()-->bcd_t(), time2bcd()-->t_bcd()
  -*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else  /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif  /* !TIME_WITH_SYS_TIME */

#include <dirent.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <syslog.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

#include "daemon_mode.h"
#include "winlib.h"

/*  #define   DEBUG   0 */
#define   DEBUG1  0
#define   DEBUG3  0
#define   BELL    0

#define   NAMELEN  1025

/* memory malloc utility macro */
/* #define MALLOC(type, n) (type*)malloc((size_t)(sizeof(type)*(n))) */
/* #define REALLOC(type, ptr, n) \ */
/* (type*)realloc((void *)ptr, (size_t)(sizeof(type)*(n))) */
/* #define FREE(a)         (void)free((void *)(a)) */

static char rcsid[] =
  "$Id: wdiskts.c,v 1.6.2.5.2.8 2010/09/20 03:33:28 uehira Exp $";

char *progname,*logfile;
int  daemon_mode, syslog_mode, exit_status;

static char tbuf[NAMELEN],latest[NAMELEN],oldest[NAMELEN],busy[NAMELEN],
  outdir[NAMELEN];
static int count,count_max,mode;
static FILE *fd;
static uint8_w  *datbuf,*sortbuf,*datbuf_tmp;
static size_t  dat_num, sort_num, array_num_datbuf;
static jmp_buf  mx;

/* prototypes */
static int sort_buf(void);
static int switch_file(int *);
static void wmemo(char *, char *);
int main(int, char *[]);

static int
sort_buf()
{
   uint32_w  re,gsize;
   uint8_w  *ptr,*ptr_dat,tt[6],*ptw,*ptw_size;
   time_t  *tim_list,tim_tmp, *tim_sort;
   size_t  tim_num, tim_sfx, *sortin;
   WIN_ch   *ch_list,ch_num,ch_tmp,ch_sfx;
   int  status;
   size_t  i,j;
   WIN_bs  secbuf_len;
   struct data_index{
     int flag;
     size_t point;
     size_t len;
   } **indx;

   status=0;
   tim_num=(size_t)0;
   ch_num=(WIN_ch)0;
   tim_list=(time_t *)NULL;
   ch_list=(WIN_ch *)NULL;

   /** sweep buf **/
   ptr=datbuf;
   do{
     re=mkuint4(ptr);
     re-=4;
     ptr+=4;
     ptr_dat=ptr;
     for(i=0;i<6;++i) tt[i]=(*ptr++);
     tim_tmp=bcd_t(tt);
     for(i=0;i<tim_num;++i)
       if(tim_tmp==tim_list[i]) break;
     if(i==tim_num){ /* new time stamp */
       tim_num++;
       if((tim_list=REALLOC(time_t,tim_list,tim_num))==NULL){
	 status=1;
	 goto end_1;
       }
       tim_list[i]=tim_tmp;
     }
     /* read & compare channel number */
     while(ptr<ptr_dat+re){
       gsize=get_sysch(ptr,&ch_tmp);
       for(i=0;i<ch_num;++i)
	 if(ch_tmp==ch_list[i]) break;
       if(i==ch_num){ /* new channel */
	 ch_num++;
	 if((ch_list=REALLOC(WIN_ch,ch_list,ch_num))==NULL){
	   status=1;
	   goto end_1;
	 }
	 ch_list[i]=ch_tmp;
       }
       ptr+=gsize;
     } /* while(ptr<ptr_dat+re) */
   } while(ptr<datbuf+dat_num);
#if DEBUG1
   fprintf(stderr,"time_num=%ld ch_num=%d\n",tim_num,ch_num);
#endif

   /** make index **/
   if((indx=MALLOC(struct data_index *,ch_num))==NULL){
     status=1;
     goto end_1;
   }
   for(i=0;i<ch_num;++i){
     if((indx[i]=MALLOC(struct data_index,tim_num))==NULL){
       status=2;
       goto end_2;
     }
   }
   for(i=0;i<ch_num;++i)
     for(j=0;j<tim_num;++j)
       indx[i][j].flag=0; /* clear flag */
   ptr=datbuf;
   do{
     re=mkuint4(ptr);
     re-=4;
     ptr+=4;
     ptr_dat=ptr;
     for(i=0;i<6;++i) tt[i]=(*ptr++);
     tim_tmp=bcd_t(tt);
     for(i=0;i<tim_num;++i)
       if(tim_tmp==tim_list[i]) break;
     tim_sfx=i;
     while(ptr<ptr_dat+re){
       gsize=get_sysch(ptr,&ch_tmp);
       for(i=0;i<ch_num;++i)
	 if(ch_tmp==ch_list[i]) break;
       ch_sfx=i;
       /* make index */
       if(!indx[ch_sfx][tim_sfx].flag){
	 indx[ch_sfx][tim_sfx].flag=1;
	 indx[ch_sfx][tim_sfx].point=ptr-datbuf;
	 indx[ch_sfx][tim_sfx].len=gsize;
       }
       ptr+=gsize;
     } /* while(ptr<ptr_dat+re) */
   } while(ptr<datbuf+dat_num);

   /** sort by time **/
   if((tim_sort=MALLOC(time_t,tim_num))==NULL){
     status=3;
     goto end_3;
   }
   if((sortin=MALLOC(size_t,tim_num))==NULL){
     status=4;
     goto end_4;
   }
   for(i=0;i<tim_num;++i) tim_sort[i]=tim_list[i];
   qsort(tim_sort,tim_num,sizeof(time_t),time_cmpq);
   for(j=0;j<tim_num;++j){
     for(i=0;i<tim_num;++i){
       if(tim_sort[j]==tim_list[i]){
	 sortin[j]=i;
	 break;
       }
     }
   }

   sort_num=0;
   ptw=sortbuf;
   for(j=0;j<tim_num;++j){
     secbuf_len=10;
     sort_num+=10;
     ptw_size=ptw;
     ptw+=4;
     t_bcd(tim_list[sortin[j]],tt);
     memcpy(ptw,tt,6);
     ptw+=6;
     for(i=0;i<ch_num;++i){
       if(indx[i][sortin[j]].flag){
	 secbuf_len+=indx[i][sortin[j]].len;
	 sort_num+=indx[i][sortin[j]].len;
	 memcpy(ptw,datbuf+indx[i][sortin[j]].point,indx[i][sortin[j]].len);
	 ptw+=indx[i][sortin[j]].len;
       }
     } /* for(i=0;i<ch_num;++i) */
     ptw_size[0]=secbuf_len>>24;
     ptw_size[1]=secbuf_len>>16;
     ptw_size[2]=secbuf_len>>8;
     ptw_size[3]=secbuf_len;
   } /* for(j=0;j<tim_num;++j) */

   FREE(sortin);
end_4:
   FREE(tim_sort);
end_3:
   for(i=0;i<ch_num;++i) FREE(indx[i]);
end_2:
   FREE(indx);
end_1:
   FREE(tim_list); FREE(ch_list);
   /*if(status==1) write_log("realloc");*/
   return(status);
   }

static int
switch_file(int *tm)
{
   FILE *fp;
   char oldst[NAMELEN];

   if(fd!=NULL){  /* if file is open, close last file */
      if(datbuf!=NULL) {
	if((sortbuf=MALLOC(uint8_w,dat_num))==NULL){
	  FREE(datbuf);
	  write_log("malloc sort");
	  longjmp(mx,-1);  /* jump to reset */
	}
	/* sort *datbuf and output to *sortbuf */
	if(sort_buf()){
	  FREE(datbuf);
	  FREE(sortbuf);
	  write_log("sortbuf");
	  longjmp(mx,-1);  /* jump to reset */
	}
	/* write to disk */
	if(fwrite(sortbuf,1,sort_num,fd)!=sort_num){
	  FREE(datbuf);
	  FREE(sortbuf);
	  write_log("fwrite");
	  longjmp(mx,-1);  /* jump to reset */
	}
        FREE(datbuf);
	FREE(sortbuf);
        array_num_datbuf=dat_num=0;
	datbuf=NULL;
      }
      fclose(fd);
      fd=NULL;
      
#if DEBUG
      printf("closed fd=%p\n",fd);
#endif
      strcpy(latest,busy);
      wmemo(WDISKT_LATEST,latest);
   }
   /* delete oldest file */
   sprintf(tbuf,"%s/%s",outdir,WDISKT_MAX);
   if((fp=fopen(tbuf,"r")) != NULL){
      fscanf(fp,"%d",&count_max);
      fclose(fp);
      if(count_max>0 && count_max<3) count_max=3;
   }
   else count_max=0;
   
   while((count=find_oldest(outdir,oldst))>=count_max && count_max){
      sprintf(tbuf,"%s/%s",outdir,oldst);
      unlink(tbuf);
      count--;
#if DEBUG
      printf("%s deleted\n",tbuf);
#endif
   }
   strcpy(oldest,oldst);
   wmemo(WDISKT_OLDEST,oldest);
   
   /* make new file name */
   if(mode==60) sprintf(busy,"%02d%02d%02d%02d",tm[0],tm[1],tm[2],tm[3]);
   else sprintf(busy,"%02d%02d%02d%02d.%02d",tm[0],tm[1],tm[2],tm[3],tm[4]);
   sprintf(tbuf,"%s/%s",outdir,busy);
   /* open new file */
   if((fd=fopen(tbuf,"a+"))==NULL) err_sys(tbuf);
   count++;
#if DEBUG
   if(fd!=NULL) printf("%s opened fd=%p\n",tbuf,fd);
#endif
   wmemo(WDISKT_BUSY,busy);
   sprintf(tbuf,"%s/%s",outdir,WDISKT_COUNT);
   fp=fopen(tbuf,"w+");
   fprintf(fp,"%d\n",count);
   fclose(fp);
   return (0);
}

static void
wmemo(char *f, char *c)
{
   FILE *fp;

   sprintf(tbuf,"%s/%s",outdir,f);
   fp=fopen(tbuf,"w+");
   fprintf(fp,"%s\n",c);
   fclose(fp);
}

int
main(int argc, char *argv[])
{
   FILE *fp;
   int i,j,shmid,tm[6],tm_save[6];
   unsigned long c_save;   /* 64bit ok */
   size_t  shp;
   WIN_bs  size, size_save;
   uint8_w *ptr,size_out[4],*ptw;
   key_t shmkey;
   struct Shm  *shm;
   
   if((progname=strrchr(argv[0],'/')) != NULL) progname++;
   else progname=argv[0];

   daemon_mode = syslog_mode = 0;
   exit_status = EXIT_SUCCESS;
   if(strcmp(progname,"wdiskts60")==0) mode=60;
   else if(strcmp(progname,"wdiskts60d")==0) {mode=60;daemon_mode=1;}
   else if(strcmp(progname,"wdiskts")==0) mode=1;
   else if(strcmp(progname,"wdisktsd")==0) {mode=1;daemon_mode=1;}
   
   if(argc<3){
     WIN_version();
     fprintf(stderr, "%s\n", rcsid);
     fprintf(stderr,
	     " usage : '%s [shm_key] [out dir] ([N of files] ([log file]))'\n",
	     progname);
     exit(1);
   }
   
   shmkey=atol(argv[1]);
   strcpy(outdir,argv[2]);
   if(argc>3) count_max=atoi(argv[3]);
   else count_max=0;
   sprintf(tbuf,"%s/%s",outdir,WDISKT_MAX);
   fp=fopen(tbuf,"w+");
   fprintf(fp,"%d\n",count_max);
   fclose(fp);
   
   if(argc>4) logfile=argv[4];
   else
     {
       logfile=NULL;
       if (daemon_mode)
	 syslog_mode = 1;
     }

   /* daemon mode */
   if (daemon_mode) {
     daemon_init(progname, LOG_USER, syslog_mode);
     umask(022);
   }

   *latest=(*oldest)=(*busy)=0;
   if((shmid=shmget(shmkey,0,0))<0) err_sys("shmget");
   if((shm=(struct Shm *)shmat(shmid,(void *)0,0))==(struct Shm *)-1)
     err_sys("shmat");
   
   sprintf(tbuf,"start, shm_key=%ld sh=%p",shmkey,shm);
   write_log(tbuf);
   
   signal(SIGTERM,(void *)end_program);
   signal(SIGINT,(void *)end_program);
   
   setjmp(mx);
 reset:
   datbuf=NULL;
   array_num_datbuf = dat_num = 0;
   fd=NULL;
   while(shm->r==(-1)) sleep(1);
   shp=shm->r;    /* read position */
   for(i=0;i<6;i++) tm_save[i]=(-1);
   c_save=shm->c;
#if  DEBUG3
   printf("c_save=%lu\n",c_save);
#endif
   for(;;){
      get_time(tm); /* get system clock */
      if(mode==60) i=time_cmp(tm,tm_save,4);
      else i=time_cmp(tm,tm_save,5);
      if(i==-1){ /* system clock jump to past */
	sprintf(tbuf,
	"system clock %02d%02d%02d.%02d%02d%02d->%02d%02d%02d.%02d%02d%02d",
	tm_save[0],tm_save[1],tm_save[2],tm_save[3],tm_save[4],tm_save[5],
	tm[0],tm[1],tm[2],tm[3],tm[4],tm[5]);
	write_log(tbuf);
	goto reset;
      }
      else if(i==1){
	for(j=0;j<6;j++) tm_save[j]=tm[j];
	switch_file(tm);
      }
      while(shp!=shm->p){
	size_save=size=mkuint4(shm->d+shp);
	size-=4;
	size_out[0]=size>>24;
	size_out[1]=size>>16;
	size_out[2]=size>>8;
	size_out[3]=size;
#if DEBUG3
	printf("shm->c=%lu c_save=%lu\n",shm->c,c_save);
#endif
	if(shm->c<c_save){  /* check counter before output */
	  write_log("reset");
	  goto reset;
	}
	c_save=shm->c;
	dat_num+=size;
	if (array_num_datbuf < dat_num) {
	  array_num_datbuf = dat_num << 1;
	  datbuf_tmp=REALLOC(uint8_w,datbuf,array_num_datbuf);
	  if(datbuf_tmp==NULL){ 
	    FREE(datbuf);
	    write_log("realloc");
	    goto reset;
	  }
	  datbuf = datbuf_tmp;
	}
	ptw=datbuf+dat_num-size;
	ptr=&size_out[0];
	i=4; while((i--)>0) *ptw++=(*ptr++);
	size-=4;
	ptr=shm->d+shp+8;
#if DEBUG
	printf("size=%5d\t dat_num=%lu\t array_num_datbuf=%lu\n",
	       size,dat_num,array_num_datbuf);
#endif
	while((size--)>0) *ptw++=(*ptr++);
	shp+=size_save;
	if(shp>shm->pl) shp=0;
      } /* while(shp!=shm->p)  */
      sleep(1);
#ifdef GC_MEMORY_LEAK_TEST
      CHECK_LEAKS();
#endif
   }  /* for(;;) */
}
