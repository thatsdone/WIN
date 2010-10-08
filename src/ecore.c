/* $Id: ecore.c,v 1.4.2.1 2010/10/08 16:18:04 uehira Exp $ */
/* ddr news program "ecore.c"
  "ecore.c" works with "fromtape.c"
  "ecore.c" makes continuously filtered and decimated data
  from EXABYTE tape data
  3/13/91-7/31/91, 9/19/91-9/26/91,6/19/92,3/24/93,6/17/94 urabe
  98.6.26 yo2000
*/
/*  03/03/07 change size by N.Nakakawaji  */
/*	NCH:400=>1500
	NAMLEN:80=>255
	in_data:100*1000=>500*1000
	output file:YYMMDDHH.mm...win format
*/
/*  03/04/25 for Endian free and fix by N.Nakakawaji */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <sys/types.h>
#include  <sys/file.h>
#include  <sys/stat.h>

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <fcntl.h>

#include "subst_func.h"

/* filter parameters: see SAITO (1978) */
#define   FILTER    1
/* following parameters give M=3 (6-th order) */
#define   FP      6.0
#define   FS      9.0
#define   AP      0.5
#define   AS      5.0

#define   FIVE_MIN  0     /* devide in 5-minute files ? */
#define   SR      20      /* sampling rate of output data */
#define   SSR     250     /* max sampling rate of input data */
/* NCH 400 => 1200 03/03/03 N.Nakakawaji */
/*#define   NCH     400  */   /* max of output channels */
/* NCH 1200 => 1500 03/03/03 N.Nakakawaji */
/*#define   NCH     1200 */     /* max of output channels changed 03/03/03 */
#define   NCH     1500     /* max of output channels changed 03/03/05 */
#define   CHMASK    0xffff    /* max of name channels */
#define   MAX_FILT  25      /* max of filter order */
/* NAMLEN 80=> 255 03/03/07 */
/*#define   NAMLEN    80*/
#define   NAMLEN   255 

/* not use out_data 03/03/07 */
/*short out_data[SR*NCH];*/
/* change size 100*1000=> 500*1000 03/02/28 */
/*unsigned char in_data[100*1000]; */
unsigned char in_data[500*1000];
int pos_table[CHMASK+1],f_out,f_in;
char path_raw[NAMLEN],file_written[NAMLEN],file_done[NAMLEN],
  file_list[NAMLEN],file_out[NAMLEN],dir_out[NAMLEN],
  name_file[NAMLEN],time_done[NAMLEN];
FILE *fp;

struct {
  int sys_ch;
  int flag_filt;
  int m_filt;
  int n_filt;
  double gn_filt;
  double coef[MAX_FILT*4];
  double uv[MAX_FILT*4];
  } ch[NCH];
/*****  deleted for Endian Free  03/04/25
mklong(ptr)
        unsigned char *ptr;
        {
        unsigned char *ptr1;
        unsigned long a;
        ptr1=(unsigned char *)&a;
        *ptr1++=(*ptr++);
        *ptr1++=(*ptr++);
        *ptr1++=(*ptr++);
        *ptr1  =(*ptr);
        return a;
        }

mkshort(ptr)
        unsigned char *ptr;
        {
        unsigned char *ptr1;
        short a;
        ptr1=(unsigned char *)&a;
        *ptr1++=(*ptr++);
        *ptr1  =(*ptr);
        return a;
        }
******/

/* added for Endian free 03/04/25 */
mklong(ptr)       
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;       
  }

mkshort(ptr)
  unsigned char *ptr;
  {
  unsigned short a;      
  a=((ptr[0]<<8)&0xff00)+(ptr[1]&0xff);
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

bcd_dec(dest,sour)
  char *sour;
  int *dest;
  {
  int cntr;
  for(cntr=0;cntr<6;cntr++)
    dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf);
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
  if(f_in!=-1) close(f_in);
  if(f_out!=-1)
    {
    close(f_out);
    fp=fopen(file_done,"w");
    fprintf(fp,"%s",time_done);
    fclose(fp);
    }
  printf("***** ecore end *****\n");
  exit(value);
  }

