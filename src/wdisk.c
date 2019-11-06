/* $Id: wdisk.c,v 1.25 2019/11/06 05:20:18 urabe Exp $ */
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
		      2010.09.27 check struct statfs.f_bavail size. 
		                 check __SVR4 and __NetBSD__ (uehira)
                      2019.11.1 added options -a,-m,-h,-d,-c,-t (urabe)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* #define USE_LARGE_FS 0 */
#ifdef USE_LARGE_FS
#define _LARGEFILE64_SOURCE
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

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

#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>

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
#include "winlib.h"

/*#define   DEBUG   0*/

#define   NAMELEN  1025

static const char rcsid[] =
  "$Id: wdisk.c,v 1.25 2019/11/06 05:20:18 urabe Exp $";

char *progname,*logfile;
int  syslog_mode, exit_status;

static char tbuf[NAMELEN],latest[NAMELEN],oldest[NAMELEN],busy[NAMELEN],
  outdir[NAMELEN],*outdir0,outdir1[NAMELEN],*chfile;
/* static unsigned char *buf; */
static int count,count_max,mode;
static FILE *fd; /* used for marker file in .wdisk when sw_dir=1 */
static int  nflag, smode;
static int  daemon_mode;
static int  sw_dir;   /* 0:none,  1:dir for each CH */
static int  sw_file;  /* 0:minute,1:hour, 2:day */
static int  sw_ext;   /* 0:none,  1:with extension */
static int  sw_name;  /* 0:CHID,  1:STN.COMP */
#define  CHIDLEN  (WIN_STANAME_LEN+WIN_STACOMP_LEN) 
/*static char chid[WIN_CHMAX][CHIDLEN];*/  /* stn.comp (or sysch) */
static char (*chid)[CHIDLEN];  /* stn.comp (or sysch) */
 
/* prototypes */
#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
static long check_space(char *, long *);
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
static int64_t check_space(char *, int64_t *);
#endif
static int switch_file(int *);
static int read_chfile(void);
static void wmemo(char *, char *);
static void bfov_error(void);
static void usage(void);
int main(int , char *[]);

#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
static long
check_space(char *path, long *fsbsize)
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
static int64_t
check_space(char *path, int64_t *fsbsize)  /* 64bit ok */
#endif
{
#if USE_LARGE_FS
  struct statfs64 fsbuf;
#else
  struct statfs fsbuf;
#endif
  char  path1[NAMELEN];

  if (snprintf(path1, sizeof(path1), "%s/.", path) >= sizeof(path1))
    bfov_error();
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
read_chfile(void)
{
  FILE *fp;
  WIN_ch sysch;
  char name[WIN_STANAME_LEN];  /* station name */
  char comp[WIN_STACOMP_LEN];  /* component name */
  int i,j; 

  if((fp=fopen(chfile,"r"))==NULL) return -1;
  for(i=0;i<WIN_CHMAX;i++) chid[i][0]=0;
  j=0;
  while(fgets(tbuf,NAMELEN,fp)!=NULL) {
    if(*tbuf=='#') continue;
    sscanf(tbuf,"%x%*d%*d%s%s",&sysch,name,comp);
    snprintf(chid[sysch],sizeof(chid[sysch]),"%s.%s",name,comp);
#if DEBUG
    printf("ch %04X '%s'\n",sysch,chid[sysch]);
#endif
    j++;
  }
  fclose(fp);
  for(i=0;i<WIN_CHMAX;i++)
    if(chid[i][0]==0) snprintf(chid[i],sizeof(chid[i]),"%04X",i);
  snprintf(tbuf,sizeof(tbuf),"%d chs read from '%s'",j,chfile);
  write_log(tbuf);
  signal(SIGHUP,(void *)read_chfile);
  return j;
}

static int
switch_file(int *tm)
{
   FILE *fp;
   char oldst[NAMELEN];
#if defined(STRUCT_STATFS_F_BAVAIL_LONG)
   long freeb, fsbsize, space_raw;  /* 64bit ok */
#elif defined(STRUCT_STATFS_F_BAVAIL_INT64)
   int64_t freeb, fsbsize, space_raw;  /* 64bit ok */
#endif

   if(sw_dir) {
     if(fd!=NULL){  /* if file is open, close last file */
       fclose(fd);
       fd=NULL;
#if DEBUG
       printf("closed fd=%p\n",fd);
#endif
     }
   }
   if (*busy && !nflag) {
     strcpy(latest,busy);  /* ok */
     wmemo(WDISK_LATEST,latest);
   }

   /* delete oldest file */
   if (smode) {
     if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,WDISK_MAX) >= sizeof(tbuf))
       bfov_error();
     if((fp=fopen(tbuf,"r")) != NULL){
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
       if (snprintf(tbuf, sizeof(tbuf), "%s/%s", outdir, oldst) >= sizeof(tbuf))
         bfov_error();
       unlink(tbuf);
       if(sw_dir) {
         if (snprintf(tbuf, sizeof(tbuf), "rm %s/*/%s*", outdir0, oldst) >= sizeof(tbuf))
           bfov_error();
         system(tbuf);
       }
       sync();
       sync();
       sync();
       count--;
       usleep(10000);
     }

     if (!nflag) {
       strcpy(oldest,oldst);  /* ok */
       wmemo(WDISK_OLDEST,oldest);
     }
   } else {  /* !smode */
     if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,WDISK_MAX) >= sizeof(tbuf))
       bfov_error();
     if((fp=fopen(tbuf,"r")) != NULL){
       fscanf(fp,"%d",&count_max);
       fclose(fp);
       if(count_max>0 && count_max<3) count_max=3;
     }
     else count_max=0;
     
     while((count=find_oldest(outdir,oldst))>=count_max && count_max){
       /* sprintf(tbuf,"%s/%s",outdir,oldst); */
       if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,oldst) >= sizeof(tbuf))
	 bfov_error();
       unlink(tbuf);
       if(sw_dir) {
         if (snprintf(tbuf, sizeof(tbuf), "rm %s/*/%s*", outdir0, oldst) >= sizeof(tbuf))
           bfov_error();
         system(tbuf);
       }
       count--;
       usleep(10000);
