/* $Id: ecore.c,v 1.3 2002/01/13 06:57:50 uehira Exp $ */
/* ddr news program "ecore.c"
  "ecore.c" works with "fromtape.c"
  "ecore.c" makes continuously filtered and decimated data
  from EXABYTE tape data
  3/13/91-7/31/91, 9/19/91-9/26/91,6/19/92,3/24/93,6/17/94 urabe
  98.6.26 yo2000
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <sys/file.h>
#include  <sys/types.h>
#include  <sys/stat.h>
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
#define   NCH     400     /* max of output channels */
#define   CHMASK    0xffff    /* max of name channels */
#define   MAX_FILT  25      /* max of filter order */
#define   NAMLEN    80

short out_data[SR*NCH];
unsigned char in_data[100*1000];
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

main(argc,argv)
  int argc;
  char *argv[];
  {
  double buf[SSR],dt;
  long lbuf[SSR],dl;
  unsigned char *ptr,*ptr_end,textbuf[NAMLEN];
  int dec_start[6],dec_end[6],dec_wtn[6],dec_now[6],dec_buf[6],
    i,j,nch,next,sys_ch,sr,pos,size,cnt_min;

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
  for(i=0;i<5;i++) dec_now[i]=dec_start[i];
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
    printf(" %04X",i);
    ch[nch].sys_ch=i;
    ch[nch].flag_filt=0;
    pos_table[i]=nch;
    if(++nch==NCH) break;
    }
  fclose(fp);
  printf("\nN of channels = %d\n",nch);

  /* read output directory */
  if(argc>5) sscanf(argv[5],"%s",dir_out);
  else strcpy(dir_out,".");
  sprintf(file_out,"%s/%02d%02d%02d.%02d%02d",dir_out,
    dec_now[0],dec_now[1],dec_now[2],dec_now[3],dec_now[4]);
  if((f_out=open(file_out,O_RDWR|O_CREAT|O_TRUNC,0666))==-1)
    {
    printf("file open error : %s\n",file_out);
    end_process(1);
    }
  printf("'%s' opened\n",file_out);
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
/*printf("next   =%02d%02d%02d%02d.%02d\n",dec_now[0],dec_now[1],dec_now[2],
  dec_now[3],dec_now[4]);
printf("written=%02d%02d%02d%02d.%02d\n",dec_wtn[0],dec_wtn[1],dec_wtn[2],
  dec_wtn[3],dec_wtn[4]);*/
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
      for(i=0;i<60;i++)
        {
        /* output dummy */
        for(j=0;j<SR*nch;j++) out_data[j]=0;
        if(write(f_out,(char *)out_data,SR*nch)==(-1))
          {
          perror("write");
          end_process(1);
          }
        dec_now[5]++;
        adj_time(dec_now);
        }
      }
    else
      {
      printf("'%s' opened\n",name_file);
      /* one minute loop */
      for(i=0;i<60;i++)
        {
        for(j=0;j<SR*nch;j++) out_data[j]=0;
        /* read one sec data */
        if(read(f_in,&size,4)<=0)
          {
          perror("read");
          break;
          }
        if(read(f_in,in_data,size-4)<=0)
          {
          perror("read");
          break;
          }
        bcd_dec(dec_buf,in_data);
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
                if(dl<(-32768)) dl=(-32768);
                else if(dl>32767) dl=32767;
                out_data[j*nch+pos]=dl;
                }
              }
            break;
          case 1:     /* not to write */
            next=0;
            break;
          case -1:    /* data absent */
            break;
          }

        if(next)      /* write data */
          {
          if(write(f_out,(char *)out_data,SR*2*nch)==(-1))
            {
            perror("write");
            end_process(1);
            }
          dec_now[5]++;
          adj_time(dec_now);
          }
        }
      close(f_in);
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
  ddp=(*dp);
  s_rate=(gh=mklong(ddp))&0xfff;
  ddp+=4;
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+4;
  else g_size=(s_rate>>1)+4;
  *dp+=4+g_size;
  sys_ch=gh>>16;
  if(idx==0) return(s_rate);

  /* read group */
  buf[0]=mklong(ddp);
  ddp+=4;
  switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        buf[i]=buf[i-1]+((*(char *)ddp)>>4);
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
        buf[i]=buf[i-1]+mkshort(ddp);
        ddp+=2;
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
