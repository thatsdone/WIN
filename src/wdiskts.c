/* $Id: wdiskts.c,v 1.3 2004/12/03 14:07:11 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>

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

#include "daemon_mode.h"
#include "subst_func.h"

#define   DEBUG   0
#define   DEBUG1  0
#define   DEBUG3  0
#define   BELL    0

/* memory malloc utility macro */
#define MALLOC(type, n) (type*)malloc((size_t)(sizeof(type)*(n)))
#define REALLOC(type, ptr, n) \
(type*)realloc((void *)ptr, (size_t)(sizeof(type)*(n)))
#define FREE(a)         (void)free((void *)(a))

char tbuf[256],latest[20],oldest[20],busy[20],outdir[80];
char *progname,logfile[256];
int count,count_max,mode;
FILE *fd;
static unsigned char  *datbuf,*sortbuf;
unsigned long  dat_num,sort_num;
jmp_buf  mx;
int  daemon_mode, syslog_mode;

mklong(ptr)
  unsigned char *ptr;
  {   
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }   
        
write_log(logfil,ptr)
     char *logfil;
     char *ptr;
{
   FILE *fp;
   int tm[6];

   if (syslog_mode)
     {
       syslog(LOG_NOTICE, "%s", ptr);
     }
   else
     {
       if(*logfil) fp=fopen(logfil,"a");
       else fp=stdout;
       get_time(tm);
       fprintf(fp,"%02d%02d%02d.%02d%02d%02d %s %s\n",
	       tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
       if(*logfil) fclose(fp);
     }
}

ctrlc()
{
   if(fd!=NULL) fclose(fd);
   write_log(logfile,"end");
   if (syslog_mode)
     closelog();
   exit(0);
}

err_sys(ptr)
     char *ptr;
{

  if (syslog_mode)
    {
      syslog(LOG_NOTICE, "%s", ptr);
    }
  else
    {
      perror(ptr);
      write_log(logfile,ptr);
    }
  if(strerror(errno)) write_log(logfile,strerror(errno));
  ctrlc();
}

adj_time_m(tm)
     int *tm;
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
	       if(tm[0]%4==0)
		 tm[2]=29;
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

get_time(rt)
     int *rt;
{
   struct tm *nt;
   long ltime;
   time(&ltime);
   nt=localtime(&ltime);
   rt[0]=nt->tm_year%100;
   rt[1]=nt->tm_mon+1;
   rt[2]=nt->tm_mday;
   rt[3]=nt->tm_hour;
   rt[4]=nt->tm_min;
   rt[5]=nt->tm_sec;
}

static int
bcd_dec(dest,sour)
     unsigned char *sour;
     int *dest;
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
  if(i>=0 && i<=99) dest[0]=i; else return 0;
  i=b2d[sour[1]];
  if(i>=1 && i<=12) dest[1]=i; else return 0;
  i=b2d[sour[2]];
  if(i>=1 && i<=31) dest[2]=i; else return 0;
  i=b2d[sour[3]];
  if(i>=0 && i<=23) dest[3]=i; else return 0;
  i=b2d[sour[4]];
  if(i>=0 && i<=59) dest[4]=i; else return 0;
  i=b2d[sour[5]];
  if(i>=0 && i<=60) dest[5]=i; else return 0;
  return 1;
}

static void
dec_bcd(dest,sour)
     unsigned int *sour;
     unsigned char *dest;
{
   int cntr;
   for(cntr=0;cntr<6;cntr++)
      dest[cntr]=(((sour[cntr]/10)<<4)&0xf0)|(sour[cntr]%10&0xf);
}

static time_t
bcd2time(bcd)
     unsigned char *bcd;
{
   unsigned int  t[6];
   struct tm     tim_str;
   time_t        tim;

   bcd_dec(t,bcd);
   if(t[0]>=70)
      tim_str.tm_year=t[0];
   else
      tim_str.tm_year=100+t[0]; /* 2000+t[0]-1900 */
   tim_str.tm_mon=t[1]-1;
   tim_str.tm_mday=t[2];
   tim_str.tm_hour=t[3];
   tim_str.tm_min=t[4];
   tim_str.tm_sec=t[5];
   tim_str.tm_isdst=0;
   if((tim=mktime(&tim_str))==(time_t)-1){
      fputs("mktime error.\n",stderr);
      exit(1);
   }
   return(tim);
}

