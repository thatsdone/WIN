/* $Id: fromtape.c,v 1.4 2002/01/13 06:57:50 uehira Exp $ */
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
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <string.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/fcntl.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>
#include  <sys/mtio.h>

#include "subst_func.h"

#define   N_FILE    30
#define   TRY_LIMIT 10
#define   NAMLEN    100
#define   MAXSIZE   500000
#define   SIZE_WBUF 50000
#define   SR_MON    5
#define   MAX_SR    1024

#define   TIME1   "9005151102"  /* 10 m / fm before this time */
#define   TIME2   "9005161000"  /* no fms before this time */
                    /* 60 m / fm after this time */
#define   TIME3   "9008031718"  /* 10 m / fm after this time */

  unsigned char wbuf[SIZE_WBUF],buf[MAXSIZE];
  int fd_exb,dec_start[6],dec_end[6],min_reserve,dp,
    dec_buf[6],dec_now[6],dec_done[6],fm_type,old_format,
    n_file;
  char name_file[NAMLEN],path_raw[NAMLEN],path_mon[NAMLEN],
    textbuf[80],name_time[NAMLEN],file_done[NAMLEN],param[100],
    mon_written[NAMLEN],raw_written[NAMLEN],exb_name[NAMLEN];
  long buf_raw[MAX_SR],buf_mon[SR_MON][2];
  FILE *fp,*f_mon,*f_raw;
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

strcmp2(s1,s2)        
char *s1,*s2;
{
  if(*s1=='0' && *s2=='9') return 1;
  else if(*s1=='9' && *s2=='0') return -1;
  else return strcmp(s1,s2);
}

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;  
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

adj_time(tm)
  int *tm;
  {
  if(tm[5]==60)
    {
    tm[5]=0;
    if(++tm[4]==60)
      {
      tm[4]=0;
      if(++tm[3]==24)
        {
        tm[3]=0;
        tm[2]++;
        switch(tm[1])
          {
          case 2:
            if(tm[0]%4==0)  /* leap year */
              {
              if(tm[2]==30)
                {
                tm[2]=1;
                tm[1]++;
                }
              break;
              }
            else
              {
              if(tm[2]==29)
                {
                tm[2]=1;
                tm[1]++;
                }
              break;
              }
          case 4:
          case 6:
          case 9:
          case 11:
            if(tm[2]==31)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
          default:
            if(tm[2]==32)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
          }
        if(tm[1]==13)
          {
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
          }
        }
      }
    }
  else if(tm[5]==-1)
    {
    tm[5]=59;
    if(--tm[4]==-1)
      {
      tm[4]=59;
      if(--tm[3]==-1)
        {
        tm[3]=23;
        if(--tm[2]==0)
          {
          switch(--tm[1])
            {
            case 2:
              if(tm[0]%4==0)
                tm[2]=29;else tm[2]=28;
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
          if(tm[1]==0)
            {
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
            }
          }
        }
      }
    }
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

end_process(value)
  int value;
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

print_usage()
  {
  printf("usage of 'fromtape' :\n");
  printf(" 'fromtape ([-options]) [YYMMDD.HHMM(1)] [YYMMDD.HHMM(2)] [raw dir] ([mon dir])'\n");
  printf("         options: -f [tape device name]\n");
  printf("                  -n [N of buffer files(>=5)]\n");  
  printf("                  -s      - control by 'USED' file\n");  
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  char *ptr;
  extern int optind;
  extern char *optarg;
  int i,c,optbase,max_file,handshake;

  printf("***** fromtape start *****\n");
/* open exabyte device */
  max_file=handshake=0;
  strcpy(exb_name,"/dev/nrst0");
  while((c=getopt(argc,argv,"sn:f:"))!=EOF)
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
  bcd_dec(dec_now,(char *)buf+4);
  printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
    buf[6],buf[7],buf[8],buf[9],mklong(buf));
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
  sprintf(raw_written,"%s/LATEST",path_raw);
  if(argc>4+optbase)
    {
    sscanf(argv[4+optbase],"%s",path_mon);
    printf("output directory (mon) : %s\n",path_mon);
    sprintf(mon_written,"%s/LATEST",path_mon);
    sprintf(file_done,"%s/USED",path_mon);
    }
  else sprintf(file_done,"%s/USED",path_raw);
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
    fwrite(buf,1,mklong(buf),f_raw);
    if(argc>4+optbase)
      {
      make_mon(buf,wbuf);
      fwrite(wbuf,1,mklong(wbuf),f_mon);
/*      printf(" %d>MON\n",mklong(wbuf));*/
      }
    if(time_cmp(dec_now,dec_end,6)==0) break;
    read_exb();
    bcd_dec(dec_now,(char *)buf+4);
    printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
      buf[6],buf[7],buf[8],buf[9],mklong(buf));
    fflush(stdout);
    if(time_cmp(dec_now,dec_end,6)>0) break;
    }
  end_process(0);
  }