#define LongFromBigEndian(a) \
  ((((unsigned char *)&(a))[0]<<24)+(((unsigned char *)&(a))[1]<<16)+ \
  (((unsigned char *)&(a))[2]<<8)+((unsigned char *)&(a))[3])

main(argc,argv)
  int argc;
  char *argv[];
  {
  double buf[SSR],dt;
  long lbuf[SSR],dl;
  unsigned char *ptr,*ptr_end,textbuf[NAMLEN];
  int dec_start[6],dec_end[6],dec_wtn[6],dec_now[6],dec_buf[6],
    i,j,nch,next,sys_ch,sr,pos,size,cnt_min;
  //int fildata[SSR];   int => long 03/04/23
  long fildata[SSR];
  unsigned char windata[500*1000],*wptr;  /* added 03/03/07 */
  int idx,bsize;  				  /* added 03/03/07 */
  unsigned char min_head[6];  		  /* added 03/03/07 */
  int c_bsize;			/* added 03/04/22 */

  f_in=f_out=(-1);
  printf("***** ecore start *****\n");
  if(argc<5)
    {
    printf("usage of 'ecore' :\n");
    printf(" 'ecore [YYMMDD.HHMM(1)] [YYMMDD.HHMM(2)] [raw dir] [ch list] ([out dir])'\n");
    printf("    ('ecore' cooperates with 'fromtape')\n");
    end_process(0);
    }

  /* read times of start and end */
  sscanf(argv[1],"%2d%2d%2d.%2d%2d",&dec_start[0],&dec_start[1],
    &dec_start[2],&dec_start[3],&dec_start[4]);
  dec_start[5]=0;
  sscanf(argv[2],"%2d%2d%2d.%2d%2d",&dec_end[0],&dec_end[1],
    &dec_end[2],&dec_end[3],&dec_end[4]);
  dec_end[5]=59;
  printf("start:%02d%02d%02d %02d%02d%02d\n",
    dec_start[0],dec_start[1],dec_start[2],
    dec_start[3],dec_start[4],dec_start[5]);
  //for(i=0;i<5;i++) dec_now[i]=dec_start[i];   5=>6 fix 03/04/25
  for(i=0;i<6;i++) dec_now[i]=dec_start[i];
  printf("end  :%02d%02d%02d %02d%02d%02d\n",
    dec_end[0],dec_end[1],dec_end[2],
    dec_end[3],dec_end[4],dec_end[5]);

  /* read input directory */
  sscanf(argv[3],"%s",path_raw);
  printf("input directory (raw) : %s\n",path_raw);
  sprintf(file_written,"%s/LATEST",path_raw);
  sprintf(file_done,"%s/USED",path_raw);

  /* read channel list */
  sscanf(argv[4],"%s",file_list);
  if((fp=fopen(file_list,"r"))==NULL)
    {
    printf("channel list file '%s' not open\n",file_list);
    end_process(1);
    }
  for(i=0;i<CHMASK;i++) pos_table[i]=(-1);
  nch=0;
  while(fgets(textbuf,NAMLEN,fp))
    {
    if(*textbuf=='#') continue;
    sscanf(textbuf,"%x",&i);
    i &= CHMASK;
    //printf(" %04X",i);
    ch[nch].sys_ch=i;
    ch[nch].flag_filt=0;
    pos_table[i]=nch;
    if(++nch==NCH) break;
    }
  fclose(fp);
  printf("\nN of channels = %d\n",nch);
  fflush(stdout); /* 03/03/07 */

  /* read output directory  add error check 03/03/07 */
  if(argc>5) sscanf(argv[5],"%s",dir_out);
  else strcpy(dir_out,".");
  if(strcmp(dir_out,path_raw)==0){
    printf("not use same directory <in & out>\n");
    end_process(1);
  }
  /*  out file => min win file 03/03/07 */
  /*sprintf(file_out,"%s/%02d%02d%02d.%02d%02d",dir_out,
    dec_now[0],dec_now[1],dec_now[2],dec_now[3],dec_now[4]);
  if((f_out=open(file_out,O_RDWR|O_CREAT|O_TRUNC,0666))==-1)
    {
    printf("file open error : %s\n",file_out);
    end_process(1);
    }
  printf("'%s' opened\n",file_out);
  */
  cnt_min=0;

  while(1)
    {
    while((fp=fopen(file_written,"r"))==NULL)
      {
      printf("waiting '%s' to be created ...\n",file_written);
      sleep(15);
      }
    fscanf(fp,"%02d%02d%02d%02d.%02d",&dec_wtn[0],&dec_wtn[1],
      &dec_wtn[2],&dec_wtn[3],&dec_wtn[4]);
    fclose(fp);
//printf("next   =%02d%02d%02d%02d.%02d\n",dec_now[0],dec_now[1],dec_now[2],
//  dec_now[3],dec_now[4]);
//printf("written=%02d%02d%02d%02d.%02d\n",dec_wtn[0],dec_wtn[1],dec_wtn[2],
//  dec_wtn[3],dec_wtn[4]);
    if(time_cmp(dec_wtn,dec_now,6)<0)
      {
      printf("waiting '%s' to be updated ...\n",file_written);
      sleep(15);
      continue;
      }
    /* file ready */
    sprintf(name_file,"%s/%02d%02d%02d%02d.%02d",path_raw,
      dec_now[0],dec_now[1],dec_now[2],dec_now[3],dec_now[4]);
    sprintf(time_done,"%02d%02d%02d%02d.%02d",
      dec_now[0],dec_now[1],dec_now[2],dec_now[3],dec_now[4]);
    if((f_in=open(name_file,O_RDONLY))==-1)
      {
      printf("no file : %s\n",name_file);

      //------------ no input file => no output file 03/03/12 
      //for(i=0;i<60;i++)
      //  {
      //  /* output dummy */
      //  for(j=0;j<sizeof(windata);j++) windata[j]=0;
      //  /*if(write(f_out,(char *)out_data,h)==(-1))*/ /*03/03/07 */
      //  if(write(f_out,(char *)windata,sizeof(windata))==(-1))
      //    {
      //    perror("write");
      //    end_process(1);
      //    }
      //  dec_now[5]++;
      //  adj_time(dec_now);
      //  }
      //-------------------------------------------------------*/
      dec_now[4]++;
      adj_time(dec_now);
      continue;  /* return loop */
      }
    else
      {
      printf("'%s' opened\n",name_file);
  	fflush(stdout); /* 03/03/07 */
      /* out file 03/03/07 */
      sprintf(file_out,"%s/%02d%02d%02d%02d.%02d",dir_out,
        dec_now[0],dec_now[1],dec_now[2],dec_now[3],dec_now[4]);
      if((f_out=open(file_out,O_RDWR|O_CREAT|O_TRUNC,0666))==-1)
        {
        printf("file open error : %s\n",file_out);
        end_process(1);
        }
      printf("'%s' opened\n",file_out);
      /* one minute loop */
      for(i=0;i<60;i++)
        {
        /* added 2lines 03/03/07 */
        idx = 0;
        wptr = windata;
        /*for(j=0;j<SR*nch;j++) out_data[j]=0;*/ /* deleted 03/03/07 */
        for(j=0;j<sizeof(windata);j++) windata[j]=0;
        /* read one sec data */
        if(read(f_in,&size,4)<=0)
          {
          perror("read");
          break;
          }
	size=mklong(&size);  /* for Endian free  03/04/25 */

	//printf("size=%d(%x)\n",size,size);	/* 030228 */
	//fflush(stdout);
        if(read(f_in,in_data,size-4)<=0)
          {
          perror("read");
          break;
          }
        bcd_dec(dec_buf,in_data);
        /* min_header save 03/03/07 */
        for(j=0;j<6;j++)
		min_head[j]=in_data[j];
//	printf("in_data:");
//	for(j=0;j<6;j++)
//		printf("%d ",in_data[j]);
//	printf("\ndec_buf:");
//	for(j=0;j<6;j++)
//		printf("%d ",dec_buf[j]);
//	printf("\n");

        /* check time of data */
        next=1;
        switch(time_cmp(dec_now,dec_buf,6))
          {
          case 0:
            ptr=in_data+6;
            ptr_end=ptr+size-10;
          /* channel loop */
            while(ptr<ptr_end)
              {
            /* get one channel data */
              sys_ch=0xffff&mkshort(ptr);
              pos=pos_table[sys_ch];
              if(pos<0)
                {
                sr=get_data(&ptr,lbuf,0);
                continue;
                }
              sr=get_data(&ptr,lbuf,1);
	      /*printf("ecore:get_data(sr=%d)\n",sr);*/  /* 03/03/03 */
              for(j=0;j<sr;j++) buf[j]=(double)lbuf[j];  
#if FILTER
            /* filtering */
              if(ch[pos].flag_filt==0)
                {
                dt=1.0/(double)sr;
                butlop(ch[pos].coef,&ch[pos].m_filt,
                  &ch[pos].gn_filt,&ch[pos].n_filt,
                  FP*dt,FS*dt,AP,AS);
/*printf("  ch %04X   filter order=%d\n",ch[pos].sys_ch,ch[pos].m_filt);*/
                if(ch[pos].m_filt>MAX_FILT)
                  {
                  printf("filter order exceeded limit\n");
                  end_process(1);
                  }
                ch[pos].flag_filt=1;
                }
              tandem(buf,buf,sr,ch[pos].coef,
                ch[pos].m_filt,1,ch[pos].uv);
#endif
            /* store data in out_data buffer */
              for(j=0;j<SR;j++)
                {
#if FILTER
                dl=(long)(buf[(j*sr)/SR]*ch[pos].gn_filt);
#else
                dl=(long)buf[(j*sr)/SR];
#endif
		/* deleted 2lines 03/03/07 */
                /*if(dl<(-32768)) dl=(-32768);*/
                /*else if(dl>32767) dl=32767;*/
		/* out_data => 1sec data  03/03/07 */
                /*out_data[j*nch+pos]=dl;*/
                fildata[j]=dl;		/* filtering data => fildata 03/03/07 */
		//printf("fildata[%d]=%x\n",j,fildata[j]);	/* /03/04/25 */
		//fflush(stdout);
                }
	      /* data => channel block by win format 03/03/07 */
		//idx = winform((long *)fildata,wptr,SR,(short)sys_ch); int=>long 03/04/23 
		idx = winform(fildata,wptr,SR,(short)sys_ch);
		wptr=wptr+idx;
		//printf("winform:idx=%d,sys_ch=%x\n",idx,sys_ch);  /* 03/04/23 */
		//fflush(stdout);
              }
            break;
          case 1:     /* not to write */
            next=0;
            break;
          case -1:    /* data absent */
            break;
          }

        if(next)      /* write data => win format 03/03/07 */
          {
	  /* write block size */
          bsize = wptr - windata + 10;
	  //printf("output bsize:%d(%x)\n",bsize,bsize); /* 03/03/14 */
	  //fflush(stdout);
	  c_bsize=mklong(&bsize);	/* added 03/04/22 */
	  //printf("output c_bsize:%d(%x)\n",c_bsize,c_bsize); /* 03/03/22 */
	  //fflush(stdout);
          if(write(f_out,&c_bsize,4)==(-1))
            {
            perror("fprintf");
            end_process(1);
            }
	  /* write min header */
          if(write(f_out,min_head,6)==(-1))
            {
            perror("write");
            end_process(1);
            }
	
	  /* write channel blocks */
          /*if(write(f_out,(char *)out_data,SR*2*nch)==(-1))*/ /*03/03/07 */
          if(write(f_out,(char *)windata,bsize-10)==(-1))
            {
            perror("write");
            end_process(1);
            }
          dec_now[5]++;
          adj_time(dec_now);
          }
        }
      close(f_in);
      close(f_out);	/* added 03/03/07 */
      }

    fp=fopen(file_done,"w");
    fprintf(fp,"%s",time_done);
    fclose(fp);
    printf("%s done\n",time_done);
    if(time_cmp(dec_now,dec_end,6)>0) break;
#if FIVE_MIN
    if(++cnt_min==5)
      {
      cnt_min=0;
      close(f_out);
      sprintf(file_out,"%s/%02d%02d%02d.%02d%02d",dir_out,
        dec_now[0],dec_now[1],dec_now[2],dec_now[3],dec_now[4]);
      if((f_out=open(file_out,O_RDWR|O_CREAT|O_TRUNC,0666))==-1)
        {
        printf("file open error : %s\n",file_out);
        end_process(1);
        }
      printf("'%s' opened\n",file_out);
      }
#endif
    }
  end_process(0);
  }

