/* $Id: recvt46.c,v 1.11 2014/09/25 10:37:10 uehira Exp $ */
/*-
 "recvt.c"      4/10/93 - 6/2/93,7/2/93,1/25/94    urabe
                2/3/93,5/25/94,6/16/94 
                1/6/95 bug in adj_time fixed (tm[0]--) 
                2/17/95 "illegal time" message stopped 
                3/15-16/95 write_log changed 
                3/26/95 check_packet_no; port# 
                10/16/95 added processing of "host table full" 
                5/22/96 support merged packet with ID="0x0A" 
                5/22/96 widen time window to +/-30 min 
                5/28/96 bcopy -> memcpy 
                6/6/96,7/9/96 discard duplicated resent packets & fix bug  
                12/29/96 "next" 
                97.6.23 RCVBUF->65535 
                97.9.4        & 50000 
                97.12.18 channel selection by file 
                97.12.23 ch no. in "illegal time" & LITTLE_ENDIAN 
                98.4.23 b2d[] etc. 
                98.6.26 yo2000 
                99.2.4  moved signal(HUP) to read_chfile() by urabe 
                99.4.19 byte-order-free 
                2000.3.13 >=A1 format 
                2000.4.24 added SS & SR check, check_time +/-60m 
                2000.4.24 strerror() 
                2000.4.25 added check_time() in >=A1 format 
                2000.4.26 host control, statistics, -a, -m, -p options 
                2001.2.20 wincpy() improved 
                2001.3.9 debugged for sh->r 
                2001.11.14 strerror(),ntohs() 
                2002.1.7 implemented multicasting (options -g, -i) 
                2002.1.7 option -n to suppress info on abnormal packets 
                2002.1.8 MAXMESG increased to 32768 bytes 
                2002.1.12 trivial fixes on 'usage' 
                2002.1.15 option '-M' necessary for receiving mon data 
                2002.3.2 wincpy2() discard duplicated data (TS+CH) 
                2002.3.2 option -B ; write blksize at EOB 
                2002.3.18 host control debugged 
                2002.5.3 N_PACKET 64->128, 'no request resend' log 
                2002.5.3 maximize RCVBUF size 
                2002.5.3,7 maximize RCVBUF size ( option '-s' )
                2002.5.14 -n debugged / -d to set ddd length 
                2002.5.23 stronger to destructed packet 
                2002.11.29 corrected byte-order of port no. in log 
                2002.12.21 disable resend request if -r 
                2003.3.24-25 -N (no pno) and -A (no TS) options 
                2004.8.9 fixed bug introduced in 2002.5.23 
                2004.10.26 daemon mode (Uehira) 
                2004.11.15 corrected byte-order of port no. in log 
                2005.2.17 option -o [source host]:[port] for send request 
                2005.2.20 option -f [ch_file] for additional ch files 
                2005.6.24 don't change optarg's content (-o) 
                2005.9.25 allow disorder of arriving packets 
                2005.9.25 host(:port) in control file
		2009.1.8  64bit?
                2010.10.4 fixed bug in check_pno().
                          ht[].pnos[] : unsigned int --> int
		2011.1.7 -e for automatically reload chfile
		          if packet comes from deny host.
		2011.2.13-15 IPv6/IPv4 version of 'recvt.c'. (Uehira)
		2014.4.6 NIC for receive can be specified by -i [IP_address or hostname] (IPv4 & IPv6)
                2014.6.27 bug in main() : 'static' struct ch_hist  chhist; fixed.
-*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <errno.h>
#include <syslog.h>

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

#include "daemon_mode.h"
#include "udpu.h"
#include "winlib.h"

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

static const char rcsid[] =
  "$Id: recvt46.c,v 1.11 2014/09/25 10:37:10 uehira Exp $";

static uint8_w rbuf[MAXMESG], ch_table[WIN_CHMAX];
static char *chfile[N_CHFILE];
static int n_ch, negate_channel, n_host, no_pinfo, n_chfile;
static int auto_reload_chfile;
static int daemon_mode;
static int family;   /* family type: AF_UNSPEC, AF_INET, AF_INET6 */

struct {
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port No. */
  int  f;  /* flag */
} static hostlist[N_HOST]; 

char *progname, *logfile;
int  syslog_mode, exit_status;

static struct {
  /* in_addr_t host; */  /* unsigned int32_t */
  /* in_port_t port; */  /* unsigned int16_t */
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
  int ppnos;	/* pointer for pnos */  /* 0 <--> N_PNOS */
  int32_w pnos[N_PNOS];
  int nosf[4]; /* 4 segments x 64 */  /* 64bit ok */
  uint8_w  nos[256/8];     /*- 64bit ok?? -*/
  unsigned long n_bytes;       /*- 64bit ok -*/
  unsigned long n_packets;     /*- 64bit ok -*/
  /*     unsigned int n_bytes; */
  /*     unsigned int n_packets; */
} ht[N_HOST];

struct ch_hist {
  int n;
  time_t (*ts)[WIN_CHMAX];
  int p[WIN_CHMAX];
};

/* prototyes */
static void read_chfile(void);
static int check_pno(struct sockaddr *, unsigned int, unsigned int,
		     int, socklen_t, ssize_t, int, int);
static size_t wincpy2(uint8_w *, time_t, uint8_w *, ssize_t, int,
		      struct ch_hist *, struct sockaddr *, socklen_t);
static void send_req(int, struct sockaddr *, socklen_t);
static void usage(void);
int main(int, char *[]);


