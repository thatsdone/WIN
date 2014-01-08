/* $Id: fromtape.c,v 1.9.2.1 2014/01/08 00:14:37 uehira Exp $ */
/*
  program "fromtape.c"
  12/10/90 - 12/13/90, 9/19/91, 10/30/91, 6/19/92  urabe
  for SUN, using mtio only 7/1/92
  3/11/93 - 5/25/93, 6/17/94, 8/17/95
  98.6.26  yo2000
  98.6.30  FreeBSD  urabe
  98.9.8   Multi-block sec
  99.4.19  byte-order-free
  2000.5.10 deleted size=<0x3c000 limit
  2002.4.30 MAXSIZE 500K->1M, SIZE_WBUF 50K->300K
  2005.3.15 introduced blpersec (blocks/sec) factor
            MAXSIZE : 1M -> 2M, TRY_LIMIT : 10 -> 16, SIZE_WBUF 300K->600K
  2005.8.10 bug in strcmp2() fixed : 0-6 > 7-9
  2010.2.17 64bit clean? (Uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <sys/types.h>
#include  <sys/uio.h>
#include  <sys/fcntl.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>
#include  <sys/mtio.h>

#include  <stdio.h>
#include  <string.h>
#include  <stdlib.h>
#include  <unistd.h>

#include "winlib.h"

#define   DEBUG1    0
#define   N_FILE    30
/* #define   TRY_LIMIT 16 */  /* moved to winlib.h */
#define   NAMLEN    256
#define   MAXSIZE   2000000
#define   SIZE_WBUF 600000
/* #define   SR_MON    5 */
/* #define   MAX_SR    1024 */

/* #define   TIME1   "9005151102" */  /* 10 m / fm before this time */
/* #define   TIME2   "9005161000" */  /* no fms before this time */
                    /* 60 m / fm after this time */
/* #define   TIME3   "9008031718" */  /* 10 m / fm after this time */

static const char rcsid[] =
  "$Id: fromtape.c,v 1.9.2.1 2014/01/08 00:14:37 uehira Exp $";

static uint8_w wbuf[SIZE_WBUF],buf[MAXSIZE];
static int fd_exb,dec_start[6],dec_end[6],min_reserve,
  dec_buf[6],dec_now[6],dec_done[6],fm_type,old_format,
  n_file;
static char name_file[NAMLEN],path_raw[NAMLEN],path_mon[NAMLEN],
  textbuf[80],name_time[NAMLEN],file_done[NAMLEN],
  mon_written[NAMLEN],raw_written[NAMLEN],exb_name[NAMLEN];
/* int32_w buf_raw[MAX_SR],buf_mon[SR_MON][2]; */
static FILE *fp,*f_mon,*f_raw;

/* prototypes */
static void end_process(int);
static void print_usage(void);
/* static int mt_pos(int, int, int); */  /* moved to winlib.c */
static void get_pos(void);
static void change_file(int, int, int);
/* static int read_exb(void); */   /* moved to winlib.c */
int main(int, char *[]);

static void
end_process(int value)
  {

  close(fd_exb);
  if(f_raw!=NULL)
    {
    fclose(f_raw);
    fp=fopen(raw_written,"w");
    fprintf(fp,"%s\n",name_time);
    fclose(fp);
    }
  if(f_mon!=NULL)
    {
    fclose(f_mon);
    fp=fopen(mon_written,"w");
    fprintf(fp,"%s\n",name_time);
    fclose(fp);
    }
  printf("\n***** fromtape end *****\n");
  exit(value);
  }

static void
print_usage(void)
  {

  WIN_version();
  fprintf(stderr,"%s\n",rcsid);
  fprintf(stderr,"usage of 'fromtape' :\n");
  fprintf(stderr," 'fromtape ([-options]) [YYMMDD.HHMM(1)] [YYMMDD.HHMM(2)] [raw dir] ([mon dir])'\n");
  fprintf(stderr,"         options: -f [tape device name]\n");
  fprintf(stderr,"                  -n [N of buffer files(>=5)]\n");  
  fprintf(stderr,"                  -s      - control by 'USED' file\n");  
  }