static void
time2bcd(time,bcd)
     time_t         time;
     unsigned char  *bcd;
{
   unsigned int t[6];
   struct tm    time_str;

   time_str=*localtime(&time);
   if(time_str.tm_year>=100)
      t[0]=time_str.tm_year-100;
   else
      t[0]=time_str.tm_year;
   t[1]=time_str.tm_mon+1;
   t[2]=time_str.tm_mday;
   t[3]=time_str.tm_hour;
   t[4]=time_str.tm_min;
   t[5]=time_str.tm_sec;
   dec_bcd(bcd,t);
}

static unsigned long
get_sysch(unsigned char *buf, unsigned short *ch)
{
  unsigned char  gh[4];
  unsigned long  sr;
  unsigned long  gsize;
  int  i;
  
  for(i=0;i<4;++i) gh[i]=buf[i];
  /* channel number */
  *ch=(((unsigned short)gh[0])<<8)+(unsigned short)gh[1];
  /* sampling rate */
  sr=(((unsigned long)(gh[2]&0x0f))<<8)+(unsigned long)gh[3];
  /* sample size */
  if((gh[2]>>4)&0x7) gsize=((gh[2]>>4)&0x7)*(sr-1)+8;
  else gsize=(sr>>1)+8;

  return(gsize);
}

static int
time_cmpq(a,b)
     unsigned long  *a,*b;
{
  if(*a<*b) return(-1);
  else if(*a==*b) return(0);
  else return(1);
}

static int
sort_buf()
{
   unsigned long  re,gsize;
   unsigned char  *ptr,*ptr_dat,tt[6],*ptw,*ptw_size;
   unsigned long  *tim_list,tim_num,tim_tmp,tim_sfx,*tim_sort,*sortin;
   unsigned short *ch_list,ch_num,ch_tmp,ch_sfx;
   int  status;
   unsigned long  i,j,secbuf_len;
   struct data_index{
     int flag;
     unsigned long point;
     unsigned long len;
   } **indx;

   status=0;
   tim_num=(unsigned long)0;
   ch_num=(unsigned short)0;
   tim_list=(unsigned long *)NULL;
   ch_list=(unsigned short *)NULL;

   /** sweep buf **/
   ptr=datbuf;
   do{
     re=mklong(ptr);
     re-=4;
     ptr+=4;
     ptr_dat=ptr;
     for(i=0;i<6;++i) tt[i]=(*ptr++);
     tim_tmp=bcd2time(tt);
     for(i=0;i<tim_num;++i)
       if(tim_tmp==tim_list[i]) break;
     if(i==tim_num){ /* new time stamp */
       tim_num++;
       if((tim_list=REALLOC(unsigned long,tim_list,tim_num))==NULL){
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
	 if((ch_list=REALLOC(unsigned short,ch_list,ch_num))==NULL){
	   status=1;
	   goto end_1;
	 }
	 ch_list[i]=ch_tmp;
       }
       ptr+=gsize;
     } /* while(ptr<ptr_dat+re) */
   } while(ptr<datbuf+dat_num);
#if DEBUG1
   fprintf(stderr,"time_num=%d ch_num=%d\n",tim_num,ch_num);
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
     re=mklong(ptr);
     re-=4;
     ptr+=4;
     ptr_dat=ptr;
     for(i=0;i<6;++i) tt[i]=(*ptr++);
     tim_tmp=bcd2time(tt);
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
   if((tim_sort=MALLOC(unsigned long,tim_num))==NULL){
     status=3;
     goto end_3;
   }
   if((sortin=MALLOC(unsigned long,tim_num))==NULL){
     status=4;
     goto end_4;
   }
   for(i=0;i<tim_num;++i) tim_sort[i]=tim_list[i];
   qsort(tim_sort,tim_num,sizeof(unsigned long),time_cmpq);
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
     time2bcd(tim_list[sortin[j]],tt);
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
   /*if(status==1) write_log(logfile,"realloc");*/
   return(status);
   }

