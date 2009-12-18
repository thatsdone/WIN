/* $Id: wed.c,v 1.5.4.3.2.4 2009/12/18 11:33:45 uehira Exp $ */
/* program "wed.c"
	"wed" edits a win format data file by time range and channles
	6/26/91,7/13/92,3/11/93,4/20/94,8/5/94,12/8/94   urabe
	2/5/97    Little Endian (uehira)
	2/5/97    High sampling rate (uehira)
        98.6.26 yo20000
        99.4.19 byte-order-free
        2000.4.17   wabort
        2002.8.5    ignore illegal lines in ch file
        2003.10.29 exit()->exit(0)
        2006.9.21 added comment on mklong()
*/

#ifdef HAVE_CONFIG_H
#include        "config.h"
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<signal.h>

#include        "winlib.h"

/* #define		DEBUG		0 */
#define		DEBUG1		0

unsigned char *buf,*outbuf;
int leng,dec_start[6],dec_end[6],dec_now[6],nch,sysch[WIN_CHMAX];
FILE *f_param;

/* bcd_dec(dest,sour) */
/*      char *sour; */
/*      int *dest; */
/* { */
/*    int cntr; */
/*    for(cntr=0;cntr<6;cntr++) */
/*      dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf); */
/* } */

wabort() {exit(0);}

get_one_record()
{
   int i;
   unsigned long  outsize;

   for(i=0;i<6;i++) dec_end[i]=dec_start[i];
   for(i=leng-1;i>0;i--){
      dec_end[5]++;
      adj_time(dec_end);
   }
#if DEBUG
   fprintf(stderr,"start:%02d%02d%02d %02d%02d%02d\n",
	   dec_start[0],dec_start[1],dec_start[2],
	   dec_start[3],dec_start[4],dec_start[5]);
   fprintf(stderr,"end  :%02d%02d%02d %02d%02d%02d\n",
	   dec_end[0],dec_end[1],dec_end[2],
	   dec_end[3],dec_end[4],dec_end[5]);
#endif
   /* read first block */
   do{
      if(read_data()<=0){
	 exit(1);
      }
      bcd_dec(dec_now,(char *)buf+4);
#if DEBUG
      fprintf(stderr,"\r%02x%02x%02x %02x%02x%02x  %5d",buf[4],buf[5],
	      buf[6],buf[7],buf[8],buf[9],*(int *)buf);
      fflush(stderr);
      /*fprintf(stderr,"%02x%02x%02x %02x%02x%02x  %5d\n",buf[4],buf[5],
	      buf[6],buf[7],buf[8],buf[9],*(int *)buf);*/
#endif
   } while(time_cmp(dec_start,dec_now,6)>0);

   while(1){
      outsize=select_ch(sysch,nch,buf,outbuf);
      if(outsize>10 || leng>=0)
	/* write one sec */
	if(fwrite(outbuf,1,outsize,stdout)==0) exit(1);
      if(leng>=0 && time_cmp(dec_now,dec_end,6)==0) break;
      /* read one sec */
      if(read_data()<=0) break;
      bcd_dec(dec_now,(char *)buf+4);
      if(leng>=0 && time_cmp(dec_now,dec_end,6)>0) break;
#if DEBUG
      fprintf(stderr,"\rnow  :%02x%02x%02x %02x%02x%02x",buf[4],buf[5],
	      buf[6],buf[7],buf[8],buf[9]);
      fflush(stderr);
#endif
   }
#if DEBUG
   fprintf(stderr," : done\n");
#endif
}

read_data()
{
   static unsigned int size;
   int re;
   unsigned char  tmpa[4];
 
   /* if(fread(&re,1,4,stdin)==0) return 0; */
   /* i=1;if(*(char *)&i) SWAPL(re); */
   if(fread(tmpa,1,4,stdin)==0) return 0;
   re = mkuint4(tmpa);
   if(buf==0){
      buf=(unsigned char *)malloc(size=re*2);
      outbuf=(unsigned char *)malloc(size=re*2);
   }
   else if(re>size){
      buf=(unsigned char *)realloc(buf,size=re*2);
      outbuf=(unsigned char *)realloc(outbuf,size=re*2);
   }
   /*  *(int *)buf=re; */
   memcpy(buf, tmpa, 4);
   re=fread(buf+4,1,re-4,stdin);
   return re;
}

select_ch(sys_ch,n_ch,old_buf,new_buf)
     unsigned char *old_buf,*new_buf;
     int *sys_ch,n_ch;
{
   int i,j,size,gsize,new_size,sr;
   unsigned char *ptr,*new_ptr,*ptr_lim;
   unsigned char gh[5];
 
   size=mkuint4(old_buf);
   ptr_lim=old_buf+size;
   ptr=old_buf+4;
   new_ptr=new_buf+4;
   for(i=0;i<6;i++) *new_ptr++=(*ptr++);
   new_size=10;
   do{
      for(i=0;i<5;i++) gh[i]=ptr[i];
      i=((((long)gh[0])<<8)+gh[1])&0x0000ffff;
#if DEBUG1
      fprintf(stderr,"i=%04X\n", i);
#endif
      /* sampling rate */
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
      for(j=0;j<n_ch;j++) if(i==sys_ch[j]) break;
      if(n_ch<0 || j<n_ch){
	 new_size+=gsize;
	 while(gsize-->0) *new_ptr++=(*ptr++);
      }
      else ptr+=gsize;
   } while(ptr<ptr_lim);
   new_buf[0]=new_size>>24;
   new_buf[1]=new_size>>16;
   new_buf[2]=new_size>>8;
   new_buf[3]=new_size;
   return new_size;
}

main(argc,argv)
     int argc;
     char *argv[];
{
   char tb[100];

   signal(SIGINT,(void *)wabort);
   signal(SIGTERM,(void *)wabort);
   
   if(argc<4){
      fprintf(stderr," usage of 'wed' :\n");
      fprintf(stderr,"   'wed [YYMMDD] [hhmmss] [len(s)] ([ch list file]) <[in_file] >[out_file]'\n");
      fprintf(stderr," example of channel list file :\n");
      fprintf(stderr,"   '01d 01e 01f 100 101 102 119 11a 11b'\n");
      exit(0);
   }
   
   if(argc>4){
      if((f_param=fopen(argv[4],"r"))==NULL){
	 perror("fopen");
	 exit(1);
      }
      nch=0;
      while(fscanf(f_param,"%s",tb)!=EOF)
        {
        if(sscanf(tb,"%x",&sysch[nch])!=1) continue;
#if DEBUG
	fprintf(stderr," %04X",sysch[nch]);
#endif
        nch++;
        }
#if DEBUG
      fprintf(stderr,"\n  <- %d chs according to '%s'\n",nch-1,argv[4]);
#endif
      fclose(f_param);
   }
   else nch=(-1);
   sscanf(argv[1],"%2d%2d%2d",dec_start,dec_start+1,dec_start+2);
   sscanf(argv[2],"%2d%2d%2d",dec_start+3,dec_start+4,dec_start+5);
   sscanf(argv[3],"%d",&leng);
   get_one_record();
   exit(0);
}