#if DEBUG
       printf("%s deleted\n",tbuf);
#endif
     }
     if (!nflag) {
       strcpy(oldest,oldst);  /* ok */
       wmemo(WDISK_OLDEST,oldest);
     }
   }
   /* make new file name */
   if(sw_file==1) {
     if (snprintf(busy,sizeof(busy),"%02d%02d%02d%02d",
		  tm[0],tm[1],tm[2],tm[3]) >= sizeof(busy))
       bfov_error();
   } else if(sw_file==2) {
     if (snprintf(busy,sizeof(busy),"%02d%02d%02d%",
		  tm[0],tm[1],tm[2]) >= sizeof(busy))
       bfov_error();
   } else { /* sw_file==0 */
     if (snprintf(busy,sizeof(busy),"%02d%02d%02d%02d.%02d",
		  tm[0],tm[1],tm[2],tm[3],tm[4]) >= sizeof(busy))
       bfov_error();
   }
   if(sw_dir) {
     if(snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,busy)>=sizeof(tbuf))
       bfov_error();
     /* open new file */
     if((fd=fopen(tbuf,"a+"))==NULL) err_sys(tbuf);
#if DEBUG
     if(fd!=NULL) printf("%s opened fd=%p\n",tbuf,fd);
#endif
   }
   count++;
   if (!nflag) {
     wmemo(WDISK_BUSY,busy);
     if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,WDISK_COUNT) >= sizeof(tbuf))
       bfov_error();
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

   if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,f) >= sizeof(tbuf))
     bfov_error();
   fp=fopen(tbuf,"w+");
   fprintf(fp,"%s\n",c);
   fclose(fp);
}

static void
bfov_error(void)
{

  write_log("Buffer overrun!");
  exit_status = EXIT_FAILURE;
  end_program();
}

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if(daemon_mode)
    fprintf(stderr,
	    " usage : '%s (-acdhmns) (-t [ch file]) [shm_key]/- [out dir] ([N of files]/[freespace in MB(-s)] ([log file]))'\n",
	    progname);
  else
    fprintf(stderr,
	    " usage : '%s (-Dacdhmns) (-t [ch file]) [shm_key]/- [out dir] ([N of files]/[freespace in MB(-s)] ([log file]))'\n",
	    progname);
}