static void
read_chfile(void)
{
  FILE *fp;
  int i, k, ii, i_chfile;
  time_t tdif, tdif2;
  char tbuf[1024], host_name[1024], tb[256];
  char  *port_ptr;  /* port No. */
  char  *host_ptr;
  static time_t ltime, ltime_p;
  struct addrinfo  hints, *res, *ai;
  int    gai_error;

  n_host=0;
  if (chfile[0] != NULL) {
    if ((fp = fopen(chfile[0], "r")) != NULL) {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile[0]);
#endif
      if (negate_channel)
	for (i = 0; i < WIN_CHMAX; i++)
	  ch_table[i] = 1;
      else
	for (i = 0; i < WIN_CHMAX; i++)
	  ch_table[i] = 0;
      ii=0;
      while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
        host_name[0] = '\0';
	if (sscanf(tbuf, "%s", host_name) == 0)  /* buffer overrun ok */
	  continue;
        if (host_name[0] == '\0' || host_name[0] == '#')
	  continue;
        if (tbuf[0] == '*') { /* match any channel */
          if (negate_channel)
	    for (i = 0; i < WIN_CHMAX; i++)
	      ch_table[i] = 0;
          else
	    for (i = 0; i < WIN_CHMAX; i++)
	      ch_table[i] = 1;
	} else if (n_host == 0 && (tbuf[0] == '+' || tbuf[0] == '-')) {
          if (tbuf[0] == '+')
	    hostlist[ii].f = 1;   /* allow */
          else
	    hostlist[ii].f = -1;  /* deny */
          if (tbuf[1] == '\r' || tbuf[1] == '\n'
	      || tbuf[1] == ' ' || tbuf[1] == '\t') {
            hostlist[ii].host_[0] = '\0';                  /* any host */
            if (tbuf[0] == '+')
	      write_log("allow from the rest");
            else
	      write_log("deny from the rest");
	    if(++ii == N_HOST) {
	      n_host = ii;
	      write_log("host control table full"); 
	    }
	  } else {
            if (sscanf(tbuf + 1, "%s", host_name) > 0) { /* hostname */
	      if (split_host_port(host_name, &host_ptr, &port_ptr))
		err_sys("Invalid host-port");
	      memset(&hints, 0, sizeof(hints));
	      hints.ai_family = family;
	      hints.ai_socktype = SOCK_DGRAM;
	      hints.ai_protocol = IPPROTO_UDP;
	      if ((gai_error = getaddrinfo(host_ptr, port_ptr, &hints, &res)) != 0) {
		(void)snprintf(tb, sizeof(tb),
			       "host '%s' port '%s' not resolved : %s",
			       host_ptr, port_ptr, gai_strerror(gai_error));
		write_log(tb);
		continue;
	      }
	      for (ai = res; ai != NULL; ai = ai->ai_next) {
		(void)getnameinfo(ai->ai_addr, ai->ai_addrlen,
				  hostlist[ii].host_, NI_MAXHOST,
				  hostlist[ii].port_, NI_MAXSERV,
				  NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
		if (tbuf[0] == '+') {
		  hostlist[ii].f = 1;  /* allow */
		  if (strchr(hostlist[ii].host_, ':') != NULL)
		    (void)snprintf(tb, sizeof(tb), "allow from host [%s]:%s",
				   hostlist[ii].host_, hostlist[ii].port_);
		  else
		    (void)snprintf(tb, sizeof(tb), "allow from host %s:%s",
				   hostlist[ii].host_, hostlist[ii].port_);
		  
		} else {
		  hostlist[ii].f = -1; /* deny */
		  if (strchr(hostlist[ii].host_, ':') != NULL)
		    (void)snprintf(tb, sizeof(tb), "deny from host [%s]:%s",
				   hostlist[ii].host_, hostlist[ii].port_);
		  else
		    (void)snprintf(tb, sizeof(tb), "deny from host %s:%s",
				   hostlist[ii].host_, hostlist[ii].port_);
		}
		write_log(tb);

		if(++ii == N_HOST) {
		  n_host = ii;
		  write_log("host control table full"); 
		}
	      } /* for (ai = res; ai != NULL; ai = ai->ai_next) */
	      if (res != NULL)
		freeaddrinfo(res);
	    }
	  }
	} else {   /* channel control part */
          sscanf(tbuf, "%x", &k);
          k &= 0xffff;
#if DEBUG
          fprintf(stderr, " %04X", k);
#endif
          if (negate_channel)
	    ch_table[k] = 0;
          else
	    ch_table[k] = 1;
	}
      }
#if DEBUG
      fprintf(stderr,"\n");
#endif
      if (ii > 0 && n_host == 0)
	n_host=ii;
      snprintf(tb, sizeof(tb), "%d host rules", n_host);
      write_log(tb);
      fclose(fp);
    } else {
#if DEBUG
      fprintf(stderr, "ch_file '%s' not open\n", chfile[0]);
#endif
      snprintf(tb, sizeof(tb), "channel list file '%s' not open", chfile[0]);
      err_sys(tb);
    }
  } else {
    for (i = 0; i < WIN_CHMAX; i++)
      ch_table[i]=1;
    n_host = 0;
  }

  for (i_chfile = 1; i_chfile < n_chfile; i_chfile++) {
    if ((fp = fopen(chfile[i_chfile], "r")) != NULL) {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile[i_chfile]);
#endif
      while (fgets(tbuf, sizeof(tbuf), fp)) {
        if (tbuf[0] == '\0' || tbuf[0] == '#')
	  continue;
        sscanf(tbuf, "%x", &k);
        k &= 0xffff;
#if DEBUG
        fprintf(stderr, " %04X", k);
#endif
        ch_table[k] = 1;
      }
      fclose(fp);
    } else {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile[i_chfile]);
#endif
      snprintf(tb, sizeof(tb),
	       "channel list file '%s' not open", chfile[i_chfile]);
      write_log(tb);
    }
  }

  n_ch = 0;
  for (i = 0;i < WIN_CHMAX; i++)
    if (ch_table[i] == 1)
      n_ch++;
  if (n_ch == WIN_CHMAX)
    snprintf(tb, sizeof(tb), "all channels");
  else
    snprintf(tb, sizeof(tb), "%d (-%d) channels", n_ch, WIN_CHMAX - n_ch);
  write_log(tb);

  time(&ltime);
  tdif = ltime - ltime_p;
  if (tdif != 0) {
    tdif2 = tdif / 2;
    if (ht[0].host[0] != '\0') {
      snprintf(tb, sizeof(tb),
	       "statistics in %ld s (pkts, B, pkts/s, B/s)", tdif);
      write_log(tb);
    }
    for (i = 0; i < N_HOST; i++) { /* print statistics for hosts */
      if (ht[i].host[0] == '\0')
	break;
      if(ht[i].n_packets == 0 && ht[i].n_bytes == 0)
	continue;
      if (strchr(ht[i].host, ':') != NULL)
	snprintf(tb, sizeof(tb), "  src [%s]:%s   %lu %lu %lu %lu",
		 ht[i].host, ht[i].port, ht[i].n_packets, ht[i].n_bytes,
		 (ht[i].n_packets + tdif2) / tdif,
		 (ht[i].n_bytes + tdif2) / tdif);
      else
	snprintf(tb, sizeof(tb), "  src %s:%s   %lu %lu %lu %lu",
		 ht[i].host, ht[i].port, ht[i].n_packets, ht[i].n_bytes,
		 (ht[i].n_packets + tdif2) / tdif,
		 (ht[i].n_bytes + tdif2) / tdif);
      write_log(tb);
      ht[i].n_packets = ht[i].n_bytes = 0;
    }
    ltime_p = ltime;
  }
  signal(SIGHUP, (void *)read_chfile);
}

/* returns -1 if dup */
static int
check_pno(struct sockaddr *from_addr, unsigned int pn, unsigned int pn_f,
	  int sock, socklen_t fromlen, ssize_t n, int nr, int req_delay)
/*  struct sockaddr *from_addr;   sender address */
/*  unsigned int pn,pn_f;           present and former packet Nos. */
/*  int sock;                       socket */
/*  socklen_t fromlen;              length of from_addr */
/*  ssize_t n;                          size of packet */
/*  int nr;                         no resend request if 1 */
/*  int req_delay;                  packet count for delayed resend-request */

