/* $Id: wck.c,v 1.2 2000/04/30 10:05:23 urabe Exp $ */
/* 
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define	DEBUG   0
#define DEBUG1  0
#define LongFromBigEndian(a) \
  ((((unsigned char *)&(a))[0]<<24)+(((unsigned char *)&(a))[1]<<16)+ \
  (((unsigned char *)&(a))[2]<<8)+((unsigned char *)&(a))[3])
int count[65536];

ctrlc() {exit(0);}

read_data(ptr,fp)
   FILE *fp;
   unsigned char **ptr;
{
   static unsigned int size;
   int re;
   
   if(fread(&re,1,4,fp)==0) return 0;
   re=LongFromBigEndian(re);
   if(*ptr==0) *ptr=(unsigned char *)malloc(size=re*2);
   else if(re>size) *ptr=(unsigned char *)realloc(*ptr,size=re*2);
   *(int *)*ptr=re;
   if(fread(*ptr+4,1,re-4,fp)==0) return 0;
#if DEBUG
/*   fprintf(stderr,"%02x%02x%02x%02x%02x%02x %d\n",*ptr[4],*ptr[5],*ptr[6],
     *ptr[7],*ptr[8],*ptr[9],re);*/
/* Segmentation Fault! Why? (uehira) */
#endif
   return re;
}

main(argc,argv)
   int argc;
   char *argv[];
{
   int i,j,mode,c,nch,sec,size,sysch,mainsize,sr,gs,ts,ss;
   FILE *f_main;
   char bytes[5],*progname;
   static unsigned char *mainbuf;
   unsigned char *ptr,*ptr_lim;
   extern int optind;
   extern char *optarg;
#define RAW 0x00
#define MON 0x01
#define RAW_HSR 0x02
#define COUNT 0x10
#define SR_MON 5
   
   signal(SIGINT,(void *)ctrlc);
   signal(SIGTERM,(void *)ctrlc);
   signal(SIGPIPE,(void *)ctrlc);
   if(progname=strrchr(argv[0],'/')) progname++;
   else progname=argv[0]; 
   mode=RAW;
   while((c=getopt(argc,argv,"mrhcu"))!=EOF)
     {
     switch(c)
       {
       case 'c':   /* "Count" mode */
         mode=(0x0f & mode)|COUNT;
         for(i=0;i<65536;i++) count[i]=0;
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
       case 'u':   /* show usage */
       default:
         fprintf(stderr,
           " usage : '%s [-r/-m/-h/-c] [(data file)/- [(sec position)]]'\n",
           progname);
         exit(1);
       }
     }

   if(argc<optind+1 || strcmp("-",argv[optind])==0) f_main=stdin;
   else if((f_main=fopen(argv[optind],"r"))==NULL){
     fprintf(stderr,
       " usage : '%s (-r/-m/-h/-c) ([data file]/- ([sec position]))'\n",
       progname);
     exit(1);
   }
   if(argc>optind+1) ss=atoi(argv[optind+1]);
   else ss=0;
   
   sec=ts=0;
   while(mainsize=read_data(&mainbuf,f_main)) {
#if DEBUG1
     printf("mainsize = %d\n", mainsize);
#endif
     ptr_lim=mainbuf+mainsize;
#if DEBUG
     printf("ptr_lim=%08x buf=%08x size=%08x\n",ptr_lim,mainbuf,mainsize);
#endif
     ptr=mainbuf+10;
     nch=0;
     do
       {
       sysch=ptr[1]+(((long)ptr[0])<<8);
       if((mode&0xf0)==COUNT) count[sysch]++;
       if((mode&0x0f)==MON)
         {
         gs=2;
         for(i=0;i<SR_MON;i++) {
           j=(ptr[gs]&0x03)*2;
           gs+=j+1;
           if(j==0) gs++;
           }
         if((mode&0xf0)==0 && sec==ss) {
           printf("%4d : ch %04X    %3d Hz  %4d B\n",nch+1,sysch,SR_MON,gs);
           }
         }
       else
         {
         if((ptr[2]&0x80)==0) /* channel header = 4 byte */
           {
           sr=ptr[3]+(((long)(ptr[2]&0x0f))<<8);
           size=(ptr[2]>>4)&0x7;
           if(size) gs=size*(sr-1)+8;
           else gs=(sr>>1)+8;
#if DEBUG
printf("gs=%d gh=%02x%02x%02x%02x%02x sr=%d gs=%d ptr=%08x ptr_lim=%08x\n",
  gs,ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],sr,gs,ptr,ptr_lim); 
#endif
           if((mode&0xf0)==0 && sec==ss)
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
             printf("%4d : ch %04X    %3d Hz  x  %s B  = %4d B\n",
               nch+1,sysch,sr,bytes,gs);
             }
           }
         else
           {
           if((mode&0x0f)==RAW_HSR) /* channel header = 5 byte */
             {
             sr=ptr[4]+(((long)ptr[3])<<8)+(((long)(ptr[2]&0x0f))<<16);
             size=(ptr[2]>>4)&0x7;
             if(size) gs=size*(sr-1)+8;
             else gs=(sr>>1)+8;
             gs++;
             if((mode&0xf0)==0 && sec==ss)
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
               printf("%4d : ch %04X    %3d Hz  x  %s B  = %4d B\n",
                 nch+1,sysch,sr,bytes,gs);
               }
             }
           else /* WIN2 format */
             {
             sr=ptr[3]+(((long)(ptr[2]&0x03))<<8);
             size=((ptr[2]>>2)&0x1F)+1; /* in bits */
             gs=((sr-1)*size-1)/8+1+8;
             if((mode&0xf0)==0 && sec==ss)
               printf("%4d : ch %04X    %3d Hz  x  %2d b  = %4d B\n",
               nch+1,sysch,sr,size,gs);
             }
           }
         }
       ptr+=gs;
       nch++;
       } while(ptr<ptr_lim);
     if((mode&0xf0)==0)
       {
       if(sec==ss) printf("\n");
       printf("%4d : %02x%02x%02x %02x%02x%02x",sec+1,mainbuf[4],
         mainbuf[5],mainbuf[6],mainbuf[7],mainbuf[8],mainbuf[9]);
       printf("   %d ch  (%d bytes)\n",nch,mainsize);
       }
     ts+=mainsize;
     sec++;
   }
   if((mode&0xf0)==0) printf("\nlength = %d s  (%d bytes)\n\n",sec,ts);
   else {for(i=0;i<65536;i++) if(count[i]>0) printf("%04X %d\n",i,count[i]);}
   exit(0);
}
