/* $Id: events.c,v 1.11 2008/12/31 08:03:56 uehira Exp $ */
/****************************************************************************
*****************************************************************************
**     program "events.c" for NEWS                                  *********
**     make event files                                             *********
**     7/6/90 - 9/27/91 urabe                                       *********
**     9/2/92 bug fix for the same 'on' time                        *********
**     10/16/92 for auto                                            *********
**     1/14/93 for RISC NEWS                                        *********
**     3/9/93 - 6/8/93 for SUN SPARC                                *********
**     10/11/94,12/1/94 bug fix                                     *********
**     events/eventsrtape                                           *********
**     12/5/94 - 12/9/94 "request-mail(uehira)" included            *********
**                        format of events.prm changed              *********
**     1/6/95            adj_time bug fixed (tm[0]--)               *********
**     4/5/95            format of 'zones.tbl' changed (to orig.)   *********
**     10/3/95        avoid endless loop by 'offset' in get_trig()  *********
**     10/3/95        added '-x' option                             *********
**     8/15/96        changed request method from mail to lpr       *********
**     97.3.25-97.8.5 Solaris                                       *********
**     97.9.1         rm -f                                         *********
**     97.10.1        FreeBSD                                       *********
**     98.2.16        fixed bug in f_frsize (statvfs)               *********
**                    fixed bug in reading zones.tbl                *********
**     98.6.26        yo2000                                        *********
**     98.6.29        eliminated dp,last_dp etc.                    *********
**     98.9.4         RTAPE makes file in temp dir then it is mv'd  *********
**     98.9.7         time_start/time_end                           *********
**     99.5.29        quit if pmon.out won't open at first          *********
**     99.6.12        not make zero-size waveform file              *********
**     99.11.9        SGI IRIX                                      *********
**     2001.1.22      used_raw & check_path                         *********
**     2002.9.5       -u [dir] for USED_EVENTS file                 *********
**     2005.8.10      bug in strcmp2()/strncmp2() fixed : 0-6 > 7-9 *********
**                                                                  *********
**   Example of parameter file (events.prm)                         *********
=============================================================================
/dat/etc/pmon.out      # trigger file
/dat/raw               # input raw file directory
/dat/trg               # output trg file directory
100:1000               # min.free space and max.used amount in outdir (MB)
/tmp                   # working directory for temporary file
20                     # pre event time in sec (<60)
3.0                    # post event time factor; LEN=PRE+TRG+POST*sqrt(TRG)
/dat/etc/channels.tbl  # master channel table file
/dat/etc/zones.tbl     # zone table file
/dat/etc/nxtzones.tbl  # list of neighboring zones
/dat/auto              # touch directory for auto-pick
/dat/request           # request touch directory
tky                    # my system code
/dat/log/request-events  # log file
#
# list of remote data servers follows.
# local (my) server may be included, but it will not be used.
#<sys>  <station list>      <printcap entry of data server>
tky     /dat/etc/tky.station    cut-saemon
sso     /dat/etc/sso.station    cut-jc3
=============================================================================
**                                                                  *********
*****************************************************************************
*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define USE_LARGE_FS 0
#ifdef USE_LARGE_FS
#define _LARGEFILE64_SOURCE
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <signal.h>
#include  <string.h>
#include  <math.h>
#include  <ctype.h>
#include  <unistd.h>
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <dirent.h>
#include  <sys/file.h>

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

#include  <sys/param.h>
#include  <sys/mount.h>

#if !defined(BSD4_4)
#include  <sys/vfs.h>
#endif

#if defined(__SVR4) || defined(__NetBSD__)
#include  <sys/statvfs.h>
#define statfs statvfs
#define f_bsize f_frsize
#endif

#include  <fcntl.h>
#include  <sys/ioctl.h>

#include "subst_func.h"

#define DEBUG     0
#define CLEAN     1
#define N_ZONES   1024
#define WED       "wed"
#define RTAPE     "rtape"
#define LEN       1024
#define NSYS      20   /* n of remote system to request data */