switch_file(tm)
  int *tm;
{
   FILE *fp;
   char oldst[20];

   if(fd!=NULL){  /* if file is open, close last file */
      if(datbuf!=NULL) {
	if((sortbuf=MALLOC(unsigned char,dat_num))==NULL){
	  FREE(datbuf);
	  write_log(logfile,"malloc sort");
	  longjmp(mx,-1);  /* jump to reset */
	}
	/* sort *datbuf and output to *sortbuf */
	if(sort_buf()){
	  FREE(datbuf);
	  FREE(sortbuf);
	  write_log(logfile,"sortbuf");
	  longjmp(mx,-1);  /* jump to reset */
	}
	/* write to disk */
	if(fwrite(sortbuf,1,sort_num,fd)!=sort_num){
	  FREE(datbuf);
	  FREE(sortbuf);
	  write_log(logfile,"fwrite");
	  longjmp(mx,-1);  /* jump to reset */
	}
        FREE(datbuf);
	FREE(sortbuf);
        dat_num=0;
	datbuf=NULL;
      }
      fclose(fd);
      fd=NULL;
      
#if DEBUG
      printf("closed fd=%d\n",fd);
#endif
      strcpy(latest,busy);
      wmemo("LATEST",latest);
   }
   /* delete oldest file */
   sprintf(tbuf,"%s/MAX",outdir);
   if(fp=fopen(tbuf,"r")){
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
   wmemo("OLDEST",oldest);
   
   /* make new file name */
   if(mode==60) sprintf(busy,"%02d%02d%02d%02d",tm[0],tm[1],tm[2],tm[3]);
   else sprintf(busy,"%02d%02d%02d%02d.%02d",tm[0],tm[1],tm[2],tm[3],tm[4]);
   sprintf(tbuf,"%s/%s",outdir,busy);
   /* open new file */
   if((fd=fopen(tbuf,"a+"))==NULL) err_sys(tbuf);
   count++;
#if DEBUG
   if(fd!=NULL) printf("%s opened fd=%d\n",tbuf,fd);
#endif
   wmemo("BUSY",busy);
   sprintf(tbuf,"%s/COUNT",outdir);
   fp=fopen(tbuf,"w+");
   fprintf(fp,"%d\n",count);
   fclose(fp);
   return 0;
}

strcmp2(s1,s2)
char *s1,*s2;
{
  if(*s1=='0' && *s2=='9') return 1;
  else if(*s1=='9' && *s2=='0') return -1;
  else return strcmp(s1,s2);
}

find_oldest(path,oldst) /* returns N of files */
     char *path,*oldst;
{
   int i;
   struct dirent *dir_ent;
   DIR *dir_ptr;
   /* find the oldest file */
   if((dir_ptr=opendir(path))==NULL) err_sys("opendir");
   i=0;
   while((dir_ent=readdir(dir_ptr))!=NULL){
      if(*dir_ent->d_name=='.') continue;
      if(!isdigit(*dir_ent->d_name)) continue;
      if(i++==0 || strcmp2(dir_ent->d_name,oldst)<0)
	strcpy(oldst,dir_ent->d_name);
   }
#if DEBUG
   printf("%d files in %s, oldest=%s\n",i,path,oldst);
#endif
   closedir(dir_ptr);
   return i;
}

wmemo(f,c)
     char *f,*c;
{
   FILE *fp;
   sprintf(tbuf,"%s/%s",outdir,f);
   fp=fopen(tbuf,"w+");
   fprintf(fp,"%s\n",c);
   fclose(fp);
}

main(argc,argv)
     int argc;
     char **argv;
{
   FILE *fp;
   int i,j,shmid,tm[6],tm_save[6];
   unsigned long shp,size,size_save,c_save;
   unsigned char *ptr,size_out[4],*ptw;
   key_t shmkey;
   struct Shm {
      unsigned long p;    /* write point */
      unsigned long pl;   /* write limit */
      unsigned long r;    /* latest */
      unsigned long c;    /* counter */
      unsigned char d[1];   /* data buffer */
   } *shm;
   
   if(progname=strrchr(argv[0],'/')) progname++;
   else progname=argv[0];

   daemon_mode = syslog_mode = 0;
   if(strcmp(progname,"wdiskts60")==0) mode=60;
   else if(strcmp(progname,"wdiskts60d")==0) {mode=60;daemon_mode=1;}
   else if(strcmp(progname,"wdiskts")==0) mode=1;
   else if(strcmp(progname,"wdisktsd")==0) {mode=1;daemon_mode=1;}
   
   if(argc<3){
      fprintf(stderr,
      " usage : '%s [shm_key] [out dir] ([N of files] ([log file]))'\n",
	      progname);
      exit(0);
   }
   
   shmkey=atoi(argv[1]);
   strcpy(outdir,argv[2]);
   if(argc>3) count_max=atoi(argv[3]);
   else count_max=0;
   sprintf(tbuf,"%s/MAX",outdir);
   fp=fopen(tbuf,"w+");
   fprintf(fp,"%d\n",count_max);
   fclose(fp);
   
   if(argc>4) strcpy(logfile,argv[4]);
   else
     {
       *logfile=0;
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
   if((shm=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
     err_sys("shmat");
   
   sprintf(tbuf,"start, shm_key=%d sh=%d",shmkey,shm);
   write_log(logfile,tbuf);
   
   signal(SIGTERM,(void *)ctrlc);
   signal(SIGINT,(void *)ctrlc);
   
   setjmp(mx);
 reset:
   datbuf=NULL;
   dat_num=0;
   fd=NULL;
   while(shm->r==(-1)) sleep(1);
   shp=shm->r;    /* read position */
   for(i=0;i<6;i++) tm_save[i]=(-1);
   c_save=shm->c;
#if  DEBUG3
   printf("c_save=%d\n",c_save);
#endif
   while(1){
      get_time(tm); /* get system clock */
      if(mode==60) i=time_cmp(tm,tm_save,4);
      else i=time_cmp(tm,tm_save,5);
      if(i==-1){ /* system clock jump to past */
	sprintf(tbuf,
	"system clock %02d%02d%02d.%02d%02d%02d->%02d%02d%02d.%02d%02d%02d",
	tm_save[0],tm_save[1],tm_save[2],tm_save[3],tm_save[4],tm_save[5],
	tm[0],tm[1],tm[2],tm[3],tm[4],tm[5]);
	write_log(logfile,tbuf);
	goto reset;
      }
      else if(i==1){
	for(j=0;j<6;j++) tm_save[j]=tm[j];
	switch_file(tm);
      }
      while(shp!=shm->p){
	size_save=size=mklong(shm->d+shp);
	size-=4;
	size_out[0]=size>>24;
	size_out[1]=size>>16;
	size_out[2]=size>>8;
	size_out[3]=size;
#if DEBUG3
	printf("shm->c=%d c_save=%d\n",shm->c,c_save);
#endif
	if(shm->c<c_save){  /* check counter before output */
	  write_log(logfile,"reset");
	  goto reset;
	}
	c_save=shm->c;
	dat_num+=size;
	if((datbuf=REALLOC(unsigned char,datbuf,dat_num))==NULL){
	  write_log(logfile,"realloc");
	  goto reset;
	}
	ptw=datbuf+dat_num-size;
	ptr=&size_out[0];
	i=4; while((i--)>0) *ptw++=(*ptr++);
	size-=4;
	ptr=shm->d+shp+8;
#if DEBUG
	printf("size=%d dat_num=%d\n",size,dat_num);
#endif
	while((size--)>0) *ptw++=(*ptr++);
	shp+=size_save;
	if(shp>shm->pl) shp=0;
      } /* while(shp!=shm->p)  */
      sleep(1);
   }  /* while(1) */
}
