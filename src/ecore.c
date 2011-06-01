/* $Id: ecore.c,v 1.6 2011/06/01 11:09:20 uehira Exp $ */

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
/*  2010/10/08 64bit clean? fixed bug (Uehira) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <sys/types.h>
#include  <sys/file.h>
#include  <sys/stat.h>

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>
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

static const char rcsid[] = 
  "$Id: ecore.c,v 1.6 2011/06/01 11:09:20 uehira Exp $";

/* not use out_data 03/03/07 */
/*short out_data[SR*NCH];*/
/* change size 100*1000=> 500*1000 03/02/28 */
/*unsigned char in_data[100*1000]; */
static uint8_w in_data[500*1000];
static int pos_table[CHMASK+1],f_out,f_in;
static char *path_raw,file_written[NAMLEN],file_done[NAMLEN],
  *file_list,file_out[NAMLEN],dir_out[NAMLEN],
  name_file[NAMLEN],time_done[NAMLEN];
static FILE *fp;

struct {
  int sys_ch;
  int flag_filt;
  int m_filt;
  int n_filt;
  double gn_filt;
  double coef[MAX_FILT*4];
  double uv[MAX_FILT*4];
  } static ch[NCH];

/* prototypes */
static void end_process(int);
static void bfov_error(void);
static WIN_sr get_data(uint8_w **, int32_w *, int);
int main(int, char *[]);

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

static void
end_process(int value)
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

static void
bfov_error(void)
{

  fprintf(stderr, "Buffer overrun!\n");
  end_process(1);
}