int space_raw,used_raw,time_flag;
char *progname,temp_path[LEN];

owari()
  {
  char textbuf[LEN];
  sprintf(textbuf,"rm %s/%s.*.%d 2>/dev/null",temp_path,progname,getpid());
#if CLEAN
  system(textbuf);
#endif
  printf("*****  %s end  *****\n",progname);
  exit(0);
  }

#define FILE_R 0
#define FILE_W 1
#define DIR_R  2
#define DIR_W  3
check_path(path,idx)
  char *path;
  int idx;
  {
  DIR *dir_ptr;
  char tb[1024];
  FILE *fp;
  if(idx==DIR_R || idx==DIR_W)
    {
    if((dir_ptr=opendir(path))==NULL)
      {
      printf("%s(%d):directory not open '%s'\n",progname,getpid(),path);
      owari();
      }
    else closedir(dir_ptr);
    if(idx==DIR_W)
      {
      sprintf(tb,"%s/%s.test.%d",path,progname,getpid());
      if((fp=fopen(tb,"w+"))==NULL)
        {
        printf("%s(%d):directory not R/W '%s'\n",progname,getpid(),path);
        owari();
        }
      else
        {
        fclose(fp);
        unlink(tb);
        }
      }
    }
  else if(idx==FILE_R)
    {
    if((fp=fopen(path,"r"))==NULL)
      {
      printf("%s(%d):file not readable '%s'\n",progname,getpid(),path);
      owari();
      }
    else fclose(fp);
    }
  else if(idx==FILE_W)
    {
    if((fp=fopen(path,"r+"))==NULL)
      {
      printf("%s(%d):file not R/W '%s'\n",progname,getpid(),path);
      owari();
      }
    else fclose(fp);
    }
  return 1;
  } 

