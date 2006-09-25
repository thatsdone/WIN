/* $Id: recvt.c,v 1.29.2.1 2006/09/25 15:00:58 uehira Exp $ */
/* "recvt.c"      4/10/93 - 6/2/93,7/2/93,1/25/94    urabe */
/*                2/3/93,5/25/94,6/16/94 */
/*                1/6/95 bug in adj_time fixed (tm[0]--) */
/*                2/17/95 "illegal time" message stopped */
/*                3/15-16/95 write_log changed */
/*                3/26/95 check_packet_no; port# */
/*                10/16/95 added processing of "host table full" */
/*                5/22/96 support merged packet with ID="0x0A" */
/*                5/22/96 widen time window to +/-30 min */
/*                5/28/96 bcopy -> memcpy */
/*                6/6/96,7/9/96 discard duplicated resent packets & fix bug */ 
/*                12/29/96 "next" */
/*                97.6.23 RCVBUF->65535 */
/*                97.9.4        & 50000 */
/*                97.12.18 channel selection by file */
/*                97.12.23 ch no. in "illegal time" & LITTLE_ENDIAN */
/*                98.4.23 b2d[] etc. */
/*                98.6.26 yo2000 */
/*                99.2.4  moved signal(HUP) to read_chfile() by urabe */
/*                99.4.19 byte-order-free */
/*                2000.3.13 >=A1 format */
/*                2000.4.24 added SS & SR check, check_time +/-60m */
/*                2000.4.24 strerror() */
/*                2000.4.25 added check_time() in >=A1 format */
/*                2000.4.26 host control, statistics, -a, -m, -p options */
/*                2001.2.20 wincpy() improved */
/*                2001.3.9 debugged for sh->r */
/*                2001.11.14 strerror(),ntohs() */
/*                2002.1.7 implemented multicasting (options -g, -i) */
/*                2002.1.7 option -n to suppress info on abnormal packets */
/*                2002.1.8 MAXMESG increased to 32768 bytes */
/*                2002.1.12 trivial fixes on 'usage' */
/*                2002.1.15 option '-M' necessary for receiving mon data */
/*                2002.3.2 wincpy2() discard duplicated data (TS+CH) */
/*                2002.3.2 option -B ; write blksize at EOB */
/*                2002.3.18 host control debugged */
/*                2002.5.3 N_PACKET 64->128, 'no request resend' log */
/*                2002.5.3 maximize RCVBUF size */
/*                2002.5.3,7 maximize RCVBUF size ( option '-s' )*/
/*                2002.5.14 -n debugged / -d to set ddd length */
/*                2002.5.23 stronger to destructed packet */
/*                2002.11.29 corrected byte-order of port no. in log */
/*                2002.12.21 disable resend request if -r */
/*                2003.3.24-25 -N (no pno) and -A (no TS) options */
/*                2004.8.9 fixed bug introduced in 2002.5.23 */
/*                2004.10.26 daemon mode (Uehira) */
/*                2004.11.15 corrected byte-order of port no. in log */
/*                2005.2.17 option -o [source host]:[port] for send request */
/*                2005.2.20 option -f [ch_file] for additional ch files */
/*                2005.6.24 don't change optarg's content (-o) */
/*                2005.9.25 allow disorder of arriving packets */
/*                2005.9.25 host(:port) in control file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>

#include "daemon_mode.h"
#include "winlib.h"
#include "subst_func.h"

#define DEBUG     0
#define DEBUG0    0
#define DEBUG1    0
#define DEBUG2    0
#define DEBUG3    0     /* -o */
#define DEBUG4    0     /* -y */
#define BELL      0
#define MAXMESG   32768
#define N_PACKET  64    /* N of old packets to be requested */  
#define N_HOST    100   /* max N of hosts */  
#define N_HIST    10    /* default length of packet history */
#define N_CHFILE  30    /* N of channel files */
#define N_PNOS    62    /* length of packet nos. history >=2 */

unsigned char rbuf[MAXMESG],ch_table[65536];
char *progname,*logfile,chfile[N_CHFILE][256];
int n_ch,negate_channel,hostlist[N_HOST][3],n_host,no_pinfo,n_chfile;
int  daemon_mode, syslog_mode, exit_status;

struct {
    int host;
    int port;
    int ppnos;	/* pointer for pnos */
    unsigned int pnos[N_PNOS];
    int nosf[4]; /* 4 segments x 64 */
    unsigned char nos[256/8];
    unsigned int n_bytes;
    unsigned int n_packets;
    } ht[N_HOST];

time_t check_ts(ptr,pre,post)
  char *ptr;
  int pre,post;
  {
  int diff,tm[6];
  time_t ts,rt;
  struct tm mt;
  if(!bcd_dec(tm,ptr)) return 0; /* out of range */
  memset((char *)&mt,0,sizeof(mt));
  if((mt.tm_year=tm[0])<50) mt.tm_year+=100;
  mt.tm_mon=tm[1]-1;
  mt.tm_mday=tm[2];
  mt.tm_hour=tm[3];
  mt.tm_min=tm[4];
  mt.tm_sec=tm[5];
  mt.tm_isdst=0;
  ts=mktime(&mt);
  /* compare time with real time */
  time(&rt);
  diff=ts-rt;
  if((pre==0 || pre<diff) && (post==0 || diff<post)) return ts;
#if DEBUG1
  printf("diff %d s out of range (%ds - %ds)\n",diff,pre,post);
#endif
  return 0;
  }

