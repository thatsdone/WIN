/* $Id: elist.c,v 1.9.4.2.2.6 2010/10/12 00:51:12 uehira Exp $ */

/* program elist.c    2/5/91 - 2/25/91 ,  4/16/92, 4/22/92  urabe */
/*                      6/10/92, 8/18/92, 10/25/92, 6/8/93, 1/5/94  */
/*      4/21/94,12/5/94,6/2/95 bug in dat_dir fixed */
/*      98.1.22 getpwuid()==NULL */
/*      98.6.26 yo2000           */
/*      2001.2.6 a lot of functions added (options -h/u/p/o/n/s) */
/*      2001.2.9 use "tac", instead of "tail -r", for Linux */
/*      2001.2.20 increase size of line-buffer to avoid overflow */
/*      2001.8.22 use pickers name read from #p line if exists */
/*      2010.10.12 fixed buf. 64bit check. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <pwd.h>

#if HAVE_DIRENT_H  /* opendir(), readdir() */
# include <dirent.h>
# define DIRNAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define DIRNAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "winlib.h"

#define   NAMLEN    128
#define   LINELEN   256
#define   OUTLEN   4096
#define   NPICK   10000  /* this is NOT the limit */
#if defined(__linux__)
#define   TAIL    "tac"
#else
#define   TAIL    "tail -r"
#endif

static const char rcsid[] =
  "$Id: elist.c,v 1.9.4.2.2.6 2010/10/12 00:51:12 uehira Exp $";

/* prototypes */
static char *getname(char *, int);
static void bfov_err(void);
static void print_usage(void);
int main(int, char *[]);
/* end of prototypes */

static void
bfov_err()
{

  fprintf(stderr, "Buffer overrun!\n");
  exit(1);
}

static char *
getname(char *name, int id)
  {
  static char t[10];
  struct passwd *pwd;

  if(*name) return (name);
  if((pwd=getpwuid(id))) return (pwd->pw_name);
  else
    {
    sprintf(t,"%d",id);
    return (t);
    }
/*  else return ".";*/
  }

static void
print_usage()
  {

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr,"Usage :  elist (-hupon) [pick dir] [pmon.out file] ([data dir] ([request dir] ..))\n");
  fprintf(stderr,"    If 'data dir' is specified, 'NOISE' files will be deleted.\n");
  fprintf(stderr,"    -h  hide 'NOISE only' events\n");
  fprintf(stderr,"    -p [pktemp file] output 'pplist' file\n");
  fprintf(stderr,"    -o [evtemp file] output file (default to stdout)\n");
  fprintf(stderr,"    -n  normal (not reverse) order\n");
  fprintf(stderr,"    -s  don`t delete 'NOISE only' event files in data dir\n");
  fprintf(stderr,"    -u  print usage\n");
  exit(1);
  }

/* str2double(t,n,m,d) */
/*   char *t; */
/*   int n,m; */
/*   double *d; */
/*   {  */
/*   char tb[20]; */
/*   strncpy(tb,t+n,m); */
/*   tb[m]=0; */
/*   if(tb[0]=='*') *d=100.0; */
/*   else *d=atof(tb); */
/*   } */

