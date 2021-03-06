/* $Id: order.c,v 1.18 2019/11/06 05:20:17 urabe Exp $ */
/*  program "order.c" 1/26/94 - 2/7/94, 6/14/94 urabe */
/*                              1/6/95 bug in adj_time(tm[0]--) fixed */
/*                              3/17/95 write_log() */
/*                              5/24/96 check_time +/-30 min */
/*                              5/28/96 bcopy,usleep -> memcpy,sleep */
/*                              8/27/96 LITTLE ENDIAN (uehira) */
/*                              98.4.23 b2d[] etc. */
/*                              98.5.21 added passing etc. */
/*                              98.6.26 yo2000 */
/*                              99.4.19 byte-order-free */
/*                              2000.4.24/2001.11.14 strerror() */
/*                              2002.2.19-28 use mktime(), -l option after order2.c */
/*                              2002.3.1 option -a ; system clock mode */
/*                              2002.3.1 eobsize_in(auto), eobsize_out(-B) */
/*                              2002.5.2 i<1000 -> 1000000 */
/*                              2002.5.7 mktime2() */
/*                              2002.5.11 timezone for SVR4 */
/*                              2004.10.21 daemon mode (uehira) */
/*                              2009.12.18 64bit? (uehira) */
/*                              2010.11.3 system clock mode (-a) : fixed a bug (uehira) */
/*                              2019.11.2 use thread for late+sysclk (urabe) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

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

#include <errno.h>
#include <syslog.h>

#include "daemon_mode.h"
#include "winlib.h"

/*#define DEBUG     0*/
#define DEBUG1    0
#define DEBUG2    0

#define NAMELEN  1025

static const char rcsid[] =
  "$Id: order.c,v 1.18 2019/11/06 05:20:17 urabe Exp $";

static int  daemon_mode;
int tow_top;

char *progname,*logfile;
int  syslog_mode, exit_status;
struct St_late {
    struct Shm *sin;
    struct Shm *slt;
    int ns;
  };

/* prototypes */
static int check_time(uint8_w *);
static time_t advance(struct Shm *, size_t *, unsigned long *, uint32_w *,
		       time_t *, size_t *);
int main(int argc, char *argv[]);

static int
check_time(uint8_w *ptr)  /* 64bit ok*/
  {
  time_t ts,rt;

  if(!(ts=bcd_t(ptr))) return (1);
  rt=time(NULL);
  if(labs(rt-ts)>60*30) return (1); /* within 30 minutes ? */
  return (0);
  }

/*** potential bug is in this function --> ok? ***/
static time_t
advance(struct Shm *shm, size_t *shp, unsigned long *c_save,
	uint32_w *size, time_t *t, size_t *shp_busy_save)
/*  input ONLY:  shm */
/*  input & output:  size, shp, c_save */
/*  output ONLY:  t, shp_busy_save */
{
  long i;
  size_t  shpp;

  shpp=(*shp); /* copy read pointer */
  i=shm->c-(*c_save);
  if(!(i<1000000 && i>=0) || *size!=mkuint4(shm->d+(*shp))) return (-2);
  if((shpp+=(*size))>shm->pl) shpp=0; /* advance read pointer by one block */
  if(shm->p==shpp) {  /* block is still busy */
    if(shp_busy_save!=NULL) *shp_busy_save=shpp;
    return (0);
  }
  *c_save=shm->c;
  *size=mkuint4(shm->d+(*shp=shpp)); /* size of next block */
  if(!(*t=bcd_t(shm->d+shpp+8))) return (-1);
  
  /* return value = TOW */
  /* potential bug --> ok? */
  return ((time_t)((int32_w)mkuint4(shm->d+shpp+4)+TIME_OFFSET));
}

static void
usage(void)
{
  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if(daemon_mode)
    fprintf(stderr,
      " usage : '%s (-aB) (-l [shm_key_late]:[shm_size_late(KB)]) [shm_key_in] \\\n\
        [shm_key_out] [shm_size(KB)] [limit_sec] ([log file])'", progname);
  else
    fprintf(stderr,
      " usage : '%s (-aBD) (-l [shm_key_late]:[shm_size_late(KB)]) [shm_key_in] \\\n\
        [shm_key_out] [shm_size(KB)] [limit_sec] ([log file])'", progname);
  fprintf(stderr,"\n");
}

