/* $Id: wtape.c,v 1.11 2004/01/29 02:31:36 urabe Exp $ */
/*
  program "wtape.c"
  8/23/89 - 8/8/90, 6/27/91, 12/24/91, 2/29/92  urabe
  3/11/93 - 3/17/93, 7/5/93, 10/24/93, 5/19/94, 5/26/94, 6/1/94
  97.6.25 SIZE_MAX->1000000
  98.6.24 yo2000
  98.6.30 FreeBSD
  98.7.14 <sys/time.h>
  99.2.4  put signal(HUP) in switch_sig()
  99.4.19 byte-order-free
  2000.4.17 deleted definition of usleep()
  2001.6.21 add options '-p', '-d' and '?'   (uehira)
  2004.1.20 avoid N*64*1024 byte block
            SIZE_MAX  1000000->2000000
  2004.1.29 avoid odd byte block size
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <fcntl.h>
#include  <sys/types.h>
#include  <signal.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>

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

#include  <sys/mtio.h>
#include  <unistd.h>

#include "subst_func.h"

#define   DEBUGFLAG 1
#define   SIZE_MAX  2000000
#define   NAMLEN    80
#define   N_EXABYTE 8
#define   DEFAULT_WAIT_MIN  0
#define   DEFAULT_PARAM_FILE  "wtape.prm"
#define   WIN_FILENAME_MAX 1024

  unsigned char buf[SIZE_MAX];
  int init_flag,wfm,new_tape,switch_req,fd_exb,exb_status[N_EXABYTE],
    exb_busy,n_exb;
  char name_buf[WIN_FILENAME_MAX],name_start[NAMLEN],exb_name[N_EXABYTE][20],
    file_done[WIN_FILENAME_MAX],raw_dir[WIN_FILENAME_MAX],
    raw_dir1[WIN_FILENAME_MAX],
    log_file[WIN_FILENAME_MAX],raw_oldest[20],raw_latest[20];
  unsigned long exb_total;  /* in KB */
  int  wait_min;
  char param_name[WIN_FILENAME_MAX];
  char *progname;
  static char rcsid[]="$Id: wtape.c,v 1.11 2004/01/29 02:31:36 urabe Exp $";

switch_sig()
  {
  switch_req=1;
  signal(SIGHUP,(void *)switch_sig);
  }

strncmp2(s1,s2,i)
char *s1,*s2;             
int i; 
{
  if(*s1=='0' && *s2=='9') return 1;
  else if(*s1=='9' && *s2=='0') return -1;
  else return strncmp(s1,s2,i);
}

strcmp2(s1,s2)        
char *s1,*s2;
{
  if(*s1=='0' && *s2=='9') return 1;
  else if(*s1=='9' && *s2=='0') return -1;
  else return strcmp(s1,s2);
}

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  end_process(1);
  }

get_unit(unit)    /* get exabyte unit */
  int unit;
  {
  int j;
  read_units("_UNITS");
  for(j=0;j<n_exb;j++) if(exb_status[j]) break;
  if(j==n_exb) return -1;
  if(unit<0 || unit>N_EXABYTE-1) unit=0;
  while(exb_status[unit]==0) if(++unit==n_exb) unit=0;
  return(unit);
  }

mt_doit(fd,ope,count)
  int fd,ope,count;
  {
  struct mtop exb_param;
  exb_param.mt_op=ope;
  exb_param.mt_count=count;
  return ioctl(fd,MTIOCTOP,(char *)&exb_param);
  }

/* mt_status(fd,type,ds,er,resid,flno) */
/*   int fd,*type,*ds,*er,*resid,*flno; */
/*   { */
/*   int re; */
/*   struct mtget exb_stat; */
/*   re=ioctl(fd,MTIOCGET,(char *)&exb_stat); */
/* /*printf("type=%d ds=%d er=%d resid=%d flno=%d\n", */
/* exb_stat.mt_type,exb_stat.mt_dsreg,exb_stat.mt_erreg, */
/* exb_stat.mt_resid,exb_stat.mt_fileno); */
/* */   
/*   *type=exb_stat.mt_type;   /* drive id */ 
/*   *ds=exb_stat.mt_dsreg;    /* */ 
/*   *er=exb_stat.mt_erreg;    /* sense key error */ 
/*   *resid=exb_stat.mt_resid; /* residual count (bytes not transferred) */ 
/*   *flno=exb_stat.mt_fileno; /* file number */
/*   return re; */
/*   } */

read_param(f_param,textbuf)
  FILE *f_param;
  unsigned char *textbuf;
  {
  do      {
    if(fgets(textbuf,(WIN_FILENAME_MAX<<1)+32,f_param)==NULL) return 1;
    } while(*textbuf=='#');
  return 0;
  }

