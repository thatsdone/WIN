/* $Id: ecore2.c,v 1.6 2016/01/05 06:38:47 uehira Exp $ */

/*
  program "ecore2.c"   4/16/93-5/13/93,7/2/93,7/5/94  urabe
                      1/6/95 bug in adj_time fixed (tm[0]--)
                      3/17/95 write_log()
                      7/23/01 4/12/02(Hinet) K.Takano
			Endian free
			NCH(Max output channel) 1500 -> 3000 -> 5000(Hinet)
			usleep(800000) -> sleep(1)
			SIGHUP -> read_chtbl()
			abnormal exit(0) -> exit(1)
			Bug: Ch file was used for Logfile if arg>5. fixed.
*/

/*                      2003/4/16  configure for any machine N.Nakawaji  */
/*                      2003/6/10  '#include <errno.h>' deleted by Urabe */
/*                      2003/7/2   introduced strerror() by Urabe/Nakagawa */
/*                      2010/10/11  64bit check (Uehira) */

/*--- for config   2003/05/02  by N.Nakawaji ---*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

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

/*--------------------------------------------*/

#include <memory.h>
#include <ctype.h>

#if HAVE_DIRENT_H
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

/* following parameters give M=3 (6-th order) */
#define	FP			6.0
#define	FS			9.0
#define	AP			0.5
#define	AS			5.0

#define	SR		20		/* sampling rate of output data */
#define	CHMASK		0xffff	/* max of name channels */
#define	NCH		5000	/* max of output channels */
#define	MAX_FILT	25		/* max of filter order */
#define	NUMLEN		256

static const char rcsid[] = 
  "$Id: ecore2.c,v 1.6 2016/01/05 06:38:47 uehira Exp $";

char	*progname, *logfile;
int     syslog_mode = 0, exit_status;

static char	Tbuf[512];
static int	Pos_table[CHMASK + 1];
struct {
  int		before_sr;
  int		sys_ch;
  int		flag_filt;
  int		m_filt;
  int		n_filt;
  double	gn_filt;
  double	coef[MAX_FILT*4];
  double	uv[MAX_FILT*4];
} static Ch[NCH];
static char	*file_list, textbuf[NUMLEN];

/* prototypes */
void read_chtbl(void);
int main(int, char *[]);

void
read_chtbl(void)
{
  FILE	*fp;
  int	i,nch;

  if ((fp = fopen(file_list, "r")) == NULL) {
    snprintf(Tbuf, sizeof(Tbuf),
	     "channel table file '%s' not open", file_list);
    write_log(Tbuf);
    exit(1);
  }

  /* table initialize */
  for (i = 0; i < CHMASK + 1; i++)
    Pos_table[i] = -1;
  nch = 0;
  if (fp != NULL) {
    while (fgets(textbuf, NUMLEN, fp)) {
      if (*textbuf == '#')
	continue;
      sscanf(textbuf, "%x", &i);
      i &= CHMASK;
      Ch[nch].sys_ch = i;
      Ch[nch].flag_filt = 0;
      Pos_table[i] = nch;
      if (++nch == NCH) {
	snprintf(Tbuf, sizeof(Tbuf),
		 "WARNING : N of channels exceed table limit = %d at CH=%04X",
		 NCH, i);
	write_log(Tbuf);
	break;
      }
    }
  }
  fclose(fp);
  snprintf(Tbuf, sizeof(Tbuf), "N of channels = %d", nch);
  write_log(Tbuf);
  
  signal(SIGHUP, (void *)read_chtbl);
}