get_mon(gm_sr,gm_raw,gm_mon)
  int gm_sr,*gm_raw,(*gm_mon)[2];
  {
  int gm_i,gm_j,gm_subr;
  switch(gm_sr)
    {
    case 100:
      gm_subr=100/SR_MON;
      break;
    case 20:
      gm_subr=20/SR_MON;
      break;
    case 120:
      gm_subr=120/SR_MON;
      break;
    default:
      gm_subr=gm_sr/SR_MON;
      break;
    }
  for(gm_i=0;gm_i<SR_MON;gm_i++)
    {
    gm_mon[gm_i][0]=gm_mon[gm_i][1]=(*gm_raw);
    for(gm_j=0;gm_j<gm_subr;gm_j++)
      {
      if(*gm_raw<gm_mon[gm_i][0]) gm_mon[gm_i][0]=(*gm_raw);
      else if(*gm_raw>gm_mon[gm_i][1]) gm_mon[gm_i][1]=(*gm_raw);
      gm_raw++;
      }
    }
  }

unsigned char *compress_mon(peaks,ptr)
  int *peaks;
  unsigned char *ptr;
  {
  /* data compression */
  if(((peaks[0]&0xffffffc0)==0xffffffc0 || (peaks[0]&0xffffffc0)==0) &&
    ((peaks[1]&0xffffffc0)==0xffffffc0 ||(peaks[1]&0xffffffc0)==0))
    {
    *ptr++=((peaks[0]&0x70)<<1)|((peaks[1]&0x70)>>2);
    *ptr++=((peaks[0]&0xf)<<4)|(peaks[1]&0xf);
    }
  else
  if(((peaks[0]&0xfffffc00)==0xfffffc00 || (peaks[0]&0xfffffc00)==0) &&
    ((peaks[1]&0xfffffc00)==0xfffffc00 ||(peaks[1]&0xfffffc00)==0))
    {
    *ptr++=((peaks[0]&0x700)>>3)|((peaks[1]&0x700)>>6)|1;
    *ptr++=peaks[0];
    *ptr++=peaks[1];
    }
  else
  if(((peaks[0]&0xfffc0000)==0xfffc0000 ||(peaks[0]&0xfffc0000)==0) &&
    ((peaks[1]&0xfffc0000)==0xfffc0000 ||(peaks[1]&0xfffc0000)==0))
    {
    *ptr++=((peaks[0]&0x70000)>>11)|((peaks[1]&0x70000)>>14)|2;
    *ptr++=peaks[0];
    *ptr++=peaks[0]>>8;
    *ptr++=peaks[1];
    *ptr++=peaks[1]>>8;
    }
  else
  if(((peaks[0]&0xfc000000)==0xfc000000 || (peaks[0]&0xfc000000)==0) &&
    ((peaks[1]&0xfc000000)==0xfc000000 ||(peaks[1]&0xfc000000)==0))
    {
    *ptr++=((peaks[0]&0x7000000)>>19)|((peaks[1]&0x7000000)>>22)|3;
    *ptr++=peaks[0];
    *ptr++=peaks[0]>>8;
    *ptr++=peaks[0]>>16;
    *ptr++=peaks[1];
    *ptr++=peaks[1]>>8;
    *ptr++=peaks[1]>>16;
    }
  else 
    {
    *ptr++=0;
    *ptr++=0;
    }
  return ptr;
  }

