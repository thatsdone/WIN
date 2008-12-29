/* $Id: rtape.c,v 1.9.2.3.2.2 2008/12/29 11:25:12 uehira Exp $ */
/*
  program "rtape.c"
  9/16/89 - 11/06/90, 6/26/91, 10/30/91, 6/26/92  urabe
  for SUN, using mtio only 7/13/92
  3/5/93 - 3/11/93, 10/24/93
  97.8.15  Sun-OS4 tape (fragmented records)
  98.6.26  yo2000
  98.6.30  FreeBSD
  98.9.8   Multi-block sec
  99.4.19  byte-order-free
  2000.5.10 deleted size=<0x3c000 limit
  2001.10.15 output to current dir if not specified
  2002.5.11 500K->1M
  2004.9.5 stopped restriction of N of chs for selection
  2005.3.15 introduced blpersec (blocks/sec) factor
            MAXSIZE : 1M -> 2M, TRY_LIMIT : 10 -> 16
  2005.8.10 bug in strcmp2() fixed : 0-6 > 7-9
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/fcntl.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>
#include  <sys/mtio.h>

#include "winlib.h"

#define   DEBUG     0
#define   TRY_LIMIT 16
#define   NAMLEN    256
#define   MAXSIZE   2000000

#define   TIME1   "9005151102"  /* 10 m / fm before this time */
#define   TIME2   "9005161000"  /* no fms before this time */
                    /* 60 m / fm after this time */
#define   TIME3   "9008031718"  /* 10 m / fm after this time */

  unsigned char buf[MAXSIZE],outbuf[MAXSIZE];
  int fd_exb,f_get,leng,dec_start[6],dec_end[6],dec_begin[6],
    dec_buf1[6],dec_now[6],ext,fm_type,nch,sysch[65536],old_format;
  char name_file[NAMLEN],path[NAMLEN],textbuf[80],
    param_file[NAMLEN],name_prev[NAMLEN],dev_file[NAMLEN];
  FILE *f_param;
  int e_ch[241]={
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
    0x00F5};

end_process(value)
  int value;
  {
  close(fd_exb);
  printf("***** rtape end *****\n");
  exit(value);
  }

print_usage()
  {
  printf("   usage of 'rtape' :\n");
  printf("     'rtape (-f [tape device]) [schedule file] [output path] ([ch list file])'\n");
  printf("     example of schedule file :\n");
  printf("        '#YYMMDD hhmmss length(s)'\n");
  printf("        '920724 030830 50'\n");
  printf("        '920724 035750 60'\n");
  printf("     example of channel list file (in hexadecimal):\n");
  printf("         '101d 101e 101f 1020 1021 1022 1029 102a 102b'\n");
  }

mt_pos(fmc,blc)
  int fmc,blc;
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
      return re;
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
      return re;
      }
    }
  return re;
  }

