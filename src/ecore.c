/* $Id: ecore.c,v 1.4.4.2.2.2 2008/11/13 03:03:02 uehira Exp $ */
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

#include  <stdio.h>
#include  <sys/file.h>
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <fcntl.h>

#include "winlib.h"

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

/* bcd_dec(dest,sour) */
/*   char *sour; */
/*   int *dest; */
/*   { */
/*   int cntr; */
/*   for(cntr=0;cntr<6;cntr++) */
/*     dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf); */
/*   } */

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
  uint8_w sizetmp[4];
  //int fildata[SSR];   int => long 03/04/23
  int32_w fildata[SSR];
  unsigned char windata[500*1000],*wptr;  /* added 03/03/07 */
  int idx,bsize;  				  /* added 03/03/07 */
  unsigned char min_head[6];  		  /* added 03/03/07 */
  uint8_w c_bsize[4];			/* added 03/04/22 */

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
        if(read(f_in,sizetmp,4)<=0)
          {
          perror("read");
          break;
          }
	size=mkuint4(sizetmp);  /* for Endian free  03/04/25 */

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
              sys_ch=0xffff&mkuint2(ptr);
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
		wptr=wptr+idx;
		//idx = winform((long *)fildata,wptr,SR,(short)sys_ch); int=>long 03/04/23 
		idx = winform(fildata,wptr,SR,(short)sys_ch);
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
	  c_bsize[0] = bsize >>24;
	  c_bsize[1] = bsize >>16;
	  c_bsize[2] = bsize >>8;
	  c_bsize[3] = bsize;
	  /* c_bsize=mkuint4(&bsize);	/\* added 03/04/22 *\/ */
	  //printf("output c_bsize:%d(%x)\n",c_bsize,c_bsize); /* 03/03/22 */
	  //fflush(stdout);
          if(write(f_out,c_bsize,4)==(-1))
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
  s_rate=(gh=mkuint4(ddp))&0xfff;
  ddp+=4;
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+4;
  else g_size=(s_rate>>1)+4;
  *dp+=4+g_size;
  sys_ch=gh>>16;
  if(idx==0) return(s_rate);

  //printf("ecore(get_data):idx=%d,b_size=%x, gh=%x, g_size=%x, sys_ch=%x\n",idx,b_size,gh,g_size,sys_ch); /* 03/04/25 */
  fflush(stdout);
  /* read group */
  buf[0]=mkuint4(ddp);
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
	/* 2lines edit 03/05/02 N.Nakawaji
        buf[i]=buf[i-1]+mkuint2(ddp);
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
        buf[i]=buf[i-1]+(mkuint4(ddp)>>8);
        ddp+=3;
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        buf[i]=buf[i-1]+mkuint4(ddp);
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