int
main(int argc, char *argv[])
  {
  int c,optbase,max_file,handshake;

  printf("***** fromtape start *****\n");
/* open exabyte device */
  max_file=handshake=0;
  strcpy(exb_name,"/dev/nrst0");
  while((c=getopt(argc,argv,"sn:f:"))!=-1)
    {
    switch(c)
      {
      case 'n':
        max_file=atoi(optarg);
        break;
      case 'f':
        strcpy(exb_name,optarg);
        break;
      case 's':
        handshake=1;
        break;
      case 'h':
      case 'u':
      default:
        print_usage();
        exit(0);
      }
    }
  optbase=optind-1;

  if((fd_exb=open(exb_name,O_RDONLY))==-1)
    {
    perror("tape unit cannot open");
    print_usage();
    exit(1);
    }

  if(max_file>0 && max_file<5)
    {
    printf("Illegal N of files = %d\n",max_file);
    print_usage();
    exit(1);
    }

  f_mon=f_raw=NULL;
  /* read one block */
  if (read_exb1(exb_name, fd_exb, buf, sizeof(buf)) < 0)
    end_process(1);
  bcd_dec(dec_now,buf+4);
  printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
    buf[6],buf[7],buf[8],buf[9],mkuint4(buf));
  fflush(stdout);
  sprintf(textbuf,"%02x%02x%02x%02x%02x",buf[4],buf[5],
    buf[6],buf[7],buf[8]);
  old_format=0;
  if(strcmp2(textbuf,TIME1)<0)
    {
    fm_type=10;
    old_format=1;
    }
  else if(strcmp2(textbuf,TIME2)<0) fm_type=0;
  else if(strcmp2(textbuf,TIME3)<0) fm_type=60;
  else fm_type=10;
  printf(" (type=%d)\n",fm_type);
  if(argc<4+optbase)
    {
    print_usage();
    end_process(0);
    }
  sscanf(argv[1+optbase],"%2d%2d%2d.%2d%2d",&dec_start[0],&dec_start[1],
    &dec_start[2],&dec_start[3],&dec_start[4]);
  dec_start[5]=0;
  sscanf(argv[2+optbase],"%2d%2d%2d.%2d%2d",&dec_end[0],&dec_end[1],
    &dec_end[2],&dec_end[3],&dec_end[4]);
  dec_end[5]=59;
  sscanf(argv[3+optbase],"%s",path_raw);
  printf("start:%02d%02d%02d %02d%02d%02d\n",
    dec_start[0],dec_start[1],dec_start[2],
    dec_start[3],dec_start[4],dec_start[5]);
  printf("end  :%02d%02d%02d %02d%02d%02d\n",
    dec_end[0],dec_end[1],dec_end[2],
    dec_end[3],dec_end[4],dec_end[5]);
  printf("output directory (raw) : %s\n",path_raw);
  sprintf(raw_written,"%s/%s",path_raw,FROMTAPE_LATEST);
  if(argc>4+optbase)
    {
    sscanf(argv[4+optbase],"%s",path_mon);
    printf("output directory (mon) : %s\n",path_mon);
    sprintf(mon_written,"%s/%s",path_mon,FROMTAPE_LATEST);
    sprintf(file_done,"%s/%s",path_mon,PMON_USED);
    }
  else sprintf(file_done,"%s/%s",path_raw,ECORE_USED);
  if(max_file>0)
    {
    printf("N of files buffered = %d ",max_file);
    if(handshake) printf("(use '%s')\n",file_done);
    else printf("(free-run)\n");
    }
  get_pos();
  min_reserve=dec_now[4]+1;
  n_file=0;
  for(;;)
    {
    if(dec_now[4]!=min_reserve)
      change_file(argc-optbase,max_file,handshake);
    min_reserve=dec_now[4];
    fwrite(buf,1,mkuint4(buf),f_raw);
    if(argc>4+optbase)
      {
      make_mon(buf,wbuf,0);
      fwrite(wbuf,1,mkuint4(wbuf),f_mon);
/*      printf(" %d>MON\n",mkuint4(wbuf));*/
      }
    if(time_cmp(dec_now,dec_end,6)==0) break;
    if (read_exb1(exb_name, fd_exb, buf, sizeof(buf)) < 0)
      end_process(1);
    bcd_dec(dec_now,buf+4);
    printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
      buf[6],buf[7],buf[8],buf[9],mkuint4(buf));
    fflush(stdout);
    if(time_cmp(dec_now,dec_end,6)>0) break;
    }
  end_process(0);
  }

