/* $Id: wdisk.c,v 1.17.2.8.2.5 2010/09/20 03:33:28 uehira Exp $ */
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
		      2009.12.21 64bit clean? (uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define USE_LARGE_FS 0
#ifdef USE_LARGE_FS
#define _LARGEFILE64_SOURCE
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#define statfs statvfs
#define f_bsize f_frsize
#endif

#include "daemon_mode.h"
#include "winlib.h"

/* #define   DEBUG   0 */
#define   BELL    0

#define   NAMELEN  1025

static char rcsid[] =
  "$Id: wdisk.c,v 1.17.2.8.2.5 2010/09/20 03:33:28 uehira Exp $";

char *progname,*logfile;
int  daemon_mode, syslog_mode, exit_status;

static char tbuf[NAMELEN],latest[NAMELEN],oldest[NAMELEN],busy[NAMELEN],
  outdir[NAMELEN];
/* static unsigned char *buf; */
static int count,count_max,mode;
static FILE *fd;
static int  nflag, smode;

/* prototypes */
static long check_space(char *, long *);
static int switch_file(int *);
static void wmemo(char *, char *);
static void usage(void);
int main(int , char *[]);

static long
check_space(char *path, long *fsbsize)  /* 64bit ok? */
{
#if USE_LARGE_FS
  struct statfs64 fsbuf;
#else
  struct statfs fsbuf;
#endif
  char  path1[NAMELEN];

  if (snprintf(path1, sizeof(path1), "%s/.", path) >= sizeof(path1)) {
    write_log("path1[]: buffer overflow");
    end_program();
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

static int
switch_file(int *tm)
{
   FILE *fp;
   char oldst[NAMELEN];
   long freeb, fsbsize, space_raw;  /* 64bit ok */

   if(fd!=NULL){  /* if file is open, close last file */
      fclose(fd);
      fd=NULL;
#if DEBUG
      printf("closed fd=%p\n",fd);
#endif
      if (!nflag) {
	strcpy(latest,busy);
	wmemo(WDISK_LATEST,latest);
      }
   }

   /* delete oldest file */
   if (smode) {
     if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,WDISK_MAX) >= sizeof(tbuf)) {
       write_log("tbuf[]: buffer overflow");
       end_program();
     }
     if((fp=fopen(tbuf,"r")) != NULL){
       fscanf(fp,"%d",&count_max);
       fclose(fp);
     }
     else count_max=0;

     for (;;) {
       freeb = check_space(outdir, &fsbsize);
       space_raw =(long)((1048576.0*(double)count_max) / (double)fsbsize);
       count = find_oldest(outdir,oldst);
#if DEBUG
       printf("freeb, space_raw: %ld %ld (%ld)\n",
	      freeb, space_raw, fsbsize);
#endif
       if (space_raw < freeb || count == 0)
	 break;
       if (snprintf(tbuf, sizeof(tbuf), "%s/%s", outdir, oldst)
	   >= sizeof(tbuf)) {
	 write_log("tbuf[]: buffer overflow");
	 end_program();
       }
       unlink(tbuf);
       sync();
       sync();
       sync();
       count--;
     }

     if (!nflag) {
       strcpy(oldest,oldst);
       wmemo(WDISK_OLDEST,oldest);
     }
   } else {  /* !smode */
     if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,WDISK_MAX) >= sizeof(tbuf)) {
       write_log("tbuf[]: buffer overflow");
       end_program();
     }
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
     if (!nflag) {
       strcpy(oldest,oldst);
       wmemo(WDISK_OLDEST,oldest);
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
   if(fd!=NULL) printf("%s opened fd=%p\n",tbuf,fd);
#endif
   if (!nflag) {
     wmemo(WDISK_BUSY,busy);
     sprintf(tbuf,"%s/%s",outdir,WDISK_COUNT);
     fp=fopen(tbuf,"w+");
     fprintf(fp,"%d\n",count);
     fclose(fp);
   }
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

static void
usage()
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if(daemon_mode)
    fprintf(stderr,
	    " usage : '%s (-ns) [shm_key]/- [out dir] ([N of files]/[freespace in MB(-s)] ([log file]))'\n",
	    progname);
  else
    fprintf(stderr,
	    " usage : '%s (-Dns) [shm_key]/- [out dir] ([N of files]/[freespace in MB(-s)] ([log file]))'\n",
	    progname);
}

int
main(int argc, char *argv[])
{
#define BUFSZ 1000000
#define BUFLIM 5000000
   FILE *fp;
   int shmid,tm[6],tm_last[6],eobsize,eobsize_count=0;
   long  i;    /* 64bit ok */
   unsigned long c_save=0;  /* 64bit ok */
   WIN_bs  size, size2, bufsiz;
   size_t  shp=0,shp_save;
   uint8_w  *ptr,*ptr_save=NULL,ptw[4],*buf=NULL;
   key_t shmkey;
   int c;
   struct Shm  *shm=NULL;
   
   if((progname=strrchr(argv[0],'/')) != NULL) progname++;
   else progname=argv[0];

   daemon_mode = syslog_mode = smode = nflag = 0;
   exit_status = EXIT_SUCCESS;

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
   
   if(strcmp(argv[0],"-")) shmkey=atol(argv[0]);
   else
     {
     if (daemon_mode)
       {
	 fprintf(stderr,
		 "daemon mode cannot active in case of data from STDIN\n");
	 exit(1);
       }
     shmkey=0;
     buf=(uint8_w *)malloc((size_t)(bufsiz=BUFSZ));
     }
   strcpy(outdir,argv[1]);
   if(argc>2) count_max=atoi(argv[2]);
   else count_max=0;
   sprintf(tbuf,"%s/%s",outdir,WDISK_MAX);
   fp=fopen(tbuf,"w+");
   if (smode)
     fprintf(fp,"%d MB\n",count_max);
   else
     fprintf(fp,"%d\n",count_max);
   fclose(fp);
   
   if(argc>3) logfile=argv[3];
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
   if(shmkey)
     {
     if((shmid=shmget(shmkey,0,0))<0) err_sys("shmget");
     if((shm=(struct Shm *)shmat(shmid,(void *)0,0))==(struct Shm *)-1)
       err_sys("shmat");
   
     sprintf(tbuf,"start, shm_key=%ld sh=%p",shmkey,shm);
     write_log(tbuf);
     }
   
   signal(SIGPIPE,(void *)end_program);
   signal(SIGTERM,(void *)end_program);
   signal(SIGINT,(void *)end_program);
   
 reset:
   for(i=0;i<5;i++) tm_last[i]=(-1);
   fd=NULL;
   eobsize=0;

   if(shmkey)
     {
     while(shm->r==(-1)) sleep(1);
     shp=shp_save=shm->r;    /* read position */

     size=mkuint4(ptr_save=shm->d+shp);
     if(mkuint4(shm->d+shp+size-4)==size) eobsize=1;
     else eobsize=0;
     eobsize_count=eobsize;
     sprintf(tbuf,"eobsize=%d",eobsize);
     write_log(tbuf);
     }
   else
     {
     if(fread(buf,4,1,stdin)<1) end_program();
     size=mkuint4(buf);
     if(size>bufsiz)
       {
	 if(size>BUFLIM || (buf=realloc(buf,(size_t)size))==NULL)
         {
         sprintf(tbuf,"malloc size=%d",size);
         write_log(tbuf);
         end_program();
         }
       else bufsiz=size;
       }
     if(fread(buf+4,size-4,1,stdin)<1) end_program();
     }
 
   for(;;){
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
        write_log(tbuf);
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
      printf(" : %d r=%ld\n",size,shp);
#endif
#if BELL
      printf("\007");fflush(stdout);
#endif
      ptw[0]=size2>>24;
      ptw[1]=size2>>16;
      ptw[2]=size2>>8;
      ptw[3]=size2;
      if(fwrite(ptw,1,4,fd)<4) write_log("fwrite");
      if(fwrite(ptr,1,size2-4,fd)<size2-4) write_log("fwrite");
      if(mode==60) fflush(fd);
	   
#if DEBUG
      printf("%d>(fd=%p) r=%ld\n",size2,fd,shp);
#endif
      if(shmkey)
        {
        if((shp+=size)>shm->pl) shp=shp_save=0;
        while(shm->p==shp) sleep(1);
        i=shm->c-c_save;
        if(!(i<5000000 && i>=0) || mkuint4(ptr_save)!=size){
  	 write_log("reset");
  	 goto reset;
        }

        size=mkuint4(ptr_save=shm->d+shp);
        if(size==mkuint4(ptr_save+size-4)) eobsize_count++;
        else eobsize_count=0;
        if(eobsize && eobsize_count==0) goto reset;
        if(!eobsize && eobsize_count>3) goto reset;
        }
      else
        {
        if(fread(buf,4,1,stdin)<1) end_program();
        size=mkuint4(buf);
        if(size>bufsiz)
          {
          if(size>BUFLIM || (buf=realloc(buf,(size_t)size))==NULL)
            {
            sprintf(tbuf,"malloc size=%d",size);
	    write_log(tbuf);
            end_program();
            }
          else bufsiz=size;
          }
        if(fread(buf+4,size-4,1,stdin)<1) end_program();
        }

   }
}