void
*th_late(void *stlt)
{
  uint32_w uni;
  time_t tow,ts; 
  size_t shp;
  uint32_w size,size2;
  uint8_w *ptr,*ptw;
  unsigned long c_save;
  char tbuf[NAMELEN];
  struct St_late *st_late;

  st_late=(struct St_late *)stlt;
reset:
  c_save=st_late->sin->c;
  size=mkuint4(ptr=st_late->sin->d+(shp=st_late->sin->r));
  if(check_time(ptr+8)){
    sleep(1);
    goto reset;
  }
  tow=tow_top=(int32_w)mkuint4(ptr+4)+TIME_OFFSET; /* TOW */ 
  ts=bcd_t(ptr+8); /* TS */
  if(st_late->slt==NULL) ptw=NULL;
  else ptw=st_late->slt->d+st_late->slt->p;

  for(;;) {
#if DEBUG1
    printf("th:tow=%ld ts=%ld shp=%ld\n",tow,ts,shp);
#endif
    if(st_late->slt!=NULL && tow>ts+st_late->ns) /* output as a late packet */
      {
      /* output late data */
      ptr=st_late->sin->d+shp;
      size2=size-4;
#if DEBUG
      printf("th:late %ld(%d) to %ld\n",ts,size2,st_late->slt->p);
#endif
      memcpy(ptw,ptr,(size_t)size2);
      ptw[0]=size2>>24;
      ptw[1]=size2>>16;
      ptw[2]=size2>>8;
      ptw[3]=size2;
      uni=(uint32_w)(time(NULL)-TIME_OFFSET);
      ptw[4]=uni>>24;
      ptw[5]=uni>>16;
      ptw[6]=uni>>8;
      ptw[7]=uni;
      ptw+=size2;
      st_late->slt->r=st_late->slt->p;
      if(ptw>st_late->slt->d+st_late->slt->pl) ptw=st_late->slt->d;
      st_late->slt->p=ptw-st_late->slt->d;
      st_late->slt->c++;
    }
    while((tow=advance(st_late->sin,&shp,&c_save,&size,&ts,NULL))<=0) {
#if DEBUG1
      if(tow) printf("th:tow=%ld\n",tow);
#endif
      if(tow==(-2)) { /* shm corrupted */
        ptr=st_late->sin->d+shp+8;
        snprintf(tbuf,sizeof(tbuf),"reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
           ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
        write_log(tbuf);
        sleep(1);
        goto reset;
      }
      usleep(10000);
    } 
    tow_top=tow;
  }
}

int
main(int argc, char *argv[])
{
  key_t shm_key_in,shm_key_out,shm_key_late=0;
  uint32_w uni;
  WIN_bs   uni2;
  time_t t,t_out,t_bottom,tow,ts,rt,rt_next;
  size_t  sizei, sizej=0, shp_in, shp;
  unsigned long  c_save, c_save_in;
  uint32_w  size, size_in, size2;
  /* int32_w  sec_1; */
  time_t  sec_1;
  int sec,late,c,pl_out,n_sec,no_data,sysclk,
    timeout_flag=0,size_next,eobsize_in,eobsize_out,i,eobsize_in_count;
  uint8_w *ptr,*ptr_save,*ptw,*ptw_save,*ptw_late=NULL,*ptr_prev,tm_out[6];
  char tbuf[NAMELEN], *ptrc;
  struct Shm *shm_in,*shm_out,*shm_late=NULL;
  size_t shp_busy_save;
  pthread_t thid;
  struct St_late st_late;
  /* extern int optind; */
  /* extern char *optarg; */

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;
  if(strcmp(progname,"orderd")==0) daemon_mode=1;

  sysclk=late=eobsize_in=eobsize_out=0;
  while((c=getopt(argc,argv,"aBDl:"))!=-1)
    {
    switch(c)
      {
      case 'a':   /* reference to absolute time (i.e. the system clock) */
        sysclk=1;
        break;
      case 'B':   /* write blksiz at EOB in shm_out */
        eobsize_out=1;
        break;
      case 'D':
	daemon_mode = 1;  /* daemon mode */
	break;   
      case 'l':   /* output late packets (i.e. order2 mode) */
        /* strcpy(tbuf,optarg); */
	if (snprintf(tbuf, sizeof(tbuf), "%s", optarg) >= sizeof(tbuf)) {
	  fprintf(stderr,"'%s': -l option : Buffer overrun!\n",progname);
	  exit(1);
	}
        if((ptrc=strchr(tbuf,':'))==0)
          {
          fprintf(stderr,"-l option requires [shm_key_late]:[shm_size_late(KB)]\n");
          exit(1);
          }
        else
          {
          *ptrc=0;
          shm_key_late=atol(tbuf);
          sizej=(size_t)atol(ptrc+1)*1000;
          late=1;
          }
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
	usage();
        exit(1);
      }
    }
  optind--;
  if(argc<5+optind)
    {
    usage();
    exit(1);
    }
   
  shm_key_in=atol(argv[1+optind]);
  shm_key_out=atol(argv[2+optind]);
  sizei=(size_t)atol(argv[3+optind])*1000;
  n_sec=atoi(argv[4+optind]);
  if(argc>5+optind) logfile=argv[5+optind];
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
   
  /* shared memory */
  shm_in = Shm_read(shm_key_in, "in");
  /* if((shmid_in=shmget(shm_key_in,0,0))<0) err_sys("shmget"); */
  /* if((shm_in=(struct Shm *)shmat(shmid_in,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat"); */
  /* sprintf(tbuf,"in : shm_key_in=%ld id=%d",shm_key_in,shmid_in); */
  /* write_log(tbuf); */
   
  shm_out = Shm_create(shm_key_out, sizei, "out");
  /* if((shmid_out=shmget(shm_key_out,sizei,IPC_CREAT|0666))<0) err_sys("shmget"); */
  /* if((shm_out=(struct Shm *)shmat(shmid_out,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat"); */
  /* sprintf(tbuf,"out: shm_key_out=%ld id=%d size=%ld", */
  /* 	  shm_key_out,shmid_out,sizei); */
  /* write_log(tbuf); */

  /* initialize buffer */
  Shm_init(shm_out, sizei);
  pl_out = shm_out->pl;
  /*   shm_out->p=shm_out->c=0; */
  /*   shm_out->pl=pl_out=(sizei-sizeof(*shm_out))/10*9; */
  /*   shm_out->r=(-1); */

  if(late) {
    shm_late = Shm_create(shm_key_late, sizej, "late");
    /* if((shmid_late=shmget(shm_key_late,sizej,IPC_CREAT|0666))<0) */
    /*   err_sys("shmget"); */
    /* if((shm_late=(struct Shm *)shmat(shmid_late,(void *)0,0))==(struct Shm *)-1) */
    /*   err_sys("shmat"); */
    /* sprintf(tbuf,"late: shm_key_late=%ld id=%d size=%ld", */
    /*       shm_key_late,shmid_late,sizej); */
    /* write_log(tbuf); */
    Shm_init(shm_late, sizej);
    /*     shm_late->p=shm_late->c=0; */
    /*     shm_late->pl=(sizej-sizeof(*shm_late))/10*9; */
    /*     shm_late->r=(-1); */
  }
 
  signal(SIGPIPE,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGTERM,(void *)end_program);

reset:
  while(shm_in->r==(-1)) sleep(1);
  c_save_in=shm_in->c;
  size_in=mkuint4(ptr_save=shm_in->d+(shp_in=shm_in->r));
  if(check_time(ptr_save+8)){
     sleep(1);
     goto reset;
  }
  sec_1=(int32_w)mkuint4(ptr_save+4)+TIME_OFFSET; /* TOW */ /* potential bug --> ok? */
  t_out=t_bottom=bcd_t(ptr_save+8); /* TS */
  if(late) {
    shp_busy_save=(-1);
    timeout_flag=0;
  }

  if(mkuint4(ptr_save+size_in-4)==size_in) eobsize_in=1;
  else {
    eobsize_in=0;
    if(sysclk) {
    write_log("wrong input SHM format (no eobsize).");
    exit(1);
    }
  }

  eobsize_in_count=eobsize_in;
  snprintf(tbuf,sizeof(tbuf),"eobsize_in=%d, eobsize_out=%d, sysclk=%d, n_sec=%d",
    eobsize_in,eobsize_out,sysclk,n_sec);
  write_log(tbuf);

  if(sysclk) { /* system clock mode */
    /* create late thread */
    st_late.sin=shm_in;
    if(late) st_late.slt=shm_late;
    else st_late.slt=NULL;
    st_late.ns=n_sec;
    if(pthread_create(&thid,NULL,th_late,(void *)&st_late) != 0) {
      perror("pthread_create() error");
      exit(1);
    } 
    rt=time(NULL);
    /* begin main loop */
    for(;;) {
      while(rt>=time(NULL)) usleep(10000);
      rt++;
      while(tow_top<rt) usleep(10000);
      t_out=rt-1-n_sec; /* focus on sec blocks with last tow */

      ptr=shm_in->d+shm_in->r; /* latest pointer */
      ptw_save=ptw=shm_out->d+shm_out->p;
      ptw+=4; /* block size */
      t_bcd(t_out,ptw); /* write TS */
      ptw+=6; /* TS */
      i=0;
#if DEBUG2
      printf("ma:rt=%ld time=%ld tow=%ld t_out=%ld\nma:",
        rt,time(NULL),tow_top,t_out);
#endif
      for(;;) { /* sweep to output data at ts==rt-n_sec */
        size=mkuint4(ptr);
        tow=(int32_w)mkuint4(ptr+4)+TIME_OFFSET;
        ts=bcd_t(ptr+8);
        if(tow<t_out-1) { /* quit loop - TOW too old */
#if DEBUG
          printf("\nma:quit>ptr=%zd size=%u tow=%ld t_out=%ld ts=%ld rt=%ld\n",
            ptr-shm_in->d,size,tow,t_out,ts,rt);
#endif
          break;
        }
        if(ts==t_out && tow<=ts+n_sec) {
#if DEBUG
          printf("out %zd(%u)>%zd ",ptr-shm_in->d,size-(4+4+6+4),ptw-shm_out->d);
#endif
          memcpy(ptw,ptr+(4+4+6),size-(4+4+6+4));
          ptw+=size-(4+4+6+4);
        }
        ptr_prev=ptr;
        if(ptr-shm_in->d>0) { /* go back one packet */
          size_next=mkuint4(ptr-4);
          ptr-=size_next;
        } else if(ptr==shm_in->d) { /* return to the tail of SHM */
          size_next=mkuint4(shm_in->d+shm_in->pl);
          ptr=shm_in->d+shm_in->pl+4-size_next;
        } else /* illegal SHM data */
          break; /* abort this t_out */
        if(ptr<shm_in->d) break; /* abort this t_out */
        if(size_next!=mkuint4(ptr)) {
          if(i) break; /* abort this t_out */
          else goto reset;
        }
        i++; /* count blocks */
      }  /* for(;;) sweep */

      if((uni2=(WIN_bs)(ptw-ptw_save))>10) { 
        if(eobsize_out) uni2+=4;
        ptw_save[0]=uni2>>24;
        ptw_save[1]=uni2>>16;
        ptw_save[2]=uni2>>8;
        ptw_save[3]=uni2;
        if(eobsize_out) {
          ptw[0]=uni2>>24;
          ptw[1]=uni2>>16;
          ptw[2]=uni2>>8;
          ptw[3]=uni2;
          ptw+=4;
        }
        shm_out->r=shm_out->p;
        if(eobsize_out && ptw>shm_out->d+pl_out)
          {shm_out->pl=ptw-shm_out->d-4;ptw=shm_out->d;}
        if(!eobsize_out && ptw>shm_out->d+shm_out->pl) ptw=shm_out->d;
        shm_out->p=ptw-shm_out->d;
        shm_out->c++;
#if DEBUG
        printf("ma:eob %d B > %zu next=%zu\n",uni2,shm_out->r,shm_out->p);
#endif
      } /* output */
    }  /* main loop */
  } /* if(sysclk) */
  else { /* conventional mode */
    for(;;){
      no_data=1;
      while(sec_1+n_sec>time(NULL)) sleep(1);
      shp=shp_in;
      c_save=c_save_in;
      size=size_in;
      ptw_save=ptw=shm_out->d+shm_out->p;
      ptw+=4;
      if(late) ptw_late=shm_late->d+shm_late->p;
      t=t_bottom;
      
      do{
	if(size==mkuint4(shm_in->d+shp+size-4)) {
	  if (++eobsize_in_count == 0) eobsize_in_count = 1;
	}
	else eobsize_in_count=0;
	if(eobsize_in && eobsize_in_count==0) goto reset;
	if(!eobsize_in && eobsize_in_count>3) goto reset;
	
	if(eobsize_in) size2=size-4;
	else size2=size;
	
	if(t==t_out){
	  ptr=shm_in->d+shp+8; /* points to YY-ss */
	  if(no_data){
	    memcpy(ptw,ptr,size2-8);
	    ptw+=size2-8;
	    no_data=0;
	  }
	  else{
	    memcpy(ptw,ptr+6,size2-14); /* skip YY-ss */
	    ptw+=size2-14;
	  }
	}
	
	if(late && shp==shp_busy_save) timeout_flag=1;
	if(late && timeout_flag){
	  if(t<t_out){
	    /* output late data */
	    ptr=shm_in->d+shp;
	    memcpy(ptw_late,ptr,(size_t)size2);
	    ptw_late[0]=size2>>24;
	    ptw_late[1]=size2>>16;
	    ptw_late[2]=size2>>8;
	    ptw_late[3]=size2;
	    uni=time(NULL);
	    ptw_late[4]=uni>>24;
	    ptw_late[5]=uni>>16;
	    ptw_late[6]=uni>>8;
	    ptw_late[7]=uni;
	    ptw_late+=size2;
	    shm_late->r=shm_late->p;
	    if(ptw_late>shm_late->d+shm_late->pl) ptw_late=shm_late->d;
	    shm_late->p=ptw_late-shm_late->d;
	    shm_late->c++;
	  }
	}
	
	while((sec=advance(shm_in,&shp,&c_save,&size,&t,&shp_busy_save))==(-1));
	if(sec==(-2)){
	  ptr=shm_in->d+shp+8;
	  snprintf(tbuf,sizeof(tbuf),
		   "reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
		   ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
	  write_log(tbuf);
	  goto reset;
	}
      } while(sec>0);
      
      if(late) timeout_flag=0;
      
      if((uni2=(WIN_bs)(ptw-ptw_save))>10){
	if(eobsize_out) uni2+=4;
	ptw_save[0]=uni2>>24;
	ptw_save[1]=uni2>>16;
	ptw_save[2]=uni2>>8;
	ptw_save[3]=uni2;
	if(eobsize_out)
	  {
	    ptw[0]=uni2>>24;
	    ptw[1]=uni2>>16;
	    ptw[2]=uni2>>8;
	    ptw[3]=uni2;
	    ptw+=4;
	  }
	shm_out->r=shm_out->p;
	if(eobsize_out && ptw>shm_out->d+pl_out)
	  {shm_out->pl=ptw-shm_out->d-4;ptw=shm_out->d;}
	if(!eobsize_out && ptw>shm_out->d+shm_out->pl) ptw=shm_out->d;
	shm_out->p=ptw-shm_out->d;
	shm_out->c++;
      }
      
      t_out++;
      while(t_bottom!=t_out){
	if(sec_1+n_sec+4>time(NULL) && t_bottom>t_out) break;
	if(t_bottom>t_out){
	  ptr=shm_in->d+shp_in+8;
	  t_bcd(t_out,tm_out);
	  snprintf(tbuf,sizeof(tbuf),
	   "passing %02X%02X%02X.%02X%02X%02X:%02X%02X(out=%02X%02X%02X.%02X%02X%02X)",
              ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
              tm_out[0],tm_out[1],tm_out[2],tm_out[3],tm_out[4],tm_out[5]);
	  write_log(tbuf);
	}
	while((sec_1=advance(shm_in,&shp_in,&c_save_in,&size_in,&t_bottom,NULL))<=0){
	  if(late && shp_in==shp_busy_save) shp_busy_save=(-1);
	  if(sec_1==(-1)) continue;
	  else if(sec_1==0){
	    sleep(1);
	    continue;
	  }
	  else if(sec_1==(-2)){
	    ptr=shm_in->d+shp_in+8;
	    snprintf(tbuf,sizeof(tbuf),
              "reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
              ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
	    write_log(tbuf);
	    goto reset;
	  }
	} /* while((sec_1=advance() */
      } /* while(t_bottom!=t_out) */
    }  /* for(;;) */
  } /* conventional mode */
}