static void
get_pos(void)
  {
  int i,try_count,fm_count,bl_count,bl_count_last,advanced,
    sec_togo,sec_togo_last;
  static double blpersec=1.0;

  try_count=0;
  do
    {
    if(try_count++==TRY_LIMIT)
      {
      printf("specified time not found !\n");
      return;
      }

  /* obtain space counts */
    fm_count=0;
    sec_togo=(-1);
    if(time_cmp(dec_start,dec_now,6)==0) return;
    else if(time_cmp(dec_start,dec_now,6)<0)
      {
      for(i=0;i<6;i++) dec_buf[i]=dec_now[i];
      while(time_cmp(dec_start,dec_buf,6)<0)
        {
        dec_buf[5]--;
        adj_time(dec_buf);
        if((fm_type==10 && dec_buf[5]==59 && dec_buf[4]%10==9) ||
          (fm_type==60 && dec_buf[5]==59 && dec_buf[4]==59))
          {
          fm_count--;
          sec_togo=(-1);
          }
        else sec_togo--;
        }
      sec_togo--; /* adjust 2005.3.14 */
      }
    else if(time_cmp(dec_start,dec_now,6)>0)
      {
      for(i=0;i<6;i++) dec_buf[i]=dec_now[i];
      while(time_cmp(dec_start,dec_buf,6)>0)
        {
        dec_buf[5]++;
        adj_time(dec_buf);
        if((fm_type==10 && dec_buf[5]==0 && dec_buf[4]%10==0) ||
          (fm_type==60 && dec_buf[5]==0 && dec_buf[4]==0))
          {
          fm_count++;
          sec_togo=0;
          }
        else sec_togo++;
        }
      }
    if(try_count>1)
      {
      advanced=sec_togo_last-sec_togo;
      if(sec_togo_last>0)
        {
        if(advanced<=0) blpersec*=2;
        else blpersec=(double)bl_count_last/(double)advanced; 
        }
      else
        {
        if(advanced>=0) blpersec*=2;
        else blpersec=(double)bl_count_last/(double)advanced;
        }
      }
    bl_count=(double)sec_togo*blpersec;
#if DEBUG1
    printf(" togo_last=%d bl_count_last=%d advanced=%d blpersec=%.1f togo=%d bl_count=%d try=%d\n",
      sec_togo_last,bl_count_last,advanced,blpersec,sec_togo,bl_count,try_count);
#endif
    sec_togo_last=sec_togo;
    bl_count_last=bl_count;
    printf(" skipping %d fms and %d blks ...",fm_count,bl_count);
    fflush(stdout);
    mt_pos(fm_count,bl_count,fd_exb);    /* positioning */
    printf("\n");
    if (read_exb1(exb_name, fd_exb, buf, sizeof(buf)) <0)  /* read one block */
      end_process(1);
    bcd_dec(dec_now,buf+4);
    printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
      buf[6],buf[7],buf[8],buf[9],mkuint4(buf));
    fflush(stdout);
    } while(time_cmp(dec_start,dec_now,6));
  }

