/*
  program "wtape.c"
  8/23/89 - 8/8/90, 6/27/91, 12/24/91, 2/29/92  urabe
  3/11/93 - 3/17/93, 7/5/93, 10/24/93, 5/19/94, 5/26/94, 6/1/94
  97.6.25 SIZE_MAX->1000000
  98.6.24 yo2000
  98.6.30 FreeBSD
  98.7.14 <sys/time.h>
  99.2.4  put signal(HUP) in switch_sig()
*/

#include  <stdio.h>
#include  <string.h>
#include  <fcntl.h>
#include  <sys/types.h>
#include  <signal.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>
#include  <sys/time.h>
#include  <sys/mtio.h>

#define   DEBUGFLAG 1
#define   SIZE_MAX  1000000
#define   NAMLEN    80
#define   N_EXABYTE 8

/*****************************/
/* 8/21/96 for little-endian (uehira) */
#ifndef         LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234    /* LSB first: i386, vax */
#endif
#ifndef         BIG_ENDIAN
#define BIG_ENDIAN      4321    /* MSB first: 68000, ibm, net */
#endif
#ifndef  BYTE_ORDER
#define  BYTE_ORDER      BIG_ENDIAN
#endif

#define SWAPU  union { long l; float f; short s; char c[4];} swap
#define SWAPL(a) swap.l=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPF(a) swap.f=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPS(a) swap.s=(a); ((char *)&(a))[0]=swap.c[1];\
    ((char *)&(a))[1]=swap.c[0]
/*****************************/

  unsigned char buf[SIZE_MAX];
  int init_flag,wfm,new_tape,switch_req,fd_exb,exb_status[N_EXABYTE],
    exb_busy,n_exb;
  char name_buf[NAMLEN],name_start[NAMLEN],exb_name[N_EXABYTE][20],
    file_done[NAMLEN],raw_dir[50],raw_dir1[50],log_file[80],
    raw_oldest[20],raw_latest[20];
  unsigned long exb_total;  /* in KB */

switch_sig()
  {
  switch_req=1;
  signal(SIGHUP,switch_sig);
  }

usleep(ms)  /* after T.I. UNIX MAGAZINE 1994.10 P.176 */
  unsigned int ms;
  {
  struct timeval tv;
  tv.tv_sec=ms/1000000;
  tv.tv_usec=ms%1000000;
  select(0,NULL,NULL,NULL,&tv);
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

mt_status(fd,type,ds,er,resid,flno)
  int fd,*type,*ds,*er,*resid,*flno;
  {
  int re;
  struct mtget exb_stat;
  re=ioctl(fd,MTIOCGET,(char *)&exb_stat);
/*printf("type=%d ds=%d er=%d resid=%d flno=%d\n",
exb_stat.mt_type,exb_stat.mt_dsreg,exb_stat.mt_erreg,
exb_stat.mt_resid,exb_stat.mt_fileno);
*/  *type=exb_stat.mt_type;   /* drive id */
  *ds=exb_stat.mt_dsreg;    /* 
  *er=exb_stat.mt_erreg;    /* sense key error */
  *resid=exb_stat.mt_resid; /* residual count (bytes not transferred) */
  *flno=exb_stat.mt_fileno; /* file number */
  return re;
  }

read_param(f_param,textbuf)
  FILE *f_param;
  unsigned char *textbuf;
  {
  do      {
    if(fgets(textbuf,200,f_param)==NULL) return 1;
    } while(*textbuf=='#');
  return 0;
  }

init_param()
  {
  char tb[100],*ptr;
  FILE *fp;
  int i,j;

  if((fp=fopen("wtape.prm","r"))==NULL)
    {
    fprintf(stderr,"parameter file 'wtape.prm' not found\007\n");
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
  int i,re,tm[5];
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
  char tbuf[80];
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
  char tbuf[80];
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
  FILE *fp;
  char tb[80];
#if DEBUGFLAG
  printf("***** wtape stop *****\n");
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
  char tb[50],tb1[50];
  int i;
  for(i=0;i<n_exb;i++) exb_status[i]=0;
  sprintf(tb,"%s/%s",raw_dir1,file);
    if((fp=fopen(tb,"r"))==NULL)
    {
    sprintf(tb1,"wtape: %s",tb);
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
  char tb[50];
  int i;
  sprintf(tb,"%s/%s",raw_dir1,file);
  fp=fopen(tb,"w+");
  for(i=0;i<n_exb;i++) if(exb_status[i]) fprintf(fp,"%d\n",i);
  fclose(fp);
  }

switch_unit(unit)
  int unit;
  {
  FILE *fp;
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
      sprintf(tb,"wtape: %s",exb_name[exb_busy]);
      perror(tb);
      switch_unit(-1);
      }
    }
  }

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned char *ptr1;
  unsigned long a;
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPU;
#endif
  ptr1=(unsigned char *)&a;
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1  =(*ptr);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(a);
#endif
  return a;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  FILE *fp;
  char tb[100];
  int i,j,re,cnt,key,io_error,unit,f_get,last_min,tm[5];
#if DEBUGFLAG
  printf("***** wtape start *****\n");
#endif
/* initialize parameters */
  init_param();

  signal(SIGTERM,ctrlc);      /* set up ctrlc routine */
  signal(SIGINT,ctrlc);     /* set up ctrlc routine */
  signal(SIGHUP,switch_sig);

/* get exabyte unit */
  
  if(argc>1)
    {
    sscanf(argv[1],"%d",&unit);
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
/* compare with the oldest */
    rmemo("OLDEST",raw_oldest);
    rmemo("LATEST",raw_latest);
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
      sprintf(tb,"wtape: %s",name_buf);
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
        if((re=write(fd_exb,buf,j))<j)
          {
#if DEBUGFLAG
          sprintf(tb,"wtape: write %d->%d",j,re);
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