get_data(dp,buf,idx)
  unsigned char **dp;
  long *buf;
  int idx;
  {
  unsigned char *ddp;
  int gh,s_rate,g_size,sys_ch,i,b_size;
  short shreg;		/* added 03/05/02 */
  ddp=(*dp);
  s_rate=(gh=mklong(ddp))&0xfff;
  ddp+=4;
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+4;
  else g_size=(s_rate>>1)+4;
  *dp+=4+g_size;
  sys_ch=gh>>16;
  if(idx==0) return(s_rate);
  
  /* printf("ecore(get_data):idx=%d,b_size=%x, gh=%x, g_size=%x, sys_ch=%x\n",idx,b_size,gh,g_size,sys_ch); */ /* 03/04/25 */
  fflush(stdout);
  /* read group */
  buf[0]=mklong(ddp);
  ddp+=4;
  switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        buf[i]=buf[i-1]+((*(char *)ddp)>>4);
        if (i == s_rate - 1)
	  break;
	buf[i+1]=buf[i]+(((char)(*(ddp++)<<4))>>4);
        }
      break;
    case 1:
      for(i=1;i<s_rate;i++)
        buf[i]=buf[i-1]+(*(char *)(ddp++));
      break;
    case 2:
      for(i=1;i<s_rate;i++)
        {
	/* 2lines edit 03/05/02 N.Nakawaji
        buf[i]=buf[i-1]+mkshort(ddp);
        ddp+=2;
	*/
	shreg=((ddp[0]<<8) & 0xff00) + (ddp[1] & 0xff);
	ddp += 2;
	buf[i] = buf[i-1] + shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        buf[i]=buf[i-1]+(mklong(ddp)>>8);
        ddp+=3;
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        buf[i]=buf[i-1]+mklong(ddp);
        ddp+=4;
        }
      break;
    default:
      return(-1); /* bad header */
    }
  /*for(i=0;i<s_rate;i++) printf("%x",buf[i]);*/  /* 03/03/03 */
  /*printf("\n");*/ /* 03/03/03 */
  return(s_rate); /* normal return */
  }