static void
change_file(int argc, int max_file, int handshake)
  {
  int i,j;

  if(f_raw!=NULL)
    {
    fclose(f_raw);
    fp=fopen(raw_written,"w");
    fprintf(fp,"%s\n",name_time);
    fclose(fp);
    }
  if(f_mon!=NULL)
    {
    fclose(f_mon);
    fp=fopen(mon_written,"w");
    fprintf(fp,"%s\n",name_time);
    fclose(fp);
    }
  if(max_file>0 && n_file++>=max_file)
    {
    if(handshake)
      {
      while((fp=fopen(file_done,"r"))==NULL) sleep(15);
      for(;;)
        {
        fscanf(fp,"%02d%02d%02d%02d.%02d",&dec_done[0],&dec_done[1],
          &dec_done[2],&dec_done[3],&dec_done[4]);
        for(i=0;i<5;i++) dec_buf[i]=dec_now[i];
        if((j=max_file/3)<2) j=2;
        for(i=0;i<j;i++)
          {
          dec_buf[5]=(-1);
          adj_time(dec_buf);
          }
        dec_buf[5]=dec_done[5]=0;
        if(time_cmp(dec_done,dec_buf,6)>0)
          {
          fclose(fp);
          for(i=0;i<(max_file*2/3);i++)
            {
            dec_buf[5]=(-1);
            adj_time(dec_buf);
            }
          sprintf(name_file,"%s/%02d%02d%02d%02d.%02d",path_raw,
            dec_buf[0],dec_buf[1],dec_buf[2],dec_buf[3],dec_buf[4]);
          unlink(name_file);
          if(argc>4)
            {
            sprintf(name_file,"%s/%02d%02d%02d%02d.%02d",path_mon,
              dec_buf[0],dec_buf[1],dec_buf[2],dec_buf[3],dec_buf[4]);
            unlink(name_file);
            }
          break;
          }
        fclose(fp);
        sleep(3);
        fp=fopen(file_done,"r");
        }
      }
    else
      {
      for(i=0;i<5;i++) dec_buf[i]=dec_now[i];
      for(i=0;i<max_file;i++)
        {
        dec_buf[5]=(-1);
        adj_time(dec_buf);
        }
      sprintf(name_file,"%s/%02d%02d%02d%02d.%02d",path_raw,
        dec_buf[0],dec_buf[1],dec_buf[2],dec_buf[3],dec_buf[4]);
      unlink(name_file);
      if(argc>4)
        {
        sprintf(name_file,"%s/%02d%02d%02d%02d.%02d",path_mon,
          dec_buf[0],dec_buf[1],dec_buf[2],dec_buf[3],dec_buf[4]);
        unlink(name_file);
        }
      }
    }
  sprintf(name_time,"%02d%02d%02d%02d.%02d",
    dec_now[0],dec_now[1],dec_now[2],dec_now[3],dec_now[4]);

  sprintf(name_file,"%s/%s",path_raw,name_time);
  if((f_raw=fopen(name_file,"w+"))==NULL)
    {
    printf("file open error\n");
    end_process(1);
    }
  if(argc>4)
    {
    sprintf(name_file,"%s/%s",path_mon,name_time);
    if((f_mon=fopen(name_file,"w+"))==NULL)
      {
      printf("file open error\n");
      end_process(1);
      }
    }
  }

/* static int */
/* read_exb() */
/*   { */
/*   int cnt,blocking,dec[6]; */
/*   ssize_t re; */
/*   uint32_w  size; */
/*   uint8_w *ptr; */

/*   cnt=0; */
/*   for(;;) */
/*     { */
/*     while((re=read(fd_exb,buf,MAXSIZE))==0) */
/*       { /\* file mark *\/ */
/*       close(fd_exb); */
/*       if((fd_exb=open(exb_name,O_RDONLY))==-1) */
/*         { */
/*         perror("exabyte unit cannot open : "); */
/*         exit(1); */
/*         } */
/*       } */
/*     if(re>0) */
/*       { */
/*       blocking=1; */
/*       size=mkuint4(buf); */
/*       /\* if(size<0) continue; *\/  /\* nonsense! *\/ */
/*       if(bcd_dec(dec,buf+4)==0) continue; */
/* #if DEBUG */
/*       printf("(%ld/%u)",re,size); */
/* #endif */
/*       ptr=buf; */
/*       while(size>re) */
/*         { */
/*         re=read(fd_exb,ptr+=re,size-=re); */
/*         blocking++; */
/* #if DEBUG */
/*         printf("(%ld/%u)",re,size); */
/* #endif */
/*         } */
/*       if(re>0) break; */
/*       } */
/*     /\* error *\/ */
/*     perror("exabyte"); */
/*     cnt++; */
/*     if(cnt==TRY_LIMIT/2) mt_pos(-2,0,fd_exb); /\* overrun ? *\/ */
/*     else if(cnt==TRY_LIMIT) end_process(1); */
/*     } */
/*   return (blocking); */
/*   } */

