/* $Id: wck.c,v 1.5.2.3 2011/06/01 12:14:54 uehira Exp $ */
/*- 
   program "wck.c"
	"wck" checks a win format data file
	7/31/91 - 7/31/91, 3/11/93, 3/28/94, 4/20/94   urabe
	8/5/96   Little Endian (uehira)
	9/12/96  sampling rate (uehira) 
        97.9.21  simplified     urabe
        97.9.24  MON format(-m) urabe
        98.11.26 WIN2 format urabe ; "-h" is necessary for HSR format
        99.4.19  BE/LE urabe
        2000.2.1 added "Count" mode
        2000.8.10 added "Table" mode
        2000.8.14 Count and Table modes made independent
        2008.11.14 64 bit env. clean. (uehira)
-*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "winlib.h"

#define RAW 0x00
#define MON 0x01
#define RAW_HSR 0x02
#define COUNT 0x10
#define TABLE 0x20
/* #define SR_MON 5 */

#define DEBUG1  0

static const char rcsid[] =
  "$Id: wck.c,v 1.5.2.3 2011/06/01 12:14:54 uehira Exp $";

char *progname;
static unsigned long count[WIN_CHMAX];

static void ctrlc(void);
static void usage(void);
int main(int, char *[]);

static void
ctrlc(void)
{

  exit(0);
}

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr,
	  " usage : '%s (-r/-m/-h/-c/-t) ([data file]/- ([sec position]))'\n",
	  progname);
}