int
main(int argc, char *argv[])
{
#define BUFSZ 1000000
#define BUFLIM 5000000
   FILE *fp;
   int tm[6],tm_last[6],eobsize,eobsize_count=0;
   long  i;    /* 64bit ok */
   unsigned long c_save=0;  /* 64bit ok */
   WIN_bs  size, size2, bufsiz, gsize, gsize2;
   WIN_ch sysch;
   size_t  shp=0,shp_save;
   uint8_w  *ptr,*ptr_save=NULL,ptw[4],*buf=NULL,*ptr_lim,*ptr_time;
   key_t shmkey;
   int c;
   struct Shm  *shm=NULL;
   
   if((progname=strrchr(argv[0],'/')) != NULL) progname++;
   else progname=argv[0];

   daemon_mode = syslog_mode = smode = nflag = 0;
   exit_status = EXIT_SUCCESS;

   if(strcmp(progname,"wdisk60")==0) sw_file=1;
   else if(strcmp(progname,"wdisk60d")==0) {sw_file=1;daemon_mode=1;}
   else if(strcmp(progname,"wdisk")==0) sw_file=0;
   else if(strcmp(progname,"wdiskd")==0) {sw_file=0;daemon_mode=1;}

   sw_dir=0;   /* single outdir, not for each CH */
   sw_ext=0;   /* no extension */
   sw_name=0;  /* use CHID, not STN.COMP */

   while ((c = getopt(argc, argv, "Dacdhmnst:")) != -1) {
     switch (c) {
     case 'D':
       daemon_mode = 1;  /* daemon mode */
       break;
     case 'a':
       sw_dir = 1;  /* dir for each CH */
       break;
     case 'c':
       sw_ext = 1;  /* add extension (CHID or STN.COMP) to file name */
       sw_dir = 1;  /* dir for each CH */
       break;
     case 'd':
       sw_file = 2;  /* day file */
       break;
     case 'h':
       sw_file = 1;  /* hour file */
       break;
     case 'm':
       sw_file = 0;  /* minute file */
       break;
     case 'n':
       nflag = 1;  /* don't make control files except 'MAX' */
       break;
     case 's':
       smode = 1;  /* space mode */
       break;
     case 't':
       sw_name=1;  /* STN.COMP insted of CHID */
       sw_dir = 1;  /* dir for each CH */
       chfile = optarg;  /* channel table */
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
     buf=(uint8_w *)win_xmalloc((size_t)(bufsiz=BUFSZ));
     }
   /* strcpy(outdir,argv[1]); */
   if(sw_dir==0)
     {
     if (snprintf(outdir,sizeof(outdir),"%s",argv[1]) >= sizeof(outdir))
       bfov_error();
     }
   else
     {
     outdir0=argv[1];
     if (snprintf(outdir,sizeof(outdir),"%s/.wdisk",argv[1]) >= sizeof(outdir))
       bfov_error();
     if(dir_check(outdir)<0)
       {
       fprintf(stderr,"cannot create directory '%s' !\n",outdir);
       exit(1);
       }
     }

   if(argc>2) count_max=atoi(argv[2]);
   else count_max=0;

   if(sw_dir)
     chid=(char(*)[CHIDLEN])win_xmalloc(sizeof(char)*CHIDLEN*WIN_CHMAX);

   if(sw_name) {
     if(read_chfile()<0) {
       fprintf(stderr,"ch file '%s' not open !\n",chfile);
       exit(1);
     }
   }
   else if(sw_dir)
     for(i=0;i<WIN_CHMAX;i++) snprintf(chid[i],sizeof(chid[i]),"%04X",i);

   if (snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,WDISK_MAX) >= sizeof(tbuf)) {
     fprintf(stderr,"'%s': Buffer overrun!\n",progname);
     exit(1);
   }
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
     shm = Shm_read(shmkey, "start");
     /* if((shmid=shmget(shmkey,0,0))<0) err_sys("shmget"); */
     /* if((shm=(struct Shm *)shmat(shmid,(void *)0,0))==(struct Shm *)-1) */
     /*   err_sys("shmat"); */
     
     /* sprintf(tbuf,"start, shm_key=%ld sh=%p",shmkey,shm); */
     /* write_log(tbuf); */
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
     snprintf(tbuf,sizeof(tbuf),"eobsize=%d",eobsize);
     write_log(tbuf);
     }
   else
     {
     if(fread(buf,4,1,stdin)<1) end_program();
     size=mkuint4(buf);
     if(size>bufsiz)
       {
       if(size>BUFLIM || (buf=(uint8_w *)win_xrealloc(buf,(size_t)size))==NULL)
	 {
	 snprintf(tbuf,sizeof(tbuf),"malloc size=%d",size);
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
	snprintf(tbuf,sizeof(tbuf),
          "illegal time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X.%02X%02X%02X (%d)",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],
          ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11],size);
        write_log(tbuf);
        if(fd!=NULL) fclose(fd);
        if(shmkey) sleep(5);
        goto reset;
        }
 
      if(sw_file==1)
        {
        for(i=0;i<4;i++) if(tm_last[i]!=tm[i]) break;
        if(i<4)
          {
          switch_file(tm);
          for(i=0;i<4;i++) tm_last[i]=tm[i];
          }
        }
      else if(sw_file==2)
        {
        for(i=0;i<3;i++) if(tm_last[i]!=tm[i]) break;
        if(i<3)
          {
          switch_file(tm);
          for(i=0;i<3;i++) tm_last[i]=tm[i];
          }
        }
      else /* sw_file==0 */
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
      printf(" : %d r=%zu\n",size,shp);
