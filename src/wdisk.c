/* $Id: wdisk.c,v 1.7 2002/05/28 09:55:32 urabe Exp $ */
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
                      2002.3.2 eobsize_in(auto)
                      2002.5.2 i<1000 -> 1000000
                      2002.5.28 read from stdin
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
#define BUFSZ 500000
#define BUFLIM 1000000
   FILE *fp;
   int i,j,last_min,shmid,shid,tm[6],eobsize,eobsize_count,bufsiz;
   unsigned long shp,shp_save,size,c_save,size2;
   unsigned char *ptr,*ptr_save,ptw[4],*buf;
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
	      " usage : '%s [shm_key]/- [out dir] ([N of files] ([log file]))'\n",
	      progname);
      exit(0);
   }
   
   if(strcmp(argv[1],"-")) shmkey=atoi(argv[1]);
   else
     {
     shmkey=0;
     buf=(unsigned char *)malloc(bufsiz=BUFSZ);
     }
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
   if(shmkey)
     {
     if((shmid=shmget(shmkey,0,0))<0) err_sys("shmget");
     if((shm=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
       err_sys("shmat");
   
     sprintf(tbuf,"start, shm_key=%d sh=%d",shmkey,shm);
     write_log(logfile,tbuf);
     }
   
   signal(SIGPIPE,(void *)ctrlc);
   signal(SIGTERM,(void *)ctrlc);
   signal(SIGINT,(void *)ctrlc);
   
 reset:
   last_min=(-1);
   fd=NULL;

   if(shmkey)
     {
     while(shm->r==(-1)) sleep(1);
     shp=shp_save=shm->r;    /* read position */

     size=mklong(ptr_save=shm->d+shp);
     if(mklong(shm->d+shp+size-4)==size) eobsize=1;
     else eobsize=0;
     eobsize_count=eobsize;
     sprintf(tbuf,"eobsize=%d",eobsize);
     write_log(logfile,tbuf);
     }
   else
     {
     if(fread(buf,4,1,stdin)<1) ctrlc();
     size=mklong(buf);
     if(size>bufsiz)
       {
       if(size>BUFLIM || (buf=realloc(buf,size))==NULL)
         {
         sprintf(tbuf,"malloc size=%d",size);
         write_log(logfile,tbuf);
         ctrlc();
         }
       else bufsiz=size;
       }
     if(fread(buf+4,size-4,1,stdin)<1) ctrlc();
     }
 
   while(1){
      if(eobsize) size2=size-4;
      else size2=size;

      if(shmkey)
        {
        c_save=shm->c;
        ptr=shm->d+shp+4;
        }
      else ptr=buf+4;
      if(!bcd_dec(tm,ptr)) /* YMDhms */
        {
        sprintf(tbuf,
          "illegal time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X.%02X%02X%02X (%d)",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],
          ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11],size);
        write_log(logfile,tbuf);
        if(fd!=NULL) fclose(fd);
        if(shmkey) sleep(5);
        goto reset;
        }
 
      if(mode==60)
        {
        if(tm[3]!=last_min) switch_file(tm);
        last_min=tm[3];
        }
      else
        {
        if(tm[4]!=last_min) switch_file(tm);
        last_min=tm[4];
        }

#if DEBUG
      for(i=0;i<6;i++) printf("%02d",tm[i]);
      printf(" : %d r=%d\n",size,shp);
#endif
#if BELL
      printf("\007");fflush(stdout);
#endif
      ptw[0]=size2>>24;
      ptw[1]=size2>>16;
      ptw[2]=size2>>8;
      ptw[3]=size2;
      if(fwrite(ptw,1,4,fd)<4) write_log(logfile,"fwrite");
      if(fwrite(ptr,1,size2-4,fd)<size2-4) write_log(logfile,"fwrite");
      if(mode==60) fflush(fd);
	   
#if DEBUG
      printf("%d>(fd=%d) r=%d\n",size2,fd,shp);
#endif
      if(shmkey)
        {
        if((shp+=size)>shm->pl) shp=shp_save=0;
        while(shm->p==shp) sleep(1);
        i=shm->c-c_save;
        if(!(i<1000000 && i>=0) || mklong(ptr_save)!=size){
  	 write_log(logfile,"reset");
  	 goto reset;
        }

        size=mklong(ptr_save=shm->d+shp);
        if(size==mklong(ptr_save+size-4)) eobsize_count++;
        else eobsize_count=0;
        if(eobsize && eobsize_count==0) goto reset;
        if(!eobsize && eobsize_count>3) goto reset;
        }
      else
        {
        if(fread(buf,4,1,stdin)<1) ctrlc();
        size=mklong(buf);
        if(size>bufsiz)
          {
          if(size>BUFLIM || (buf=realloc(buf,size))==NULL)
            {
            sprintf(tbuf,"malloc size=%d",size);
            write_log(logfile,tbuf);
            ctrlc();
            }
          else bufsiz=size;
          }
        if(fread(buf+4,size-4,1,stdin)<1) ctrlc();
        }

   }
}