get_one_record(blocking)
  int blocking;
  {
  int i,re,try_count,fm_count,bl_count,bl_count_last,advanced,
    sec_togo,sec_togo_last;
  static double blpersec=1.0;
  for(i=0;i<6;i++) dec_end[i]=dec_start[i];
  for(i=leng-1;i>0;i--)
    {
    dec_end[5]++;
    adj_time(dec_end);
    }
  printf("start:%02d%02d%02d %02d%02d%02d\n",
    dec_start[0],dec_start[1],dec_start[2],
    dec_start[3],dec_start[4],dec_start[5]);
  printf("end  :%02d%02d%02d %02d%02d%02d\n",
    dec_end[0],dec_end[1],dec_end[2],
    dec_end[3],dec_end[4],dec_end[5]);

  try_count=0;
  do
    {
    if(try_count++==TRY_LIMIT)
      {
      printf("specified time not found !\n");
      return(1);
      }

  /* obtain space counts */
    fm_count=0;
    sec_togo=(-1);
    if(time_cmp(dec_start,dec_now,6)<0)
      {
      for(i=0;i<6;i++) dec_buf1[i]=dec_now[i];
      while(time_cmp(dec_start,dec_buf1,6)<0)
        {
        dec_buf1[5]--;
        adj_time(dec_buf1);
        if((fm_type==10 && dec_buf1[5]==59 && dec_buf1[4]%10==9) ||
          (fm_type==60 && dec_buf1[5]==59 && dec_buf1[4]==59))
          {
          fm_count--;
          sec_togo=(-1);
          }
        else sec_togo--;
        }
      sec_togo--; /* adjust 2005.3.13 */
      }
    else if(time_cmp(dec_start,dec_now,6)>0)
      {
      for(i=0;i<6;i++) dec_buf1[i]=dec_now[i];
      while(time_cmp(dec_start,dec_buf1,6)>0)
        {
        dec_buf1[5]++;
        adj_time(dec_buf1);
        if((fm_type==10 && dec_buf1[5]==0 && dec_buf1[4]%10==0) ||
          (fm_type==60 && dec_buf1[5]==0 && dec_buf1[4]==0))
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
    mt_pos(fm_count,bl_count);  /* positioning */
    read_exb();  /* read one block */
    bcd_dec(dec_now,(char *)buf+4);
    printf("\n%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
      buf[6],buf[7],buf[8],buf[9],mkuint4(buf));
    fflush(stdout);
    } while(time_cmp(dec_start,dec_now,6));
  if((f_get=open(name_file,O_RDWR|O_CREAT|O_TRUNC,0664))==-1)
    {
    printf("file open error\n");
    close(f_get);
    fclose(f_param);
    end_process(1);
    }
  while(1)
    {
    select_ch(buf,outbuf,old_format);
  /* write one sec */
    re=write(f_get,(char *)outbuf,mkuint4(outbuf));
    if(time_cmp(dec_now,dec_end,6)==0) break;
  /* read one sec */
    read_exb();
    bcd_dec(dec_now,(char *)buf+4);
    if(time_cmp(dec_now,dec_end,6)>0) break;
    printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
      buf[6],buf[7],buf[8],buf[9],mkuint4(buf));
    fflush(stdout);
    }
  printf(" : done\n");
  }

read_exb()
  {
  int cnt,re,size,blocking,dec[6];
  char *ptr;
  cnt=0;
  while(1)
    {
    while((re=read(fd_exb,(char *)buf,MAXSIZE))==0)
      { /* file mark */
      close(fd_exb);
      if((fd_exb=open(dev_file,O_RDONLY))==-1)
        {
        perror("exabyte unit cannot open : ");
        exit(1);
        }
      }
    if(re>0)
      {
      blocking=1;
      size=mkuint4(buf);
      if(size<0) continue;
      if(bcd_dec(dec,(char *)buf+4)==0) continue;
#if DEBUG
      printf("(%d/%d)",re,size);
#endif
      ptr=(char *)buf;
      while(size>re)
        {
        re=read(fd_exb,ptr+=re,size-=re);
        blocking++;
#if DEBUG
        printf("(%d/%d)",re,size);
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
  return blocking;
  }

select_ch(old_buf,new_buf,old_form)
  unsigned char *old_buf,*new_buf;
  int old_form;
  {
  int i,j,size,gsize,new_size,sr;
  unsigned char *ptr,*new_ptr,*ptr_lim;
  unsigned int gh;
  size=mkuint4(old_buf);
  ptr_lim=old_buf+size;
  ptr=old_buf+4;
  new_ptr=new_buf+4;
  for(i=0;i<6;i++) *new_ptr++=(*ptr++);
  new_size=10;
  do
    {
    gh=mkuint4(ptr);
    i=gh>>16;
    if(old_form)
      {
      if((i&0xff00)==0) i=e_ch[i%241];  /* sys_ch */
      sr=gh&0xfff;
      gsize=((gh>>12)&0xf)*sr+4;
      }
    else
      {
      sr=gh&0xfff;
      if((gh>>12)&0xf) gsize=((gh>>12)&0xf)*(sr-1)+8;
      else gsize=(sr>>1)+8;
      }
    if(sysch[i])
      {
      new_size+=gsize;
      while(gsize-->0) *new_ptr++=(*ptr++);
      }
    else ptr+=gsize;
    } while(ptr<ptr_lim);
  new_buf[0]=new_size>>24;
  new_buf[1]=new_size>>16;
  new_buf[2]=new_size>>8;
  new_buf[3]=new_size;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  char *ptr;
  extern int optind;
  extern char *optarg;
  int i,c,optbase,blocking;

  printf("***** rtape start *****\n");
  strcpy(dev_file,"/dev/nrst0");
  while((c=getopt(argc,argv,"f:"))!=-1)
    {
    switch(c)
      {
      case 'f':   /* specify parameter file name */
        strcpy(dev_file,optarg);
        break;
      case 'h':
      default:
        print_usage();
        exit(0);
      }
    }
  optbase=optind-1;

  /* open exabyte device */
  if((fd_exb=open(dev_file,O_RDONLY))==-1)
    {
    perror("exabyte unit cannot open : ");
    print_usage();
    exit(1);
    }

  /* read one block */
  if((blocking=read_exb())<=0) blocking=1;
  bcd_dec(dec_begin,(char *)buf+4);
  for(i=0;i<6;i++) dec_now[i]=dec_begin[i];
  sprintf(textbuf,"%02x%02x%02x%02x%02x",buf[4],buf[5],buf[6],buf[7],buf[8]);
  old_format=0;
  if(strcmp2(textbuf,TIME1)<0)
    {
    fm_type=10;
    old_format=1;
    }
  else if(strcmp2(textbuf,TIME2)<0) fm_type=0;
  else if(strcmp2(textbuf,TIME3)<0) fm_type=60;
  else fm_type=10;
  printf("%02x%02x%02x %02x%02x%02x  %d (type=%d)\n",buf[4],buf[5],
    buf[6],buf[7],buf[8],buf[9],mkuint4(buf),fm_type);

  if(argc<2+optbase)
    {
    print_usage();
    end_process(0);
    }

  for(i=0;i<65536;i++) sysch[i]=1;
  if(argc>2+optbase) sscanf(argv[2+optbase],"%s",path);
  else strcpy(path,".");
  if(argc>3+optbase)
    {
    if((f_param=fopen(argv[3+optbase],"r"))==NULL)
      {
      perror("fopen");
      end_process(1);
      }
    for(i=0;i<65536;i++) sysch[i]=0;
    while(fscanf(f_param,"%x",&i)!=EOF)
      {
      i&=0xffff;
      sysch[i]=1;
      printf(" %03X",i);
      }
    nch=0;
    for(i=0;i<65536;i++) if(sysch[i]) nch++;
    printf("\n  <- %d chs according to '%s'\n",nch,argv[3+optbase]);
    fclose(f_param);
    }
  sscanf(argv[1+optbase],"%s",param_file);
  if((f_param=fopen(param_file,"r"))==NULL)
    {
    perror("fopen");
    end_process(1);
    }
  ext=0;
  while(fgets(textbuf,80,f_param)!=NULL)
    {
    if(*textbuf=='#') continue;
    sscanf(textbuf,"%2d%2d%2d %2d%2d%2d %d",
      dec_start,dec_start+1,dec_start+2,
      dec_start+3,dec_start+4,dec_start+5,&leng);
    sprintf(name_file,"%s/%02d%02d%02d.%02d%02d%02d",
      path,dec_start[0],dec_start[1],dec_start[2],
      dec_start[3],dec_start[4],dec_start[5]);
    if(strcmp(name_file,name_prev)==0)
      sprintf(name_file+strlen(name_file),"%d",ext++);
    else ext=0;
    printf("output file name = %s\n",name_file);
    strcpy(name_prev,name_file);
    get_one_record(blocking);
    close(f_get);
    }
  fclose(f_param);
  end_process(0);
  }
