/*
 * $Id: winadd.c,v 1.1.2.1 2001/11/02 11:43:41 uehira Exp $
 *
 * winadd.c  (Uehira Kenji)
 *  last modified  2000/2/29
 *
 *  How to compile : need 'win_system.h'
 *    cc -O winadd.c -o winadd
 *
 *  98/12/23  malloc bug (indx[i][j].name)
 *  99/1/6    skip no data file
 *  2000/2/29  bye-order free. delete NR code.
 *
 */

static const char rcsid[] =
   "$Id: winadd.c,v 1.1.2.1 2001/11/02 11:43:41 uehira Exp $";

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

#include <memory.h>

#include "win_system.h"
#include "subst_func.h"

#define   DEBUG  0

#define  CH_INC     5
#define  TIME_INC   5
#define  MIN_LEN    8  /* minimum channel block length */

struct data_index {
   int            flag;
   char           *name;
   unsigned long  point;
   unsigned long  len;
};
typedef struct data_index  INDX;

static void
usage()
{

   fprintf(stderr, "%s\n", rcsid);
   fputs("Usage : winadd [options] file1 file2 ... > output\n",stderr);
   fputs("  options : -n         : do not append dummy headers.\n"
	 ,stderr);
   /* fputs("            -o outfile : place output in file outfile.\n",stderr); */
   fputs("            -h         : print this message.\n",stderr);
}

static void
memory_error()
{
   fputs("Cannot allocate memory!\n",stderr);
   exit(1);
}

static void
dec_bcd(dest,sour)
     unsigned int *sour;
     unsigned char *dest;
{
   int cntr;
   for(cntr=0;cntr<6;cntr++)
      dest[cntr]=(((sour[cntr]/10)<<4)&0xf0)|(sour[cntr]%10&0xf);
}

static time_t
bcd2time(bcd)
     unsigned char *bcd;
{
   int  t[6];
   struct tm     time_str;
   time_t        time;

   bcd_dec(t,bcd);
   if(t[0]>=70)
      time_str.tm_year=t[0];
   else
      time_str.tm_year=100+t[0]; /* 2000+t[0]-1900 */
   time_str.tm_mon=t[1]-1;
   time_str.tm_mday=t[2];
   time_str.tm_hour=t[3];
   time_str.tm_min=t[4];
   time_str.tm_sec=t[5];
   time_str.tm_isdst=0;

   if((time=mktime(&time_str))==(time_t)-1){
      fputs("mktime error.\n",stderr);
      exit(1);
   }
   return(time);
}

static void
time2bcd(time,bcd)
     time_t         time;
     unsigned char  *bcd;
{
   unsigned int t[6];
   struct tm    time_str;

   time_str=*localtime(&time);
   if(time_str.tm_year>=100)
      t[0]=time_str.tm_year-100;
   else
      t[0]=time_str.tm_year;
   t[1]=time_str.tm_mon+1;
   t[2]=time_str.tm_mday;
   t[3]=time_str.tm_hour;
   t[4]=time_str.tm_min;
   t[5]=time_str.tm_sec;
   dec_bcd(bcd,t);
}