init_param()
  {
  char tb[(WIN_FILENAME_MAX<<1)+32],*ptr;
  FILE *fp;

  if((fp=fopen(param_name,"r"))==NULL)
    {
    fprintf(stderr,"parameter file '%s' not found\007\n",param_name);
#if DEBUGFLAG
    printf("***** %s abort *****\n\n",progname);
#endif
    usage();
    exit(1);
    }
  read_param(fp,tb);
  if((ptr=strchr(tb,':'))==0)
    {
    sscanf(tb,"%s",raw_dir);
    sscanf(tb,"%s",raw_dir1);
    }
  else
    {
    *ptr=0;
    sscanf(tb,"%s",raw_dir);
    sscanf(ptr+1,"%s",raw_dir1);
    }
  read_param(fp,tb);
  sscanf(tb,"%s",log_file);
        for(n_exb=0;n_exb<N_EXABYTE;n_exb++)
    {
    if(read_param(fp,tb)) break;
    sscanf(tb,"%s",exb_name[n_exb]);
    }

/* read exabyte mask file $raw_dir1/UNITS */
  read_units("UNITS");
  write_units("_UNITS");
  sprintf(file_done,"%s/%s",raw_dir1,"USED");
  init_flag=1;
  wfm=0;
  }

/* read rsv file */
read_rsv(tm)
  int *tm;
  {
  FILE *fp;
  tm[0]=tm[1]=tm[2]=tm[3]=tm[4]=0;
  if((fp=fopen(file_done,"r"))!=NULL)
    {
    fscanf(fp,"%2d%2d%2d%2d.%2d",&tm[0],&tm[1],&tm[2],&tm[3],&tm[4]);
    fclose(fp);
    }
  }

/* write rsv file */
write_rsv(tm)
  int *tm;
  {
  FILE *fp;
  fp=fopen(file_done,"w+");
  fprintf(fp,"%02d%02d%02d%02d.%02d\n",tm[0],tm[1],tm[2],tm[3],tm[4]);
  fclose(fp);
  }

