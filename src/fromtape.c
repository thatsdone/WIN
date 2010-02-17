/* $Id: fromtape.c,v 1.7.2.3.2.7 2010/02/17 10:13:33 uehira Exp $ */
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
#define   TRY_LIMIT 16
#define   NAMLEN    256
#define   MAXSIZE   2000000
#define   SIZE_WBUF 600000
/* #define   SR_MON    5 */
/* #define   MAX_SR    1024 */

#define   TIME1   "9005151102"  /* 10 m / fm before this time */
#define   TIME2   "9005161000"  /* no fms before this time */
                    /* 60 m / fm after this time */
#define   TIME3   "9008031718"  /* 10 m / fm after this time */

static char rcsid[] =
  "$Id: fromtape.c,v 1.7.2.3.2.7 2010/02/17 10:13:33 uehira Exp $";

static uint8_w wbuf[SIZE_WBUF],buf[MAXSIZE];
static int fd_exb,dec_start[6],dec_end[6],min_reserve,
  dec_buf[6],dec_now[6],dec_done[6],fm_type,old_format,
  n_file;
static char name_file[NAMLEN],path_raw[NAMLEN],path_mon[NAMLEN],
  textbuf[80],name_time[NAMLEN],file_done[NAMLEN],
  mon_written[NAMLEN],raw_written[NAMLEN],exb_name[NAMLEN];
/* int32_w buf_raw[MAX_SR],buf_mon[SR_MON][2]; */
static FILE *fp,*f_mon,*f_raw;

/* moved to winlib.h */
/*  int e_ch[241]={
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
    0x0080,0x0081,0x0082,0x0083,0x0084,0x0085,0x0086,0x0087,
    0x0088,0x0089,0x008A,0x008B,0x008C,0x008D,0x008E,0x008F,
    0x0090,0x0091,0x0092,0x0093,0x0094,0x0095,0x0096,0x0097,
    0x0098,0x0099,0x009A,0x009B,0x009C,0x009D,0x009E,0x009F,
    0x00A0,0x00A1,0x00A2,0x00A3,0x00A4,0x00A5,0x00A6,0x00A7,
    0x00A8,0x00A9,0x00AA,0x00AB,0x00AC,0x00AD,0x00AE,0x00AF,
    0x00B0,0x00B1,0x00B2,0x00B3,0x00B4,0x00B5,0x00B6,0x00B7,
    0x00B8,0x00B9,0x00BA,0x00BB,0x00BC,0x00BD,0x00BE,0x00BF,
    0x00C0,0x00C1,0x00C2,0x00C3,0x00C4,0x00C5,0x00C6,0x00C7,
    0x00C8,0x00C9,0x00CA,0x00CB,0x00CC,0x00CD,0x00CE,0x00CF,
    0x00D0,0x00D1,0x00D2,0x00D3,0x00D4,0x00D5,0x00D6,0x00D7,
    0x00D8,0x00D9,0x00DA,0x00DB,0x00DC,0x00DD,0x00DE,0x00DF,
    0x00E5,0x00E6,0x00E7,0x00E8,0x00E9,0x00EA,0x00EB,0x00EC,
    0x00ED,0x00EE,0x00EF,0x00F0,0x00F1,0x00F2,0x00F3,0x00F4,
    0x00F5};*/

/* prototypes */
static void end_process(int);
static void print_usage(void);
static int mt_pos(int, int);
static void get_pos(void);
static void change_file(int, int, int);
static int read_exb(void);
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
print_usage()
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
  read_exb();
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
  while(1)
    {
    if(dec_now[4]!=min_reserve)
      change_file(argc-optbase,max_file,handshake);
    min_reserve=dec_now[4];
    fwrite(buf,1,mkuint4(buf),f_raw);
    if(argc>4+optbase)
      {
      make_mon(buf,wbuf);
      fwrite(wbuf,1,mkuint4(wbuf),f_mon);
/*      printf(" %d>MON\n",mkuint4(wbuf));*/
      }
    if(time_cmp(dec_now,dec_end,6)==0) break;
    read_exb();
    bcd_dec(dec_now,buf+4);
    printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
      buf[6],buf[7],buf[8],buf[9],mkuint4(buf));
    fflush(stdout);
    if(time_cmp(dec_now,dec_end,6)>0) break;
    }
  end_process(0);
  }

static int
mt_pos(int fmc, int blc)
  {
  struct mtop exb_param;
  int re;

  re=0;
  if(fmc)
    {
    if(fmc>0)
      {
      exb_param.mt_op=MTFSF;
      exb_param.mt_count=fmc;
      }
    else
      {
      exb_param.mt_op=MTBSF;
      exb_param.mt_count=(-fmc);
      }
    if((re=ioctl(fd_exb,MTIOCTOP,(char *)&exb_param))==-1)
      {
      perror("error in space fms");
      printf("processing continues ... ");
      fflush(stdout);
      return (re);
      }
    }
  if(blc)
    {
    if(blc>0)
      {
      exb_param.mt_op=MTFSR;
      exb_param.mt_count=blc;
      }
    else
      {
      exb_param.mt_op=MTBSR;
      exb_param.mt_count=(-blc);
      }
    if((re=ioctl(fd_exb,MTIOCTOP,(char *)&exb_param))==-1)
      {
      perror("error in space records");
      printf("processing continues ... ");
      fflush(stdout);
      return (re);
      }
    }
  return (re);
  }

static void
get_pos()
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
    mt_pos(fm_count,bl_count);    /* positioning */
    printf("\n");
    read_exb();           /* read one block */
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
      while(1)
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

static int
read_exb()
  {
  int cnt,blocking,dec[6];
  ssize_t re;
  uint32_w  size;
  uint8_w *ptr;

  cnt=0;
  while(1)
    {
    while((re=read(fd_exb,buf,MAXSIZE))==0)
      { /* file mark */
      close(fd_exb);
      if((fd_exb=open(exb_name,O_RDONLY))==-1)
        {
        perror("exabyte unit cannot open : ");
        exit(1);
        }
      }
    if(re>0)
      {
      blocking=1;
      size=mkuint4(buf);
      /* if(size<0) continue; */  /* nonsense! */
      if(bcd_dec(dec,buf+4)==0) continue;
#if DEBUG
      printf("(%ld/%u)",re,size);
#endif
      ptr=buf;
      while(size>re)
        {
        re=read(fd_exb,ptr+=re,size-=re);
        blocking++;
#if DEBUG
        printf("(%ld/%u)",re,size);
#endif
        }
      if(re>0) break;
      }
    /* error */
    perror("exabyte");
    cnt++;
    if(cnt==TRY_LIMIT/2) mt_pos(-2,0); /* overrun ? */
    else if(cnt==TRY_LIMIT) end_process(1);
    }
  return (blocking);
  }