#define   PI    3.141593
#define   HP    1.570796
#include  <math.h>

/*
+      BUTTERWORTH LOW PASS FILTER COEFFICIENT
+
+      ARGUMENTS
+        H      : FILTER COEFFICIENTS
+        M      : ORDER OF FILTER  (M=(N+1)/2)
+        GN     : GAIN FACTOR
+        N      : ORDER OF BUTTERWORTH FUNCTION
+        FP     : PASS BAND FREQUENCY  (NON-DIMENSIONAL)
+        FS     : STOP BAND FREQUENCY
+        AP     : MAX. ATTENUATION IN PASS BAND
+        AS     : MIN. ATTENUATION IN STOP BAND
+
+      M. SAITO  (17/XII/75)
*/
butlop(h,m,gn,n,fp,fs,ap,as)
  double *h,fp,fs,ap,as,*gn;
  int *m,*n;
  {
  double wp,ws,tp,ts,pa,sa,cc,c,dp,g,fj,c2,sj,tj,a;
  int k,j;
  if(fabs(fp)<fabs(fs)) wp=fabs(fp)*PI;
  else wp=fabs(fs)*PI;
  if(fabs(fp)>fabs(fs)) ws=fabs(fp)*PI;
  else ws=fabs(fs)*PI;
  if(wp==0.0 || wp==ws || ws>=HP)
    {
    printf("? (butlop) invalid input : fp=%14.6e fs=%14.6e ?\n",fp,fs);
    return(1);
    }
/****  DETERMINE N & C */
  tp=tan(wp);
  ts=tan(ws);
  if(fabs(ap)<fabs(as)) pa=fabs(ap);
  else pa=fabs(as);
  if(fabs(ap)>fabs(as)) sa=fabs(ap);
  else sa=fabs(as);
  if(pa==0.0) pa=0.5;
  if(sa==0.0) sa=5.0;
  if((*n=(int)fabs(log(pa/sa)/log(tp/ts))+0.5)<2) *n=2;
  cc=exp(log(pa*sa)/(double)(*n))/(tp*ts);
  c=sqrt(cc);

  dp=HP/(double)(*n);
  *m=(*n)/2;
  k=(*m)*4;
  g=fj=1.0;
  c2=2.0*(1.0-c)*(1.0+c);

  for(j=0;j<k;j+=4)
    {
    sj=pow(cos(dp*fj),2.0);
    tj=sin(dp*fj);
    fj=fj+2.0;
    a=1.0/(pow(c+tj,2.0)+sj);
    g=g*a;
    h[j  ]=2.0;
    h[j+1]=1.0;
    h[j+2]=c2*a;
    h[j+3]=(pow(c-tj,2.0)+sj)*a;
    }
/****  EXIT */
  *gn=g;
  if(*n%2==0) return(0);
/****  FOR ODD N */
  *m=(*m)+1;
  *gn=g/(1.0+c);
  h[k  ]=1.0;
  h[k+1]=0.0;
  h[k+2]=(1.0-c)/(1.0+c);
  h[k+3]=0.0;
  return(0);
  }

