/* $Id: order.c,v 1.2 2000/04/30 10:05:22 urabe Exp $ */
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
/*                              2000.4.24 strerror() */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>

#define SWAPL(a) a=(((a)<<24)|((a)<<8)&0xff0000|((a)>>8)&0xff00|((a)>>24)&0xff)

char *progname,logfile[256];
struct Shm {
  unsigned long p;    /* write point */
  unsigned long pl;   /* write limit */
  unsigned long r;    /* latest */
  unsigned long c;    /* counter */
  unsigned char d[1]; /* data buffer */
};

mklong(ptr)
  unsigned char *ptr;
  {   
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }   

get_time(rt)
     int *rt;
{
  struct tm *nt;
  unsigned long ltime;
  time(&ltime);
  nt=localtime(&ltime);
  rt[0]=nt->tm_year%100;
  rt[1]=nt->tm_mon+1;
  rt[2]=nt->tm_mday;
  rt[3]=nt->tm_hour;
  rt[4]=nt->tm_min;
  rt[5]=nt->tm_sec;
}

adj_time(tm)
  int *tm;
{
  if(tm[5]==60){
    tm[5]=0;
    if(++tm[4]==60){
      tm[4]=0;
      if(++tm[3]==24){
        tm[3]=0;
        tm[2]++;
        switch(tm[1]){
          case 2:
            if(tm[0]%4==0){
              if(tm[2]==30){
                tm[2]=1;
                tm[1]++;
              }
              break;
            }
            else{
              if(tm[2]==29){
                tm[2]=1;
                tm[1]++;
              }
              break;
            }
          case 4:
          case 6:
          case 9:
          case 11:
            if(tm[2]==31){
              tm[2]=1;
              tm[1]++;
            }
            break;
          default:
            if(tm[2]==32){
              tm[2]=1;
              tm[1]++;
            }
            break;
        }
        if(tm[1]==13){
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
        }
      }
    }
  }
  else if(tm[5]==-1){
    tm[5]=59;
    if(--tm[4]==-1){
      tm[4]=59;
      if(--tm[3]==-1){
        tm[3]=23;
        if(--tm[2]==0){
          switch(--tm[1]){
            case 2:
              if(tm[0]%4==0) tm[2]=29;
              else tm[2]=28;
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
          if(tm[1]==0){
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
          }
        }
      }
    }
  }
}

adj_time_m(tm)
  int *tm;
{
  if(tm[4]==60){
    tm[4]=0;
    if(++tm[3]==24){
      tm[3]=0;
      tm[2]++;
      switch(tm[1]){
        case 2:
          if(tm[0]%4==0){
            if(tm[2]==30){
              tm[2]=1;
              tm[1]++;
            }
            break;
          }
          else{
            if(tm[2]==29){
              tm[2]=1;
              tm[1]++;
            }
            break;
          }
        case 4:
        case 6:
        case 9:
        case 11:
          if(tm[2]==31){
            tm[2]=1;
            tm[1]++;
          }
          break;
        default:
          if(tm[2]==32){
            tm[2]=1;
            tm[1]++;
          }
          break;
      }
      if(tm[1]==13){
        tm[1]=1;
        if(++tm[0]==100) tm[0]=0;
      }
    }
  }
  else if(tm[4]==-1){
    tm[4]=59;
    if(--tm[3]==-1){
      tm[3]=23;
      if(--tm[2]==0){
        switch(--tm[1]){
          case 2:
            if(tm[0]%4==0) tm[2]=29;
            else tm[2]=28;
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
        if(tm[1]==0){
          tm[1]=12;
          if(--tm[0]==-1) tm[0]=99;
        }
      }
    }
  }
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

check_time(ptr)
  char *ptr;
  {
  static int tm_prev[6],flag;
  int tm[6],rt1[6],rt2[6],i,j;

  if(!bcd_dec(tm,ptr)) return 1; /* out of range */
  if(flag && time_cmp(tm,tm_prev,5)==0) return 0;
  else flag=0;

  /* compare time with real time */
  get_time(rt1);
  for(i=0;i<5;i++) rt2[i]=rt1[i];
  for(i=0;i<30;i++)  /* within 30 minutes ? */
    {
    if(time_cmp(tm,rt1,5)==0 || time_cmp(tm,rt2,5)==0)
      {
      for(j=0;j<5;j++) tm_prev[j]=tm[j];
      flag=1;
      return 0;
      }
    rt1[4]++;
    adj_time_m(rt1);
    rt2[4]--;
    adj_time_m(rt2);
    }
  return 1; /* illegal time */
  }

write_log(logfil,ptr)
  char *logfil;
  char *ptr;
{
  FILE *fp;
  int tm[6];
  if(*logfil) fp=fopen(logfil,"a");
  else fp=stdout;
  get_time(tm);
  fprintf(fp,"%02d%02d%02d.%02d%02d%02d %s %s\n",
    tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
  if(*logfil) fclose(fp);
}

ctrlc()
{
  write_log(logfile,"end");
  exit(0);
}

err_sys(ptr)
  char *ptr;
{
  perror(ptr);
  write_log(logfile,ptr);
  if(strerror(errno)) write_log(strerror(errno));
  ctrlc();
}

advance(shm,shp,c_save,size,tm)
  struct Shm *shm;
  unsigned int *shp,*size,*c_save;
  int *tm;
{
  int shpp,tmp;

  shpp=(*shp);
  if(shm->c<*c_save || *size!=mklong(shm->d+(*shp))) return -2;
  if(shpp+(*size)>shm->pl) shpp=0; /* advance pointer */
  else shpp+=(*size);
  if(shm->p==shpp) return 0;
  *c_save=shm->c;
  *size=mklong(shm->d+(*shp=shpp));
  if(!bcd_dec(tm,shm->d+shpp+8)) return -1;
  return mklong(shm->d+shpp+4);
}

main(argc,argv)
  int argc;
  char *argv[];
{
  key_t shm_key_in,shm_key_out;
  unsigned long uni;
  int tm[6],tm_out[6],tm_bottom[6],i,j,k,c_save_in,c_save_out,shp_in,shp_out,
    sec,sec_1,size,shmid_in,shmid_out,size_in,size_out,shp,c_save,n_sec,no_data;
  unsigned char *ptr,*ptr_save,*ptw,*ptw_save,tbuf[100];
  struct Shm *shm_in,*shm_out;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
   
  if(argc<5){
    printf(
" usage : '%s [shm_key_in] [shm_key_out] [shm_size(KB)] [limit_sec] ([log file])'\n",
      progname);
    exit(0);
  }
   
  shm_key_in=atoi(argv[1]);
  shm_key_out=atoi(argv[2]);
  size=atoi(argv[3])*1000;
  n_sec=atoi(argv[4]);
  if(argc>5) strcpy(logfile,argv[5]);
  else *logfile=0;
   
  /* shared memory */
  if((shmid_in=shmget(shm_key_in,0,0))<0) err_sys("shmget");
  if((shm_in=(struct Shm *)shmat(shmid_in,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");
  sprintf(tbuf,"in : shm_key_in=%d id=%d",shm_key_in,shmid_in);
  write_log(logfile,tbuf);
   
  if((shmid_out=shmget(shm_key_out,size,IPC_CREAT|0666))<0) err_sys("shmget");
  if((shm_out=(struct Shm *)shmat(shmid_out,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");
  sprintf(tbuf,"out: shm_key_out=%d id=%d size=%d",shm_key_out,shmid_out,size);
  write_log(logfile,tbuf);
   
  /* initialize buffer */
  shm_out->p=shm_out->c=0;
  shm_out->pl=(size-sizeof(*shm_out))/10*9;
  shm_out->r=(-1);
   
  signal(SIGPIPE,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGTERM,(void *)ctrlc);
   
reset:
  while(shm_in->r==(-1)) sleep(1);
  c_save_in=shm_in->c;
  size_in=mklong(ptr_save=shm_in->d+(shp_in=shm_in->r));
  if(check_time(ptr_save+8)){
     sleep(1);
     goto reset;
  }
  memcpy(&sec_1,ptr_save+4,4);
  i=1;if(*(char *)&i) SWAPL(sec_1);
  bcd_dec(tm_bottom,ptr_save+8);
  for(i=0;i<6;i++) tm_out[i]=tm_bottom[i];

  while(1){
    no_data=1;
    while(sec_1+n_sec>time(0)) sleep(1);
    shp=shp_in;
    c_save=c_save_in;
    size=size_in;
    ptw_save=ptw=shm_out->d+shm_out->p;
    ptw+=4;
    for(i=0;i<6;i++) tm[i]=tm_bottom[i];

    do{
      if(time_cmp(tm,tm_out,6)==0){
        ptr=shm_in->d+shp+8; /* points to YY-ss */
        if(no_data){
          memcpy(ptw,ptr,size-8);
          ptw+=size-8;
          no_data=0;
        }
        else{
          memcpy(ptw,ptr+6,size-14); /* skip YY-ss */
          ptw+=size-14;
        }
      }
      while((sec=advance(shm_in,&shp,&c_save,&size,tm))==(-1));
      if(sec==(-2)){
        ptr=shm_in->d+shp+8;
        sprintf(tbuf,"reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
        write_log(logfile,tbuf);
        goto reset;
      }
    } while(sec>0);

    if((uni=ptw-ptw_save)>4){
      ptw_save[0]=uni>>24;
      ptw_save[1]=uni>>16;
      ptw_save[2]=uni>>8;
      ptw_save[3]=uni;
      shm_out->r=shm_out->p;
      if(ptw>shm_out->d+shm_out->pl) ptw=shm_out->d;
      shm_out->p=ptw-shm_out->d;
      shm_out->c++;
    }

    tm_out[5]++;
    adj_time(tm_out); /* next time to output */
    while(i=time_cmp(tm_bottom,tm_out,6)){
      if(sec_1+n_sec+4>time(0) && i>0) break;
      if(i>0){
        ptr=shm_in->d+shp_in+8;
        sprintf(tbuf,
"passing %02X%02X%02X.%02X%02X%02X:%02X%02X(out=%02d%02d%02d.%02d%02d%02d)",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
          tm_out[0],tm_out[1],tm_out[2],tm_out[3],tm_out[4],tm_out[5]);
        write_log(logfile,tbuf);
      }
      while((sec_1=advance(shm_in,&shp_in,&c_save_in,&size_in,tm_bottom))<=0){
        if(sec_1==(-1)) continue;
        else if(sec_1==0){
          sleep(1);
          continue;
        }
        else if(sec_1==(-2)){
          ptr=shm_in->d+shp_in+8;
          sprintf(tbuf,"reset %02X%02X%02X.%02X%02X%02X:%02X%02X",
            ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
          write_log(logfile,tbuf);
          goto reset;
        }
      }
    }
  }
}
