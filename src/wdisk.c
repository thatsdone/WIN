/* $Id: wdisk.c,v 1.4 2002/01/13 06:57:52 uehira Exp $ */
/*
  program "wdisk.c"   4/16/93-5/13/93,7/2/93,7/5/94  urabe
                      1/6/95 bug in adj_time fixed (tm[0]--)
                      3/17/95 write_log()
                      5/20/96 wdisk60
                      5/24/96 check_time +/-30min
                      5/28/96 usleep -> sleep
		      8/27/96 LITTLE ENDIAN (uehira)
		      98.5.19 sleep() in illegal time
		      98.6.26 strcmp2() for year of 2000
                      99.4.19 byte-order-free
                      2000.4.24 strerror()
*/

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

#include "subst_func.h"

#define   DEBUG   0
#define   BELL    0

char tbuf[256],latest[20],oldest[20],busy[20],outdir[80];
char *progname,logfile[256];
unsigned char *buf;
int count,count_max,mode;
FILE *fd;

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
   if(*logfil) fp=fopen(logfil,"a");
   else fp=stdout;
   get_time(tm);
   fprintf(fp,"%02d%02d%02d.%02d%02d%02d %s %s\n",
	   tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
   if(*logfil) fclose(fp);
}

ctrlc()
{
   if(fd!=NULL) fclose(fd);
   write_log(logfile,"end");
   exit(0);
}

err_sys(ptr)
     char *ptr;
{
   perror(ptr);
   write_log(logfile,ptr);
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

check_time(tm)
     int *tm;
{
   static int tm_prev[6],flag;
   int rt1[6],rt2[6],i,j;
   
   if(flag && time_cmp(tm,tm_prev,5)==0) return 0;
   else flag=0;
   
   /* compare time with real time */
   get_time(rt1);
   for(i=0;i<5;i++) rt2[i]=rt1[i];
   for(i=0;i<30;i++){  /* within 30 minutes ? */
      if(time_cmp(tm,rt1,5)==0 || time_cmp(tm,rt2,5)==0){
	 for(j=0;j<5;j++) tm_prev[j]=tm[j];
	 flag=1;
#if DEBUG2
	 printf("diff=%d m\n",i);
#endif
	 return 0;
      }
      rt1[4]++;
      adj_time_m(rt1);
      rt2[4]--;
      adj_time_m(rt2);
   }
#if DEBUG2
   printf("diff>%d m\n",i);
#endif
   return 1; /* illegal time */
}

bcd_dec(dest,sour)
     char *sour;
     int *dest;
{
   int cntr;
  for(cntr=0;cntr<6;cntr++)
    dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf);
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

switch_file(tm)
  int *tm;
{
   FILE *fp;
   char oldst[20];
   if(fd!=NULL){  /* if file is open, close last file */
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
   int i,j,last_min,shmid,shid,tm[6];
   unsigned long shp,shp_save,shp_prev,pl[3],size,c_save,tmp;
   unsigned char *ptr,*ptr_save;
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
   if(strcmp(progname,"wdisk60")==0) mode=60;
   else mode=1;
   
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
   else *logfile=0;

   *latest=(*oldest)=(*busy)=0;
   if((shmid=shmget(shmkey,0,0))<0) err_sys("shmget");
   if((shm=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
     err_sys("shmat");
   
   sprintf(tbuf,"start, shm_key=%d sh=%d",shmkey,shm);
   write_log(logfile,tbuf);
   
   signal(SIGTERM,(void *)ctrlc);
   signal(SIGINT,(void *)ctrlc);
   
 reset:
   last_min=(-1);
   fd=NULL;
   while(shm->r==(-1)) sleep(1);
   shp=shp_save=shm->r;    /* read position */
   for(i=0;i<3;i++) pl[i]=shm->pl*(i+1)/3;
   
   while(1){
      size=mklong(ptr_save=shm->d+shp);
      c_save=shm->c;
      bcd_dec(tm,shm->d+shp+4); /* YMDhms */
      if(check_time(tm)){
	 sprintf(tbuf,
	 "illegal time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X.%02X%02X%02X (%d)",
	 shm->d[shp+4],shm->d[shp+5],shm->d[shp+6],
	 shm->d[shp+7],shm->d[shp+8],shm->d[shp+9],
	 shm->d[shp+10],shm->d[shp+11],shm->d[shp+12],
	 shm->d[shp+13],shm->d[shp+14],shm->d[shp+15],size);
	 write_log(logfile,tbuf);
	 if(fd!=NULL) fclose(fd);
         sleep(5);
	 goto reset;
      }
      
      if(mode==60) i=(tm[3]!=last_min);
      else i=(tm[4]!=last_min);
      
      if(i){
	 if(shp-shp_save>0){
	    if(fwrite(shm->d+shp_save,1,shp-shp_save,fd)<shp-shp_save)
	      write_log(logfile,"fwrite");
	    if(mode==60) fflush(fd);
	    
#if DEBUG
	    printf("%d>(fd=%d) r=%d\n",shp-shp_save,fd,shp_save);
#endif
	    shp_save=shp;
	 }
	 switch_file(tm);
      }
      if(mode==60) last_min=tm[3];
      else last_min=tm[4];
#if DEBUG
      for(i=0;i<6;i++) printf("%02d",tm[i]);
      printf(" : %d r=%d\n",size,shp);
#endif
#if BELL
      printf("\007");fflush(stdout);
#endif
      shp_prev=shp;
      shp+=size;
      for(i=0;i<3;i++)
	if(shp_prev<=pl[i] && shp>pl[i]){
	   if(fwrite(shm->d+shp_save,1,shp-shp_save,fd)<shp-shp_save)
	     write_log(logfile,"fwrite");
	   if(mode==60) fflush(fd);
	   
#if DEBUG
	   printf("%d>(fd=%d) r=%d\n",shp-shp_save,fd,shp_save);
#endif
	   shp_save=shp;
	   break;
	}
      if(shp>shm->pl) shp=shp_save=0;
      while(shm->p==shp) sleep(1);
      tmp=mklong(ptr_save);
      if(shm->c<c_save || tmp!=size){
	 write_log(logfile,"reset");
	 goto reset;
      }
   }
}