int
main(int argc,char *argv[])
  {
/*   extern int optind; */
/*   extern char *optarg; */
  FILE *fp,*fa,*fee,*fpp;
  struct dirent *dir_ent;
  struct stat st_buf;
  DIR *dir_ptr;
  struct Pk {
    char dfname[14],diagnos[20],fname[18],name[10],near[12];
    uid_t user;
    mode_t mode;  /* OLD : u_short */
    int hypo,mech,pick,np,ns,nm;
    float lat,lon,dep,mag;
    } *pk;
  char pick_dir[NAMLEN],out_file[NAMLEN],dat_dir[NAMLEN],req_dir[3][NAMLEN],
    filename[NAMLEN],textbuf[OUTLEN],group[20],mes1[20],mes2[20],mes3[20],
    tbuf[OUTLEN],gbuf[OUTLEN],name_dat[NAMLEN],name_ch[NAMLEN],
    name_sv[NAMLEN],*ptr,ppfile[NAMLEN],eefile[NAMLEN],outbuf[OUTLEN],
    tmpfile[NAMLEN];
  int i,npick,t[6],ton[6],init,j,m,k,kk,no_file,noise,not_noise,re,search,
    npick_lim,c,reverse,hidenoise,kkk,pn,fn,sn,mn,nstn,delete;
  double pt,pe,pomc,st,se,somc,mag;

  *ppfile=(*eefile)=0;
  reverse=delete=1;
  hidenoise=0;
  while((c=getopt(argc,argv,"hup:o:ns"))!=-1)
    {
    switch(c)
      {
      case 'o':  /* write evtemp file (ee list) */
        strcpy(eefile,optarg);
        break;
      case 'p':  /* write pktemp file (pp list)  */
        strcpy(ppfile,optarg);
        break;
      case 'n':  /* normal order (i.e. not reverse order) */
        reverse=0;
        break;
      case 'h':  /* hide NOISE only events */
        hidenoise=1;
        break;
      case 's':  /* not delete NOISE only events in trg dir*/
        delete=0;
        break;
      case 'u':
      default:
        print_usage();
      }
    }

  optind--;
  if(argc<3+optind) print_usage();
  else
    {
    if(argc>=4+optind && strcmp(argv[3+optind],"-")!=0)
      strcpy(dat_dir,argv[3+optind]);
    else *dat_dir=0;
    if(argc>=5+optind)
      {
      strcpy(req_dir[0],argv[4+optind]);
      if(req_dir[0][strlen(req_dir[0])-1]=='/')
      strcat(req_dir[0],"*");
      }
    else *req_dir[0]=0;
    if(argc>=6+optind)
      {
      strcpy(req_dir[1],argv[5+optind]);
      if(req_dir[1][strlen(req_dir[1])-1]=='/')
      strcat(req_dir[1],"*");
      }
    else *req_dir[1]=0;
    if(argc>=7+optind)
      {
      strcpy(req_dir[2],argv[6+optind]);
      if(req_dir[2][strlen(req_dir[2])-1]=='/')
      strcat(req_dir[2],"*");
      }
    else *req_dir[2]=0;
    }
  strcpy(pick_dir,argv[1+optind]);
  strcpy(out_file,argv[2+optind]);

  /* read pick files */
  if((dir_ptr=opendir(pick_dir))==NULL)
    {
    printf("directory '%s' not open\n",pick_dir);
    exit(1);
    }

  npick_lim=NPICK;
  if((pk=(struct Pk *)malloc(sizeof(*pk)*npick_lim))==NULL)
    {
    fprintf(stderr,"malloc (Npicks=%d) failed !\n",npick_lim);
    exit(1);
    }

  i=0;
  while((dir_ent=readdir(dir_ptr))!=NULL)
    {
    if(*dir_ent->d_name=='.') continue;
    strcpy(pk[i].fname,dir_ent->d_name);
    if (snprintf(filename,sizeof(filename),"%s/%s",
		 pick_dir,dir_ent->d_name) >= sizeof(filename))
      bfov_err();

    if((fp=fopen(filename,"r"))==NULL)
      {
      printf("file '%s' not open\n",filename);
      continue;
      }

    stat(filename,&st_buf);
    pk[i].user=st_buf.st_uid;
    pk[i].mode=st_buf.st_mode;
    fgets(textbuf,sizeof(textbuf),fp);
    pk[i].diagnos[0]=pk[i].name[0]=0;
    sscanf(textbuf,"%*s%s%s%s",pk[i].dfname,pk[i].diagnos,pk[i].name);
    pk[i].diagnos[5]=pk[i].name[5]=0;
    pk[i].pick=pk[i].hypo=pk[i].mech=pk[i].np=pk[i].ns=pk[i].nm=0;
    pk[i].mag=9.9;
    pn=sn=fn=mn=0;
    while(fgets(textbuf,sizeof(textbuf),fp)!=NULL)
      {
      if(strncmp(textbuf,"#p",2)==0)
        {
        pn++;
        pk[i].pick=1;
        continue;
        }
      if(strncmp(textbuf,"#s",2)==0)
        {
        sn++;
        continue;
        }
      if(strncmp(textbuf,"#f",2)==0)
        {
        fn++;
        pk[i].hypo=1;
        if(fn==1) sscanf(textbuf,"%*s%*s%*s%*s%*s%*s%*s%f%f%f%f",
          &pk[i].lat,&pk[i].lon,&pk[i].dep,&pk[i].mag);
        if(fn==5) sscanf(textbuf,"%*s%d",&nstn);
        if(fn>5 && fn<=5+nstn)
          {
	  sscanf(textbuf,"%*s%s",pk[i].near);
          str2double(textbuf,39+3,7,&pt);
          str2double(textbuf,46+3,6,&pe);
          str2double(textbuf,52+3,7,&pomc);
          str2double(textbuf,59+3,7,&st);
          str2double(textbuf,66+3,6,&se);
          str2double(textbuf,72+3,7,&somc);
          str2double(textbuf,89+3,5,&mag);
          if(pt!=0.0 || pe!=0.0 || pomc!=0.0) pk[i].np++;
          if(st!=0.0 || se!=0.0 || somc!=0.0) pk[i].ns++;
          if(mag!=9.9) pk[i].nm++;
          } 
        continue;
        }
      if(strncmp(textbuf,"#m",2)==0)
        {
        mn++;
        pk[i].mech=1;
        continue;
        }
      }
    fclose(fp);

    if(++i==npick_lim)
      {
      if((pk=(struct Pk *)realloc((char *)pk,sizeof(*pk)*(npick_lim+NPICK)))==NULL)
        {
        fprintf(stderr,"realloc failed !  Npicks=%d (size=%d)\n",
          npick_lim,sizeof(*pk)*npick_lim);
        break;
        }
      else npick_lim+=NPICK;
/*printf("npick_lim=%d\n",npick_lim);*/
      }
    }
  npick=i;
  closedir(dir_ptr);
/*printf("npick=%d\n",npick);*/
  if(*ppfile)
    {
    if((fpp=fopen(ppfile,"w"))==NULL)
      fprintf(stderr,"file '%s' not open.\n",ppfile);
    else
      {
fprintf(fpp,"-----------------------------------------------------------------------------\n");
fprintf(fpp,"pickfile          trgfile       picker P   S   M Lat.  Lon.   Dep. M  nearest\n");
fprintf(fpp,"-----------------------------------------------------------------------------\n");
      fclose(fpp);
      }
    if (snprintf(tbuf,sizeof(tbuf),"sort >> %s",ppfile) >= sizeof(tbuf))
      bfov_err();
    if((fpp=popen(tbuf,"w"))==NULL)
      fprintf(stderr,"pipe '%s' not open.\n",tbuf);    
    else
      {
      for(i=0;i<npick;i++)
        {
        fprintf(fpp,"%s %s %4.4s%4d%4d%4d",pk[i].fname,pk[i].dfname,
          getname(pk[i].name,pk[i].user),pk[i].np,pk[i].ns,pk[i].nm);
        if(pk[i].hypo) fprintf(fpp,"%6.2f%7.2f%4.0f M%3.1f %4.4s\n",
            pk[i].lat,pk[i].lon,pk[i].dep,pk[i].mag,pk[i].near);
        else fprintf(fpp," -     -       -    -    -\n");
        }
      pclose(fpp);
      }
    }

  if((fp=fopen(out_file,"r"))==NULL)
    {
    printf("file '%s' not open\n",out_file);
    exit(1);
    }

  if (snprintf(tmpfile,sizeof(tmpfile),
	       "/tmp/elist.%d",getpid()) >= sizeof(tmpfile))
    bfov_err();
  if((fee=fopen(tmpfile,"w"))==NULL)
    {
    fprintf(stderr,"tempfile '%s' not open.\n",tmpfile);
    exit(1);
    }
  init=1;
#define ON 1
#define OFF 0
  search=ON;
  while(fgets(textbuf,sizeof(textbuf),fp))
    {
    if(*textbuf==' ') continue;
    *mes1=(*mes2)=(*mes3)=0;
    sscanf(textbuf,"%2d%2d%2d.%2d%2d%2d%s%s%s",
      &t[0],&t[1],&t[2],&t[3],&t[4],&t[5],mes1,mes2,mes3);
/*printf("%s",textbuf);*/
    if(search==ON) /* search "on" */
      {
      if(strcmp(mes1,"on,")) continue;
      if(init==0 && time_cmp(t,ton,6)<0) continue;
      init=0;
      for(i=0;i<6;i++) ton[i]=t[i];
      strcpy(group,mes3);
      search=OFF;
/*printf("ON=%02d%02d%02d.%02d%02d%02d",t[0],t[1],t[2],t[3],t[4],t[5]);*/
      }
    else if(search==OFF)
      {
      if(strcmp(mes1,"off,")) continue;
      if(time_cmp(t,ton,6)<0) continue;
      if((ptr=strchr(textbuf,'\n')) != NULL) *ptr=0;
      if(*mes2) strcpy(gbuf,strchr(textbuf,*mes2));
      else *gbuf=0;
/*printf("OFF=%02d%02d%02d.%02d%02d%02d",t[0],t[1],t[2],t[3],t[4],t[5]);*/
      i=1;
      while(time_cmp(t,ton,6)>0)
        {
        t[5]--;
        adj_time(t);
        i++;
        }
      m=(int)(-2.36+2.85*log10((double)i+3.0*sqrt((double)i))+0.5);
          /* After Tsumura */
/*printf("i=%d, M=%d\n",i,m);*/
      if (snprintf(outbuf,sizeof(outbuf),"%02d%02d%02d.%02d%02d%02d",
		   ton[0],ton[1],ton[2],
		   ton[3],ton[4],ton[5]) >= sizeof(outbuf))
	bfov_err();
      if (snprintf(textbuf,sizeof(textbuf),"%02d%02d%02d.%02d%02d%02d",
		   ton[0],ton[1],ton[2],
		   ton[3],ton[4],ton[5]) >= sizeof(textbuf))
	bfov_err();
      no_file=0;
      if(*dat_dir)
        {
	if (snprintf(name_dat,sizeof(name_dat),"%s/%s",
		     dat_dir,textbuf) >= sizeof(name_dat))
	  bfov_err();
        if (snprintf(name_ch,sizeof(name_ch),
		     "%s.ch",name_dat) >= sizeof(name_ch))
	  bfov_err();
        if (snprintf(name_sv,sizeof(name_sv),
		     "%s.sv",name_dat) >= sizeof(name_sv))
	  bfov_err();
        if((fa=fopen(name_dat,"r"))==NULL) no_file=1;
        else fclose(fa);
        }
      if(*req_dir[0])
        {
	if (snprintf(tbuf,sizeof(tbuf),
		     "test \"`ls %s/%s ",req_dir[0],textbuf) >= sizeof(tbuf))
	  bfov_err();
        if(*req_dir[1])
	  if (snprintf(tbuf+strlen(tbuf),sizeof(tbuf)-strlen(tbuf),
		       "%s/%s ",req_dir[1],textbuf)
	      >= sizeof(tbuf)-strlen(tbuf))
	    bfov_err();
        if(*req_dir[2])
	  if (snprintf(tbuf+strlen(tbuf),sizeof(tbuf)-strlen(tbuf),
		       "%s/%s ",req_dir[2],textbuf)
	      >= sizeof(tbuf)-strlen(tbuf))
	    bfov_err();
        strcat(tbuf,"2>/dev/null`\"");
        re=system(tbuf); /* re==0 if flag file exists */
        }
      else re=256;
      if(re==0) strcat(outbuf,"&"); /* not joined yet */
      else if(no_file) strcat(outbuf,"x"); /* no data file */
      else strcat(outbuf," ");
      j=0;
      noise=not_noise=0;
      for(i=0;i<npick;i++)
        {
        if(strcmp(pk[i].dfname,textbuf)==0)
          {
        /* picker name */
          ptr=getname(pk[i].name,pk[i].user);
          if((pk[i].mode & S_IWGRP) == 0) /* PRIVATE */
            {
            strcat(outbuf," ");
            kkk=strlen(outbuf);
            for(kk=0;kk<strlen(ptr);kk++)
              {
              if(kk==5) break;
              outbuf[kkk+kk]=toupper(ptr[kk]);
              }
            outbuf[kkk+kk]=0;
            }
          else {
	    if (snprintf(outbuf+strlen(outbuf),sizeof(outbuf)-strlen(outbuf),
			 " %-.5s",ptr) >= sizeof(outbuf)-strlen(outbuf))
	      bfov_err();
	  }
          if(strlen(ptr)<5)
            {
            for(kk=0;kk<5-strlen(ptr);kk++) strcat(outbuf," ");
            }
          strcat(outbuf," ");
        /* diagnosis */
          if(pk[i].diagnos[0]!=0)
            {
            if(strcmp(pk[i].diagnos,"NOISE")==0 || 
              strcmp(pk[i].diagnos,"noise")==0) noise=1;
            else not_noise=1;
            if (snprintf(outbuf+strlen(outbuf),sizeof(outbuf)-strlen(outbuf),
			 "%s",pk[i].diagnos) >= sizeof(outbuf)-strlen(outbuf))
	      bfov_err();
            for(k=0;k<5-strlen(pk[i].diagnos);k++) strcat(outbuf," ");
            }
          else
            {
            strcat(outbuf,"     ");
            not_noise=1;
            }
        /* status */
          strcat(outbuf," ");
          if(pk[i].pick) strcat(outbuf,"P");
          else strcat(outbuf,".");
          if(pk[i].hypo) strcat(outbuf,"H");
          else strcat(outbuf,".");
          if(pk[i].mag<9.0) strcat(outbuf,"M");
          else strcat(outbuf,".");
          if(pk[i].mech) strcat(outbuf,"M");
          else strcat(outbuf,".");
          strcat(outbuf," ");
          j=1;
          }
        }
      if(j==0) strcat(outbuf,"                  ");
      else if(*dat_dir && no_file==0 && noise==1 && not_noise==0 && delete)
        {
        unlink(name_dat);
        unlink(name_ch);
        unlink(name_sv);
        }
      for(i=0;i<m;i++) strcat(outbuf,"*");
      for(i=m;i<7;i++) strcat(outbuf," ");
      if (snprintf(outbuf+strlen(outbuf),sizeof(outbuf)-strlen(outbuf),
		   "  %s %s",group,gbuf) >= sizeof(outbuf)-strlen(outbuf))
	bfov_err();
/*    if(*dat_dir && no_file==1 && noise==1 && not_noise==0)*/
      if(noise==1 && not_noise==0)
        {
        if(!hidenoise) fprintf(fee,"%s#\n",outbuf);
        }
      else fprintf(fee,"%s\n",outbuf);
      *group=(*gbuf)=0;
      search=ON;
      }
    }
  fclose(fp);
  fclose(fee);

  if(*eefile)
    {
    if((fee=fopen(eefile,"w"))==NULL)
      {
      fprintf(stderr,"eefile '%s' not open.\n",eefile);
      exit(1);
      }
    fprintf(fee,"---------------------------------------------------\n");
    fprintf(fee,"YYMMDD hhmmss  picker      PHMM 1.....7   region(s)\n");
    fprintf(fee," date   time                       M      triggered\n");
    fprintf(fee,"---------------------------------------------------\n");
    fclose(fee);
    if(reverse) {
      if (snprintf(tbuf,sizeof(tbuf),
		   "%s %s >> %s",TAIL,tmpfile,eefile) >= sizeof(tbuf))
	bfov_err();
    } else {
      if (snprintf(tbuf,sizeof(tbuf),
		   "cat %s >> %s",tmpfile,eefile) >= sizeof(tbuf))
	bfov_err();
    }
    system(tbuf);
    }
  else
    {
    printf("---------------------------------------------------\n");
    printf("YYMMDD hhmmss  picker      PHMM 1.....7   region(s)\n");
    printf(" date   time                       M      triggered\n");
    printf("---------------------------------------------------\n");
    fflush(stdout);
    if(reverse) {
      if (snprintf(tbuf,sizeof(tbuf),"%s %s",TAIL,tmpfile) >= sizeof(tbuf))
	bfov_err();
    } else {
      if (snprintf(tbuf,sizeof(tbuf),"cat %s",tmpfile) >= sizeof(tbuf))
	bfov_err();
    }
    system(tbuf);
    }
  unlink(tmpfile);
  exit(0);
  }