int
main(int argc, char *argv[])
  {
  double buf[SSR],dt;
  int32_w lbuf[SSR],dl;
  uint8_w *ptr,*ptr_end;
  char  textbuf[NAMLEN];
  int dec_start[6],dec_end[6],dec_wtn[6],dec_now[6],dec_buf[6],
    i,j,nch,next,sys_ch,pos,cnt_min;
  /* int fildata[SSR];   int => long 03/04/23 */
  WIN_sr  sr;
  uint32_w  size;
  uint8_w sizetmp[4];
  int32_w fildata[SSR];
  uint8_w windata[500*1000],*wptr;  /* added 03/03/07 */
  /* WIN_bs idx;  				  /\* added 03/03/07 *\/ */
  uint32_w  bsize;
  uint8_w min_head[6];  		  /* added 03/03/07 */
  uint8_w c_bsize[4];			/* added 03/04/22 */

  f_in=f_out=(-1);
  printf("***** ecore start *****\n");
  if(argc<5)
    {
    WIN_version();
    printf("%s\n", rcsid);
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
  path_raw = argv[3];
  printf("input directory (raw) : %s\n",path_raw);
  if (snprintf(file_written, sizeof(file_written), "%s/%s", path_raw,
	       WDISK_LATEST) >= sizeof(file_written)) /* or FROMTAPE_LATEST */
    bfov_error();
  if (snprintf(file_done, sizeof(file_done), "%s/%s",
	       path_raw, ECORE_USED) >= sizeof(file_done))
    bfov_error();

  /* read channel list */
  file_list = argv[4];
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

  for(;;)
    {
    while((fp=fopen(file_written,"r"))==NULL)
      {
      printf("waiting '%s' to be created ...\n",file_written);
      sleep(15);
      }
    fscanf(fp,"%02d%02d%02d%02d.%02d",&dec_wtn[0],&dec_wtn[1],
      &dec_wtn[2],&dec_wtn[3],&dec_wtn[4]);
    fclose(fp);
/* //printf("next   =%02d%02d%02d%02d.%02d\n",dec_now[0],dec_now[1],dec_now[2], */
/* //  dec_now[3],dec_now[4]); */
/* //printf("written=%02d%02d%02d%02d.%02d\n",dec_wtn[0],dec_wtn[1],dec_wtn[2], */
/* //  dec_wtn[3],dec_wtn[4]); */
    if(time_cmp(dec_wtn,dec_now,6)<0)
      {
      printf("waiting '%s' to be updated ...\n",file_written);
      sleep(15);
      continue;
      }
    /* file ready */
    if (snprintf(name_file, sizeof(name_file),"%s/%02d%02d%02d%02d.%02d",
		 path_raw,  dec_now[0], dec_now[1],
		 dec_now[2], dec_now[3], dec_now[4]) >= sizeof(name_file))
      bfov_error();
    if (snprintf(time_done, sizeof(time_done), "%02d%02d%02d%02d.%02d",
		 dec_now[0], dec_now[1], dec_now[2],
		 dec_now[3], dec_now[4]) >= sizeof(time_done))
      bfov_error();
    if((f_in=open(name_file,O_RDONLY))==-1)
      {
      printf("no file : %s\n",name_file);

      /* /\*\//------------ no input file => no output file 03/03/12  */
      /* //for(i=0;i<60;i++) */
      /* //  { */
      /* //  /\* output dummy *\/ */
      /* //  for(j=0;j<sizeof(windata);j++) windata[j]=0; */
      /* //  /\*if(write(f_out,(char *)out_data,h)==(-1))*\/ /\*03/03/07 *\/ */
      /* //  if(write(f_out,(char *)windata,sizeof(windata))==(-1)) */
      /* //    { */
      /* //    perror("write"); */
      /* //    end_process(1); */
      /* //    } */
      /* //  dec_now[5]++; */
      /* //  adj_time(dec_now); */
      /* //  } */
      /* //-------------------------------------------------------*\/ */
      dec_now[4]++;
      adj_time(dec_now);
      continue;  /* return loop */
      }
    else
      {
      printf("'%s' opened\n",name_file);
      fflush(stdout); /* 03/03/07 */
      /* out file 03/03/07 */
      if (snprintf(file_out, sizeof(file_out), "%s/%02d%02d%02d%02d.%02d",
		   dir_out, dec_now[0], dec_now[1],
		   dec_now[2],dec_now[3],dec_now[4]) >= sizeof(file_out))
	bfov_error();
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
	/* idx = 0; */
        wptr = windata;
        /*for(j=0;j<SR*nch;j++) out_data[j]=0;*/ /* deleted 03/03/07 */
        /* for(j=0;j<sizeof(windata);j++) windata[j]=0; */
	memset(windata, 0, sizeof(windata));
        /* read one sec data */
        if(read(f_in,sizetmp,4)!=4)
          {
          perror("read");
          break;
          }
	size=mkuint4(sizetmp);  /* for Endian free  03/04/25 */
	if (sizeof(in_data) < size - 4)
	  bfov_error();
	/* //printf("size=%d(%x)\n",size,size);	/\* 030228 *\/ */
	/* //fflush(stdout); */
        if(read(f_in,in_data,size-4)!=size-4)
          {
          perror("read");
          break;
          }
        bcd_dec(dec_buf,in_data);
        /* min_header save 03/03/07 */
        for(j=0;j<6;j++)
		min_head[j]=in_data[j];
/* //	printf("in_data:"); */
/* //	for(j=0;j<6;j++) */
/* //		printf("%d ",in_data[j]); */
/* //	printf("\ndec_buf:"); */
/* //	for(j=0;j<6;j++) */
/* //		printf("%d ",dec_buf[j]); */
/* //	printf("\n"); */

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
                dl=(int32_w)(buf[(j*sr)/SR]*ch[pos].gn_filt);
#else
                dl=(int32_w)buf[(j*sr)/SR];
#endif
		/* deleted 2lines 03/03/07 */
                /*if(dl<(-32768)) dl=(-32768);*/
                /*else if(dl>32767) dl=32767;*/
		/* out_data => 1sec data  03/03/07 */
                /*out_data[j*nch+pos]=dl;*/
                fildata[j]=dl;		/* filtering data => fildata 03/03/07 */
		/* //printf("fildata[%d]=%x\n",j,fildata[j]);	/\* /03/04/25 *\/ */
		/* //fflush(stdout); */
                }
	      /* data => channel block by win format 03/03/07 */
		/* //idx = winform((long *)fildata,wptr,SR,(short)sys_ch); int=>long 03/04/23  */
		wptr += winform(fildata,wptr,SR,(WIN_ch)sys_ch);
		/* wptr=wptr+idx; */
		/* //printf("winform:idx=%d,sys_ch=%x\n",idx,sys_ch);  /\* 03/04/23 *\/ */
		/* //fflush(stdout); */
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
	    bsize = (uint32_w)(wptr - windata + 10);
	  //printf("output bsize:%d(%x)\n",bsize,bsize); /* 03/03/14 */
	  //fflush(stdout);
	  c_bsize[0] = bsize >>24;
	  c_bsize[1] = bsize >>16;
	  c_bsize[2] = bsize >>8;
	  c_bsize[3] = bsize;
	  /* c_bsize=mkuint4(&bsize);	/\* added 03/04/22 *\/ */
	  /* //printf("output c_bsize:%d(%x)\n",c_bsize,c_bsize); /\* 03/03/22 *\/ */
	  /* //fflush(stdout); */
          if(write(f_out,c_bsize,4) != 4)
            {
            perror("fprintf");
            end_process(1);
            }
	  /* write min header */
          if(write(f_out,min_head,6) != 6)
            {
            perror("write");
            end_process(1);
            }
	
	  /* write channel blocks */
          /*if(write(f_out,(char *)out_data,SR*2*nch)==(-1))*/ /*03/03/07 */
          if(write(f_out,windata,bsize-10) != bsize-10)
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
      if (snprintf(file_out, sizeof(file_out), "%s/%02d%02d%02d.%02d%02d",
		   dir_out, dec_now[0], dec_now[1], dec_now[2],
		   dec_now[3],dec_now[4]) >= sizeof(file_out))
	bfov_error();
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

static WIN_sr
get_data(uint8_w **dp, int32_w *buf, int idx)
  {
  uint8_w *ddp;
  WIN_sr  s_rate;
  WIN_ch  sys_ch;
  WIN_bs  g_size;
  /* int gh,i,b_size; */
  /* short shreg;	 */	/* added 03/05/02 */

  ddp=(*dp);
  g_size = win_get_chhdr(ddp, &sys_ch, &s_rate);
  *dp += g_size;
#if DEBUG
  printf("ecore(get_data):idx=%d,s_rate=%d g_size=%u, sys_ch=%04X\n",
	 idx,s_rate,g_size,sys_ch);
  fflush(stdout);
#endif
  if (idx == 0)
    return(s_rate);

  g_size = win2fix(ddp, buf, &sys_ch, &s_rate);
  if (g_size == 0)
    return (-1);
  else
    return(s_rate);

  /* s_rate=(gh=mkuint4(ddp))&0xfff; */
  /* ddp+=4; */
  /* if((b_size=(gh>>12)&0xf)) g_size=b_size*(s_rate-1)+4; */
  /* else g_size=(s_rate>>1)+4; */
  /* *dp+=4+g_size; */
  /* sys_ch=gh>>16; */
  /* if(idx==0) return(s_rate); */

  //printf("ecore(get_data):idx=%d,b_size=%x, gh=%x, g_size=%x, sys_ch=%x\n",idx,b_size,gh,g_size,sys_ch); /* 03/04/25 */
  /* fflush(stdout); */
  /* read group */
  /* buf[0]=mkuint4(ddp); */
  /* ddp+=4; */
  /* switch(b_size) */
  /*   { */
  /*   case 0: */
  /*     for(i=1;i<s_rate;i+=2) */
  /*       { */
  /*       buf[i]=buf[i-1]+((*(char *)ddp)>>4); */
  /*       buf[i+1]=buf[i]+(((char)(*(ddp++)<<4))>>4); */
  /*       } */
  /*     break; */
  /*   case 1: */
  /*     for(i=1;i<s_rate;i++) */
  /*       buf[i]=buf[i-1]+(*(char *)(ddp++)); */
  /*     break; */
  /*   case 2: */
  /*     for(i=1;i<s_rate;i++) */
  /*       { */
  /* 	/\* 2lines edit 03/05/02 N.Nakawaji */
  /*       buf[i]=buf[i-1]+mkuint2(ddp); */
  /*       ddp+=2; */
  /* 	*\/ */
  /* 	shreg=((ddp[0]<<8) & 0xff00) + (ddp[1] & 0xff); */
  /* 	ddp += 2; */
  /* 	buf[i] = buf[i-1] + shreg; */
  /*       } */
  /*     break; */
  /*   case 3: */
  /*     for(i=1;i<s_rate;i++) */
  /*       { */
  /*       buf[i]=buf[i-1]+(mkuint4(ddp)>>8); */
  /*       ddp+=3; */
  /*       } */
  /*     break; */
  /*   case 4: */
  /*     for(i=1;i<s_rate;i++) */
  /*       { */
  /*       buf[i]=buf[i-1]+mkuint4(ddp); */
  /*       ddp+=4; */
  /*       } */
  /*     break; */
  /*   default: */
  /*     return(-1); /\* bad header *\/ */
  /*   } */
  /* /\*for(i=0;i<s_rate;i++) printf("%x",buf[i]);*\/  /\* 03/03/03 *\/ */
  /* /\*printf("\n");*\/ /\* 03/03/03 *\/ */
  /* return(s_rate); /\* normal return *\/ */
  }