read_chfile()
  {
  FILE *fp;
  int i,j,k,ii,i_chfile;
  char tbuf[1024],host_name[80],tb[256],*ptr;
  struct hostent *h;
  static time_t ltime,ltime_p;

  n_host=0;
  if(*chfile[0])
    {
    if((fp=fopen(chfile[0],"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile[0]);
#endif
      if(negate_channel) for(i=0;i<65536;i++) ch_table[i]=1;
      else for(i=0;i<65536;i++) ch_table[i]=0;
      ii=0;
      while(fgets(tbuf,1024,fp))
        {
        *host_name=0;
        if(sscanf(tbuf,"%s",host_name)==0) continue;
        if(*host_name==0 || *host_name=='#') continue;
        if(*tbuf=='*') /* match any channel */
          {
          if(negate_channel) for(i=0;i<65536;i++) ch_table[i]=0;
          else for(i=0;i<65536;i++) ch_table[i]=1;
          }
        else if(n_host==0 && (*tbuf=='+' || *tbuf=='-'))
          {
          if(*tbuf=='+') hostlist[ii][0]=1;   /* allow */
          else hostlist[ii][0]=(-1);          /* deny */
          if(tbuf[1]=='\r' || tbuf[1]=='\n' || tbuf[1]==' ' || tbuf[1]=='\t')
            {
            hostlist[ii][1]=0;                  /* any host */
            if(*tbuf=='+') write_log("allow from the rest");
            else write_log("deny from the rest");
            }
          else
            {
            if(sscanf(tbuf+1,"%s",host_name)>0) /* hostname */
              {
              if((ptr=strchr(host_name,':'))==0)
                {
                hostlist[ii][2]=0;
                }
              else
                {
                *ptr=0;
                hostlist[ii][2]=atoi(ptr+1);
                }
              if(!(h=gethostbyname(host_name)))
                {
                sprintf(tb,"host '%s' not resolved",host_name);
                write_log(tb);
                continue;
                }
              memcpy((char *)&hostlist[ii][1],h->h_addr,4);
              if(*tbuf=='+') sprintf(tb,"allow");
              else sprintf(tb,"deny");
              sprintf(tb+strlen(tb)," from host %d.%d.%d.%d:%d",
 ((unsigned char *)&hostlist[ii][1])[0],((unsigned char *)&hostlist[ii][1])[1],
 ((unsigned char *)&hostlist[ii][1])[2],((unsigned char *)&hostlist[ii][1])[3],
                hostlist[ii][2]);
              write_log(tb);
              }
            }
          if(++ii==N_HOST)
            {
            n_host=ii;
            write_log("host control table full"); 
            }
          }
        else
          {
          sscanf(tbuf,"%x",&k);
          k&=0xffff;
#if DEBUG
          fprintf(stderr," %04X",k);
#endif
          if(negate_channel) ch_table[k]=0;
          else ch_table[k]=1;
          }
        }
#if DEBUG
      fprintf(stderr,"\n");
#endif
      if(ii>0 && n_host==0) n_host=ii;
      sprintf(tb,"%d host rules",n_host);
      write_log(tb);
      fclose(fp);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile[0]);
#endif
      sprintf(tb,"channel list file '%s' not open",chfile[0]);
      write_log(tb);
      write_log("end");
      exit(1);
      }
    }
  else
    {
    for(i=0;i<65536;i++) ch_table[i]=1;
    n_host=0;
    }

  for(i_chfile=1;i_chfile<n_chfile;i_chfile++)
    {
    if((fp=fopen(chfile[i_chfile],"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile[i_chfile]);
#endif
      while(fgets(tbuf,1024,fp))
        {
        if(*tbuf==0 || *tbuf=='#') continue;
        sscanf(tbuf,"%x",&k);
        k&=0xffff;
#if DEBUG
        fprintf(stderr," %04X",k);
#endif
        ch_table[k]=1;
        }
      fclose(fp);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile[i_chfile]);
#endif
      sprintf(tb,"channel list file '%s' not open",chfile[i_chfile]);
      write_log(tb);
      }
    }

  n_ch=0;
  for(i=0;i<65536;i++) if(ch_table[i]==1) n_ch++;
  if(n_ch==65536) sprintf(tb,"all channels");
  else sprintf(tb,"%d (-%d) channels",n_ch,65536-n_ch);
  write_log(tb);

  time(&ltime);
  j=ltime-ltime_p;
  k=j/2;
  if(ht[0].host)
    {
    sprintf(tb,"statistics in %d s (pkts, B, pkts/s, B/s)",j);
    write_log(tb);
    }
  for(i=0;i<N_HOST;i++) /* print statistics for hosts */
    {
    if(ht[i].host==0) break;
    sprintf(tb,"  src %d.%d.%d.%d:%d   %d %d %d %d",
      ((unsigned char *)&ht[i].host)[0],((unsigned char *)&ht[i].host)[1],
      ((unsigned char *)&ht[i].host)[2],((unsigned char *)&ht[i].host)[3],
      ntohs(ht[i].port),ht[i].n_packets,ht[i].n_bytes,(ht[i].n_packets+k)/j,
      (ht[i].n_bytes+k)/j);
    write_log(tb);
    ht[i].n_packets=ht[i].n_bytes=0;
    }
  ltime_p=ltime;
  signal(SIGHUP,(void *)read_chfile);
  }

check_pno(from_addr,pn,pn_f,sock,fromlen,n,nr,req_delay) /* returns -1 if dup */
  struct sockaddr_in *from_addr;  /* sender address */
  unsigned int pn,pn_f;           /* present and former packet Nos. */
  int sock;                       /* socket */
  int fromlen;                    /* length of from_addr */
  int n;                          /* size of packet */
  int nr;                         /* no resend request if 1 */
  int req_delay;                  /* packet count for delayed resend-request */
  {
  int i,j,k,seg,ppnos_prev,ppnos_now,pn_prev,pn_now,dup;
  int host_;  /* 32-bit-long host address in network byte-order */
  int port_;  /* port No. in network byte-order */
  unsigned int pn_1;
  static unsigned int mask[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
  unsigned char pnc;
  char tb[256];

#if DEBUG4
  printf("%d(%d)",pn,pn_f);
#endif
  j=(-1);
  dup=0;
  host_=from_addr->sin_addr.s_addr;
  port_=from_addr->sin_port;
  for(i=0;i<n_host;i++)
    {
    if(hostlist[i][0]==1 && (hostlist[i][1]==0 || (hostlist[i][1]==host_
      && (hostlist[i][2]==0 || hostlist[i][2]==ntohs(port_))))) break;
    if(hostlist[i][0]==(-1) && (hostlist[i][1]==0 || (hostlist[i][1]==host_
      && (hostlist[i][2]==0 || hostlist[i][2]==ntohs(port_)))))
      {
      if(!no_pinfo)
        {
        sprintf(tb,"deny packet from host %d.%d.%d.%d:%d",
          ((unsigned char *)&host_)[0],((unsigned char *)&host_)[1],
          ((unsigned char *)&host_)[2],((unsigned char *)&host_)[3],ntohs(port_));
        write_log(tb);
        }
      return -1;
      }
    }
  for(i=0;i<N_HOST;i++)
    {
    if(ht[i].host==0) break;
    if(ht[i].host==host_ && ht[i].port==port_)
      {
      j=1;
      ht[i].pnos[ht[i].ppnos]=pn;
      if(++ht[i].ppnos==N_PNOS) ht[i].ppnos=0;
      if((seg=(pn>>6)+2)>3) seg-=4;
      if(ht[i].nosf[seg]) /* if 1, not cleared yet */
        {
        for(k=(seg<<3);k<(seg<<3)+8;k++) ht[i].nos[k]=0; /* clear bits */
        ht[i].nosf[seg]=0; /* cleared */
#if DEBUG4
        printf("clr%d ",seg);
#endif
        }
      if(ht[i].nos[pn>>3]&mask[pn&0x07]) dup=1 ; /* bit was already 1 */
      ht[i].nos[pn>>3]|=mask[pn&0x07]; /* set bit for the packet no */
      ht[i].nosf[pn>>6]=1; /* filling the segment */
      ht[i].n_bytes+=n;
      ht[i].n_packets++;
      break;
      }
    }
  if(i==N_HOST)   /* table is full */
    {
    for(i=0;i<N_HOST;i++) ht[i].host=0;
    write_log("host table full - flushed.");
    i=0;
    }
  if(j<0)
    {
    ht[i].host=host_;
    ht[i].port=port_;
    ht[i].pnos[(ht[i].ppnos=0)]=pn;
    for(k=1;k<N_PNOS;k++) ht[i].pnos[0]=(-1);
    ht[i].ppnos++;
    for(k=0;k<32;k++) ht[i].nos[k]=0; /* clear all bits for pnos */
    ht[i].nos[pn>>3]|=mask[pn&0x07]; /* set bit for the packet no */
    ht[i].nosf[pn>>6]=1;
    sprintf(tb,"registered host %d.%d.%d.%d:%d (%d)",
      ((unsigned char *)&host_)[0],((unsigned char *)&host_)[1],
      ((unsigned char *)&host_)[2],((unsigned char *)&host_)[3],ntohs(port_),i);
    write_log(tb);
    ht[i].n_bytes=n;
    ht[i].n_packets=1;
    }
  else /* check packet no */
    {
    if((ppnos_prev=ht[i].ppnos-2-req_delay)<0) ppnos_prev+=N_PNOS;
    if((ppnos_now=ht[i].ppnos-1-req_delay)<0) ppnos_now+=N_PNOS;
    pn_prev=ht[i].pnos[ppnos_prev];
    pn_now=ht[i].pnos[ppnos_now];
    if(pn_prev>=0 && pn_now>=0)
      {
#if DEBUG4
      printf("(%d>%d)",pn_prev,pn_now);
#endif
      pn_1=(pn_prev+1)&0xff; /* expected */
      if(!nr && (pn_now!=pn_1))
        {
        if(((pn_now-pn_1)&0xff)<N_PACKET) do
          { /* send request-resend packet(s) */
          if(!(ht[i].nos[pn_1>>3]&mask[pn_1&0x07]))
            {
            pnc=pn_1;
            sendto(sock,&pnc,1,0,(struct sockaddr *)from_addr,fromlen);
            sprintf(tb,"request resend %s:%d #%d",
              inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1);
            write_log(tb);
#if DEBUG1
            printf("<%d ",pn_1);
#endif
            }
#if DEBUG4
          else
            {
            sprintf(tb,"dropped but already received %s:%d #%d",
              inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1);
            write_log(tb);
            }
#endif
          } while((pn_1=(++pn_1&0xff))!=pn_now);
        else
          {
          if(((pn_now-pn_1)&0xff)>192 && !dup)
            {
#if DEBUG4
            sprintf(tb,"inverted order but OK %s:%d #%d",
              inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_now);
            write_log(tb);
#endif
            }
          else
            {
            sprintf(tb,"no request resend %s:%d #%d-#%d",
              inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1,
              (pn_now-1)&0xff);
            write_log(tb);
            }
          }
        }
      }
    }
  if(pn!=pn_f)
    {
    if(ht[i].nos[pn_f>>3]&mask[pn_f&0x07])
      {   /* if the resent packet is duplicated, return with -1 */
      if(!no_pinfo)
        {
        sprintf(tb,"discard duplicated resent packet #%d for #%d",pn,pn_f);
        write_log(tb);
        }
      return -1;
      }
#if DEBUG4
    else if(!no_pinfo)
      {
      sprintf(tb,"received resent packet #%d for #%d",pn,pn_f);
      write_log(tb);
      }
#endif
    }
  return 0;
  }

wincpy2(ptw,ts,ptr,size,mon,chhist,from_addr)
  unsigned char *ptw,*ptr;
  time_t ts;
  int size,mon;
  struct { int n; time_t (*ts)[65536]; int p[65536];} *chhist;
  struct sockaddr_in *from_addr;
  {
#define MAX_SR 500
#define MAX_SS 4
#define SR_MON 5
  int sr,n,ss;
  unsigned char *ptr_lim,*ptr1;
  unsigned short ch;
  int gs,i,j,aa,bb,k;
  unsigned long gh;
  char tb[256];

  ptr_lim=ptr+size;
  n=0;
  do    /* loop for ch's */
    {
    if(!mon)
      {
      gh=mklong(ptr);
      ch=(gh>>16)&0xffff;
      sr=gh&0xfff;
      ss=(gh>>12)&0xf;
      if(ss) gs=ss*(sr-1)+8;
      else gs=(sr>>1)+8;
      if(sr>MAX_SR || ss>MAX_SS || ptr+gs>ptr_lim)
        {
        if(!no_pinfo)
          {
          sprintf(tb,
"ill ch hdr %02X%02X%02X%02X %02X%02X%02X%02X psiz=%d sr=%d ss=%d gs=%d rest=%d from %s:%d",
            ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
            size,sr,ss,gs,ptr_lim-ptr,
            inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port));
          write_log(tb);
          }
        return n;
        }
      }
    else /* mon format */
      {
      ch=mkshort(ptr1=ptr);
      ptr1+=2;
      for(i=0;i<SR_MON;i++)
        {
        aa=(*(ptr1++));
        bb=aa&3;
        if(bb) for(k=0;k<bb*2;k++) ptr1++;
        else ptr1++;
        }
      gs=ptr1-ptr;
      if(ptr+gs>ptr_lim)
        {
        if(!no_pinfo)
          {
          sprintf(tb,
"ill ch blk %02X%02X%02X%02X %02X%02X%02X%02X psiz=%d sr=%d gs=%d rest=%d from %s:%d",
            ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
            size,sr,gs,ptr_lim-ptr,
            inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port));
          write_log(tb);
          }
        return n;
        }
      }
    if(ch_table[ch] && ptr+gs<=ptr_lim)
      {
      for(i=0;i<chhist->n;i++) if(chhist->ts[i][ch]==ts) break;
      if(i==chhist->n) /* TS not found in last chhist->n packets */
        {
        if(chhist->n>0)
          {
          chhist->ts[chhist->p[ch]][ch]=ts;
          if(++chhist->p[ch]==chhist->n) chhist->p[ch]=0;
          }
#if DEBUG1
        fprintf(stderr,"%5d",gs);
#endif
        memcpy(ptw,ptr,gs);
        ptw+=gs;
        n+=gs;
        }
#if DEBUG1
      else
        fprintf(stderr,"%5d(!%04X)",gs,ch);
#endif
      }
    ptr+=gs;
    } while(ptr<ptr_lim);
  return n;
  }

send_req(sock,host_addr)
  struct sockaddr_in *host_addr;  /* sender address */
  int sock;                       /* socket */
  {
#define SWAPS(a) (((a)<<8)&0xff00|((a)>>8)&0xff)
  int i,j;
/*
send list of chs : 2B/ch,1024B/packet->512ch/packet max 128packets
header: magic number,seq,n,list...
if all channels, seq=n=0 and no list.
*/
  unsigned seq,n_seq;
  struct { char mn[4]; unsigned short seq[2]; unsigned short chlist[512];}
    sendbuf;
  strcpy(sendbuf.mn,"REQ");
  if(n_ch<65536)
    {
    seq=1;
    if(n_ch==0) n_seq=1;
    else n_seq=(n_ch-1)/512+1;
    sendbuf.seq[1]=SWAPS(n_seq);
    j=0;
    for(i=0;i<65536;i++)
      {
      sendbuf.seq[0]=SWAPS(seq);
      if(ch_table[i]) sendbuf.chlist[j++]=SWAPS(i);
      if(j==512)
        {
        sendto(sock,&sendbuf,8+2*j,0,(struct sockaddr *)host_addr,
          sizeof(*host_addr));
#if DEBUG3
        printf("send channel list to %s:%d (%d): %s %d/%d %04X %04X %04X ...\n",
          inet_ntoa(host_addr->sin_addr),ntohs(host_addr->sin_port),
          n_ch,sendbuf.mn,SWAPS(sendbuf.seq[0]),SWAPS(sendbuf.seq[1]),
          SWAPS(sendbuf.chlist[0]),SWAPS(sendbuf.chlist[1]),SWAPS(sendbuf.chlist[2]));
#endif
        j=0;
        seq++;
        }
      }
    if(j>0)
      {
      sendto(sock,&sendbuf,8+2*j,0,(struct sockaddr *)host_addr,
        sizeof(*host_addr));
#if DEBUG3
        printf("send channel list to %s:%d (%d): %s %d/%d %04X %04X %04X ...\n",
          inet_ntoa(host_addr->sin_addr),ntohs(host_addr->sin_port),
          n_ch,sendbuf.mn,SWAPS(sendbuf.seq[0]),SWAPS(sendbuf.seq[1]),
          SWAPS(sendbuf.chlist[0]),SWAPS(sendbuf.chlist[1]),SWAPS(sendbuf.chlist[2]));
#endif
      seq++;
      }
    }
  else /* all channels */
    {
    sendbuf.seq[0]=sendbuf.seq[1]=0;
    sendto(sock,&sendbuf,8,0,(struct sockaddr *)host_addr,sizeof(*host_addr));
#if DEBUG3
    printf("send channel list to %s:%d (%d): %s %d/%d\n",
      inet_ntoa(host_addr->sin_addr),ntohs(host_addr->sin_port),
      n_ch,sendbuf.mn,SWAPS(sendbuf.seq[0]),SWAPS(sendbuf.seq[1]));
#endif
    }
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  key_t shm_key;
  int shmid;
  unsigned long uni;
  unsigned char *ptr,tm[6],*ptr_size,*ptr_size2,host_name[1024];
  int i,j,k,size,fromlen,n,re,nlen,sock,nn,all,pre,post,c,mon,pl,eobsize,
    sbuf,noreq,no_ts,no_pno,req_delay;
  struct sockaddr_in to_addr,from_addr,host_addr;
  unsigned short to_port,host_port;
  extern int optind;
  extern char *optarg;
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *sh;
  char tb[256],tb2[256];
  struct ip_mreq stMreq;
  char mcastgroup[256]; /* multicast address */
  char interface[256]; /* multicast interface */
  time_t ts,sec,sec_p;
  struct { int n; time_t (*ts)[65536]; int p[65536];} chhist;
  struct hostent *h;
  struct timeval timeout;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;

  if(strcmp(progname,"recvtd")==0) daemon_mode=1;

  if(strcmp(progname,"recvtd")==0)
    sprintf(tb,
	    " usage : '%s (-AaBnMNr) (-d [len(s)]) (-m [pre(m)]) (-p [post(m)]) \\\n\
              (-i [interface]) (-g [mcast_group]) (-s sbuf(KB)) \\\n\
              (-o [src_host]:[src_port]) (-f [ch file]) \\\n\
              [port] [shm_key] [shm_size(KB)] ([ctl file]/- ([log file]))'",
	    progname);
  else
    sprintf(tb,
	    " usage : '%s (-AaBDnMNr) (-d [len(s)]) (-m [pre(m)]) (-p [post(m)]) \\\n\
              (-i [interface]) (-g [mcast_group]) (-s sbuf(KB)) \\\n\
              (-o [src_host]:[src_port]) (-f [ch file]) (-y [req_delay])\\\n\
              [port] [shm_key] [shm_size(KB)] ([ctl file]/- ([log file]))'",
	    progname);

  all=no_pinfo=mon=eobsize=noreq=no_ts=no_pno=0;
  pre=post=0;
  *interface=(*mcastgroup)=(*host_name)=0;
  sbuf=256;
  chhist.n=N_HIST;
  n_chfile=1;
  req_delay=0;
  while((c=getopt(argc,argv,"AaBDd:f:g:i:m:MNno:p:rs:y:"))!=EOF)
    {
    switch(c)
      {
      case 'A':   /* don't check time stamps */
        no_ts=1;
        break;
      case 'a':   /* accept >=A1 packets */
        all=1;
        break;
      case 'B':   /* write blksize at EOB for backward search */
        eobsize=1;
        break;
      case 'D':
	daemon_mode = 1;  /* daemon mode */
	break;   
      case 'd':   /* length of packet history in sec */
        chhist.n=atoi(optarg);
        break;
      case 'i':   /* interface (ordinary IP address) which receive mcast */
        strcpy(interface,optarg);
        break;
      case 'f':   /* channel list file */
        strcpy(chfile[n_chfile++],optarg);
        break;
      case 'g':   /* multicast group (multicast IP address) */
        strcpy(mcastgroup,optarg);
        break;
      case 'm':   /* time limit before RT in minutes */
        pre=atoi(optarg);
        if(pre<0) pre=(-pre);
        break;
      case 'M':   /* 'mon' format data */
        mon=1;
        break;
      case 'N':   /* no packet nos */
        no_pno=no_ts=1;
        break;
      case 'n':   /* supress info on abnormal packets */
        no_pinfo=1;
        break;
      case 'o':   /* host and port for request */
        strcpy(tb2,optarg);
        if(ptr=(unsigned char *)strchr(tb2,':'))
          {
          *ptr=0;
          host_port=atoi(ptr+1);
          }
        else
          {
          fprintf(stderr," option -o requires '[host]:[port]' pair !\n",c);
          fprintf(stderr,"%s\n",tb);
          exit(1);
          }
        strcpy(host_name,tb2);
        break;
      case 'p':   /* time limit after RT in minutes */
        post=atoi(optarg);
        break;
      case 'r':   /* disable resend request */
        noreq=1;
        break;
      case 's':   /* preferred socket buffer size (KB) */
        sbuf=atoi(optarg);
        break;
      case 'y':   /* packet count for delayed resend-request */
        req_delay=atoi(optarg);
        if(req_delay>N_PNOS-2 || req_delay<0)
          {
          fprintf(stderr," resend-request delay < %d !\n",N_PNOS-1);
          fprintf(stderr,"%s\n",tb);
          exit(1);
          }
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
        exit(1);
      }
    }
  optind--;
  if(argc<4+optind)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }
  pre=(-pre*60);
  post*=60;
  to_port=atoi(argv[1+optind]);
  shm_key=atoi(argv[2+optind]);
  size=atoi(argv[3+optind])*1000;
  *chfile[0]=0;
  if(argc>4+optind)
    {
    if(strcmp("-",argv[4+optind])==0) *chfile[0]=0;
    else
      {
      if(argv[4+optind][0]=='-')
        {
        strcpy(chfile[0],argv[4+optind]+1);
        negate_channel=1;
        }
      else
        {
        strcpy(chfile[0],argv[4+optind]);
        negate_channel=0;
        }
      }
    }
  if(n_chfile==1 && (*chfile[0])==0) n_chfile=0;

  if(argc>5+optind) logfile=argv[5+optind];
  else
    {
      logfile=NULL;
      if (daemon_mode)
	syslog_mode = 1;
    }

  if((chhist.ts=
      (time_t (*)[65536])malloc(65536*chhist.n*sizeof(time_t)))==NULL)
    {
    chhist.n=N_HIST;
    if((chhist.ts=
        (time_t (*)[65536])malloc(65536*chhist.n*sizeof(time_t)))==NULL)
      {
      fprintf(stderr,"malloc failed (chhist.ts)\n");
      exit(1);
      }
    }

   /* daemon mode */
   if (daemon_mode) {
     daemon_init(progname, LOG_USER, syslog_mode);
     umask(022);
   }

  sprintf(tb,"n_hist=%d size=%d req_delay=%d",chhist.n,
    65536*chhist.n*sizeof(time_t),req_delay);
  write_log(tb);

  /* shared memory */
  if((shmid=shmget(shm_key,size,IPC_CREAT|0666))<0) err_sys("shmget");
  if((sh=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");

  /* initialize buffer */
  sh->c=0;
  sh->pl=pl=(size-sizeof(*sh))/10*9;
  sh->p=0;
  sh->r=(-1);

  sprintf(tb,"start shm_key=%d id=%d size=%d",shm_key,shmid,size);
  write_log(tb);
  if(all) write_log("accept >=A1 packets");
  sprintf(tb,"TS window %ds - +%ds",pre,post);
  write_log(tb);

  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");
  for(j=sbuf;j>=16;j-=4)
    {
    i=j*1024;
    if(setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))>=0)
      break;
    }
  if(j<16) err_sys("SO_RCVBUF setsockopt error\n");
  sprintf(tb,"RCVBUF size=%d",j*1024);
  write_log(tb);

  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  to_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  to_addr.sin_port=htons(to_port);
  if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0) err_sys("bind");

  if(*mcastgroup){
    stMreq.imr_multiaddr.s_addr=inet_addr(mcastgroup);
    if(*interface) stMreq.imr_interface.s_addr=inet_addr(interface);
    else stMreq.imr_interface.s_addr=INADDR_ANY;
    if(setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&stMreq,
      sizeof(stMreq))<0) err_sys("IP_ADD_MEMBERSHIP setsockopt error\n");
  }

  if(*host_name) /* host_name and host_port specified */
    {
    /* source host/port */
    if(!(h=gethostbyname(host_name))) err_sys("can't find host");
    memset((char *)&host_addr,0,sizeof(host_addr));
    host_addr.sin_family=AF_INET;
    memcpy((caddr_t)&host_addr.sin_addr,h->h_addr,h->h_length);
    host_addr.sin_port=htons(host_port);
    for(j=sbuf;j>=16;j-=4)
      {
      i=j*1024;
      if(setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))>=0)
        break;
      }
    if(j<16) err_sys("SO_SNDBUF setsockopt error\n");
    sprintf(tb,"SNDBUF size=%d",j*1024);
    write_log(tb);
    }

  if(noreq) write_log("resend request disabled");
  if(no_ts) write_log("time-stamps not interpreted");
  if(no_pno) write_log("packet numbers not interpreted");
  if(*host_name)
    {
    sprintf(tb,"send channel list to %s:%d",
      inet_ntoa(host_addr.sin_addr),ntohs(host_addr.sin_port));
    write_log(tb);
    }

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  for(i=0;i<6;i++) tm[i]=(-1);
  ptr=ptr_size=sh->d+sh->p;
  read_chfile();
  time(&sec);
  sec_p=sec-1;

  while(1)
    {
    if(*host_name) /* send request */
      {
      time(&sec);
      if(sec!=sec_p)
        {
        send_req(sock,&host_addr);
        sec_p=sec;
        }
      }

    k=1<<sock;
    timeout.tv_sec=0;
    timeout.tv_usec=500000;
    if(select(sock+1,(fd_set *)&k,NULL,NULL,&timeout)<=0) continue;

    fromlen=sizeof(from_addr);
    n=recvfrom(sock,rbuf,MAXMESG,0,(struct sockaddr *)&from_addr,&fromlen);
#if DEBUG0
    if(rbuf[0]==rbuf[1]) printf("%d ",rbuf[0]);
    else printf("%d(%d) ",rbuf[0],rbuf[1]);
    for(i=0;i<16;i++) printf("%02X",rbuf[i]);
    printf(" (%d)\n",n);
#endif

    if(no_ts) /* no time stamp */
      {
      ptr+=4;   /* size */
      ptr+=4;   /* time of write */
      if(no_pno) memcpy(ptr,rbuf,nn=n);
      else
        {
        if(check_pno(&from_addr,rbuf[0],rbuf[1],sock,fromlen,n,noreq,req_delay)<0) continue;
        memcpy(ptr,rbuf+2,nn=n-2);
        }
      ptr+=nn;
      uni=time(0);
      ptr_size[4]=uni>>24;  /* tow (H) */
      ptr_size[5]=uni>>16;
      ptr_size[6]=uni>>8;
      ptr_size[7]=uni;      /* tow (L) */
      ptr_size2=ptr;
      if(eobsize) ptr+=4; /* size(2) */
      uni=ptr-ptr_size;
      ptr_size[0]=uni>>24;  /* size (H) */
      ptr_size[1]=uni>>16;
      ptr_size[2]=uni>>8;
      ptr_size[3]=uni;      /* size (L) */
      if(eobsize)
        {
        ptr_size2[0]=ptr_size[0];  /* size (H) */
        ptr_size2[1]=ptr_size[1];
        ptr_size2[2]=ptr_size[2];
        ptr_size2[3]=ptr_size[3];  /* size (L) */
        }
      sh->r=sh->p;      /* latest */
      if(eobsize && ptr>sh->d+pl) {sh->pl=ptr-sh->d-4;ptr=sh->d;}
      if(!eobsize && ptr>sh->d+sh->pl) ptr=sh->d;
      sh->p=ptr-sh->d;
      sh->c++;
      ptr_size=ptr;
#if DEBUG1
      printf("%s:%d(%d)>",inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),n);
      printf("%d(%d) ",rbuf[0],rbuf[1]);
      rbuf[n]=0;
      printf("%s\n",rbuf);
#endif
      continue;
      }

    if(check_pno(&from_addr,rbuf[0],rbuf[1],sock,fromlen,n,noreq,req_delay)<0) continue;

  /* check packet ID */
    if(rbuf[2]<0xA0)
      {
      for(i=0;i<6;i++) if(rbuf[i+2]!=tm[i]) break;
      if(i==6)  /* same time */
        {
        nn=wincpy2(ptr,ts,rbuf+8,n-8,mon,&chhist,&from_addr);
        ptr+=nn;
        uni=time(0);
        ptr_size[4]=uni>>24;  /* tow (H) */
        ptr_size[5]=uni>>16;
        ptr_size[6]=uni>>8;
        ptr_size[7]=uni;      /* tow (L) */
        }
      else
        {
        if((ptr-ptr_size)>14)
          {
          ptr_size2=ptr;
          if(eobsize) ptr+=4; /* size(2) */
          uni=ptr-ptr_size;
          ptr_size[0]=uni>>24;  /* size (H) */
          ptr_size[1]=uni>>16;
          ptr_size[2]=uni>>8;
          ptr_size[3]=uni;      /* size (L) */
          if(eobsize)
            {
            ptr_size2[0]=ptr_size[0];  /* size (H) */
            ptr_size2[1]=ptr_size[1];
            ptr_size2[2]=ptr_size[2];
            ptr_size2[3]=ptr_size[3];  /* size (L) */
            }
#if DEBUG2
          printf("%d - %d (%d) %d / %d\n",ptr_size-sh->d,uni,ptr_size2-sh->d,
            sh->pl,pl);
#endif
#if DEBUG1
          printf("(%d)",time(0));
          for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
          printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
          }
        else ptr=ptr_size;
        if(!(ts=check_ts(rbuf+2,pre,post)))
          {
          if(!no_pinfo)
            {
            sprintf(tb,"ill time %02X:%02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
              rbuf[0],rbuf[1],rbuf[2],rbuf[3],rbuf[4],rbuf[5],rbuf[6],rbuf[7],
              rbuf[8],rbuf[9],rbuf[10],rbuf[11],rbuf[12],rbuf[13],rbuf[14],
              rbuf[15],inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port));
            write_log(tb);
            }
          for(i=0;i<6;i++) tm[i]=(-1);
          continue;
          }
        else
          {
          sh->r=sh->p;      /* latest */
          if(eobsize && ptr>sh->d+pl) {sh->pl=ptr-sh->d-4;ptr=sh->d;}
          if(!eobsize && ptr>sh->d+sh->pl) ptr=sh->d;
          sh->p=ptr-sh->d;
          sh->c++;
          ptr_size=ptr;
          ptr+=4;   /* size */
          ptr+=4;   /* time of write */
          memcpy(ptr,rbuf+2,6);
          ptr+=6;
          nn=wincpy2(ptr,ts,rbuf+8,n-8,mon,&chhist,&from_addr);
          ptr+=nn;
          memcpy(tm,rbuf+2,6);
          uni=time(0);
          ptr_size[4]=uni>>24;  /* tow (H) */
          ptr_size[5]=uni>>16;
          ptr_size[6]=uni>>8;
          ptr_size[7]=uni;      /* tow (L) */
          }
        }
      }
    else if(rbuf[2]==0xA0) /* merged packet */
      {
      nlen=n-3;
      j=3;
      while(nlen>0)
        {
        n=(rbuf[j]<<8)+rbuf[j+1];
        if(n<8 || n>nlen)
          {
          if(!no_pinfo)
            {
sprintf(tb,"ill blk n=%d(%02X%02X) %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d #%d(#%d) at %d",
              n,rbuf[j],rbuf[j+1],rbuf[j+2],rbuf[j+3],rbuf[j+4],rbuf[j+5],
              rbuf[j+6],rbuf[j+7],rbuf[j+8],rbuf[j+9],rbuf[j+10],rbuf[j+11],
              rbuf[j+12],rbuf[j+13],rbuf[j+14],rbuf[j+15],
              inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),
              rbuf[0],rbuf[1],j);
            write_log(tb);
            }
          for(i=0;i<6;i++) tm[i]=(-1);
          break;
          }
        j+=2;
        for(i=0;i<6;i++) if(rbuf[j+i]!=tm[i]) break;
        if(i==6)  /* same time */
          {
          nn=wincpy2(ptr,ts,rbuf+j+6,n-8,mon,&chhist,&from_addr);
          ptr+=nn;
          uni=time(0);
          ptr_size[4]=uni>>24;  /* tow (H) */
          ptr_size[5]=uni>>16;
          ptr_size[6]=uni>>8;
          ptr_size[7]=uni;      /* tow (L) */
          }
        else
          {
          if((ptr-ptr_size)>14) /* data copied - close the sec block */
            {
            ptr_size2=ptr;
            if(eobsize) ptr+=4; /* size(2) */
            uni=ptr-ptr_size;
            ptr_size[0]=uni>>24;  /* size (H) */
            ptr_size[1]=uni>>16;
            ptr_size[2]=uni>>8;
            ptr_size[3]=uni;      /* size (L) */
            if(eobsize)
              {
              ptr_size2[0]=ptr_size[0];  /* size (H) */
              ptr_size2[1]=ptr_size[1];
              ptr_size2[2]=ptr_size[2];
              ptr_size2[3]=ptr_size[3];  /* size (L) */
              }
#if DEBUG2
          printf("%d - %d (%d) %d / %d\n",ptr_size-sh->d,uni,ptr_size2-sh->d,
            sh->pl,pl);
#endif
#if DEBUG1
            printf("(%d)",time(0));
            for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
            printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
            sh->r=sh->p;      /* latest */
            if(eobsize && ptr>sh->d+pl) {sh->pl=ptr-sh->d-4;ptr=sh->d;}
            if(!eobsize && ptr>sh->d+sh->pl) ptr=sh->d;
            sh->p=ptr-sh->d;
            sh->c++;
            ptr_size=ptr;
            }
          else /* sec block empty - reuse the space */
            ptr=ptr_size;
          if(!(ts=check_ts(rbuf+j,pre,post))) /* illegal time */
            {
            if(!no_pinfo)
              {
              sprintf(tb,"ill time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
                rbuf[j],rbuf[j+1],rbuf[j+2],rbuf[j+3],rbuf[j+4],rbuf[j+5],
                rbuf[j+6],rbuf[j+7],rbuf[j+8],rbuf[j+9],rbuf[j+10],rbuf[j+11],
                rbuf[j+12],rbuf[j+13],
                inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port));
              write_log(tb);
              }
            for(i=0;i<6;i++) tm[i]=(-1);
            }
          else /* valid time stamp */
            {
            ptr+=4;   /* size */
            ptr+=4;   /* time of write */
            memcpy(ptr,rbuf+j,6);
            ptr+=6;
            nn=wincpy2(ptr,ts,rbuf+j+6,n-8,mon,&chhist,&from_addr);
            ptr+=nn;
            memcpy(tm,rbuf+j,6);
            uni=time(0);
            ptr_size[4]=uni>>24;  /* tow (H) */
            ptr_size[5]=uni>>16;
            ptr_size[6]=uni>>8;
            ptr_size[7]=uni;      /* tow (L) */
            }
          }
        nlen-=n;
        j+=n-2;
        }
      }
    else if(all) /* rbuf[2]>=0xA1 with packet ID */
      {
      if(!(ts=check_ts(rbuf+3,pre,post)))
        {
        if(!no_pinfo)
          {
          sprintf(tb,"ill time %02X:%02X %02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
            rbuf[0],rbuf[1],rbuf[2],rbuf[3],rbuf[4],rbuf[5],rbuf[6],rbuf[7],
            rbuf[8],rbuf[9],rbuf[10],rbuf[11],rbuf[12],rbuf[13],rbuf[14],
            rbuf[15],rbuf[16],inet_ntoa(from_addr.sin_addr),
            ntohs(from_addr.sin_port));
          write_log(tb);
          }
        for(i=0;i<6;i++) tm[i]=(-1);
        continue;
        }
      else
        {
        ptr_size=ptr;
        ptr+=4;   /* size */
        ptr+=4;   /* time of write */
        memcpy(ptr,rbuf+2,n-2);
        ptr+=n-2;
        ptr_size2=ptr;
        if(eobsize) ptr+=4; /* size(2) */
        uni=ptr-ptr_size;
        ptr_size[0]=uni>>24;  /* size (H) */
        ptr_size[1]=uni>>16;
        ptr_size[2]=uni>>8;
        ptr_size[3]=uni;      /* size (L) */
        if(eobsize)
          {
          ptr_size2[0]=ptr_size[0];  /* size (H) */
          ptr_size2[1]=ptr_size[1];
          ptr_size2[2]=ptr_size[2];
          ptr_size2[3]=ptr_size[3];  /* size (L) */
          }
        memcpy(tm,rbuf+3,6);
        uni=time(0);
        ptr_size[4]=uni>>24;  /* tow (H) */
        ptr_size[5]=uni>>16;
        ptr_size[6]=uni>>8;
        ptr_size[7]=uni;      /* tow (L) */

        sh->r=sh->p;      /* latest */
        if(eobsize && ptr>sh->d+pl) {sh->pl=ptr-sh->d-4;ptr=sh->d;}
        if(!eobsize && ptr>sh->d+sh->pl) ptr=sh->d;
        sh->p=ptr-sh->d;
        sh->c++;
        }
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    }
  }