int
main(int argc, char *argv[])
{
   int i,j,k,ii,chs,chs16,mode,c,nch;
   uint32_w mainsize,gs;
   int size;
   unsigned long ts,sec,ss;
   WIN_ch   sysch;
   WIN_sr   sr;
   FILE *f_main;
   char bytes[5];
   static uint8_w *mainbuf=NULL;
   size_t  mainbuf_siz;
   uint8_w *ptr,*ptr_lim;
   
   signal(SIGINT,(void *)ctrlc);
   signal(SIGTERM,(void *)ctrlc);
   signal(SIGPIPE,(void *)ctrlc);
   if((progname=strrchr(argv[0],'/')) != NULL) progname++;
   else progname=argv[0]; 
   mode=RAW;
   while((c=getopt(argc,argv,"mrhctu"))!=-1)
     {
     switch(c)
       {
       case 'c':   /* "Count" mode */
         mode|=COUNT;
         for(i=0;i<WIN_CHMAX;i++) count[i]=0;
         break;
       case 'm':   /* MON data */
         mode=(0xf0 & mode)|MON;
         break;
       case 'r':   /* RAW data */
         mode=(0xf0 & mode)|RAW;
         break;
       case 'h':   /* High sampling rate format RAW data */
         mode=(0xf0 & mode)|RAW_HSR;
         break;
       case 't':   /* "Table" mode (in Count mode) */
         mode|=TABLE;
         for(i=0;i<WIN_CHMAX;i++) count[i]=0;
         break;
       case 'u':   /* show usage */
       default:
	 usage();
         exit(1);
       }
     }

   if(argc<optind+1 || strcmp("-",argv[optind])==0) f_main=stdin;
   else if((f_main=fopen(argv[optind],"r"))==NULL){
     usage();
     exit(1);
   }
   if(argc>optind+1) sscanf(argv[optind+1], "%lu", &ss);
   else ss=0;
   
   sec=ts=0;
   while((mainsize=read_onesec_win(f_main,&mainbuf,&mainbuf_siz))) {
#if DEBUG1
     printf("mainsize = %u\n", mainsize);
#endif
     ptr_lim=mainbuf+mainsize;
#if DEBUG
     printf("ptr_lim=%p buf=%p size=%08x\n",ptr_lim,mainbuf,mainsize);
#endif
     ptr=mainbuf+10;
     nch=0;
     do
       {
/*        sysch=(WIN_ch)ptr[1]+(((WIN_ch)ptr[0]<<8)); */
       if(mode&MON)
	 gs=get_sysch_mon(ptr,&sysch);
       else
	 gs=win_chheader_info(ptr,&sysch,&sr,&size);
       if(mode&(COUNT|TABLE)) count[sysch]++;
       if(mode&MON)
         {
         if(!(mode&(COUNT|TABLE)) && sec==ss) {
           printf("%4d : ch %04hX    %3d Hz  %4u B\n",nch+1,sysch,SR_MON,gs);
           }
         }
       else
         {
         if((ptr[2]&0x80)==0) /* channel header = 4 byte */
           {
/*            sr=ptr[3]+(((WIN_sr)(ptr[2]&0x0f))<<8); */
/*            size=(ptr[2]>>4)&0x7; */
/*            if(size) gs=size*(sr-1)+8; */
/*            else gs=(sr>>1)+8; */
#if DEBUG
printf("gs=%u gh=%02x%02x%02x%02x%02x sr=%u gs=%u ptr=%p ptr_lim=%p\n",
  gs,ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],sr,gs,ptr,ptr_lim); 
#endif
           if(!(mode&(COUNT|TABLE)) && sec==ss)
             {
             switch(size)
               {
               case 0:
                 strcpy(bytes,"0.5");
                 break;
               case 1:
                 strcpy(bytes,"1  ");
                 break;
               case 2:
                 strcpy(bytes,"2  ");
                 break;
               case 3:
                 strcpy(bytes,"3  ");
                 break;
               case 4:
                 strcpy(bytes,"4  ");
                 break;
               default:
                 strcpy(bytes,"  ?");
                 break;
               }
             printf("%4d : ch %04hX    %3u Hz  x  %s B  = %4u B\n",
               nch+1,sysch,sr,bytes,gs);
             }
           }
         else              /* channel header = 5 byte */
           {
           if(mode&RAW_HSR)
             {
/*              sr=ptr[4]+(((WIN_sr)ptr[3])<<8)+(((WIN_sr)(ptr[2]&0x0f))<<16); */
/*              size=(ptr[2]>>4)&0x7; */
/*              if(size) gs=size*(sr-1)+8; */
/*              else gs=(sr>>1)+8; */
/*              gs++; */
             if(!(mode&(COUNT|TABLE)) && sec==ss)
               {
               switch(size)
                 {
                 case 0:
                   strcpy(bytes,"0.5");
                   break;
                 case 1:
                   strcpy(bytes,"1  ");
                   break;
                 case 2:
                   strcpy(bytes,"2  ");
                   break;
                 case 3:
                   strcpy(bytes,"3  ");
                   break;
                 case 4:
                   strcpy(bytes,"4  ");
                   break;
                 default:
                   strcpy(bytes,"  ?");
                   break;
                 }
               printf("%4d : ch %04hX    %3u Hz  x  %s B  = %4u B\n",
                 nch+1,sysch,sr,bytes,gs);
               }
             }
           else /* WIN2 format */
             {
             sr=ptr[3]+(((WIN_sr)(ptr[2]&0x03))<<8);
             size=((ptr[2]>>2)&0x1F)+1; /* in bits */
             gs=((sr-1)*size-1)/8+1+8;
             if(!(mode&(COUNT|TABLE)) && sec==ss)
               printf("%4d : ch %04hX    %3u Hz  x  %2u b  = %4d B\n",
               nch+1,sysch,sr,size,gs);
             }
           }
         }
       ptr+=gs;
       nch++;
       } while(ptr<ptr_lim);
     if(!(mode&(COUNT|TABLE)))
       {
       if(sec==ss) printf("\n");
       printf("%4ld : %02x%02x%02x %02x%02x%02x",sec+1,mainbuf[4],
	      mainbuf[5],mainbuf[6],mainbuf[7],mainbuf[8],mainbuf[9]);
       printf("   %d ch  (%u bytes)\n",nch,mainsize);
       }
     ts+=mainsize;
     sec++;
   }
   if(!(mode&(COUNT|TABLE))) printf("\nlength = %lu s  (%lu bytes)\n\n",sec,ts);
   if(mode&TABLE)
     {
     chs=chs16=0;
     printf("    +000             +100             +200             +300\n");
     for(i=0;i<0x10000;i+=0x100)
       {
       if(i%0x1000==0) printf("%04X ",i);
       else if(i%0x400==0) printf("%4X ",i&0xFFF);
       for(j=i;j<i+0x100;j+=0x10)
         {
         ii=0;
         for(k=j;k<j+0x10;k++) if(count[k]>0) {ii++;chs++;}
         if(ii)
           {
           if(ii<0x10) printf("%X",ii);
           else printf("G");
           chs16++;
           }
         else printf("-");
         }
       if(i%0x400==0x300) printf("\n");
       else printf(" ");
       }
     printf("%d chs used (max %d)\n",chs,0x10000);
     }
   if(mode&COUNT)
     {for(i=0;i<WIN_CHMAX;i++) if(count[i]>0) printf("%04X %lu\n",i,count[i]);}
   exit(0);
}
