/* $Id: wdisk.c,v 1.16.2.5 2010/10/12 13:08:56 uehira Exp $ */
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
                      2004.8.1 added option -n (uehira)
                      2004.9.4 added option -s (uehira)
                      2004.10.4 daemon mode (-D) (uehira)
                      2004.12.15 bug fixed. - use tm_min[i] instead of last_min
                      2005.8.10 bug in strcmp2() fixed : 0-6 > 7-9
                      2006.12.5 use double in calculating space_raw
                      2007.1.15 i<1000000 -> 5000000,
                                BUFSZ 500000->1000000, BUFLIM 1000000->5000000
		      2010.09.27 check struct statfs.f_bavail size. 
		                 check __SVR4 and __NetBSD__ (uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define USE_LARGE_FS 0
#ifdef USE_LARGE_FS
#define _LARGEFILE64_SOURCE
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
#include <unistd.h>
#include <syslog.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#if !defined(BSD4_4) && defined(HAVE_SYS_VFS_H)
#include <sys/vfs.h>
#endif

#if defined(__SVR4) || defined(__NetBSD__)
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#define statfs statvfs
#define f_bsize f_frsize
#endif

#include "daemon_mode.h"
#include "subst_func.h"

#define   DEBUG   0
#define   BELL    0

#define   NAMELEN  1025

char tbuf[NAMELEN],latest[NAMELEN],oldest[NAMELEN],busy[NAMELEN],
  outdir[NAMELEN];
char *progname,logfile[NAMELEN];
unsigned char *buf;
int count,count_max,mode;
FILE *fd;
int  nflag, smode, daemon_mode, syslog_mode;

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
    syslog(LOG_NOTICE, "%s", ptr);
  else
    {
      perror(ptr);
      write_log(logfile,ptr);
    }
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
   time_t ltime;
   time(&ltime);
   nt=localtime(&ltime);
   rt[0]=nt->tm_year%100;
   rt[1]=nt->tm_mon+1;
   rt[2]=nt->tm_mday;
   rt[3]=nt->tm_hour;
   rt[4]=nt->tm_min;
   rt[5]=nt->tm_sec;
}

#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
long
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
int64_t
#endif
check_space(path,fsbsize)
     char *path;
#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
     long *fsbsize;
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
     int64_t *fsbsize;
#endif
{
#if USE_LARGE_FS
  struct statfs64 fsbuf;
#else
  struct statfs fsbuf;
#endif
  char  path1[NAMELEN];

  if (snprintf(path1, sizeof(path1), "%s/.", path) >= sizeof(path1)) {
    write_log(logfile, "path1[]: buffer overflow");
    ctrlc();
  }
#if USE_LARGE_FS
  if (statfs64(path1, &fsbuf) < 0)
#else
  if (statfs(path1, &fsbuf) < 0)
#endif
    err_sys("statfs");

  *fsbsize = fsbuf.f_bsize;

  return (fsbuf.f_bavail);
}

switch_file(tm)
  int *tm;
{
   FILE *fp;
   char oldst[NAMELEN];
#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
   long freeb, fsbsize, space_raw;
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
   int64_t freeb, fsbsize, space_raw;
#endif

   if(fd!=NULL){  /* if file is open, close last file */
      fclose(fd);
      fd=NULL;
#if DEBUG
      printf("closed fd=%d\n",fd);
#endif
      if (!nflag) {
	strcpy(latest,busy);
	wmemo("LATEST",latest);
      }
   }

   /* delete oldest file */
   if (smode) {
     if (snprintf(tbuf,sizeof(tbuf),"%s/MAX",outdir) >= sizeof(tbuf)) {
       write_log(logfile, "tbuf[]: buffer overflow");
       ctrlc();
     }
     if(fp=fopen(tbuf,"r")){
       fscanf(fp,"%d",&count_max);
       fclose(fp);
     }
     else count_max=0;

     for (;;) {
       freeb = check_space(outdir, &fsbsize);
#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
       space_raw =(long)((1048576.0*(double)count_max) / (double)fsbsize);
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
       space_raw =(int64_t)((1048576.0*(double)count_max) / (double)fsbsize);
#endif
       count = find_oldest(outdir,oldst);
#if DEBUG
#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
       printf("freeb, space_raw: %ld %ld (%ld)\n",
	      freeb, space_raw, fsbsize);
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
       printf("freeb, space_raw: %lld %lld (%lld)\n",
	      freeb, space_raw, fsbsize);
#endif
#endif
       if (space_raw < freeb || count == 0)
	 break;
       if (snprintf(tbuf, sizeof(tbuf), "%s/%s", outdir, oldst)
	   >= sizeof(tbuf)) {
	 write_log(logfile, "tbuf[]: buffer overflow");
	 ctrlc();
       }
       unlink(tbuf);
       sync();
       sync();
       sync();
       count--;
     }

     if (!nflag) {
       strcpy(oldest,oldst);
       wmemo("OLDEST",oldest);
     }
   } else {
     if (snprintf(tbuf,sizeof(tbuf),"%s/MAX",outdir) >= sizeof(tbuf)) {
       write_log(logfile, "tbuf[]: buffer overflow");
       ctrlc();
     }
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
     if (!nflag) {
       strcpy(oldest,oldst);
       wmemo("OLDEST",oldest);
     }
   }
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
   if (!nflag) {
     wmemo("BUSY",busy);
     sprintf(tbuf,"%s/COUNT",outdir);
     fp=fopen(tbuf,"w+");
     fprintf(fp,"%d\n",count);
     fclose(fp);
   }
   return 0;
}