adj_time(tm)
  int *tm;
  {
  if(tm[4]==60)
    {
    tm[4]=0;
    if(++tm[3]==24)
      {
      tm[3]=0;
      tm[2]++;
      switch(tm[1])
        {
        case 2:
          if(tm[0]%4)
            {
            if(tm[2]==29)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
            }
          else
            {
            if(tm[2]==30)
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
  else if(tm[4]==-1)
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

ctrlc()
  {
  close_exb("ON INT");
  end_process(0);
  }

close_exb(tb)
  char *tb;
  {
  FILE *fp;
  int re,tm[5];
  /* write exabyte logging file */
  fp=fopen(log_file,"a+");
  if(exb_total)
    {
    read_rsv(tm);
    fprintf(fp,"%02d/%02d/%02d %02d:%02d  %4d MB  %s\n",
      tm[0],tm[1],tm[2],tm[3],tm[4],
      (exb_total+512)/1024,tb);
    }
  else fprintf(fp,"\n");
  fclose(fp);
  /* write end mark */
  re=(-1);
  write(fd_exb,&re,4);
  if(wfm==0) mt_doit(fd_exb,MTWEOF,1);
  mt_doit(fd_exb,MTOFFL,1);   /* rewind and unload */
  close(fd_exb);
  }

rmemo(f,c)
    char *f,*c;
    {
    FILE *fp;
    char tbuf[WIN_FILENAME_MAX];
    sprintf(tbuf,"%s/%s",raw_dir,f);
    while((fp=fopen(tbuf,"r"))==NULL)
    {
    fprintf(stderr,"file '%s' not found\007\n",tbuf);
    sleep(1);
    }
    fscanf(fp,"%s",c);
    fclose(fp);
    }

wmemo(f,c)
    char *f,*c;
    {
    FILE *fp;
    char tbuf[WIN_FILENAME_MAX];
    sprintf(tbuf,"%s/%s",raw_dir1,f);
    fp=fopen(tbuf,"w+");
    fprintf(fp,"%s\n",c);
    fclose(fp);
    }

wmemon(f,c)
    char *f;
    int c;
    {
    char tbuf[10];
    sprintf(tbuf,"%d",c);
    wmemo(f,tbuf);
  }

end_process(code)
  int code;
  {

#if DEBUGFLAG
  printf("***** %s stop *****\n",progname);
#endif
  exb_status[exb_busy]=0;
  write_units("_UNITS");
  wmemon("EXABYTE",-1);
  exit(code);
  }

read_units(file)
  char *file;
  {
  FILE *fp;
  char tb[WIN_FILENAME_MAX],tb1[WIN_FILENAME_MAX];
  int i;
  for(i=0;i<n_exb;i++) exb_status[i]=0;
  sprintf(tb,"%s/%s",raw_dir1,file);
  if((fp=fopen(tb,"r"))==NULL)
    {
    sprintf(tb1,"%s: %s",progname,tb);
    perror(tb1);
    for(i=0;i<n_exb;i++) exb_status[i]=1;
    }
  else while(read_param(fp,tb)==0)
    {
    sscanf(tb,"%d",&i);
    if(i<n_exb && i>=0) exb_status[i]=1;
    }
  fclose(fp);
  }

write_units(file)
  char *file;
  {
  FILE *fp;
  char tb[WIN_FILENAME_MAX];
  int i;
  sprintf(tb,"%s/%s",raw_dir1,file);
  fp=fopen(tb,"w+");
  for(i=0;i<n_exb;i++) if(exb_status[i]) fprintf(fp,"%d\n",i);
  fclose(fp);
  }

switch_unit(unit)
  int unit;
  {
  char tb[100];
  int re,i;
  read_units("_UNITS");
  switch_req=0;
  if(unit<0 || unit>=n_exb)
    {
    exb_status[exb_busy]=0;
    write_units("_UNITS");
    unit=exb_busy;
    if(++unit==n_exb) unit=0;
    }
  if((re=get_unit(unit))<0)
    {
    fprintf(stderr,"NO EXABYTE UNITS AVAILABLE !!\n");
    for(i=0;i<10;i++)
      {
      fprintf(stderr,"\007");
      usleep(100000);
      }
    end_process(1);
    }
  wmemon("EXABYTE",exb_busy=re);
  new_tape=1;
  wmemon("TOTAL",exb_total=0);
#if DEBUGFLAG
  printf("new exb unit #%d (%s)\n",exb_busy,exb_name[exb_busy]);
#endif
  if((fd_exb=open(exb_name[exb_busy],O_WRONLY))==-1)
    {
    if((fd_exb=open(exb_name[exb_busy],O_WRONLY))==-1)
      {
      sprintf(tb,"%s: %s",progname,exb_name[exb_busy]);
      perror(tb);
      switch_unit(-1);
      }
    }
  }

mklong(ptr)       
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;       
  }

usage()
{

  fprintf(stderr,"%s\n",rcsid);
  fprintf(stderr,
	  "Usage: %s (options) ([unit])\n",
	  progname);
  fprintf(stderr, "List of options:\n");
  fprintf(stderr,
	  " -p [param file]   set parameter file(default:wtape.prm)\n");
  fprintf(stderr,
	  " -d [delay]        set delay in minute(default:0)\n");
  fprintf(stderr,
	  " -?                print this message\n");
  exit(1);
}

main(argc,argv)
  int argc;
  char *argv[];
  {
  FILE *fp;
  char tb[100];
  int i,j,re,cnt,io_error,unit,f_get,last_min,tm[5];
  int ch,max_num,tm1[5],k;
  char max_c[100];
  extern int optind;
  extern char *optarg;

  if((progname=strrchr(argv[0],'/'))) progname++;
  else progname=argv[0];
    
#if DEBUGFLAG
  printf("***** %s start *****\n",progname);
#endif

  wait_min=0;
  sprintf(param_name,"%s",DEFAULT_PARAM_FILE);
  while((ch=getopt(argc,argv,"p:d:?"))!=-1)
    {
      switch (ch)
	{
	case 'p':   /* parameter file */
	  strcpy(param_name,optarg);
	  break;
	case 'd':   /* delay minute */
	  wait_min=atoi(optarg);
	  break;
	case '?':
	default:
	  usage();
	}
    }
  argc-=optind;
  argv+=optind;

/* initialize parameters */
  init_param();

  signal(SIGTERM,(void *)ctrlc);      /* set up ctrlc routine */
  signal(SIGINT,(void *)ctrlc);     /* set up ctrlc routine */
  signal(SIGHUP,(void *)switch_sig);

  rmemo("COUNT",max_c);
  max_num=atoi(max_c);
  if(wait_min<0 || wait_min>max_num-70)
    {
      fprintf(stderr,"Invalid delay minute (0<=[delay]<=%d).\n",
	      max_num-70,DEFAULT_WAIT_MIN);
      end_process(1);
    } 

/* get exabyte unit */
  if(argc>0)
    {
    sscanf(argv[0],"%d",&unit);
    if(unit<0 || unit>=n_exb) unit=0;
    }
  else unit=0;
  switch_unit(unit);

/* read wtape.rsv */
  read_rsv(tm);
  while(1)
    {
    tm[4]++;
    adj_time(tm);
    sprintf(name_start,"%02d%02d%02d%02d.%02d",
        tm[0],tm[1],tm[2],tm[3],tm[4]);
    rmemo("OLDEST",raw_oldest);
    rmemo("LATEST",raw_latest);
    if(wait_min)
      {
	sscanf(raw_latest,"%02d%02d%02d%02d.%02d",
	       &tm1[0],&tm1[1],&tm1[2],&tm1[3],&tm1[4]);
	for(k=0;k<wait_min;++k)
	  {
	    tm1[4]--;
	    adj_time(tm1);
	  }
	sprintf(raw_latest,"%02d%02d%02d%02d.%02d",
		tm1[0],tm1[1],tm1[2],tm1[3],tm1[4]);
      }
/* compare with the oldest */
    if(strcmp2(name_start,raw_oldest)<=0)
      {
      strcpy(name_start,raw_oldest);
      sscanf(name_start,"%2d%2d%2d%2d.%2d",
        &tm[0],&tm[1],&tm[2],&tm[3],&tm[4]);
/*      tm[4]++;
      adj_time(tm);
*/      sprintf(name_start,"%02d%02d%02d%02d.%02d",
        tm[0],tm[1],tm[2],tm[3],tm[4]);
#if DEBUGFLAG
      printf("next = %s\n",name_start);
#endif
      }
/* compare with the latest */
    else
      {
#if DEBUGFLAG
      printf("next = %s\n",name_start);
#endif
      if(strcmp2(name_start,raw_latest)>0) /* enter wait */
        while(strncmp2(name_start,raw_latest,8)>=0)
          { /* wait approx. 1 h */
          if(switch_req)
            {
            close_exb("BY REQ");
            switch_unit(-1);
            }
          else sleep(60);
          rmemo("LATEST",raw_latest);
	  if(wait_min)
	    {
	      sscanf(raw_latest,"%02d%02d%02d%02d.%02d",
		     &tm1[0],&tm1[1],&tm1[2],&tm1[3],&tm1[4]);
	      for(k=0;k<wait_min;++k)
		{
		  tm1[4]--;
		  adj_time(tm1);
		}
	      sprintf(raw_latest,"%02d%02d%02d%02d.%02d",
		      tm1[0],tm1[1],tm1[2],tm1[3],tm1[4]);
	    }
          }
      }
    if(switch_req)
      {
      close_exb("BY REQ");
      switch_unit(-1);
      }

/* open disk file */
    sprintf(name_buf,"%s/%s",raw_dir,name_start);
    if((f_get=open(name_buf,O_RDONLY))==-1)
      {
#if DEBUGFLAG
      sprintf(tb,"%s: %s",progname,name_buf);
      perror(tb);
#endif
      continue;
      }

    while(1)
      {
      cnt=0;
      io_error=0;
      for(i=0;i<60;i++)
        {
        /* read one sec */
        re=read(f_get,(char *)buf,4); /* read size */
        j=mklong(buf);      /* record size */
        if(j<4 || j>SIZE_MAX) break;
        re=read(f_get,(char *)buf+4,j-4); /* read rest (data) */
        if(re==0) break;
        if((buf[8]&0xf0)!=(last_min&0xf0) && init_flag==0)
          {
#if DEBUGFLAG
          printf("write fm\n");
#endif
          mt_doit(fd_exb,MTWEOF,1);
          wfm=1;
          lseek(fd_exb,0,0); /* for SUN-OS bug */
          }
        last_min=buf[8];
/*
#if DEBUGFLAG
        printf("%4x:",j);
        fflush(stdout);
#endif
*/
        /* write one sec */
        if(j%2) j++; /* avoid odd byte block size */
        if(j%(64*1024)==0) j+=2; /* avoid N*64KB block size */
        if((re=write(fd_exb,buf,j))<j)
          {
#if DEBUGFLAG
          sprintf(tb,"%s: write %d->%d",progname,j,re);
          perror(tb);
#endif
          io_error=1;
          break;
          }
        init_flag=0;
        wfm=0;
#if DEBUGFLAG
        printf(".");
        fflush(stdout);
#endif
        if(re>0) wmemon("TOTAL",exb_total+=((re+512)/1024));
        cnt+=j;
        }
      if(io_error)
        {
        lseek(f_get,0,0); /* rewind file */
        close_exb("ON ERROR");
        switch_unit(-1);  /* change exabyte */
        continue;
        }
      break;
      }

/* close disk file */
#if DEBUGFLAG
    printf("\n");
#endif
    close(f_get);
    if(new_tape)
      {
      fp=fopen(log_file,"a+");
      fprintf(fp,"unit %d  %02X/%02X/%02X %02X:%02X  -->  ",
        exb_busy,buf[4],buf[5],buf[6],buf[7],buf[8]);
      fclose(fp);
      new_tape=0;
      }
    write_rsv(tm);
#if DEBUGFLAG
    printf("%s < %d   total= %d KB\n",name_buf,cnt,exb_total);
#endif
    if(switch_req)
      {
      close_exb("BY REQ");
      switch_unit(-1);  /* change exabyte */
      }
    }
  }