#endif

      if(sw_dir==0) {
        ptw[0]=size2>>24;
        ptw[1]=size2>>16;
        ptw[2]=size2>>8;
        ptw[3]=size2;
        if(snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir,busy)>=sizeof(tbuf))
          bfov_error();
        if((fp=fopen(tbuf,"a+"))==NULL) err_sys(tbuf);
        flock(fileno(fp),LOCK_EX);
        fseek(fp,0,SEEK_END); /* file might have been altered */
        if(fwrite(ptw,1,4,fp)<4) write_log("fwrite");
        if(fwrite(ptr,1,size2-4,fp)<size2-4) write_log("fwrite");
        flock(fileno(fp),LOCK_UN);
        fclose(fp);
#if DEBUG
        printf(" %d>%s (fp=%p)\n",size2,busy,fp);
#endif
      } else { /* open-write-close for each ch */
        ptr_time=ptr;
        ptr_lim=ptr+size2-WIN_BSLEN;
        ptr+=WIN_TM_LEN;
        do { /* file name -> outdir0/chid/busy(.chid) */
          gsize=get_sysch(ptr,&sysch);
	  snprintf(outdir1,sizeof(outdir1),"%s/%s",outdir0,chid[sysch]);
          if(dir_check(outdir1)<0) {
            fprintf(stderr,"cannot create directory '%s' !\n",outdir1);
            exit(1);
          }
	  if(sw_ext)
            snprintf(tbuf,sizeof(tbuf),"%s/%s.%s",outdir1,busy,chid[sysch]);
       	  else snprintf(tbuf,sizeof(tbuf),"%s/%s",outdir1,busy);
          if((fp=fopen(tbuf,"a+"))==NULL) err_sys(tbuf);
          gsize2=gsize+WIN_BSLEN+WIN_TM_LEN;
          ptw[0]=gsize2>>24;
          ptw[1]=gsize2>>16;
          ptw[2]=gsize2>>8;
          ptw[3]=gsize2;
          flock(fileno(fp),LOCK_EX);
          fseek(fp,0,SEEK_END); /* file might have been altered */
          if(fwrite(ptw,1,4,fp)<4) write_log("fwrite"); /* size */
          if(fwrite(ptr_time,1,WIN_TM_LEN,fp)<WIN_TM_LEN)
            write_log("fwrite"); /* time stamp */
          if(fwrite(ptr,1,gsize,fp)<gsize) write_log("fwrite"); /* ch block */
          flock(fileno(fp),LOCK_UN);
          fclose(fp);
#if DEBUG
          printf(" %04X:%d>%s\n",sysch,gsize2,tbuf);
#endif
          ptr+=gsize;
        } while(ptr<ptr_lim);
      }
   
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
        if(size==mkuint4(ptr_save+size-4)) {
	  if (++eobsize_count == 0) eobsize_count = 1;
	}
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
	  if(size>BUFLIM || (buf=(uint8_w *)win_xrealloc(buf,(size_t)size))==NULL)
            {
	    snprintf(tbuf,sizeof(tbuf),"malloc size=%d",size);
	    write_log(tbuf);
            end_program();
            }
          else bufsiz=size;
          }
        if(fread(buf+4,size-4,1,stdin)<1) end_program();
        }

   }
}