strcmp2(s1,s2)
char *s1,*s2;
{
  if((*s1>='0' && *s1<='5') && (*s2<='9' && *s2>='6')) return 1;
  else if((*s1<='9' && *s1>='7') && (*s2>='0' && *s2<='6')) return -1;
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

usage()
{

  if(daemon_mode)
    fprintf(stderr,
	    " usage : '%s (-ns) [shm_key]/- [out dir] ([N of files]/[freespace in MB(-s)] ([log file]))'\n",
	    progname);
  else
    fprintf(stderr,
	    " usage : '%s (-Dns) [shm_key]/- [out dir] ([N of files]/[freespace in MB(-s)] ([log file]))'\n",
	    progname);
}

main(argc,argv)
     int argc;
     char **argv;
{
#define BUFSZ 1000000
#define BUFLIM 5000000
   FILE *fp;
   int i,j,shmid,shid,tm[6],tm_last[6],eobsize,eobsize_count,bufsiz;
   unsigned long shp,shp_save,size,c_save,size2;
   unsigned char *ptr,*ptr_save,ptw[4],*buf;
   key_t shmkey;
   int c;
   struct Shm {
      unsigned long p;    /* write point */
      unsigned long pl;   /* write limit */
      unsigned long r;    /* latest */
      unsigned long c;    /* counter */
      unsigned char d[1];   /* data buffer */
   } *shm;
   
   if(progname=strrchr(argv[0],'/')) progname++;
   else progname=argv[0];

   daemon_mode = syslog_mode = smode = nflag = 0;

   if(strcmp(progname,"wdisk60")==0) mode=60;
   else if(strcmp(progname,"wdisk60d")==0) {mode=60;daemon_mode=1;}
   else if(strcmp(progname,"wdisk")==0) mode=1;
   else if(strcmp(progname,"wdiskd")==0) {mode=1;daemon_mode=1;}

   while ((c = getopt(argc, argv, "Dns")) != -1) {
     switch (c) {
     case 'D':
       daemon_mode = 1;  /* daemon mode */
       break;
     case 'n':
       nflag = 1;  /* don't make control files except 'MAX' */
       break;
     case 's':
       smode = 1;  /* space mode */
       break;
     default:
       usage();
       exit(1);
     }
   }
   argc -= optind;
   argv += optind;

   if(argc<2){
     usage();
     exit(1);
   }
   
   if(strcmp(argv[0],"-")) shmkey=atoi(argv[0]);
   else
     {
     if (daemon_mode)
       {
	 fprintf(stderr,
		 "daemon mode cannot active in case of data from STDIN\n");
	 exit(1);
       }
     shmkey=0;
     buf=(unsigned char *)malloc(bufsiz=BUFSZ);
     }
   strcpy(outdir,argv[1]);
   if(argc>2) count_max=atoi(argv[2]);
   else count_max=0;
   sprintf(tbuf,"%s/MAX",outdir);
   fp=fopen(tbuf,"w+");
   if (smode)
     fprintf(fp,"%d MB\n",count_max);
   else
     fprintf(fp,"%d\n",count_max);
   fclose(fp);
   
   if(argc>3) strcpy(logfile,argv[3]);
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
   for(i=0;i<5;i++) tm_last[i]=(-1);
   fd=NULL;
   eobsize=0;

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
        for(i=0;i<4;i++) if(tm_last[i]!=tm[i]) break;
        if(i<4)
          {
          switch_file(tm);
          for(i=0;i<4;i++) tm_last[i]=tm[i];
          }
        }
      else
        {
        for(i=0;i<5;i++) if(tm_last[i]!=tm[i]) break;
        if(i<5)
          {
          switch_file(tm);
          for(i=0;i<5;i++) tm_last[i]=tm[i];
          }
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
        if(!(i<5000000 && i>=0) || mklong(ptr_save)!=size){
  	 write_log(logfile,"reset");
  	 goto reset;
        }

        size=mklong(ptr_save=shm->d+shp);
        if(size==mklong(ptr_save+size-4)) {
	  if (++eobsize_count == 0) eobsize_count = 1;
	}
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