/*
+      RECURSIVE FILTERING : F(Z) = (1+A*Z+AA*Z**2)/(1+B*Z+BB*Z**2)
+
+      ARGUMENTS
+        X      : INPUT TIME SERIES
+        Y      : OUTPUT TIME SERIES  (MAY BE EQUIVALENT TO X)
+        N      : LENGTH OF X & Y
+        H      : FILTER COEFFICIENTS ; H(1)=A, H(2)=AA, H(3)=B, H(4)=BB
+        NML    : >0 ; FOR NORMAL  DIRECTION FILTERING
+                 <0 ; FOR REVERSE DIRECTION FILTERING
+    uv   : past data and results saved
+
+      M. SAITO  (6/XII/75)
*/
recfil(x,y,n,h,nml,uv)
  int n,nml;
  double *x,*y,*h,*uv;
  {
  int i,j,jd;
  double a,aa,b,bb,u1,u2,u3,v1,v2,v3;
  if(n<=0)
    {
    printf("? (recfil) invalid input : n=%d ?\n",n);
    return(1);
    }
  if(nml>=0)
    {
    j=0;
    jd=1;
    }
  else
    {
    j=n-1;
    jd=(-1);
    }
  a =h[0];
  aa=h[1];
  b =h[2];
  bb=h[3];
  u1=uv[0];
  u2=uv[1];
  v1=uv[2];
  v2=uv[3];
/****  FILTERING */
  for(i=0;i<n;i++)
    {
    u3=u2;
    u2=u1;
    u1=x[j];
    v3=v2;
    v2=v1;
    v1=u1+a*u2+aa*u3-b*v2-bb*v3;
    y[j]=v1;
    j+=jd;
    }
  uv[0]=u1;
  uv[1]=u2;
  uv[2]=v1;
  uv[3]=v2;
  return(0);
  }

