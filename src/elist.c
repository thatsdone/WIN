/* $Id: elist.c,v 1.2 2000/04/30 10:05:22 urabe Exp $ */
/* program elist.c    2/5/91 - 2/25/91 ,  4/16/92, 4/22/92  urabe */
/*                      6/10/92, 8/18/92, 10/25/92, 6/8/93, 1/5/94  */
/*      4/21/94,12/5/94,6/2/95 bug in dat_dir fixed */
/*      98.1.22 getpwuid()==NULL */
/*      98.6.26 yo2000           */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>   /* opendir(), readdir() */
#include <math.h>
#include <ctype.h>
#include <pwd.h>
#define   NAMLEN    80
#define   LINELEN   250
#define   NPICK   30000

char *getname(id)
  int id;
  {
  struct passwd *pwd;
  if(pwd=getpwuid(id)) return pwd->pw_name;
  else return ".";
  }

main(argc,argv)
    int argc;
    char *argv[];
  {
  FILE *fp,*fa;
  struct dirent *dir_ent;
  struct stat st_buf;
  DIR *dir_ptr;
  static char dfname[NPICK][18],diagnos[NPICK][8];
  static uid_t user[NPICK];
  static u_short mode[NPICK];
  static int hypo[NPICK],mech[NPICK],pick[NPICK];
  static float mag[NPICK];
  static char pick_dir[NAMLEN],out_file[NAMLEN],dat_dir[NAMLEN],
    req_dir[3][NAMLEN],
    filename[NAMLEN],textbuf[LINELEN],group[20],mes1[20],
    tbuf[LINELEN],mes2[20],gbuf[LINELEN],name_dat[NAMLEN],
    name_ch[NAMLEN],name_sv[NAMLEN],*ptr;
  int i,npick,t[6],toff[6],init,j,m,k,kk,no_file,noise,not_noise,re;
  long dp;
  static int magni[7]={7,15,35,80,100,120,130};
    /* boundaries for M=0/1/2/3/4/5/6/7 */
  if(argc<3)
    {
    printf("Usage :  elist [pick dir] [pmon.out file] ([data dir] ([request dir] ..))\n");
    printf("    If 'data directory' is specified, 'NOISE' files will be deleted.\n");
    exit(1);
    }
  else
    {
    if(argc>=4 && strcmp(argv[3],"-")!=0) strcpy(dat_dir,argv[3]);
    else *dat_dir=0;
    if(argc>=5)
      {
      strcpy(req_dir[0],argv[4]);
      if(req_dir[0][strlen(req_dir[0])-1]=='/')
      strcat(req_dir[0],"*");
      }
    else *req_dir[0]=0;
    if(argc>=6)
      {
      strcpy(req_dir[1],argv[5]);
      if(req_dir[1][strlen(req_dir[1])-1]=='/')
      strcat(req_dir[1],"*");
      }
    else *req_dir[1]=0;
    if(argc>=7)
      {
      strcpy(req_dir[2],argv[6]);
      if(req_dir[2][strlen(req_dir[2])-1]=='/')
      strcat(req_dir[2],"*");
      }
    else *req_dir[2]=0;
    }
  strcpy(pick_dir,argv[1]);
  strcpy(out_file,argv[2]);

  /* read pick files */
  if((dir_ptr=opendir(pick_dir))==NULL)
    {
    printf("directory '%s' not open\n",pick_dir);
    exit(1);
    }
  i=0;
  while((dir_ent=readdir(dir_ptr))!=NULL)
    {
    if(*dir_ent->d_name=='.') continue;
    sprintf(filename,"%s/%s",pick_dir,dir_ent->d_name);

    if((fp=fopen(filename,"r"))==NULL)
      {
      printf("file '%s' not open\n",filename);
      continue;
      }

    stat(filename,&st_buf);
    user[i]=st_buf.st_uid;
    mode[i]=st_buf.st_mode;
    fgets(textbuf,LINELEN,fp);
    diagnos[i][0]=0;
    sscanf(textbuf,"%*s%s%5s",dfname[i],diagnos[i]);
    diagnos[i][5]=0;
    pick[i]=hypo[i]=mech[i]=0;
    mag[i]=9.9;
    while(fgets(textbuf,LINELEN,fp)!=NULL)
      {
      if(strncmp(textbuf,"#p",2)==0) pick[i]=1;
      if(strncmp(textbuf,"#f",2)==0)
        {
        hypo[i]=1;
        sscanf(textbuf,"%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%f",&mag[i]);
        break;
        }
      }
    if(hypo[i]) while(fgets(textbuf,LINELEN,fp)!=NULL)
      {
      if(strncmp(textbuf,"#m",2)==0)
        {
        mech[i]=1;
        break;
        }
      }
    fclose(fp);

    if(++i==NPICK)
      {
      printf("buffer full (N=%d)\n",NPICK);
      break;
      }
    }
  npick=i;
  closedir(dir_ptr);
/*
for(i=0;i<npick;i++)
printf("%3d : %s %d %3.1f %d %d %d\n",
i,dfname[i],hypo[i],mag[i],mech[i],user[i],mode[i]);
*/
  if((fp=fopen(out_file,"r"))==NULL)
    {
    printf("file '%s' not open\n",out_file);
    exit(1);
    }
  fseek(fp,0,2);
  dp=ftell(fp)-2;

  init=1;
  printf("---------------------------------------------------\n");
  printf(" date   time                       M      triggered\n");
  printf("YYMMDD hhmmss  picker      PHMM 1.....7   region(s)\n");
  printf("---------------------------------------------------\n");

  do
    {
    dp=get_line(fp,dp,textbuf);
    if(*textbuf==' ')
      {
      if(dp<=0) break;
      else continue;
      }
    *mes2=0;
    sscanf(textbuf,"%2d%2d%2d.%2d%2d%2d%s%s%s",
      &t[0],&t[1],&t[2],&t[3],&t[4],&t[5],mes1,mes2,group);
    if(strcmp(mes1,"on,")==0 && init==0)
      {
      i=1;
      while(time_cmp(t,toff,6)!=0)
        {
        toff[5]--;
        adj_time(toff);
        i++;
        }
/*      for(m=0;m<7;m++) if(i<=magni[m]) break;*/
      m=(int)(-2.36+2.85*log10((double)i+
        3.0*sqrt((double)i))+0.5);      /* after Tsumura */
      printf("%02d%02d%02d.%02d%02d%02d",
        t[0],t[1],t[2],t[3],t[4],t[5]);
      no_file=0;
      if(*dat_dir)
        {
        strcpy(name_dat,dat_dir);
        strcat(name_dat,"/");
        strncat(name_dat,textbuf,13);
        strcpy(name_ch,name_dat);
        strcat(name_ch,".ch");
        strcpy(name_sv,name_dat);
        strcat(name_sv,".sv");
        if((fa=fopen(name_dat,"r"))==NULL) no_file=1;
        else fclose(fa);
        }
      if(*req_dir[0])
        {
        sprintf(tbuf,"test \"`ls %s/%.13s ",req_dir[0],textbuf);
        if(*req_dir[1]) sprintf(tbuf+strlen(tbuf),
          "%s/%.13s ",req_dir[1],textbuf);
        if(*req_dir[2]) sprintf(tbuf+strlen(tbuf),
          "%s/%.13s ",req_dir[2],textbuf);
        strcat(tbuf,"2>/dev/null`\"");
        re=system(tbuf); /* re==0 if flag file exists */
        }
      else re=256;
      if(re==0) putchar('&'); /* not joined yet */
      else if(no_file) putchar('x'); /* no data file */
      else putchar(' ');
      j=0;
      noise=not_noise=0;
      for(i=0;i<npick;i++)
        {
        if(strncmp(dfname[i],textbuf,13)==0)
          {
        /* picker name */
          ptr=getname(user[i]);
          if((mode[i] & S_IWGRP) == 0) /* PRIVATE */
            {
            putchar(' ');
            for(kk=0;kk<strlen(ptr);kk++)
              {
              if(kk==5) break;
              putchar(toupper(ptr[kk]));
              }
            }
          else printf(" %-.5s",ptr);
          if(strlen(ptr)<5)
            {
            for(kk=0;kk<5-strlen(ptr);kk++) putchar(' ');
            }
          putchar(' ');
        /* diagnosis */
          if(diagnos[i][0]!=0)
            {
            if(strcmp(diagnos[i],"NOISE")==0 || 
              strcmp(diagnos[i],"noise")==0) noise=1;
            else not_noise=1;
            printf("%s",diagnos[i]);
            for(k=0;k<5-strlen(diagnos[i]);k++) putchar(' ');
            }
          else
            {
            printf("     ");
            not_noise=1;
            }
        /* status */
          putchar(' ');
          if(pick[i]) putchar('P');
          else putchar('.');
          if(hypo[i]) putchar('H');
          else putchar('.');
          if(mag[i]<9.0) putchar('M');
          else putchar('.');
          if(mech[i]) putchar('M');
          else putchar('.');
          putchar(' ');
          j=1;
          }
        }
      if(j==0) printf("                  ");
      else if(*dat_dir && no_file==0 && noise==1 && not_noise==0)
        {
        unlink(name_dat);
        unlink(name_ch);
        unlink(name_sv);
        }
      for(i=0;i<m;i++) putchar('*');
      for(i=m;i<7;i++) putchar(' ');
      printf("  %s %s",group,gbuf);
/*      if(*dat_dir && no_file==1 && noise==1 && not_noise==0)*/
      if(noise==1 && not_noise==0) printf("#\n");
      else putchar('\n');     
      *group=(*gbuf)=0;
      }
    else if(strcmp(mes1,"off,")==0)
      {
      init=0;
      for(i=0;i<6;i++) toff[i]=t[i];
      if(ptr=strchr(textbuf,'\n')) *ptr=0;
      if(*mes2) strcpy(gbuf,strchr(textbuf,*mes2));
      else *gbuf=0;
      }
    } while(dp>0);
  fclose(fp);
  exit(0);
  }

get_line(fp,dp,buf)
  FILE *fp;
  long dp;
  char *buf;
  {
  int c;
  fseek(fp,dp,0);
  while((c=getc(fp))!=0x0a)
    {
    if(--dp<0)
      {
      fseek(fp,0,0);
      break;
      }
    fseek(fp,dp,0);
    }
  fgets(buf,LINELEN,fp);
  --dp;
  return(dp);
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
            if(tm[0]%4)
              {
              if(tm[2]==29)
                {
                tm[2]=1;
                tm[1]++;
                }
              break;
              }
            else
              {
              if(tm[2]==30)
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
              if(tm[0]%4) tm[2]=28;else tm[2]=29;
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