win2fix(ptr,abuf,sys_ch,sr) /* returns group size in bytes */
  unsigned char *ptr; /* input */
  register long *abuf;/* output */
  long *sys_ch;       /* sys_ch */
  long *sr;           /* sr */
  {
  int b_size,g_size;
  register int i,s_rate;
  register unsigned char *dp;
  unsigned int gh;
  short shreg;
  int inreg;

  dp=ptr;
  gh=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  *sr=s_rate=gh&0xfff;
/*  if(s_rate>MAX_SR) return 0;*/
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16)&0xffff;

  /* read group */
  abuf[0]=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  if(s_rate==1) return g_size;  /* normal return */
  switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        abuf[i]=abuf[i-1]+((*(char *)dp)>>4);
        abuf[i+1]=abuf[i]+(((char)(*(dp++)<<4))>>4);
        }
      break;
    case 1:
      for(i=1;i<s_rate;i++)
        abuf[i]=abuf[i-1]+(*(char *)(dp++));
      break;
    case 2:
      for(i=1;i<s_rate;i++)
        {
        shreg=((dp[0]<<8)&0xff00)+(dp[1]&0xff);
        dp+=2;
        abuf[i]=abuf[i-1]+shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00);
        dp+=3;
        abuf[i]=abuf[i-1]+(inreg>>8);
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
        dp+=4;
        abuf[i]=abuf[i-1]+inreg;
        }
      break;
    default:
      return 0; /* bad header */
    }
  return g_size;  /* normal return */
  }

make_mon(ptr,ptw) /* for one minute */
  unsigned char *ptr,*ptw;
  {
  unsigned char *ptr_lim,*ptw_start;
  int i,j,k,ch,sr;
  unsigned long uni;

  /* make mon data */
  ptr_lim=ptr+mklong(ptr);
  ptw_start=ptw;
  ptr+=4;
  ptw+=4;               /* size (4) */
  for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */

  do    /* loop for chs */
    {
    ptr+=win2fix(ptr,buf_raw,&ch,&sr);
    *ptw++=ch>>8;
    *ptw++=ch;
    get_mon(sr,buf_raw,buf_mon);  /* get mon data from raw */
    for(i=0;i<SR_MON;i++) ptw=compress_mon(buf_mon[i],ptw);
    } while(ptr<ptr_lim);
  uni=ptw-ptw_start;
  ptw_start[0]=uni>>24; /* size (H) */
  ptw_start[1]=uni>>16;
  ptw_start[2]=uni>>8;
  ptw_start[3]=uni;     /* size (L) */
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

get_pos()
  {
  int i,try_count,fm_count,bl_count,bl_count_last,advanced,
    sec_togo,sec_togo_last;
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
    bl_count=(-1);
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
          bl_count=(-1);
          }
        else bl_count--;
        }
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
          bl_count=0;
          }
        else bl_count++;
        }
      }
    sec_togo=bl_count;
    if(try_count>1 && fm_count==0)
      {
      sec_togo=bl_count;
      advanced=sec_togo_last-sec_togo;
      if((advanced>0 && sec_togo>advanced) ||
          (advanced<0 && sec_togo<advanced))
        {
        bl_count=bl_count_last;
        try_count--;
        }
      else if(advanced==0) bl_count=bl_count_last*2;
#if DEBUG
      printf(" togo_last=%d advanced=%d togo=%d bl_count=%d try=%d\n",
        sec_togo_last,advanced,sec_togo,bl_count,try_count);
#endif
      }
    sec_togo_last=sec_togo;
    bl_count_last=bl_count;
    printf(" skipping %d fms and %d blks ...",fm_count,bl_count);
    fflush(stdout);
    mt_pos(fm_count,bl_count);    /* positioning */
    printf("\n");
    read_exb();           /* read one block */
    bcd_dec(dec_now,(char *)buf+4);
    printf("\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
      buf[6],buf[7],buf[8],buf[9],mklong(buf));
    fflush(stdout);
    } while(time_cmp(dec_start,dec_now,6));
  }

change_file(argc,max_file,handshake)
  int argc,max_file,handshake;
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

read_exb()
  {
  int cnt,re,size,blocking,dec[6];
  struct mtop exb_param;
  char *ptr;
  cnt=0;
  while(1)
    {
    while((re=read(fd_exb,(char *)buf,MAXSIZE))==0)
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
      size=mklong(buf);
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