/*
+      RECURSIVE FILTERING IN SERIES
+
+      ARGUMENTS
+        X      : INPUT TIME SERIES
+        Y      : OUTPUT TIME SERIES  (MAY BE EQUIVALENT TO X)
+        N      : LENGTH OF X & Y
+        H      : COEFFICIENTS OF FILTER
+        M      : ORDER OF FILTER
+        NML    : >0 ; FOR NORMAL  DIRECTION FILTERING
+                 <0 ;     REVERSE DIRECTION FILTERING
+    uv   : past data and results saved
+
+      SUBROUTINE REQUIRED : RECFIL
+
+      M. SAITO  (6/XII/75)
*/
tandem(x,y,n,h,m,nml,uv)
  double *x,*y,*h,*uv;
  int n,m,nml;
  {
  int i;
  if(n<=0 || m<=0)
    {
    printf("? (tandem) invalid input : n=%d m=%d ?\n",n,m);
    return(1);
    }
/****  1-ST CALL */
  recfil(x,y,n,h,nml,uv);
/****  2-ND AND AFTER */
  if(m>1) for(i=1;i<m;i++) recfil(y,y,n,&h[i*4],nml,&uv[i*4]);
  return(0);
  }


/* winform.c  4/30/91,99.4.19   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */
/*   Added 03/03/07 */