/*  global : hostlist, ht, n_host, no_pinfo, auto_reload_chfile */
/*  uinsiged int OK  */
{
  int i, j, k, seg, dup;
  int32_w  ppnos_prev, ppnos_now, pn_prev, pn_now;
  /* in_addr_t host_; */  /* 32-bit-long host address in network byte-order */
  /* in_port_t port_; */  /* 16-bit-long port No. in network byte-order */
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port No. */
  unsigned int pn_1;  /* 64bit ok */
  static unsigned int 
    mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  uint8_w pnc;   /* 64bit ok */
  char tb[256];

#if DEBUG4
  printf("%u(%u)", pn, pn_f);
#endif

  j = (-1);
  dup = 0;
  /* host_=from_addr->sin_addr.s_addr; */
  /* port_=from_addr->sin_port; */
  if (getnameinfo(from_addr, fromlen,
		  host_, sizeof(host_), port_, sizeof(port_),
		  NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV))
    err_sys("getnameinfo");

  for (i = 0; i < n_host; i++) {
    if (hostlist[i].f == 1 && (hostlist[i].host_[0] == '\0' || (strcmp(hostlist[i].host_, host_) == 0 && (hostlist[i].port_[0] == '0' || strcmp(hostlist[i].port_, port_) == 0))))
      break;
    if (hostlist[i].f == (-1) && (hostlist[i].host_[0] == '\0' || (strcmp(hostlist[i].host_, host_) == 0 && (hostlist[i].port_[0] == '0' || strcmp(hostlist[i].port_, port_) == 0)))) {
      if (!no_pinfo) {
	if (strchr(host_, ':') != NULL)
	  snprintf(tb, sizeof(tb),
		   "deny packet from host [%s]:%s", host_, port_);
	else
	  snprintf(tb, sizeof(tb),
		   "deny packet from host %s:%s", host_, port_);
        write_log(tb);
      }
      /* automatically reload chfile */
      if (auto_reload_chfile)
	read_chfile();
      return (-2);
    }
  }
  for (i = 0; i < N_HOST; i++) {
    if (ht[i].host[0] == '\0')
      break;
    if (strcmp(ht[i].host, host_) == 0 && strcmp(ht[i].port, port_) == 0) {
      j = 1;
      ht[i].pnos[ht[i].ppnos] = pn;
      if (++ht[i].ppnos == N_PNOS)
	ht[i].ppnos = 0;
      if ((seg = (pn >> 6) + 2) > 3)
	seg -= 4;
      if (ht[i].nosf[seg])  {  /* if 1, not cleared yet */
        for (k = (seg << 3); k < (seg << 3) + 8; k++)
	  ht[i].nos[k] = 0; /* clear bits */
        ht[i].nosf[seg] = 0; /* cleared */
#if DEBUG4
        printf("clr%d ", seg);
#endif
      }
      if (ht[i].nos[pn >> 3] & mask[pn & 0x07])
	dup = 1 ; /* bit was already 1 */
      ht[i].nos[pn >> 3] |= mask[pn & 0x07]; /* set bit for the packet no */
      ht[i].nosf[pn >> 6] = 1; /* filling the segment */
      ht[i].n_bytes += n;
      ht[i].n_packets++;
      break;
    }
  }
  if (i == N_HOST) {  /* table is full */
    for (i = 0; i < N_HOST; i++)
      ht[i].host[0] = '\0';
    write_log("host table full - flushed.");
    i = 0;
  }
  if (j < 0) {
    (void)strcpy(ht[i].host, host_);  /* buffer overrun ok */
    (void)strcpy(ht[i].port, port_);  /* buffer overrun ok */
    ht[i].pnos[(ht[i].ppnos = 0)] = pn;
    for (k = 1; k < N_PNOS; k++)
      ht[i].pnos[k] = (-1);
    ht[i].ppnos++;
    for (k = 0; k < 32; k++)
      ht[i].nos[k] = 0; /* clear all bits for pnos */
    ht[i].nos[pn >> 3] |= mask[pn & 0x07]; /* set bit for the packet no */
    ht[i].nosf[pn >> 6] = 1;
    if (strchr(host_, ':') != NULL)
      snprintf(tb, sizeof(tb), "registered host [%s]:%s (%d)", host_, port_, i);
    else
      snprintf(tb, sizeof(tb), "registered host %s:%s (%d)", host_, port_, i);
    write_log(tb);
    ht[i].n_bytes = n;
    ht[i].n_packets = 1;
  } else { /* check packet no */
    if ((ppnos_prev = ht[i].ppnos - 2 - req_delay) < 0)
      ppnos_prev += N_PNOS;
    if ((ppnos_now = ht[i].ppnos - 1 - req_delay) < 0)
      ppnos_now += N_PNOS;
    pn_prev = ht[i].pnos[ppnos_prev];
    pn_now = ht[i].pnos[ppnos_now];
    if (pn_prev >= 0 && pn_now >= 0) {
#if DEBUG4
      printf("(%d>%d)", pn_prev, pn_now);
#endif
      pn_1 = (pn_prev + 1) & 0xff; /* expected */
      if (!nr && (pn_now != pn_1)) {
        if (((pn_now - pn_1) & 0xff) < N_PACKET)
	  do {   /* send request-resend packet(s) */
	    if (!(ht[i].nos[pn_1 >> 3] & mask[pn_1 & 0x07])) {
	      pnc = (uint8_w)pn_1;
	      if (sendto(sock, &pnc, 1, 0, from_addr, fromlen) < 0)
		err_sys("check_pno: sendto");
	      if (strchr(host_, ':') != NULL)
		snprintf(tb, sizeof(tb), "request resend [%s]:%s #%d",
			 host_, port_, pn_1);
	      else
		snprintf(tb, sizeof(tb), "request resend %s:%s #%d",
			 host_, port_, pn_1);
	      write_log(tb);
#if DEBUG1
	      printf("<%u ", pn_1);
#endif
            }
#if DEBUG4
	    else {
	      snprintf(tb, sizeof(tb), "dropped but already received %s:%s #%d",
		       host_, port_, pn_1);
	      write_log(tb);
            }
#endif
          } while ((pn_1 = (++pn_1 & 0xff)) != pn_now);
        else {
          if (((pn_now - pn_1) & 0xff) > 192 && !dup) {
#if DEBUG4
	    snprintf(tb,sizeof(tb),"inverted order but OK %s:%d #%d",
		     host_, port_, pn_now);
            write_log(tb);
#endif
	  } else {
	    if (strchr(host_, ':') != NULL)
	      snprintf(tb,sizeof(tb),"no request resend [%s]:%s #%d-#%d",
		       host_, port_, pn_1, (pn_now - 1) & 0xff);
	    else
	      snprintf(tb,sizeof(tb),"no request resend %s:%s #%d-#%d",
		       host_, port_, pn_1, (pn_now - 1) & 0xff);
            write_log(tb);
	  }
	}
      }
    }
  }
  if (pn != pn_f) {
    if (ht[i].nos[pn_f >> 3] & mask[pn_f & 0x07]) {
      /* if the resent packet is duplicated, return with -1 */
      if (!no_pinfo) {
	snprintf(tb, sizeof(tb), 
		 "discard duplicated resent packet #%d for #%d", pn, pn_f);
        write_log(tb);
      }
      return (-1);
    }
#if DEBUG4
    else if (!no_pinfo) {
      snprintf(tb, sizeof(tb), "received resent packet #%d for #%d", pn, pn_f);
      write_log(tb);
    }
#endif
  }
  return (0);
}

/* 64bit ok */
static size_t
wincpy2(uint8_w *ptw, time_t ts, uint8_w *ptr, ssize_t size, int mon,
	struct ch_hist *chhist, struct sockaddr *from_addr, socklen_t fromlen)
{
#define MAX_SR 500
#define MAX_SS 4
  /* #define SR_MON 5 */
  int ss;
  size_t n;
  WIN_sr sr;
  uint8_w *ptr_lim, *ptr1;
  WIN_ch ch;
  int i,k;
  int32_w aa, bb;  /* must be check later!! */
  /* uint32_w gh,gs; */
  uint32_w gs;
  char tb[256];
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port No. */