static void
win_file_read(name,ch,ch_num,ch_num_arr,time,time_num,time_num_arr)
     char            *name;
     unsigned short  **ch;
     time_t          **time;
     int             *ch_num,*ch_num_arr,*time_num,*time_num_arr;
{
   FILE           *fp;
   unsigned char  tt[6],gh[5],re_c[4];
   unsigned char  *ptr,*buf;
   WIN_blocksize  re,re_debug;
   unsigned long  gsize,sr;
   time_t         time_tmp;
   unsigned short ch_tmp;
   int            i;

   if(NULL==(fp=fopen(name,"r"))){
      fprintf(stderr,"Warring! Skip file : %s\n",name);
      /* exit(1); */ 
      return;
   }

   /* read 1 sec block size */
   while(fread(re_c,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN){
      re=mklong(re_c);
      re-=WIN_BLOCKSIZE_LEN;
      if(NULL==(buf=MALLOC(unsigned char,re))){
	 fclose(fp);
	 memory_error();
      }
      if(re!=(re_debug=fread(buf,1,re,fp))){
	 fprintf(stderr,"Input file is strange! : %s\n",name);
	 fprintf(stderr,"  Require=%u [byte], but %u [byte]\n",re,re_debug);
	 FREE(buf);
	 break;
      }
      ptr=buf;
      /* read & compare time */
      for(i=0;i<WIN_TIME_LEN;++i)
	 tt[i]=*ptr++;
      time_tmp=bcd2time(tt);
      for(i=0;i<*time_num;++i)
	 if(time_tmp==(*time)[i])
	    break;
      if(i==*time_num){
	 (*time_num)++;
	 if(*time_num>*time_num_arr){
	    if(NULL==(*time=REALLOC(time_t,*time,*time_num_arr+TIME_INC)))
	       memory_error();
	    *time_num_arr+=TIME_INC;
	 }
	 (*time)[i]=time_tmp;
      }
      /* read & compare channel number */
      while(ptr<buf+re){
	 for(i=0;i<5;++i)
	    gh[i]=ptr[i];
	 /* ch number */
	 ch_tmp=(((unsigned short)gh[0])<<8)+(unsigned short)gh[1];
	 for(i=0;i<*ch_num;++i)
	    if(ch_tmp==(*ch)[i])
	       break;
	 if(i==*ch_num){
	    (*ch_num)++;
	    if(*ch_num>*ch_num_arr){
	       if(NULL==(*ch=REALLOC(unsigned short,*ch,*ch_num+CH_INC)))
		  memory_error();
	       *ch_num_arr+=CH_INC;
	    }
	    (*ch)[i]=ch_tmp;
	 }
	 
	 /* get sampling rate and calculate 1 channel size */
	 if((gh[2]&0x80)==0x0) /* channel header = 4 byte */
	    sr=gh[3]+(((long)(gh[2]&0x0f))<<8);
	 else                     /* channel header = 5 byte */
	    sr=gh[4]+(((long)gh[3])<<8)+(((long)(gh[2]&0x0f))<<16);
	 /* size */
	 if((gh[2]>>4)&0x7)
	    gsize=((gh[2]>>4)&0x7)*(sr-1)+8;
	 else
	    gsize=(sr>>1)+8;
	 if(gh[2]&0x80)
	    gsize++;
	 ptr+=gsize;
      } /* while(ptr<buf+re) */
      FREE(buf);
   } /* while(fread(re_c,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN) */
   fclose(fp);
}

static void
get_index(name,indx,ch,ch_num,time,time_num)
     char            *name;
     INDX            **indx;
     unsigned short  *ch;
     time_t          *time;
     int             ch_num,time_num;
{
   FILE   *fp;
   unsigned char  tt[6],gh[5],re_c[4];
   unsigned char  *ptr,*buf;
   unsigned long  gsize,sr,point;
   WIN_blocksize  re;
   time_t         time_tmp;
   unsigned short ch_tmp;
   int            i;
   int            time_sfx,ch_sfx;
   
   if(NULL==(fp=fopen(name,"r"))){
      /* fprintf(stderr,"Cannot open input file : %s\n",name); */
      /* exit(1); */
      return;
   }
   point=(unsigned long)ftell(fp);

   /* read 1 sec block size */
   while(fread(re_c,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN){
      re=mklong(re_c);
      re-=4;
      if(NULL==(buf=MALLOC(unsigned char,re))){
	 fclose(fp);
	 memory_error();
      }
      if(re!=fread(buf,1,re,fp)){
	 FREE(buf);
	 fclose(fp);
	 return;
      }
      ptr=buf;
      /* read & compare time */
      for(i=0;i<WIN_TIME_LEN;++i)
	 tt[i]=*ptr++;
      time_tmp=bcd2time(tt);
      for(i=0;i<time_num;++i)
	 if(time_tmp==time[i])
	    break;
      time_sfx=i;
      /* read & compare channel number */
      while(ptr<buf+re){
	 for(i=0;i<5;++i)
	    gh[i]=ptr[i];
	 /* ch number */
	 ch_tmp=(((unsigned short)gh[0])<<8)+(unsigned short)gh[1];
	 for(i=0;i<ch_num;++i)
	    if(ch_tmp==ch[i])
	       break;
	 ch_sfx=i;
	 /* get sampling rate and calculate 1 channel size */
	 if((gh[2]&0x80)==0x0) /* channel header = 4 byte */
	    sr=gh[3]+(((long)(gh[2]&0x0f))<<8);
	 else                  /* channel header = 5 byte */
	    sr=gh[4]+(((long)gh[3])<<8)+(((long)(gh[2]&0x0f))<<16);
	 /* size */
	 if((gh[2]>>4)&0x7)
	    gsize=((gh[2]>>4)&0x7)*(sr-1)+8;
	 else
	    gsize=(sr>>1)+8;
	 if(gh[2]&0x80)
	    gsize++;
	 /* make index */
	 if(!indx[ch_sfx][time_sfx].flag){
	   indx[ch_sfx][time_sfx].flag=1;
	   strcpy(indx[ch_sfx][time_sfx].name,name);
	   indx[ch_sfx][time_sfx].point=point+ptr-buf+4;
	   indx[ch_sfx][time_sfx].len=gsize;
#if DEBUG
	   fprintf(stdout,"indx[%d][%d]-->%d  name=%s  point=%d  len=%d\n",
		   ch_sfx,time_sfx,indx[ch_sfx][time_sfx].flag,
		   indx[ch_sfx][time_sfx].name,indx[ch_sfx][time_sfx].point,
		   indx[ch_sfx][time_sfx].len);
#endif
	 }
	 ptr+=gsize;
      }  /* while(ptr<buf+re) */
      point=(unsigned long)ftell(fp);
      FREE(buf);
   } /* while(fread(re_c,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN) */
   fclose(fp);
}

static int
time_cmpq(a,b)
     unsigned long  *a,*b;
{
  if(*a<*b) return(-1);
  else if(*a==*b) return(0);
  else return(1);
}

int
main(argc,argv)
     int  argc;
     char **argv;
{
   char   *outfile;
   int  dummy_flag,out_flag;
   int  i,j,c,max_name_len;
   int  ch_num,time_num,ch_num_arr,time_num_arr;
   static time_t  *time[1];
   static unsigned short  *ch[1];
   FILE  *fp_out,*fp_in;
   unsigned char tt[6];
   unsigned char *outbuf,*secbuf,*ptr;
   unsigned long secbuf_len,len_max;
   extern int optind;
   extern char *optarg;
   INDX   **indx;
   unsigned long  *sortin,*time_sort;

   if(argc==1){
      usage();
      exit(0);
   }

   dummy_flag=1;
   out_flag=max_name_len=0;
   ch_num=time_num=0;
   while((c=getopt(argc,argv,"o:nh"))!=EOF){
      switch(c){
      case 'o':   /* set output file */
	 if(NULL==(outfile=MALLOC(char,strlen(optarg)+1)))
	    memory_error();
	 strcpy(outfile,optarg);
	 out_flag=1;
	 break;
      case 'n':   /* don't append dummy data */
	 dummy_flag=0;
	 break;
      case 'h':   /* print usage */
      default:
	 usage();
	 exit(0);
      }
   }
   if(out_flag){
      if(NULL==(fp_out=fopen(outfile,"w"))){
	 fprintf(stderr,"Cannot open output file : %s\n",outfile);
	 exit(1);
      }
   }
   else
      fp_out=stdout;

#if DEBUG   
   fprintf(stderr,"outfile=%s  dummy flag=%d\n",outfile,dummy_flag);
#endif

   if(NULL==(*time=MALLOC(time_t,TIME_INC)))
      memory_error();
   if(NULL==(*ch=MALLOC(unsigned short,CH_INC)))
      memory_error();
   time_num_arr=TIME_INC;
   ch_num_arr=CH_INC;

   /* sweep all input file(s) and get channel number and time length */
   for(i=optind; i<argc; ++i){ 
      if(max_name_len<strlen(argv[i])) max_name_len=strlen(argv[i]);
      win_file_read(argv[i],ch,&ch_num,&ch_num_arr,
		    time,&time_num,&time_num_arr);
   }
   /* malloc memory for INDX */
   if(NULL==(indx=MALLOC(INDX *,ch_num))) memory_error();
   for(i=0;i<ch_num;++i)
     if(NULL==(indx[i]=MALLOC(INDX,time_num))) memory_error();
   for(i=0;i<ch_num;++i)
      for(j=0;j<time_num;++j){
	 indx[i][j].flag=0;  /* clear flag */
	 if(NULL==(indx[i][j].name=MALLOC(char,max_name_len+1)))
	   memory_error();
      }
   /* Make INDX table */
   for(i=optind; i<argc; ++i)
      get_index(argv[i],indx,*ch,ch_num,*time,time_num);
   
   /* sort time */
   if(NULL==(time_sort=MALLOC(unsigned long,time_num))) memory_error();
   if(NULL==(sortin=MALLOC(unsigned long,time_num))) memory_error();
   for(i=0;i<time_num;++i) time_sort[i]=(*time)[i];
   qsort(time_sort,time_num,sizeof(unsigned long),time_cmpq);
   for(i=0;i<time_num;++i){
     for(j=0;j<time_num;++j){
       if(time_sort[i]==(*time)[j]){
	 sortin[i]=j;
	 break;
       }
     }
   }
#if DEBUG
   for(i=0;i<time_num;++i){
     fprintf(stderr,"%d  %d:  %d\n",
	     (*time)[i],time_sort[i],(*time)[sortin[i]]);
     /* sleep(1); */
   }
#endif
   FREE(time_sort);

   /* make first time data */
   len_max=MIN_LEN;
   secbuf_len=10;
   for(i=0;i<ch_num;++i){
      if(indx[i][sortin[0]].flag){
	 secbuf_len+=indx[i][sortin[0]].len;
	 if(len_max<indx[i][sortin[0]].len) len_max=indx[i][sortin[0]].len;
      }
      else{
	 if(dummy_flag)
	    secbuf_len+=MIN_LEN;
      }
   }
   if(NULL==(secbuf=MALLOC(unsigned char,secbuf_len))) memory_error();
   if(NULL==(outbuf=MALLOC(unsigned char,len_max))) memory_error();
   ptr=secbuf;
   ptr[0]=secbuf_len>>24;
   ptr[1]=secbuf_len>>16;
   ptr[2]=secbuf_len>>8;
   ptr[3]=secbuf_len;
   ptr+=WIN_BLOCKSIZE_LEN;
   time2bcd((*time)[sortin[0]],tt);
   memcpy(ptr,tt,WIN_TIME_LEN);  /* copy time stamp */
   ptr+=WIN_TIME_LEN;
   for(i=0;i<ch_num;++i){
      if(indx[i][sortin[0]].flag){
	 fp_in=fopen(indx[i][sortin[0]].name,"r");
	 fseek(fp_in,indx[i][sortin[0]].point,0);
	 fread(outbuf,1,indx[i][sortin[0]].len,fp_in);
	 memcpy(ptr,outbuf,indx[i][sortin[0]].len);
	 ptr+=indx[i][sortin[0]].len;
	 fclose(fp_in);
      }
      else{
	 if(dummy_flag){
 	    outbuf[0]=(*ch)[i]>>8;
	    outbuf[1]=(*ch)[i];
	    outbuf[2]=0; outbuf[3]=1;
	    outbuf[4]=outbuf[5]=outbuf[6]=outbuf[7]=0;
	    memcpy(ptr,outbuf,MIN_LEN);  /* copy dummy data */
	    ptr+=MIN_LEN;
	 }
      }
   }
   fwrite(secbuf,1,secbuf_len,fp_out);
   FREE(secbuf); FREE(outbuf);
   
   /* make another time */
   for(j=1;j<time_num;++j){
      /* if time jumps, add dummy data */
      if(dummy_flag && (((*time)[sortin[j]]-(*time)[sortin[j-1]])!=1)){
	 secbuf_len=18;
	 len_max=MIN_LEN;
	 if(NULL==(secbuf=MALLOC(unsigned char,secbuf_len))) memory_error();
	 if(NULL==(outbuf=MALLOC(unsigned char,len_max))) memory_error();
	 ptr=secbuf;
	 ptr[0]=secbuf_len>>24;
	 ptr[1]=secbuf_len>>16;
	 ptr[2]=secbuf_len>>8;
	 ptr[3]=secbuf_len;
	 ptr+=WIN_BLOCKSIZE_LEN;
	 time2bcd((*time)[sortin[j-1]]+1,tt);
	 memcpy(ptr,tt,WIN_TIME_LEN);  /* copy time stamp */
	 ptr+=WIN_TIME_LEN;
	 outbuf[0]=(*ch)[i]>>8;
	 outbuf[1]=(*ch)[i];
	 outbuf[2]=0; outbuf[3]=1;
	 outbuf[4]=outbuf[5]=outbuf[6]=outbuf[7]=0;
	 memcpy(ptr,outbuf,MIN_LEN);  /* copy dummy data */
	 fwrite(secbuf,1,secbuf_len,fp_out);
	 FREE(secbuf); FREE(outbuf);
      }      
      len_max=MIN_LEN;
      secbuf_len=10;
      for(i=0;i<ch_num;++i)
	 if(indx[i][sortin[j]].flag){
	    secbuf_len+=indx[i][sortin[j]].len;
	    if(len_max<indx[i][sortin[j]].len) len_max=indx[i][sortin[j]].len;
	 }
      if(NULL==(secbuf=MALLOC(unsigned char,secbuf_len))) memory_error();
      if(NULL==(outbuf=MALLOC(unsigned char,len_max))) memory_error();
      ptr=secbuf;
      ptr[0]=secbuf_len>>24;
      ptr[1]=secbuf_len>>16;
      ptr[2]=secbuf_len>>8;
      ptr[3]=secbuf_len;
      ptr+=WIN_BLOCKSIZE_LEN;
      time2bcd((*time)[sortin[j]],tt);
      memcpy(ptr,tt,WIN_TIME_LEN);  /* copy time stamp */
      ptr+=WIN_TIME_LEN;
      for(i=0;i<ch_num;++i)
	 if(indx[i][sortin[j]].flag){
	    fp_in=fopen(indx[i][sortin[j]].name,"r");
	    fseek(fp_in,indx[i][sortin[j]].point,0);
	    fread(outbuf,1,indx[i][sortin[j]].len,fp_in);
	    memcpy(ptr,outbuf,indx[i][sortin[j]].len);
	    ptr+=indx[i][sortin[j]].len;
	    fclose(fp_in);
	 }
      fwrite(secbuf,1,secbuf_len,fp_out);
      FREE(secbuf); FREE(outbuf);
   }
   if(out_flag)
      fclose(fp_out);

#if DEBUG
   for(i=0;i<ch_num;++i){
      for(j=0;j<time_num;++j)
	 fprintf(stdout,"%d",indx[i][j].flag);
      fprintf(stdout,"\n");
   }
   fprintf(stderr,"ch_num=%d  ch_num_arr=%d time_num=%d  time_num_arr=%d\n",
	   ch_num,ch_num_arr,time_num,time_num_arr);
   for(i=0;i<time_num;++i)
      fprintf(stdout,"TIME=%ld\n",time[0][i]);
   for(i=0;i<ch_num;++i)
      fprintf(stdout,"CH=%04X\n",ch[0][i]);
#endif

   return (0);
}