extend_time(tm1,tm2,pre,post_fac)
  int *tm1,*tm2,pre;
  double post_fac;
  {
  int i,tm[6],post,len;
  for(i=0;i<6;i++) tm[i]=tm1[i];
  len=0;
  while(time_cmp(tm,tm2,6)<0)
    {
    tm[5]++;
    adj_time(tm);
    len++;
    }
  post=(int)(sqrt((double)len)*post_fac);
  len+=pre+post;
  for(i=0;i<pre;i++)
    {
    tm1[5]--;
    adj_time(tm1);
    }
  for(i=0;i<post;i++)
    {
    tm2[5]++;
    adj_time(tm2);
    }
  return len;
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
            if(tm[0]%4==0)
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

get_trig(trigfile,init_flag,tbuf,pbuf)
  char *trigfile,*tbuf,*pbuf;
  int init_flag;
  {
/* get the line next to 'pbuf'.
   if init_flag, get the 'on' line next to 'pbuf' */
  static long offset;
  long ofs;
  char *ptr;
  FILE *fp;
#if DEBUG
  printf("offset=%d prev=%s",offset,pbuf);
#endif
  while(1)
    {
    while(1)
      {
      if((fp=fopen(trigfile,"r"))==NULL)
        {
        printf("%s(%d):'%s' not found. Waiting ...\n",
          progname,getpid(),trigfile);
        goto again;
        }
      if(*pbuf)
        {
        fseek(fp,offset,0);
        ptr=fgets(tbuf,LEN,fp);
        if(init_flag || ptr==NULL || !(ptr && strcmp(tbuf,pbuf)==0))
          {
#if DEBUG
          printf("rewind\n");
          printf("pbuf=%s\n",pbuf);
#endif
          rewind(fp);
          do
          {
            if(fgets(tbuf,LEN,fp)==NULL)
              {
              printf("%s(%d):unexpected EOF in '%s'. Waiting ...\n",
                progname,getpid(),trigfile);
              fclose(fp);
              goto again;
              }
            } while(strncmp(tbuf,pbuf,17));
          }
#if DEBUG
        else printf("hit!\n");
#endif
        }
      ofs=ftell(fp);
      ptr=fgets(tbuf,LEN,fp);   /* read a new line */
      fclose(fp);
      if(ptr) break; /* a valid new line was read */
      else if(time_flag) owari();
again:sleep(30);
      }
    offset=ofs;
    strcpy(pbuf,tbuf);
    if(*tbuf==' ') continue;
    if(init_flag && strncmp(tbuf+14,"on",2)) continue;
      /* if init_flag, wait only 'on' */
    break;
    }
  }

strncmp2(s1,s2,i)
char *s1,*s2;
int i;
{
  if((*s1>='0' && *s1<='5') && (*s2<='9' && *s2>='6')) return 1;
  else if((*s1<='9' && *s1>='7') && (*s2>='0' && *s2<='6')) return -1;
  else return strncmp(s1,s2,i);
}

strcmp2(s1,s2)
char *s1,*s2;
{
  if((*s1>='0' && *s1<='5') && (*s2<='9' && *s2>='6')) return 1;
  else if((*s1<='9' && *s1>='7') && (*s2>='0' && *s2<='6')) return -1;
  else return strcmp(s1,s2);
}

check_space(path)
  char *path;
  {
  FILE *fp;
#if USE_LARGE_FS
  struct statfs64 fsbuf;
#else
  struct statfs fsbuf;
#endif
  int i,dirblocks;
  struct dirent *dir_ent;
  DIR *dir_ptr;
  char name_buf[256],oldest[256],oldest2[256],newest[256],path1[256];
  struct stat st_buf;

  /* check disk space */
  sprintf(path1,"%s/.",path);
  while(1)
    {
    /* find oldest file and sum up file sizes */
    if((dir_ptr=opendir(path))==NULL)
      {
      printf("%s:data path open error '%s' (%d)",progname,path,getpid());
      owari();
      }
    i=dirblocks=0;
    *oldest=(*oldest2)=(*newest)=0;
    while((dir_ent=readdir(dir_ptr))!=NULL)
      {
      if(!isdigit(dir_ent->d_name[0])) continue;
      else if(!isdigit(dir_ent->d_name[strlen(dir_ent->d_name)-1]))
        {
        if(used_raw)
          {
          sprintf(name_buf,"%s/%s",path,dir_ent->d_name);
          stat(name_buf,&st_buf);
          dirblocks+=st_buf.st_blocks;
          }
        continue;
        }
      if(*oldest==0)
        {
        strcpy(oldest,dir_ent->d_name);
        strcpy(oldest2,dir_ent->d_name);
        strcpy(newest,dir_ent->d_name);
        }
      else
        {
        if(strcmp2(dir_ent->d_name,oldest2)<0)
          {
          if(strcmp2(dir_ent->d_name,oldest)<0)
            {
            strcpy(oldest2,oldest);
            strcpy(oldest,dir_ent->d_name);
            }
          else strcpy(oldest2,dir_ent->d_name);
          }
        if(strcmp2(dir_ent->d_name,newest)>0) strcpy(newest,dir_ent->d_name);
        }
      if(used_raw)
        {
        sprintf(name_buf,"%s/%s",path,dir_ent->d_name);
        stat(name_buf,&st_buf);
        dirblocks+=st_buf.st_blocks;
        }
      i++;
      }
#if 0
    printf("%d files in %s, %d KB used, oldest=%s 2nd-oldest=%s newest=%s\n",
      i,path,(dirblocks+1)/2,oldest,oldest2,newest);
#endif
    closedir(dir_ptr);
#if USE_LARGE_FS
    if(statfs64(path1,&fsbuf)<0)
#else
    if(statfs(path1,&fsbuf)<0)
#endif
      {
      printf("%s:statfs : %s (%d)",progname,path1,getpid());
      owari();
      }
    printf("%dMB free(min.%dMB), %d MB used(max.%dMB)\n",
      fsbuf.f_bavail/((1024*1024)/fsbuf.f_bsize),space_raw,
      (dirblocks+1)/(2*1024),used_raw);
    sprintf(name_buf,"%s/%s",path,"FREESPACE");
    fp=fopen(name_buf,"w+");
    fprintf(fp,"%d %d %d %d\n",fsbuf.f_bavail/((1024*1024)/fsbuf.f_bsize),
      space_raw,(dirblocks+1)/(2*1024),used_raw);
    fclose(fp);
    /* check free/used spaces */
    if(fsbuf.f_bavail/((1024*1024)/fsbuf.f_bsize)<space_raw ||
        (used_raw>0 && (dirblocks+1)/(2*1024)>used_raw))
      {
      sprintf(name_buf,"rm -f %s/%s*",path,oldest);
      system(name_buf);
      printf("%s\n",name_buf);
      sprintf(name_buf,"%s/%s",path,"OLDEST");
      fp=fopen(name_buf,"w+");
      fprintf(fp,"%s\n",oldest2);
      fclose(fp);
      sprintf(name_buf,"%s/%s",path,"COUNT");
      fp=fopen(name_buf,"w+");
      fprintf(fp,"%d\n",--i);
      fclose(fp);
      }
    else
      {
      sprintf(name_buf,"%s/%s",path,"OLDEST");
      fp=fopen(name_buf,"w+");
      fprintf(fp,"%s\n",oldest);
      fclose(fp);
      sprintf(name_buf,"%s/%s",path,"COUNT");
      fp=fopen(name_buf,"w+");
      fprintf(fp,"%d\n",i);
      fclose(fp);
      break;
      }
    }
  }

read_param(f_param,textbuf)
  FILE *f_param;
  unsigned char *textbuf;
  {
  do
    {
    if(fgets(textbuf,LEN,f_param)==NULL)
      {
      printf("%s:error in paramater file\n",progname);
      owari();
      }
    } while(*textbuf=='#');
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

print_usage(progname)
  char *progname;
  {
  printf("usage of %s :\n",progname);
  printf("  %s [param file] ([YYMMDD.hhmm(1)] [YYMMDD.hhmm(2)])\n",progname);
  if(strcmp(progname,"eventsrtape")==0)
    {
    printf("       options: -f [tape device]\n");
    printf("                -u [directory for 'USED' file]\n");
    printf("                -x [zone to be ignored]\n");
    printf("   ('param file's format is the same as that of 'events')\n");
    }
  else
    {
    printf("       options: -x [zone to be ignored]\n");
    printf("                -u [directory for 'USED' file]\n");
    }
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  extern int optind;
  extern char *optarg;
  int c,optbase;
  FILE *fp_trig,*f_param,*fp,*fq,*fr,*fs;
  int i,j,init,pre_event_sec,sec_len,last_min,rtape,ix,
    nsys,tm1[6],tm2[6],tm[6],rt[6];
  double post_event_fac;
  static char textbuf[LEN],textbuf1[LEN],out_path[LEN],zone[20],
    trig_file[LEN],request_path[LEN],sys_code[20],stan[20],chs[20],
    sys[NSYS][20],stan_list[NSYS][LEN],lpr_name[NSYS][LEN],
    mess[40],mess1[40],mess2[40],raw_path[LEN],lastline[LEN],
    file_name[LEN],param_file[LEN],tbl_path[LEN],temp_name[LEN],
    auto_path[LEN],zone_list[LEN],time_start[20],time_end[20],
    tapeunit[LEN],raw_file[20],lpr_body[LEN],
    zone_file[LEN],ch_file[LEN],nxt_file[LEN],log_file[LEN],
    file_chs[LEN],rtape_file[LEN],tmpfile1[LEN],tmpfile2[LEN],
    tmpfile3[LEN],xzones[N_ZONES][20],file_used[LEN];
  char *ptr,*ptr_lim,*ptw;
  struct stat st_buf;

  ix=0;
  *file_used=0;
  strcpy(tapeunit,"/dev/nrst0");
  while((c=getopt(argc,argv,"f:x:u:"))!=-1)
    {
    switch(c)
      {
      case 'f':
        strcpy(tapeunit,optarg);
        break;
      case 'u':
        strcpy(file_used,optarg);
        strcat(file_used,"/USED_EVENTS");
        break;
      case 'x':
        strcpy(xzones[ix++],optarg);
        break;
      case 'h':
      default:
        print_usage(progname);
        exit(0);
      }
    }
  optbase=optind-1;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<2+optbase)
    {
    print_usage(progname);
    exit(1);
    }

  strcpy(param_file,argv[1+optbase]);
  if(strcmp(progname,"eventsrtape")==0) rtape=1;
  else rtape=0;

  if(argc>=4+optbase)
    {
    strcpy(time_start,argv[2+optbase]);
    strcpy(time_end,argv[3+optbase]);
    time_flag=1;
    }
  else time_flag=0;

  printf("*****  %s start  *****\n",progname);

  /* open parameter file */ 
  if((f_param=fopen(param_file,"r"))==NULL)
    {
    printf("%s:'%s' not open\n",progname,param_file);
    owari();
    }
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",trig_file);
  check_path(trig_file,FILE_R);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",raw_path);
  check_path(raw_path,DIR_R);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",out_path);
  check_path(out_path,DIR_W);
  read_param(f_param,textbuf1);
  sscanf(textbuf1,"%s",textbuf);
  if((ptr=strchr(textbuf,':'))==0)
    {
    sscanf(textbuf,"%d",&space_raw);
    used_raw=0;
    }
  else
    {
    *ptr=0;
    sscanf(textbuf,"%d",&space_raw);
    sscanf(ptr+1,"%d",&used_raw);
    }
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",temp_path);
  check_path(temp_path,DIR_W);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%d",&pre_event_sec);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%lf",&post_event_fac);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",ch_file);
  check_path(ch_file,FILE_R);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",zone_file);
  check_path(zone_file,FILE_R);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",nxt_file);
  check_path(nxt_file,FILE_R);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",auto_path);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",request_path);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",sys_code);
  read_param(f_param,textbuf);
  sscanf(textbuf,"%s",log_file);
  nsys=0;
  while(fgets(textbuf,LEN,f_param))
    {
    if(*textbuf=='#') continue;
    sscanf(textbuf,"%s%s%s",sys[nsys],stan_list[nsys],lpr_name[nsys]);
    if(strcmp(sys[nsys],sys_code)==0) printf(" *");
    else printf("  ");
    printf("%d   %s   %s   %s\n",nsys,sys[nsys],stan_list[nsys],
      lpr_name[nsys]);
    if(++nsys==NSYS) break;
    }
  fclose(f_param);

  sprintf(temp_name,"%s.tmp.%d",progname,getpid());
  sprintf(rtape_file,"%s/%s.rtape.%d",temp_path,progname,getpid());
  sprintf(file_chs,"%s/%s.ch.%d",temp_path,progname,getpid());
  sprintf(tmpfile1,"%s/%s.tmp1.%d",temp_path,progname,getpid());
  sprintf(tmpfile2,"%s/%s.tmp2.%d",temp_path,progname,getpid());
  sprintf(tmpfile3,"%s/%s.tmp3.%d",temp_path,progname,getpid());
  sprintf(lpr_body,"%s/%s.lpr.%d",temp_path,progname,getpid());