  if (getnameinfo(from_addr, fromlen,
		  host_, sizeof(host_), port_, sizeof(port_),
		  NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV))
    err_sys("getnameinfo");

  ptr_lim = ptr + size;
  n = 0;
  do {   /* loop for ch's */
    if (!mon) {
      gs = win_chheader_info(ptr, &ch, &sr, &ss);
      if (sr > MAX_SR || ss > MAX_SS || ptr + gs > ptr_lim) {
        if(!no_pinfo) {
	  if (strchr(host_, ':') != NULL)
	    snprintf(tb,sizeof(tb),
		     "ill ch hdr %02X%02X%02X%02X %02X%02X%02X%02X psiz=%zd sr=%d ss=%d gs=%u rest=%ld from [%s]:%s",
		     ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6],
		     ptr[7], size, sr, ss, gs, ptr_lim - ptr, host_, port_);
	  else
	    snprintf(tb,sizeof(tb),
		     "ill ch hdr %02X%02X%02X%02X %02X%02X%02X%02X psiz=%zd sr=%d ss=%d gs=%u rest=%ld from %s:%s",
		     ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6],
		     ptr[7], size, sr, ss, gs, ptr_lim - ptr, host_, port_);
	  write_log(tb);
	}
        return (n);
      }
    } else {  /* mon format */
      ch = mkuint2(ptr1 = ptr);
      ptr1 += 2;
      for (i = 0; i < SR_MON; i++) {
        aa = (*(ptr1++));
        bb = aa & 3;
        if(bb)
	  for (k = 0; k < bb*2; k++)
	    ptr1++;
        else
	  ptr1++;
      }
      gs = ptr1 - ptr;
      if (ptr + gs > ptr_lim) {
        if(!no_pinfo) {
	  if (strchr(host_, ':') != NULL)
	    snprintf(tb, sizeof(tb),
		     "ill ch blk %02X%02X%02X%02X %02X%02X%02X%02X psiz=%zd sr=%d gs=%u rest=%ld from [%s]:%s",
		     ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], 
		     ptr[7], size, sr, gs, ptr_lim - ptr, host_, port_);
	  else
	    snprintf(tb, sizeof(tb),
		     "ill ch blk %02X%02X%02X%02X %02X%02X%02X%02X psiz=%zd sr=%d gs=%u rest=%ld from %s:%s",
		     ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], 
		     ptr[7], size, sr, gs, ptr_lim - ptr, host_, port_);
          write_log(tb);
	}
        return (n);
      }
    }
    if (ch_table[ch] && ptr + gs <= ptr_lim) {
      for (i = 0;i < chhist->n; i++)
	if (chhist->ts[i][ch] == ts)
	  break;
      if (i == chhist->n) {  /* TS not found in last chhist->n packets */
        if (chhist->n > 0) {
          chhist->ts[chhist->p[ch]][ch] = ts;
          if (++chhist->p[ch] == chhist->n)
	    chhist->p[ch] = 0;
	}
#if DEBUG1
        fprintf(stderr, "%5d", gs);
#endif
        memcpy(ptw, ptr, gs);
        ptw += gs;
        n += gs;
      }
#if DEBUG1
      else
        fprintf(stderr,"%5d(!%04X)",gs,ch);
#endif
    }
    ptr += gs;
  } while (ptr < ptr_lim);

  return (n);
}