int
main(int argc, char *argv[])
{
  double	dbuf[512], dt;
  int	j, pos;
  WIN_sr  sr;
  int32_w	dl;
  int32_w lbuf[512], work[512];
  unsigned long	 counter_save_in;  /* 64bit ok */
  size_t shp_in, shp_in_save, shm_size, done_size, write_point;
  WIN_bs  ch_size;
  uint32_w  size_in;
  uint8_w	*ptr, *ptr_save, *ptw,
    out_data[SR * NCH], out_buf[100], cmp_tbl[20];
  key_t	shmkey_in, shmkey_out;
  struct Shm  *shm_in, *shm_out;
  WIN_ch  ch_no;
  uint32_w  gsize;
  int ss_mode = SSIZE5_MODE, ssf_flag = 0;
  /* struct Ch_header { */
  /*   unsigned short	ch_no; */
  /*   unsigned short	s_size_rate; */
  /* } ch_header; */
  
  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  exit_status = EXIT_SUCCESS;

  if (argc < 5) {
    WIN_version();
    fprintf(stderr, "%s\n", rcsid);
    fprintf(stderr,
	    " usage : '%s [shm_key_in] [shm_key_out] [shm_size(KB)] [chlist] ([log file])'\n",
	    progname);
    exit(1);
  }

  if (argc > 5)
    logfile = argv[5];
  else
    logfile = NULL;

  shmkey_in = atol(argv[1]);
  shmkey_out = atol(argv[2]);
  shm_size = (size_t)atol(argv[3]) * 1000;

  /* read channel list */
  file_list = argv[4];
  read_chtbl();

  /* shered memory */
  shm_in = Shm_read(shmkey_in, "in");
  /* if ((shmid_in = shmget(shmkey_in, 0, 0)) < 0) */
  /* 	err_sys("shmget_in"); */
  /* if ((shm_in = (struct Shm *)shmat(shmid_in, (void *)0, SHM_RDONLY)) == */
  /* 	(struct Shm *)-1) */
  /* 	err_sys("shmat_in"); */
  
  /* sprintf(Tbuf, "in : shmkey_in=%d shmid_in=%d", shmkey_in, shmid_in); */
  /* write_log(Tbuf); */
  
  shm_out = Shm_create(shmkey_out, shm_size, "out");
  /* if ((shmid_out = shmget(shmkey_out, shm_size, IPC_CREAT|0666)) < 0) */
  /* 	err_sys("shmget_out"); */
  /* if ((shm_out = (struct Shm *)shmat(shmid_out, (void *)0, 0)) == */
  /* 	(struct Shm *)-1) */
  /* 	err_sys("shmat_out"); */
  
  /* sprintf(Tbuf, "out : shmkey_out=%d shmid_out=%d shm_size=%d", */
  /* 	shmkey_out, shmid_out, shm_size); */
  /* write_log(Tbuf); */
  
  /* initialize output buffer */
  Shm_init(shm_out, shm_size);
  /* 	shm_out->p = shm_out->c = 0; */
  /* 	shm_out->pl = (shm_size - sizeof(*shm_out)) / 10 * 9; */
  /* 	shm_out->r = -1; */
  
  ptw = shm_out->d;

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

 reset:
  while (shm_in->r == (-1))
    sleep(1);
  shp_in = shp_in_save = shm_in->r;	/* read position */
  
  for (;;) {
    ptr = ptr_save = shm_in->d + shp_in;
    size_in = mkuint4(ptr_save);
    memcpy(cmp_tbl, ptr_save, 10);
    counter_save_in = shm_in->c;
    done_size = 0;
    /* フイルタリング手続き */
    ptr += (4 + 6);	/* ブロックサイズ+秒ブロックシフト */
    done_size = 4 + 6;	/* ブロックサイズ+秒ブロック */
    write_point = 0;

    for (;;) {		/* 1秒分のチャネル処理ループ */
      if (done_size >= size_in) {
	/* 秒ブロックの終了 shm_outへ書き出し */
	if (memcmp(cmp_tbl, ptr_save, 10)) {
	  write_log("ERROR : ptr_save modified. reset");
	  goto reset;
	}
	/* 先頭のブロックサイズと時刻ヘッダ
	   時刻ヘッダはコピー */
	memcpy(ptw, ptr_save, 10);
	ptw[0]= ( (write_point +10) >>24 ) & 0xff;
	ptw[1]= ( (write_point +10) >>16 ) & 0xff;
	ptw[2]= ( (write_point +10) >> 8 ) & 0xff;
	ptw[3]= ( (write_point +10)      ) & 0xff;
	ptw += 10;
	/* フィルタリング済みデータを書き出し */
	memcpy(ptw, out_data, write_point);
	ptw += write_point;
	/* shm_outポインタの更新 */
	if (ptw > (shm_out->d + shm_out->pl))
	  ptw = shm_out->d;
	shm_out->r = shm_out->p;	/* latest */
	shm_out->p = ptw - shm_out->d;
	shm_out->c++;
	break;
      }

      /* チャネルヘッダ:ch_no(2B)+sampleing(0.5B+1.5B) */
      /* ch_header.ch_no = mkuint2(ptr); */
      /* ch_header.s_size_rate = mkuint2(ptr+2); */
      gsize = win_get_chhdr(ptr, &ch_no, &sr);

      pos = Pos_table[ch_no];
      if (pos < 0) { /* チャネル表にないのでskip */
	/* ch_size = get_ch_size(&ptr); */
	ptr += gsize;
	done_size += gsize;
	continue;
      }
      /* sampling rate(1.5B) */
      /* sr = ch_header.s_size_rate & 0x0fff; */
      
      if (sr <= 20) {
	/* 20Hz以下は、そのままセット */
	/* ch_size = get_ch_size(&ptr); */
	memcpy(&out_data[write_point], ptr, gsize);
	write_point += gsize;
	ptr += gsize;
	done_size += gsize;
      } else {
	/* winフォーマットをlbufに展開 */
	WIN_ch sys_ch0; /*dummy*/
	WIN_sr sr0; /*dummy*/
	/* ch_size = get_ch_size(&ptr); */
	win2fix(ptr,lbuf,&sys_ch0,&sr0);
	ptr += gsize;
	done_size += gsize;
	
	/* lbuf -> (double)dbuf */
	for (j = 0; j < sr; j++)
	  dbuf[j] = (double)lbuf[j];
	
	/* フイルタリング処理 */
	if (Ch[pos].flag_filt == 0) {
	  dt = 1.0 / (double)sr;
	  butlop(Ch[pos].coef, &Ch[pos].m_filt,
		 &Ch[pos].gn_filt, &Ch[pos].n_filt,
		 FP * dt, FS * dt, AP, AS);
	  /* 保留!!!*/
	  if (Ch[pos].m_filt > MAX_FILT) {
	    snprintf(Tbuf, sizeof(Tbuf),
		     "WARNING : filter order exceeded limit");
	    write_log(Tbuf);
	    /*end_process(1);*/
	  }
	  /**/
	  Ch[pos].flag_filt = 1;
	}
	/**************************************
				tandem(dbuf, dbuf, sr, Ch[pos].coef,
					Ch[pos].m_filt, 1, Ch[pos].uv);
	****************************************/
	{
	  int t_i;
	  if(sr<=0 || Ch[pos].m_filt<=0) {
	    snprintf(Tbuf, sizeof(Tbuf), 
		     "? (tandem) invalid input : sr=%d Ch[pos].m_filt=%d ?\n",
		     sr,Ch[pos].m_filt);
	    write_log(Tbuf);
	  } else {
	    {
	      int i,j,jd;
	      double a,aa,b,bb,u1,u2,u3,v1,v2,v3;
	      j=0; jd=1;
	      a =Ch[pos].coef[0]; aa=Ch[pos].coef[1]; b =Ch[pos].coef[2]; bb=Ch[pos].coef[3];
	      u1=Ch[pos].uv[0]; u2=Ch[pos].uv[1]; v1=Ch[pos].uv[2]; v2=Ch[pos].uv[3];
	      /****  FILTERING */
	      for(i=0;i<sr;i++) {
		u3=u2; u2=u1; u1=dbuf[j];
		v3=v2; v2=v1; v1=u1+a*u2+aa*u3-b*v2-bb*v3;
		dbuf[j]=v1;
		j+=jd;
	      }
	      Ch[pos].uv[0]=u1; Ch[pos].uv[1]=u2; Ch[pos].uv[2]=v1; Ch[pos].uv[3]=v2;
	    }
	    if(Ch[pos].m_filt>1)
	      for(t_i=1;t_i<Ch[pos].m_filt;t_i++) {
		{
		  int i,j,jd;
		  double a,aa,b,bb,u1,u2,u3,v1,v2,v3;
		  j=0; jd=1;
		  a =Ch[pos].coef[t_i*4+0]; aa=Ch[pos].coef[t_i*4+1]; b =Ch[pos].coef[t_i*4+2]; bb=Ch[pos].coef[t_i*4+3];
		  u1=Ch[pos].uv[t_i*4+0]; u2=Ch[pos].uv[t_i*4+1]; v1=Ch[pos].uv[t_i*4+2]; v2=Ch[pos].uv[t_i*4+3];
		  /****  FILTERING */
		  for(i=0;i<sr;i++) {
		    u3=u2; u2=u1; u1=dbuf[j];
		    v3=v2; v2=v1; v1=u1+a*u2+aa*u3-b*v2-bb*v3;
		    dbuf[j]=v1;
		    j+=jd;
		  }
		  Ch[pos].uv[t_i*4+0]=u1; Ch[pos].uv[t_i*4+1]=u2; Ch[pos].uv[t_i*4+2]=v1; Ch[pos].uv[t_i*4+3]=v2;
		}
	      }
	  }
	}
	/* re-sample data by SR */
	for (j = 0; j < SR; j++) {
	  dl = (int32_w)(dbuf[(j * sr) / SR] *
			 Ch[pos].gn_filt);
	  work[j] = dl;
	}
	/* ch_size = winform(work, out_buf, SR, ch_no); */
	ch_size = mk_windata(work, out_buf, SR, ch_no, ss_mode, ssf_flag);
	/* store data in Out_data buffer */
	memcpy(&out_data[write_point], out_buf, ch_size);
	write_point += ch_size;
      }
    }
    shp_in += size_in;
    if (shp_in > shm_in->pl)
      shp_in = shp_in_save = 0;
    while (shm_in->p == shp_in)
      sleep(1); /* usleep(800000) -> sleep(1) */
    if (shm_in->c < counter_save_in) {
      write_log("ERROR : counter over. reset");
      goto reset;
    }
    if (mkuint4(ptr_save) != size_in) {
      write_log("ERROR : ptr_save or size_in modified. reset");
      goto reset;
    }
  }
}