/*  printf("space=%d\n",space_raw); */
  *lastline=0;
  if((fp_trig=fopen(trig_file,"r"))==NULL)
    {
    printf("%s(%d):'%s' not open\n",progname,getpid(),trig_file);
/*    if(time_flag) owari();*/
    owari();
    }
  else
    {
    if(*file_used && (fp=fopen(file_used,"r"))!=NULL)
      {
      fgets(textbuf,20,fp);
      fclose(fp);
      strncpy(lastline,textbuf,13);
      strcat(lastline," on,");
      }
    else if(time_flag==0)
      {
      while(fgets(lastline,LEN,fp_trig)!=NULL);
      if(*lastline) printf("last line = %s",lastline);
      }
    fclose(fp_trig);
    }
printf("lastline=%s\n",lastline);

  signal(SIGINT,(void *)owari);
  signal(SIGTERM,(void *)owari);

  init=1;
  while(1)
    {
    get_trig(trig_file,init,textbuf,lastline);  /* read trigger file */
    if(init) init=0;
    if(time_flag)
      {
      if(strncmp2(time_start,textbuf,strlen(time_start))>0) continue;
      else if(strncmp2(textbuf,time_end,strlen(time_end))>0) owari();
      }
    printf(">%s",textbuf);
  /* process "on" */
    *mess=(*mess1)=(*mess2)=0;
    sscanf(textbuf,"%2d%2d%2d.%2d%2d%2d%s%s%s",&tm1[0],&tm1[1],
      &tm1[2],&tm1[3],&tm1[4],&tm1[5],mess,mess1,mess2);
    if(strncmp(mess,"on",2)||strlen(mess2)==0)
      {
      printf("%s:illegal line : %s\n",progname,textbuf);
      continue;
      }
    sscanf(textbuf,"%s",file_name);
    strcpy(zone_list,mess2);    /* first triggered area */

    sprintf(textbuf,"%s/%s",out_path,"BUSY");
    fp=fopen(textbuf,"w+");
    fprintf(fp,"%s\n",file_name);
    fclose(fp);

    while(1)
      {
    /* read trigger file */
      get_trig(trig_file,init,textbuf,lastline);
      printf(">%s",textbuf);
    /* process "off" or "cont" */
      *mess=(*mess1)=0;
      sscanf(textbuf,"%2d%2d%2d.%2d%2d%2d%s%s",&tm2[0],&tm2[1],
        &tm2[2],&tm2[3],&tm2[4],&tm2[5],mess,mess1);
      if(strncmp(mess,"cont",2)==0) continue;
      if(strncmp(mess,"off",2)==0) break;
      else goto ill;
      }

    for(i=0;i<ix;i++) if(strncmp(mess2,xzones[i],7)==0) break;
    if(i<ix)
      {
      printf("trigger at %s ignored\n",xzones[i]);
      continue;
      }
    sec_len=extend_time(tm1,tm2,pre_event_sec,post_event_fac);
    check_space(out_path);

    if(*mess1)
      {
      strcat(zone_list," ");
      strcat(zone_list,strchr(textbuf,*mess1));
      zone_list[strlen(zone_list)-1]=0; /* CR -> NULL */
      }   /* other triggered areas */

    ptr=zone_list;
    ptr_lim=zone_list+strlen(zone_list);

  /* make list of zones including neighboring zones in "tmpfile1" */
    sprintf(textbuf,"sort -u>%s",tmpfile1);
    fp=popen(textbuf,"w");
    do {
      ptr=strtok(ptr," \t\n");
      fprintf(fp,"%s\n",ptr);
      fq=fopen(nxt_file,"r"); /* open "nxtzones.tbl" */
      while(fgets(textbuf,LEN,fq)!=NULL) {
        if(*textbuf=='#') continue;
        if((ptw=strtok(textbuf," \t\n"))==NULL) continue;
        if(strncmp(ptw,ptr,7)==0) {
          while(ptw=strtok(NULL," \t\n")) fprintf(fp,"%s\n",ptw);
        break;
        }
      }
      fclose(fq);
    } while((ptr+=strlen(ptr)+1)<ptr_lim);
    pclose(fp);

  /* make list of stations in "tmpfile2" */
    sprintf(textbuf,"sort -u>%s",tmpfile2);
    fr=popen(textbuf,"w");
    fq=fopen(tmpfile1,"r");
    while(fgets(textbuf,LEN,fq)!=NULL) {
      sscanf(textbuf,"%s",zone);
      fp=fopen(zone_file,"r"); /* open "zones.tbl" */
      while(fgets(textbuf,LEN,fp)!=NULL) {
        if((ptw=strtok(textbuf," \t\n"))==NULL) continue;
        while(*ptw=='#') ptw++;
        if(strncmp(ptw,zone,7)==0) {
          while(ptw=strtok(NULL," \t\n")) fprintf(fr,"%s\n",ptw);
        break;
        }
      }
      fclose(fp);
    }
    fclose(fq);
    pclose(fr);

  /* make station list file for each sys */
    if(!rtape) for(i=0;i<nsys;i++){
      if(strcmp(sys[i],sys_code)==0) continue; /* my sys */
      sprintf(textbuf,"sort %s|comm -12 - %s>%s/%s.%s.%d",
        stan_list[i],tmpfile2,temp_path,progname,sys[i],getpid());
      system(textbuf);
      sprintf(textbuf,"sort %s|comm -13 - %s>%s",
        stan_list[i],tmpfile2,tmpfile1);
      system(textbuf);
      sprintf(textbuf,"cp %s %s",tmpfile1,tmpfile2);
      system(textbuf);
    }

  /* "tmpfile2" contains list of stations excluding those of other sys */
  /* make channel list file "file_chs" and ".ch file" for my sys */
    fp=fopen(ch_file,"r"); /* "channels.tbl" file */
    sprintf(textbuf,"%s/%s.ch",out_path,file_name);
    fq=fopen(textbuf,"w+"); /* channel table file (.ch) */
    fr=fopen(tmpfile2,"r"); /* station list of my sys */
    fs=fopen(file_chs,"w+"); /* list of channels */

  /* make ".ch" file */
    while(fgets(textbuf,LEN,fp)){
      if(*textbuf=='#') continue;
      sscanf(textbuf,"%s%*s%*s%s",chs,stan);
      rewind(fr);
      while(fscanf(fr,"%s",zone)!=EOF){
        if(strcmp(stan,zone)) continue;
        fputs(textbuf,fq);
        fprintf(fs,"%s\n",chs);
        break;
      }
    }
    fclose(fr);
    fclose(fq);
    fclose(fp);
    fclose(fs);

    sprintf(textbuf,"%s/%s.ch",out_path,file_name);
    chmod(textbuf,0664);

  /* make data file */
    if(rtape)
      {
      fp=fopen(rtape_file,"w+");
      fprintf(fp,"%02d%02d%02d %02d%02d%02d %d\n",
        tm1[0],tm1[1],tm1[2],tm1[3],tm1[4],tm1[5],sec_len);
      fclose(fp);
      sprintf(textbuf,"%s -f %s %s %s %s",RTAPE,tapeunit,rtape_file,
        temp_path,file_chs);
#if DEBUG
      printf("'%s'\n",textbuf);
#endif
      if(system(textbuf)) owari();
      sprintf(textbuf,"mv %s/%02d%02d%02d.%02d%02d%02d %s/%s",
        temp_path,tm1[0],tm1[1],tm1[2],tm1[3],tm1[4],tm1[5],
        out_path,temp_name);
      system(textbuf);
      }
    else
      {
      for(i=0;i<6;i++) tm[i]=tm1[i];
      last_min=(-1);
      while(time_cmp(tm,tm2,6)<0)
        {
        if(tm[4]!=last_min)
          {
          if(last_min==-1)
            sprintf(textbuf1,
              "| %s %02d%02d%02d %02d%02d%02d %d %s > %s/%s",
              WED,tm1[0],tm1[1],tm1[2],tm1[3],tm1[4],tm1[5],
              sec_len,file_chs,out_path,temp_name);
          else
            sprintf(textbuf1,
              "| %s %02d%02d%02d %02d%02d%02d %d %s >> %s/%s",
              WED,tm1[0],tm1[1],tm1[2],tm1[3],tm1[4],tm1[5],
              sec_len,file_chs,out_path,temp_name);
          sprintf(raw_file,"%02d%02d%02d%02d.%02d",
            tm[0],tm[1],tm[2],tm[3],tm[4]);
        /* wait if LATEST is not yet */
          while(1)
            {
            sprintf(textbuf,"%s/LATEST",raw_path);
            if(fp=fopen(textbuf,"r"))
              {
              *textbuf=0;
              fgets(textbuf,20,fp);
              fclose(fp);
              if(*textbuf && strncmp2(raw_file,textbuf,11)<=0)
                break;
              }
            sleep(10);
            }
          sprintf(textbuf,"cat %s/%s %s",raw_path,raw_file,textbuf1);
#if DEBUG
printf("'%s'\n",textbuf);
#endif
          system(textbuf);
          last_min=tm[4];
          }     
        tm[5]++;
        adj_time(tm);
        }
      }

    sprintf(textbuf,"%s/%s",out_path,temp_name);
    stat(textbuf,&st_buf);
    sprintf(textbuf,"%s/%s",out_path,file_name);
    if(st_buf.st_size || stat(textbuf,&st_buf))
      {  /* resultant waveform file is not empty or dest file does not exist */
      sprintf(textbuf,"mv %s/%s %s/%s",out_path,temp_name,out_path,file_name);
      system(textbuf);
      sprintf(textbuf,"%s/%s",out_path,file_name);
      chmod(textbuf,0664);
      }
    else
      {
      sprintf(textbuf,"%s/%s",out_path,temp_name);
      unlink(textbuf);
      }

    if(!rtape) for(i=0;i<nsys;i++){
      if(strcmp(sys[i],sys_code)==0) continue; /* my sys */
      fp=fopen(lpr_body,"w+");
      fprintf(fp,"# time %02d%02d%02d.%02d%02d%02d %d\n",
        tm1[0],tm1[1],tm1[2],tm1[3],tm1[4],tm1[5],sec_len);
      fprintf(fp,"# station");

      sprintf(textbuf,"%s/%s.%s.%d",temp_path,progname,sys[i],getpid());
      fq=fopen(textbuf,"r");
      j=0;
      while(fscanf(fq,"%s",textbuf)!=EOF){
        fprintf(fp," %s",textbuf);
        j++;
      }
      fprintf(fp,"\n");
      fclose(fq);
      fprintf(fp,"# client %s\n",sys_code);
      fclose(fp);
      if(j==0) continue;
      
      sprintf(textbuf,"/usr/ucb/lpr -P%s<%s",lpr_name[i],lpr_body);
      system(textbuf);

      printf("%s: data requested to '%s'\n",progname,sys[i]);
      get_time(rt);
      fp=fopen(log_file,"a");
fprintf(fp,"%02d/%02d/%02d %02d:%02d:%02d %s %02d%02d%02d.%02d%02d%02d %d",
        rt[0],rt[1],rt[2],rt[3],rt[4],rt[5],sys[i],
        tm1[0],tm1[1],tm1[2],tm1[3],tm1[4],tm1[5],sec_len);
      sprintf(textbuf,"%s/%s.%s.%d",temp_path,progname,sys[i],getpid());
      fq=fopen(textbuf,"r");
      while(fscanf(fq,"%s",textbuf)!=EOF) fprintf(fp," %s",textbuf);
      fprintf(fp,"\n");
      fclose(fq);
      fclose(fp);

      sprintf(textbuf,"touch %s/%s/%s",request_path,sys[i],file_name);
      system(textbuf);
      sprintf(textbuf,"%s/%s/%s",request_path,sys[i],file_name);
      chmod(textbuf,0666);
    }

    sprintf(textbuf,"touch %s/%s",auto_path,file_name);
    system(textbuf);
    sprintf(textbuf,"%s/%s",auto_path,file_name);
    chmod(textbuf,0666);

    sprintf(textbuf,"%s/%s",out_path,"LATEST");
    fp=fopen(textbuf,"w+");
    fprintf(fp,"%s\n",file_name);
    fclose(fp);

    sprintf(textbuf,"%s/%s",out_path,"BUSY");
    fp=fopen(textbuf,"w+");
    fprintf(fp,"             \n");
    fclose(fp);

    sprintf(textbuf,"rm %s/%s.*.%d",temp_path,progname,getpid());
#if CLEAN
    system(textbuf);
#endif

    if(*file_used && (fp=fopen(file_used,"w+"))!=NULL)
      {
      fprintf(fp,"%s\n",file_name);
      fclose(fp);
      }
    printf("%s:'%s/%s' completed\n",progname,out_path,file_name);
    continue;

ill:  printf("%s:illegal line : %s\n",progname,textbuf);
    init=1;
    }
  }