winform(inbuf,outbuf,sr,sys_ch)
  long *inbuf;      /* input data array for one sec*/
  unsigned char *outbuf;  /* output data array for one sec */
  int sr;         /* n of data (i.e. sampling rate) */
  unsigned short sys_ch;  /* 16 bit long channel ID number */
  {
  int dmin,dmax,aa,bb,br,i,byte_leng;
  long *ptr;
  unsigned char *buf;

  /* differentiate and obtain min and max */
  ptr=inbuf;
  bb=(*ptr++);
  dmax=dmin=0;
  for(i=1;i<sr;i++)
    {
    aa=(*ptr);
    *ptr++=br=aa-bb;
    bb=aa;
    if(br>dmax) dmax=br;
    else if(br<dmin) dmin=br;
    }

  /* determine sample size */
  if(((dmin&0xfffffff8)==0xfffffff8 || (dmin&0xfffffff8)==0) &&
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0)) byte_leng=0;
  else if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0)) byte_leng=1;
  else if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0)) byte_leng=2;
  else if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0)) byte_leng=3;
  else byte_leng=4;
  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  *buf++=inbuf[0]>>24;
  *buf++=inbuf[0]>>16;
  *buf++=inbuf[0]>>8;
  *buf++=inbuf[0];
  /* second and after */
  //printf("in winform: byte_leng=%d\n",byte_leng);  /* 03/04/23 */
  fflush(stdout);
  switch(byte_leng)
    {
    case 0:
      for(i=1;i<sr-1;i+=2)
        *buf++=(inbuf[i]<<4)|(inbuf[i+1]&0xf);
      if(i==sr-1) *buf++=(inbuf[i]<<4);
      break;
    case 1:
      for(i=1;i<sr;i++)
        *buf++=inbuf[i];
      break;
    case 2:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>24;
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    }
  return (int)(buf-outbuf);
  }