static void
send_req(int sock, struct sockaddr *host_addr, socklen_t host_addrlen)
/* struct sockaddr *host_addr;    sender address */
/* socklen_t host_addrlen;        size of struct sockaddr */
/* int sock;                      socket */
{
  int i, j;
  /*-
    send list of chs : 2B/ch,1024B/packet->512ch/packet max 128packets
    header: magic number,seq,n,list...
    if all channels, seq=n=0 and no list.
    -*/
  unsigned int seq, n_seq;
  struct {
    char mn[4];
    uint16_w seq[2];
    uint16_w chlist[512];
  } sendbuf;
#if DEBUG3
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port no. */
#endif
  
#if DEBUG3
  getnameinfo(host_addr, host_addrlen,
	      host_, sizeof(host_), port_, sizeof(port_),
	      NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
#endif

  strcpy(sendbuf.mn, "REQ");
  if (n_ch < WIN_CHMAX) {
    seq = 1;
    if (n_ch == 0)
      n_seq = 1;
    else
      n_seq = (n_ch - 1) / 512 + 1;
    sendbuf.seq[1] = MKSWAP16(n_seq);
    j = 0;
    for (i = 0; i < WIN_CHMAX; i++) {
      sendbuf.seq[0] = MKSWAP16(seq);
      if (ch_table[i])
	sendbuf.chlist[j++] = MKSWAP16(i);
      if (j == 512) {
        if (sendto(sock, &sendbuf, 8 + 2 * j, 0, host_addr, host_addrlen) < 0)
	  err_sys("sendto");
#if DEBUG3
        printf("send channel list to %s:%s (%d): %s %d/%d %04X %04X %04X ...\n",
	       host_, port_, n_ch, sendbuf.mn, MKSWAP16(sendbuf.seq[0]),
	       MKSWAP16(sendbuf.seq[1]), MKSWAP16(sendbuf.chlist[0]),
	       MKSWAP16(sendbuf.chlist[1]), MKSWAP16(sendbuf.chlist[2]));
#endif
        j = 0;
        seq++;
      }
    }
    if (j > 0) {
      if (sendto(sock, &sendbuf, 8 + 2 * j, 0, host_addr, host_addrlen) < 0)
	err_sys("sendto");
#if DEBUG3
      printf("send channel list to %s:%s (%d): %s %d/%d %04X %04X %04X ...\n",
	     host_, port_, n_ch, sendbuf.mn, MKSWAP16(sendbuf.seq[0]),
	     MKSWAP16(sendbuf.seq[1]), MKSWAP16(sendbuf.chlist[0]),
	     MKSWAP16(sendbuf.chlist[1]), MKSWAP16(sendbuf.chlist[2]));
#endif
      seq++;
    }
  }
  else { /* all channels */
    sendbuf.seq[0] = sendbuf.seq[1] = 0;
    if (sendto(sock, &sendbuf, 8, 0, host_addr, host_addrlen) < 0)
      err_sys("sendto");
#if DEBUG3
    printf("send channel list to %s:%s (%d): %s %d/%d\n",
	   host_,port_, n_ch, sendbuf.mn,
	   MKSWAP16(sendbuf.seq[0]), MKSWAP16(sendbuf.seq[1]));
#endif
  }
}

static void
usage(void)
{
  
  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if (daemon_mode)
    fprintf(stderr,
	    " usage : '%s (-46AaBenMNr) (-d [len(s)]) (-m [pre(m)]) (-p [post(m)]) \\\n\
              (-i [interface]) (-g [mcast_group]) (-s sbuf(KB)) \\\n\
              (-o [src_host]:[src_port]) (-f [ch file]) \\\n\
              [port] [shm_key] [shm_size(KB)] ([ctl file]/- ([log file]))'\n",
	    progname);
  else
    fprintf(stderr,
	    " usage : '%s (-46AaBDenMNr) (-d [len(s)]) (-m [pre(m)]) (-p [post(m)]) \\\n\
              (-i [interface]) (-g [mcast_group]) (-s sbuf(KB)) \\\n\
              (-o [src_host]:[src_port]) (-f [ch file]) (-y [req_delay])\\\n\
              [port] [shm_key] [shm_size(KB)] ([ctl file]/- ([log file]))'\n",
	    progname);
}

int
main(int argc, char *argv[])
{
  key_t shm_key;  /*- 64bit ok -*/
  struct conntable  *ct, *ct_top = NULL;
  char  *input_port, *host_port;
  int  maxsoc;
  struct sockaddr_storage  ss, ss1;
  struct sockaddr *from_addr = (struct sockaddr *)&ss;
  struct sockaddr *host_addr = (struct sockaddr *)&ss1;
  socklen_t  host_addrlen;
  struct addrinfo hints, *res, *ai;
  int  gai_error;
  int  sock, sock_tmp;
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port no. */
  /* int shmid; */      /*- 64bit ok -*/
  uint32_w uni;  /*- 64bit ok -*/
  WIN_bs  uni2;  /*- 64bit ok -*/
  uint8_w *ptr,tm[6],*ptr_size,*ptr_size2;  /*- 64bit ok -*/
  char *host_name;  /*- 64bit ok -*/
  int i,j,all,c,mon,eobsize,      /*- 64bit ok -*/
    sbuf,noreq,no_ts,no_pno,req_delay;   /*- 64bit ok -*/
  socklen_t fromlen;  /*- 64bit ok -*/
  size_t size,pl,nn;     /*- 64bit ok -*/
  ssize_t n,nlen;  /*- 64bit ok -*/
  time_t pre,post;    /*- 64bit ok -*/
  /* struct sockaddr_in from_addr,host_addr; */  /*- 64bit ok -*/
  /* uint16_t  host_port;  */ /*- 64bit ok -*/
  struct Shm  *sh;
  char tb[256];
  /* struct ip_mreq stMreq; */  /*- 64bit ok -*/
  char mcastgroup[256]; /* multicast address */
  char interface[256]; /* network interface */
  time_t ts,sec,sec_p;  /*- 64bit ok -*/
  static struct ch_hist  chhist;
  /* struct hostent *h; */
  struct timeval timeout;
  fd_set  rset;

  if ((progname = strrchr(argv[0],'/')) != NULL)
    progname++;
  else
    progname = argv[0];

  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;
  family = AF_UNSPEC;

  if (strcmp(progname,"recvt46d") == 0)
    daemon_mode = 1;
  else if (strcmp(progname,"recvt4") == 0)
    family = AF_INET;
  else if (strcmp(progname,"recvt4d") == 0) {
    family = AF_INET;
    daemon_mode = 1;
  }
  else if (strcmp(progname,"recvt6") == 0)
    family = AF_INET6;
  else if (strcmp(progname,"recvt6d") == 0) {
    family = AF_INET6;
    daemon_mode = 1;
  }
    
  all = no_pinfo = mon = eobsize = noreq = no_ts = no_pno = 0;
  pre = post = 0;
  auto_reload_chfile = 0;
  *interface = (*mcastgroup) = 0;
  host_name = NULL;
  sbuf = DEFAULT_RCVBUF;
  chhist.n = N_HIST;
  n_chfile = 1;
  req_delay = 0;
  for (i = 0; i < N_HOST; i++)
    ht[i].host[0] = '\0';

  while ((c = getopt(argc, argv, "46AaBDd:ef:g:i:m:MNno:p:rs:y:")) != -1) {
    switch(c) {
    case '4':   /* IPv4 only */
      family = AF_INET;
      break;
    case '6':   /* IPv6 only */
      family = AF_INET6;
      break;
    case 'A':   /* don't check time stamps */
      no_ts = 1;
      break;
    case 'a':   /* accept >=A1 packets */
      all = 1;
      break;
    case 'B':   /* write blksize at EOB for backward search */
      eobsize = 1;
      break;
    case 'D':
      daemon_mode = 1;  /* daemon mode */
      break;   
    case 'd':   /* length of packet history in sec */
      chhist.n = atoi(optarg);
      break;
    case 'e':  /* automatically reload chfile if packet comes from denyhost */
      auto_reload_chfile = 1;
      break;
    case 'f':   /* channel list file */
      if (n_chfile < N_CHFILE)
	chfile[n_chfile++] = optarg;
      else
	fprintf(stderr,
		"Num exceeded. Ignore channel list file: %s\n", optarg);
      break;
    case 'g':   /* multicast group (multicast IP address) */
      if (snprintf(mcastgroup, sizeof(mcastgroup), "%s", optarg)
	  >= sizeof(mcastgroup)) {
	fprintf(stderr,"'%s': -g option : Buffer overrun!\n",progname);
	exit(1);
      }
      break;
    case 'i':   /* interface (ordinary IP address) for receive */
      if (snprintf(interface, sizeof(interface), "%s", optarg)
	  >= sizeof(interface)) {
	fprintf(stderr,"'%s': -i option : Buffer overrun!\n",progname);
	exit(1);
      }
      break;
    case 'm':   /* time limit before RT in minutes */
      pre = atol(optarg);
      if (pre < 0)
	pre = (-pre);
      break;
    case 'M':   /* 'mon' format data */
      mon = 1;
      break;
    case 'N':   /* no packet nos */
      no_pno = no_ts = 1;
      break;
    case 'n':   /* supress info on abnormal packets */
      no_pinfo = 1;
      break;
    case 'o':   /* host and port for request */
      if (split_host_port(optarg, &host_name, &host_port)) {
	fprintf(stderr," Invalid hostname !\n");
	usage();
	exit(1);
      }
      if (host_port == NULL) {
	fprintf(stderr," option -o requires '[host]:[port]' pair !\n");
	usage();
	exit(1);
      }
      break;
    case 'p':   /* time limit after RT in minutes */
      post = atol(optarg);
      break;
    case 'r':   /* disable resend request */
      noreq = 1;
      break;
    case 's':   /* preferred socket buffer size (KB) */
      sbuf = atoi(optarg);
      break;
    case 'y':   /* packet count for delayed resend-request */
      req_delay = atoi(optarg);
      if (req_delay > N_PNOS-2 || req_delay < 0) {
	fprintf(stderr," resend-request delay < %d !\n", N_PNOS - 1);
	usage();
	exit(1);
      }
      break;
    default:
      fprintf(stderr," option -%c unknown\n", c);
      usage();
      exit(1);
    }
  }
  optind--;
  if (argc < 4 + optind) {
    usage();
    exit(1);
  }
  pre = (-pre * 60);
  post *= 60;
  /* to_port=(uint16_t)atoi(argv[1+optind]); */
  input_port = argv[1 + optind];
  shm_key = atol(argv[2 + optind]);
  size = (size_t)atol(argv[3 + optind]) * 1000;
  chfile[0] = NULL;
  if (argc > 4 + optind) {
    if (strcmp("-", argv[4 + optind]) == 0)
      chfile[0] = NULL;
    else {
      if (argv[4 + optind][0] == '-') {
        chfile[0] = argv[4 + optind] + 1;
        negate_channel = 1;
      } else {
        chfile[0] = argv[4 + optind];
        negate_channel = 0;
      }
    }
  }
  if (n_chfile == 1 && (chfile[0]) == NULL)
    n_chfile = 0;

  if (argc > 5 + optind)
    logfile = argv[5 + optind];
  else {
    logfile = NULL;
    if (daemon_mode)
      syslog_mode = 1;
  }
  
  if((chhist.ts =
      (time_t (*)[WIN_CHMAX])win_xmalloc(WIN_CHMAX * chhist.n * sizeof(time_t))) == NULL) {
    chhist.n = N_HIST;
    if((chhist.ts =
        (time_t (*)[WIN_CHMAX])win_xmalloc(WIN_CHMAX * chhist.n * sizeof(time_t))) == NULL) {
      fprintf(stderr, "malloc failed (chhist.ts)\n");
      exit(1);
    }
    /* Later, insert some warning messages into here. */
  }

  /* daemon mode */
  if (daemon_mode) {
    daemon_init(progname, LOG_USER, syslog_mode);
    umask(022);
  }
  
  snprintf(tb, sizeof(tb),
	   "n_hist=%d size=%zd req_delay=%d auto_reload_chfile=%d",
	   chhist.n, WIN_CHMAX * chhist.n * sizeof(time_t), req_delay,
	   auto_reload_chfile);
  write_log(tb);

  /* shared memory */
  write_log("start");
  sh = Shm_create(shm_key, size, "in");

  /* initialize buffer */
  Shm_init(sh, size);
  pl = sh->pl;

  if (all)
    write_log("accept >=A1 packets");
  snprintf(tb, sizeof(tb), "TS window %lds - +%lds", pre, post);
  write_log(tb);

  /* 'in' port of localhost */
  if (*mcastgroup)
      ct_top = udp_accept(input_port, &maxsoc, sbuf, family, NULL);
  else {
    if (*interface)
      ct_top = udp_accept(input_port, &maxsoc, sbuf, family, interface);
    else
      ct_top = udp_accept(input_port, &maxsoc, sbuf, family, NULL);
  }
  if (ct_top == NULL)
    err_sys("udp_accept");
  maxsoc++;

  if (*mcastgroup) {
    for (ct = ct_top; ct != NULL; ct = ct->next) {
      if (*interface)
	mcast_join(ct->soc, mcastgroup, interface);
      else
	mcast_join(ct->soc, mcastgroup, NULL);
    }
  }

  if(host_name != NULL) /* host_name and host_port specified */
    {
    /* source host/port */
/*     if(!(h=gethostbyname(host_name))) err_sys("can't find host"); */
/*     memset((char *)&host_addr,0,sizeof(host_addr)); */
/*     host_addr.sin_family=AF_INET; */
/*     memcpy((caddr_t)&host_addr.sin_addr,h->h_addr,h->h_length); */
/*     host_addr.sin_port=htons(host_port); */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    if ((gai_error = getaddrinfo(host_name, host_port, &hints, &res)) != 0) {
      (void)snprintf(tb, sizeof(tb), 
		     "getaddrinfo : %s", gai_strerror(gai_error));
      err_sys(tb);
    }
    for (ai = res; ai != NULL; ai = ai->ai_next) {
      sock_tmp = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (sock_tmp < 0)
	continue;
      
      /* success */
      (void)close(sock_tmp);
      break;
    }
    if (ai == NULL) {
      if (strchr(host_name, ':') != NULL)
	(void)snprintf(tb, sizeof(tb), "%s: [%s]:%s",
		       strerror(errno), host_name, host_port);
      else
	(void)snprintf(tb, sizeof(tb), "%s: %s:%s",
		       strerror(errno), host_name, host_port);
      err_sys(tb);
    }
    /* search same family */
    for (ct = ct_top; ct != NULL; ct = ct->next) {
      /* printf("sock=%d: %d %d\n",  */
      /* 	     ct->soc, sockfd_to_family(ct->soc) ,ai->ai_addr->sa_family); */
      if (sockfd_to_family(ct->soc) == ai->ai_addr->sa_family)
	break;
    }
    if (ct == NULL)
      err_sys("host_addr : no match family");
    sock = ct->soc;
    memcpy(host_addr, ai->ai_addr, ai->ai_addrlen);
    host_addrlen = ai->ai_addrlen;

    if (res != NULL)
      freeaddrinfo(res);

    for (j = sbuf; j >= 16; j -= 4) {
      i = j * 1024;
      if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &i, sizeof(i)) >= 0)
        break;
    }
    if (j < 16)
      err_sys("SO_SNDBUF setsockopt error\n");
    snprintf(tb, sizeof(tb), "SNDBUF size=%d", j * 1024);
    write_log(tb);
    }  /* if (*hostname) */

  if (noreq)
    write_log("resend request disabled");
  if (no_ts)
    write_log("time-stamps not interpreted");
  if (no_pno)
    write_log("packet numbers not interpreted");
  if (host_name != NULL) {
    gai_error = getnameinfo(host_addr, host_addrlen, host_, sizeof(host_),
			    port_, sizeof(port_),
			    NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
    if (gai_error) {
      (void)snprintf(tb, sizeof(tb), 
		     "getnameinfo : %s", gai_strerror(gai_error));
      err_sys(tb);
    } else {
      if (strchr(host_, ':') != NULL)
	(void)snprintf(tb, sizeof(tb),
		       "send channel list to [%s]:%s", host_, port_);
      else
	(void)snprintf(tb, sizeof(tb),
		       "send channel list to %s:%s", host_, port_);
      write_log(tb);
    }
  }

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  for (i = 0; i < 6; i++)
    tm[i] = (-1);
  ptr = ptr_size = sh->d + sh->p;
  read_chfile();
  time(&sec);
  sec_p = sec-1;

  FD_ZERO(&rset);

  /*** main loop ***/
  for (;;) {
    if(host_name != NULL) { /* send request */
      time(&sec);
      if (sec != sec_p) {
	send_req(sock, host_addr, host_addrlen);
        sec_p = sec;
      }
    }

    for (ct = ct_top; ct != NULL; ct = ct->next)
      FD_SET(ct->soc, &rset);
    /* k = 1 << sock; */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    if (select(maxsoc, &rset, NULL, NULL, &timeout) <= 0)
      continue;

    /* read data */
    for (ct = ct_top; ct != NULL; ct = ct->next) {
      if (!FD_ISSET(ct->soc, &rset))
	continue;

      /* fromlen=sizeof(from_addr); */
      fromlen = sizeof(ss);
      n = recvfrom(ct->soc, rbuf, MAXMESG, 0, from_addr, &fromlen);
      getnameinfo(from_addr, fromlen, host_, sizeof(host_), port_,
		  sizeof(port_), NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
#if DEBUG0
      if (rbuf[0] == rbuf[1])
	printf("%d ",rbuf[0]);
      else
	printf("%d(%d) ",rbuf[0], rbuf[1]);
      for (i = 0; i < 16; i++)
	printf("%02X",rbuf[i]);
      printf(" (%d)\n",n);
#endif
      
      if (no_ts) { /* no time stamp */
	ptr += 4;   /* size */
	ptr += 4;   /* time of write */
	if (no_pno)
	  memcpy(ptr, rbuf, nn = n);
	else {
	  if (check_pno(from_addr, rbuf[0], rbuf[1], ct->soc, fromlen,
			n, noreq, req_delay) < 0)
	    continue;
	  memcpy(ptr, rbuf + 2, nn = n - 2);
	}
	ptr += nn;
	uni = (uint32_w)(time(NULL) - TIME_OFFSET);
	ptr_size[4] = uni >> 24;  /* tow (H) */
	ptr_size[5] = uni >> 16;
	ptr_size[6] = uni >> 8;
	ptr_size[7] = uni;      /* tow (L) */
	ptr_size2 = ptr;
	if (eobsize)
	  ptr += 4; /* size(2) */
	uni2 = (WIN_bs)(ptr - ptr_size);
	ptr_size[0] = uni2 >> 24;  /* size (H) */
	ptr_size[1] = uni2 >> 16;
	ptr_size[2] = uni2 >> 8;
	ptr_size[3] = uni2;      /* size (L) */
	if (eobsize) {
	  ptr_size2[0] = ptr_size[0];  /* size (H) */
	  ptr_size2[1] = ptr_size[1];
	  ptr_size2[2] = ptr_size[2];
	  ptr_size2[3] = ptr_size[3];  /* size (L) */
	}
	sh->r = sh->p;      /* latest */
	if (eobsize && ptr > sh->d + pl) {
	  sh->pl = ptr - sh->d -4;
	  ptr = sh->d;
	}
	if (!eobsize && ptr > sh->d + sh->pl)
	  ptr = sh->d;
	sh->p = ptr - sh->d;
	sh->c++;
	ptr_size = ptr;
#if DEBUG1
	/* getnameinfo(from_addr, fromlen, host_, sizeof(host_), port_, */
/* 		    sizeof(port_), NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV); */
	printf("%s:%s(%d)>",host_,port_,n);
	printf("%d(%d) ",rbuf[0],rbuf[1]);
	rbuf[n]=0;
	printf("%s\n",rbuf);
#endif
	continue;
      }
      
      if (check_pno(from_addr, rbuf[0], rbuf[1], ct->soc, fromlen,
		    n, noreq, req_delay) < 0)
	continue;
      
      /* check packet ID */
      if (rbuf[2] < 0xA0) {
	for (i = 0; i < 6; i++)
	  if (rbuf[i + 2] != tm[i]) 
	    break;
	if (i == 6) { /* same time */
	  nn = wincpy2(ptr, ts, rbuf + 8, n - 8,
		       mon, &chhist, from_addr,fromlen);
	  ptr += nn;
	  uni = (uint32_w)(time(NULL) - TIME_OFFSET);
	  ptr_size[4] = uni >> 24;  /* tow (H) */
	  ptr_size[5] = uni >> 16;
	  ptr_size[6] = uni >> 8;
	  ptr_size[7] = uni;      /* tow (L) */
	} else {
	  if ((ptr - ptr_size) > 14) {
	    ptr_size2 = ptr;
	    if (eobsize)
	      ptr += 4; /* size(2) */
	    uni2 = (WIN_bs)(ptr - ptr_size);
	    ptr_size[0] = uni2 >> 24;  /* size (H) */
	    ptr_size[1] = uni2 >> 16;
	    ptr_size[2] = uni2 >> 8;
	    ptr_size[3] = uni2;      /* size (L) */
	    if (eobsize) {
	      ptr_size2[0] = ptr_size[0];  /* size (H) */
	      ptr_size2[1] = ptr_size[1];
	      ptr_size2[2] = ptr_size[2];
	      ptr_size2[3] = ptr_size[3];  /* size (L) */
            }
#if DEBUG2
	    printf("%d - %d (%d) %lu / %lu\n",ptr_size-sh->d,uni2,ptr_size2-sh->d,
		   sh->pl,pl);
#endif
#if DEBUG1
	    printf("(%d)",time(NULL));
	    for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
	    printf(" : %d > %d\n",uni2,ptr_size-sh->d);
#endif
	  } else
	    ptr = ptr_size;
	  if (!(ts = check_ts(rbuf + 2, pre, post))) {
	    if (!no_pinfo) {
	      if (strchr(host_, ':') != NULL)
		snprintf(tb, sizeof(tb),
			 "ill time %02X:%02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from [%s]:%s",
			 rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5],
			 rbuf[6], rbuf[7], rbuf[8], rbuf[9], rbuf[10], rbuf[11],
			 rbuf[12], rbuf[13], rbuf[14], rbuf[15], host_, port_);
	      else
		snprintf(tb, sizeof(tb),
			 "ill time %02X:%02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%s",
			 rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5],
			 rbuf[6], rbuf[7], rbuf[8], rbuf[9], rbuf[10], rbuf[11],
			 rbuf[12], rbuf[13], rbuf[14], rbuf[15], host_, port_);
	      write_log(tb);
	    }
	    for (i = 0; i < 6; i++)
	      tm[i] = (-1);
	    continue;
	  } else {
	    sh->r = sh->p;      /* latest */
	    if (eobsize && ptr > sh->d + pl) {
	      sh->pl = ptr - sh->d - 4;
	      ptr = sh->d;
	    }
	    if (!eobsize && ptr > sh->d + sh->pl)
	      ptr = sh->d;
	    sh->p = ptr - sh->d;
	    sh->c++;
	    ptr_size = ptr;
	    ptr += 4;   /* size */
	    ptr += 4;   /* time of write */
	    memcpy(ptr, rbuf + 2, 6);
	    ptr += 6;
	    nn = wincpy2(ptr, ts, rbuf + 8, n - 8,
			 mon, &chhist, from_addr, fromlen);
	    ptr += nn;
	    memcpy(tm, rbuf + 2, 6);
	    uni = (uint32_w)(time(NULL) - TIME_OFFSET);
	    ptr_size[4] = uni >> 24;  /* tow (H) */
	    ptr_size[5] = uni >> 16;
	    ptr_size[6] = uni >> 8;
	    ptr_size[7] = uni;      /* tow (L) */
	  }
	}
      } else if (rbuf[2] == 0xA0) { /* merged packet */
	nlen = n - 3;
	j = 3;
	while (nlen > 0) {
	  n = (rbuf[j] << 8) + rbuf[j + 1];
	  if (n < 8 || n > nlen) {
	    if (!no_pinfo) {
	      if (strchr(host_, ':') != NULL)
		snprintf(tb, sizeof(tb),
			 "ill blk n=%zd(%02X%02X) %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from [%s]:%s #%d(#%d) at %d",
			 n, rbuf[j], rbuf[j+1], rbuf[j+2], rbuf[j+3], rbuf[j+4],
			 rbuf[j+5], rbuf[j+6], rbuf[j+7], rbuf[j+8], rbuf[j+9],
			 rbuf[j+10], rbuf[j+11], rbuf[j+12], rbuf[j+13],
			 rbuf[j+14], rbuf[j+15],
			 host_, port_, rbuf[0], rbuf[1], j);
	      else
		snprintf(tb, sizeof(tb),
			 "ill blk n=%zd(%02X%02X) %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%s #%d(#%d) at %d",
			 n, rbuf[j], rbuf[j+1], rbuf[j+2], rbuf[j+3], rbuf[j+4],
			 rbuf[j+5], rbuf[j+6], rbuf[j+7], rbuf[j+8], rbuf[j+9],
			 rbuf[j+10], rbuf[j+11], rbuf[j+12], rbuf[j+13],
			 rbuf[j+14], rbuf[j+15],
			 host_, port_, rbuf[0], rbuf[1], j);
	      write_log(tb);
	    }
	    for (i = 0; i < 6; i++) 
	      tm[i] = (-1);
	    break;
	  }
	  j += 2;
	  for (i = 0; i < 6; i++)
	    if (rbuf[j + i] != tm[i])
	      break;
	  if (i == 6) { /* same time */
	    nn = wincpy2(ptr, ts, rbuf + j + 6, n - 8,
			 mon, &chhist, from_addr, fromlen);
	    ptr += nn;
	    uni = (uint32_w)(time(NULL) - TIME_OFFSET);
	    ptr_size[4] = uni >> 24;  /* tow (H) */
	    ptr_size[5] = uni >> 16;
	    ptr_size[6] = uni >> 8;
	    ptr_size[7] = uni;      /* tow (L) */
	  } else {
	    if ((ptr - ptr_size) > 14) { /* data copied - close the sec block */
	      ptr_size2 = ptr;
	      if (eobsize)
		ptr += 4; /* size(2) */
	      uni2 = (WIN_bs)(ptr - ptr_size);
	      ptr_size[0] = uni2 >> 24;  /* size (H) */
	      ptr_size[1] = uni2 >> 16;
	      ptr_size[2] = uni2 >> 8;
	      ptr_size[3] = uni2;      /* size (L) */
	      if (eobsize) {
		ptr_size2[0] = ptr_size[0];  /* size (H) */
		ptr_size2[1] = ptr_size[1];
		ptr_size2[2] = ptr_size[2];
		ptr_size2[3] = ptr_size[3];  /* size (L) */
	      }
#if DEBUG2
	      printf("%d - %d (%d) %lu / %lu\n",
		     ptr_size-sh->d,uni2,ptr_size2-sh->d, sh->pl,pl);
#endif
#if DEBUG1
	      printf("(%d)",time(NULL));
	      for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
	      printf(" : %d > %d\n",uni2,ptr_size-sh->d);
#endif
	      sh->r = sh->p;      /* latest */
	      if (eobsize && ptr > sh->d + pl) {
		sh->pl = ptr - sh->d - 4;
		ptr = sh->d;
	      }
	      if (!eobsize && ptr > sh->d + sh->pl)
		ptr = sh->d;
	      sh->p = ptr - sh->d;
	      sh->c++;
	      ptr_size = ptr;
	    }
	    else /* sec block empty - reuse the space */
	      ptr = ptr_size;
	    if (!(ts = check_ts(rbuf + j, pre, post))) { /* illegal time */
	      if (!no_pinfo) {
		if (strchr(host_, ':') != NULL)
		  snprintf(tb, sizeof(tb),
			   "ill time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from [%s]:%s",
			   rbuf[j], rbuf[j+1], rbuf[j+2], rbuf[j+3], rbuf[j+4],
			   rbuf[j+5], rbuf[j+6], rbuf[j+7], rbuf[j+8], rbuf[j+9],
			   rbuf[j+10], rbuf[j+11], rbuf[j+12], rbuf[j+13],
			   host_, port_);
		else
		  snprintf(tb, sizeof(tb),
			   "ill time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%s",
			   rbuf[j], rbuf[j+1], rbuf[j+2], rbuf[j+3], rbuf[j+4],
			   rbuf[j+5], rbuf[j+6], rbuf[j+7], rbuf[j+8], rbuf[j+9],
			   rbuf[j+10], rbuf[j+11], rbuf[j+12], rbuf[j+13],
			   host_, port_);
		write_log(tb);
	      }
	      for (i = 0; i < 6; i++)
		tm[i] = (-1);
	    } else { /* valid time stamp */
	      ptr += 4;   /* size */
	      ptr += 4;   /* time of write */
	      memcpy(ptr, rbuf + j, 6);
	      ptr += 6;
	      nn = wincpy2(ptr, ts, rbuf + j + 6, n - 8,
			   mon, &chhist, from_addr, fromlen);
	      ptr += nn;
	      memcpy(tm, rbuf + j, 6);
	      uni = (uint32_w)(time(NULL) - TIME_OFFSET);
	      ptr_size[4] = uni >> 24;  /* tow (H) */
	      ptr_size[5] = uni >> 16;
	      ptr_size[6] = uni >> 8;
	      ptr_size[7] = uni;      /* tow (L) */
	    }
	  }
	  nlen -= n;
	  j += n - 2;
	}
      } else if (all) { /* rbuf[2]>=0xA1 with packet ID */
	if (!(ts = check_ts(rbuf + 3, pre, post))) {
	  if (!no_pinfo) {
	    if (strchr(host_, ':') != NULL)
	      snprintf(tb, sizeof(tb),
		       "ill time %02X:%02X %02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from [%s]:%s",
		       rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5],
		       rbuf[6], rbuf[7], rbuf[8], rbuf[9], rbuf[10], rbuf[11],
		       rbuf[12], rbuf[13], rbuf[14], rbuf[15], rbuf[16],
		       host_, port_);
	    else
	      snprintf(tb, sizeof(tb),
		       "ill time %02X:%02X %02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%s",
		       rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5],
		       rbuf[6], rbuf[7], rbuf[8], rbuf[9], rbuf[10], rbuf[11],
		       rbuf[12], rbuf[13], rbuf[14], rbuf[15], rbuf[16],
		       host_, port_);
	    write_log(tb);
	  }
	  for (i = 0; i < 6; i++)
	    tm[i] = (-1);
	  continue;
	} else {
	  ptr_size = ptr;
	  ptr += 4;   /* size */
	  ptr += 4;   /* time of write */
	  memcpy(ptr, rbuf + 2, n - 2);
	  ptr += n - 2;
	  ptr_size2 = ptr;
	  if (eobsize)
	    ptr += 4; /* size(2) */
	  uni2 = (WIN_bs)(ptr - ptr_size);
	  ptr_size[0] = uni2 >> 24;  /* size (H) */
	  ptr_size[1] = uni2 >> 16;
	  ptr_size[2] = uni2 >> 8;
	  ptr_size[3] = uni2;      /* size (L) */
	  if (eobsize) {
	    ptr_size2[0] = ptr_size[0];  /* size (H) */
	    ptr_size2[1] = ptr_size[1];
	    ptr_size2[2] = ptr_size[2];
	    ptr_size2[3] = ptr_size[3];  /* size (L) */
	  }
	  memcpy(tm, rbuf + 3, 6);
	  uni = (uint32_w)(time(NULL) - TIME_OFFSET);
	  ptr_size[4] = uni >> 24;  /* tow (H) */
	  ptr_size[5] = uni >> 16;
	  ptr_size[6] = uni >> 8;
	  ptr_size[7] = uni;      /* tow (L) */
	  
	  sh->r = sh->p;      /* latest */
	  if (eobsize && ptr > sh->d + pl) {
	    sh->pl = ptr - sh->d - 4;
	    ptr = sh->d;
	  }
	  if (!eobsize && ptr > sh->d+ sh->pl)
	    ptr = sh->d;
	  sh->p = ptr - sh->d;
	  sh->c++;
	}
      }
#if BELL
      fprintf(stderr,"\007");
      fflush(stderr);
#endif
    }  /* for (ct = ct_top; ct != NULL; ct = ct->next) */
  }  /* for (;;) **** main loop *** */
}
