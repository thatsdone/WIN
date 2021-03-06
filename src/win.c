/****************************************************************
*     seismogram analysis program "win" for X-Window Ver.11     *
* 90.6.9 -      (C) Urabe Taku / All Rights Reserved.           *
****************************************************************/
/* 
   $Id: win.c,v 1.72 2016/01/05 06:38:49 uehira Exp $

   High Samping rate
     9/12/96 read_one_sec 
     9/13/96 read_one_sec_mon
     9/13/96 make_sec_table

   98.7.2 yo2000

   2010/06/23 : 64bit clean??
*/
#define HINET_EXTENTION_1 1  /* roll over the monitor window */
#define HINET_EXTENTION_2 1  /* search */
#define HINET_EXTENTION_3 1  /* paste-up in the monitor window */
/*  #define HINET_WIN32   0  */     /* Hi-net WIN32 format */
#if ! HINET_WIN32
#define NAME_PRG      "win"
#else
#define NAME_PRG      "win32"
#endif
#define WIN_VERSION   "2015.12.15(+Hi-net)"

static const char rcsid[] =
  "$Id: win.c,v 1.72 2016/01/05 06:38:49 uehira Exp $";

#define DEBUG_AP      0   /* for debugging auto-pick */
/* 5:sr, 4:ch, 3:sec, 2:find_pick, 1:all */
/************ HOW TO COMPILE THE PROGRAM **************************
NEWS-OS 4.x    : cc win.c -O -lm -lX11 -o win
Sun-OS 4.x     : cc win.c -O -lm -lX11 -o win
Solaris 2.x    : cc win.c -w -O -I/usr/openwin/include -L/usr/openwin/lib \
                          -lm -lX11 -o win
IRIX 6.5       : cc win.c -n32 -signed -w -O -D__SVR4 -lm -lX11 -o win
FreeBSD 2/3/4  : cc win.c -w -I/usr/X11R6/include -L/usr/X11R6/lib -lm \
                          -lX11 -o win
HP-UX          : cc win.c -DSYSTEMV -I/usr/include/X11R4 -lm -lX11 -o win'
                           (contrib. T.Matsuzawa)
*******************************************************************
----------<EXAMPLE OF PARAMETER FILE 'win.prm'>----------------------
/dat/trg                     - 1- default directory for data file
/dat/etc/channels.tbl        - 2- channel table file
/dat/etc/zones.tbl           - 3- file for grouping of stations
/dat/picks/man/              - 4- directory for pick files(:WO dir)
hypomh                       - 5- hypomh program
/dat/etc/struct.eri          - 6- velocity structure for hypomh
/dat/etc/map.japan.km        - 7- map data file
.                            - 8- output directory for cut-out wave data
B4                           - 9-           data format (C/B4/B2/A)
/dat/etc/filter.prm          -10- filter setting file
lp                           -11- printer name(:mailer printer)
upper                        -12- upper or lower hemisphere projection
/dat/etc/labels.tbl          -13- label table file
/dat/final/man               -14- directory for hypocenter data files
100.0                        -15- pixels/in. of printer in hardcopying
/tmp                         -16- temporary working directory
#
#- 2- adding an '*' to the channel table file name forces to use 
#   the file even if a ".ch"(".CH") file exists.
#
#- 4- if you specify the second directory separated by ":" from the
#   first one, then it is treated as a 'write-only' directory. 
#
#- 7- map file must have a binary format. Text-to-binary conversion is
#   done by 'mapconv' mode ('win -c').
#
#- 9- for data format,
# B4 : binary 4-byte etc.
# C : numerical characters
#
#-11- for printer name (printcap entry)
#   "xwd|xpr -device ps|lpr"
#   adding an '*' to the printer name specifies "xwd|lpr -x"
#   specifying just '&' makes PostScript file "win.ps"
#   specifying just '*' makes XWD file "win.xwd"
#
#   if you specify the second printer separated by ":" from
#   the first one, then "pick" files are submitted to the printer.
#
#-14- if the directory can't be opened, hypocenter data are read
#   from pick files (see -4-), but it takes long time.
#   File containing hypocenters can be either text- or binary-
#   formatted :
# '92 11 19 16  4  4.373  35.18776 140.20154  30.105  2.6  auto  BLAST'
# (first line of HYPOMH output plus picker's name and diagnostic),
#   which is made by program 'pick2final' as
#   '(cd [pick dir] ; ls -l | pick2final) > [text hypo file]'
#   or
# 'struct FinalB', which is made by program 'pick2finalb' as
#   '(cd [pick dir] ; ls -l | pick2finalb) > [binary hypo file]'
#

----------<EXAMPLE OF FILTER SETTING FILE>--------------------------
bpf fl=5.0  fh=20.0 fs=30.0 ap=0.5 as=5.0
hpf fp=10.0 fs=5.0          ap=0.5 as=5.0
lpf fp=0.5  fs=0.8          ap=0.5 as=5.0
bpf fl=0.5  fh=1.0  fs=1.5  ap=0.5 as=5.0
bpf fl=1.0  fh=2.0  fs=3.0  ap=0.5 as=5.0
bpf fl=2.0  fh=4.0  fs=6.0  ap=0.5 as=5.0
bpf fl=4.0  fh=8.0  fs=12.0 ap=0.5 as=5.0
bpf fl=8.0  fh=16.0 fs=24.0 ap=0.5 as=5.0
bpf fl=16.0 fh=32.0 fs=48.0 ap=0.5 as=5.0
#
# number of filters given here is up to N_FILTERS(=30)
#
# for description of parameters, see 
# M. SAITO, 1978, An automatic design algorithm for band selective
# recursive digital filters, Butsuri Tanko, 31, 112-135.

----------<EXAMPLE OF LABEL TABLE FILE>-----------------------------
NOISE
FAR
BLAST
SONIC
LOCAL
 (up to N_LABELS(=30))

*******************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

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

#if defined(SYSTEMV)
#define atanh(x)  (0.5*log((1.0+x)/(1.0-x)))
#else
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

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

#include <sys/file.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <math.h>
#include <memory.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include "winlib.h"
#include "timsac.h"

  typedef XPoint lPoint;    /* short ! */
  static  XSizeHints sizehints;

  typedef struct {
    lPoint  origin;
    lPoint  extent;
    } lRectangle;
  typedef struct {
    int8_w type;   /* BM_FB or BM_MEM */
    int    depth;  /* bitmap depth (OLD : char) */
    lRectangle rect; /* defined area */
    /* char *base; */  /* for BM_MEM */
    XID drw;     /* Window or Pixmap */
    } lBitmap;

#define PICK_SERVER_PORT   7130
#define W_X     150
#define W_Y     60
#define BM_FB   0 /* frame buffer */
#define BM_MEM  1 /* bitmap in memory (XY format) */

  struct ms_data {
    int  md_sw; /* mouse button */
#define  MS_BUTNL 0x04
#define  MS_BUTNM 0x02
#define  MS_BUTNR 0x01
    int  md_x;  /* x coordinate */
    int  md_y;  /* y coordinate */
    };
  struct ms_event {
    struct ms_data mse_data; /* mouse X, Y and button status */
    char mse_trig;      /* trigger that caused this event */
#define MSE_MOTION 0 /* mouse movement */
#define MSE_BUTTON 1 /* mouse buttons */
#define MSE_EXP    2 /* exposure */
    char mse_dir;    /* key or button direction */
#define MSE_DOWN   0 /* down */
#define MSE_UP     1 /* up */
#define MSE_UNKOWN 2 /* unkown */
    char mse_code;   /* key or button code */
#define MSE_BUTNR 0  /* right button */
#define MSE_BUTNM 1  /* middle button */
#define MSE_BUTNL 2  /* left button */
    char mse_key;    /* modifier key */
#define MSE_NONE  0  /* no modifier */
#define MSE_SHIFT 1  /* +shift */
#define MSE_CTRL  2  /* +control */
    struct timeval mse_time; /* time when this event occurred */
    };

#define W_WIDTH   (780-16)
#define W_HEIGHT  816

#define BF_0      GXclear        /* 0 */
#define BF_1      GXset          /* 1 */
#define BF_S      GXcopy         /* src */
#define BF_SI     GXcopyInverted /* ~src */
#define BF_D      GXnoop         /* dst */
#define BF_DI     GXinvert       /* ~dst */
#define BF_SIDA   GXandInverted  /* ~src & dst */
#define BF_SDIO   GXorReverse    /* src | ~dst */
#define BF_SDX    GXxor          /* src ^ dst */
#define BF_SDXI   GXequiv        /* ~src ^ dst */
#define BF_SDO    GXor           /* src | dst */
#define BF_SDOI   GXnor          /* ~(src | dst) */
#define BF_SIDO   GXorInverted   /* ~src | dst */
#define BF_SDIA   GXandReverse   /* src & ~dst */
#define BF_SDA    GXand          /* src & dst */
#define BF_SDAI   GXnand         /* ~(src & dst) */
  static Display *disp;
  static Cursor x11_cursor;
  static XColor c_black,c_white,c_cursor,c_c;
  static Colormap colormap;
  /* static XSetWindowAttributes att; */

/*#define OLD_FORMAT      0 */
               /* 1 for data before May, 1990 ;
                (1) compressed epo data, 
                (2) no 0.5 byte data, 
                (3) not differential data, 
                (4) epo channel 0-241 (not real ch) */
#define KILL_SPIKE        1
#define NOT_KILL_SPIKE    0
#define PI          3.141592654
#define HP          (PI/2.0)
#define WHITE       0x00
#define MAX_SHORT    32000
#if HINET_WIN32
 #define MEMORY_LIMIT (1000000*8*10) /* pixels for one bitmap */
#else
 #define MEMORY_LIMIT (1000000*8) /* pixels for one bitmap */
#endif
#define WIDTH_TEXT   8
#if HINET_WIN32
  #define HEIGHT_TEXT 14
#else
  #define HEIGHT_TEXT  16
#endif
#define CODE_START   32
#define N_BM        100   /* size of text buffer (chars) */
#define WB          (WIDTH_TEXT*5)    /* width of a box */
#define HW          (WIDTH_TEXT/2)    /* half of font width */
#define MARGIN      (HEIGHT_TEXT+4)
#define HEIGHT_FUNC (MARGIN+1)
#define YBASE_MON   (MARGIN+HEIGHT_FUNC)
#define TICKL       (WIDTH_TEXT*3/2)
#define PIXELS_PER_SEC_MON  10
#define SR_LOW       1  /* lower limit of sampling rate */
#define N_CH_NAME    0x10000 /* maximum n of name channels */
                             /* 16 bit channel field */
#if HINET_WIN32
  #define NAMLEN       256
#else
  #define NAMLEN       80
#endif
#define STNLEN   WIN_STANAME_LEN /* (length of station code)+1 */
#define CMPLEN   WIN_STACOMP_LEN /* (length of component code)+1 */
#define WIDTH_INFO_C 18
#define WIDTH_INFO      (WIDTH_TEXT*WIDTH_INFO_C)
#define WIDTH_INFO_ZOOM (WIDTH_TEXT*WIDTH_INFO_C)
#define PSUP_LMARGIN    (WIDTH_TEXT*16)
#define PSUP_RMARGIN    (WIDTH_TEXT*11)
#define PSUP_TMARGIN    (MARGIN+HEIGHT_TEXT)
#define PSUP_BMARGIN    HEIGHT_TEXT

#define SIZE_CURSOR     (23*2+1)

#define LINES_PER_ZOOM  5
#define PIXELS_PER_LINE (HEIGHT_TEXT+9)
#define HEIGHT_ZOOM   (PIXELS_PER_LINE*LINES_PER_ZOOM)
#define N_ZOOM_MAX    (2000/HEIGHT_ZOOM)
#define CENTER_ZOOM   (HEIGHT_ZOOM/2)
#define BORDER        2
#define MIN_ZOOM      BORDER
#define MAX_ZOOM      (HEIGHT_ZOOM-1)
#define Y_LINE1       ((MARGIN-HEIGHT_TEXT)/2)
#define Y_TIME        ((MARGIN-HEIGHT_TEXT)/2+HEIGHT_FUNC)
#define ZOOM_LENGTH        4     /* initial value */
#define ZOOM_LENGTH_MAX  240     /* max */
#define ZOOM_LENGTH_MIN    1     /* min */
#define SCALE_MAX    26     /* max */
#define SHIFT         4     /* 1/N */
#define P             0     /* mark index : P */
#define S             1     /* mark index : S */
#define X             2     /* mark index : X or F */
#define MD            3     /* mark index : max defl. */
#define LINELEN     1024    /* size of line buffer (OLD : 256) */
#define PDPI        100.0   /* default of printer's DPI */
#define PPK_INIT      1     /* initial ppk_idx */
#define PPK_HYPO      2     /* ppk_idx for hypo */
#define MAX_FILT    100     /* max order of filter */

#define LPTN_FF       0
#define LPTN_55       1
#define LPTN_33       2
#define LPTN_0F       3
#define LPTN_01       4
#define LPTN_0001     5
#define N_LPTN        6

#define N_LABELS      30
#define N_FILTERS     30

#define PARAM_PATH    1  /* default directory for data file */
#define PARAM_CHS     2  /* channel table file */
#define PARAM_ZONES   3  /* name of zone file */
#define PARAM_OUT     4  /* directory for pick files (R/W) */
#define PARAM_HYPO    5  /* name of hypo-location program file */
#define PARAM_STRUCT  6  /* name of structure file */
#define PARAM_MAP     7  /* name of map data file */
#define PARAM_WAVE    8  /* directory for cutout data files */
#define PARAM_FMT     9  /* cutout data format */
#define PARAM_FILT    10 /* name of filter setting file */
#define PARAM_PRINTER 11 /* printer name */
#define PARAM_HEMI    12 /* upper or lower hemisphere projection */
#define PARAM_LABELS  13 /* name of label table (NOISE,FAR etc.) */
#define PARAM_FINAL   14 /* directory for other hypos */
#define PARAM_DPI     15 /* dot/inch of hardcopy printer */
#define PARAM_TEMP    16 /* temporary working directory */

#define X_Z_CHN       0
#define Y_Z_CHN       (PIXELS_PER_LINE*0)
#define W_Z_CHN       WIDTH_INFO_ZOOM

#define X_Z_POL       0
#define Y_Z_POL       (PIXELS_PER_LINE*1)
#define W_Z_POL       (WIDTH_INFO_ZOOM/3)
#define X_Z_SCL       W_Z_POL
#define Y_Z_SCL       Y_Z_POL
#define W_Z_SCL       (WIDTH_INFO_ZOOM-W_Z_POL)

#define X_Z_TSC       0
#define Y_Z_TSC       (PIXELS_PER_LINE*2)
#define W_Z_TSC       (WIDTH_INFO_ZOOM/2)
#define X_Z_SFT       W_Z_TSC
#define Y_Z_SFT       Y_Z_TSC
#define W_Z_SFT       (WIDTH_INFO_ZOOM-W_Z_TSC)

#define X_Z_FLT       0
#define Y_Z_FLT       (PIXELS_PER_LINE*3)
#define W_Z_FLT       WIDTH_INFO_ZOOM

#define X_Z_GET       0
#define Y_Z_GET       (PIXELS_PER_LINE*4)
#define W_Z_GET       (WIDTH_INFO_ZOOM/3)
#define X_Z_PUT       W_Z_GET
#define Y_Z_PUT       Y_Z_GET
#define W_Z_PUT       W_Z_GET
#define X_Z_CLS       (X_Z_PUT+W_Z_PUT)
#define Y_Z_CLS       Y_Z_GET
#define W_Z_CLS       (WIDTH_INFO_ZOOM-X_Z_CLS)
#define Y_Z_OFS       ((PIXELS_PER_LINE-HEIGHT_TEXT)/2)
#define X_Z_ARR       ((W_Z_TSC-32)/2)
#define Y_Z_ARR       ((PIXELS_PER_LINE-16)/2)

#define MODE_NORMAL   0 /* for main_mode & map_mode */
#define MODE_GET      1 /* for main_mode */
#define MODE_PICK     2 /* for main_mode */
#define MODE_GETPICK  3 /* for main_mode */
#define MODE_FIND1  1 /* for map_mode (looking for a corner) */
#define MODE_FIND2  2 /* for map_mode (dragging) */
#define MODE_TS1    3 /* for map_mode (looking for a corner) */
#define MODE_TS2    4 /* for map_mode (dragging) */
#define MODE_TS3    5 /* for map_mode (plot) */
#define MODE_DOWN   0 /* for mecha_mode */
#define MODE_UP     1 /* for mecha_mode */
#define LOOP_NO   (-1)
#define LOOP_MAIN   0
#define LOOP_MAP    1
#define LOOP_MECHA  2
#define LOOP_PSUP   3

#define N_SYM      11   /* n of hypo symbols */
#define N_SYM_USE   5   /* not N_SYM */
#define N_SYM_STN   4   /* n of stn symbols */
/* main screen */
#define EVDET       1
#define AUTPK       2
#define HYPO        3
#define SAVE        4
#define LR          6
#define UD          7
#define OPEN        0
/* main screen(2) */
#define RFSH        1
#define QUIT        2
#define COPY        3
#define MAP         4
#define FINL        5
#define LIST        6
#define LOAD        7
#define UNLD        8
#define CLER        9
#define MECH       10
#define PSTUP      11
/* map scrreen */
#define RETN        2
#define VERT        4
#define STNS        5
#define TMSP        6
#define OTHRS       1
#define RATIO       1
/* psup screen */
/* mech screen */
#define UPPER       6

#define   LEVEL_1   0.010   /* error width */
#define   LEVEL_2   0.030   /* good sharpness */
#define   LEVEL_3   0.100   /* lower limit of (aic_all-aic) */

  static char cursor_color[20]={"blue"},
    *func_main[]={"OPEN" ,"EVDET","AUTPK","HYPO","SAVE","","    ","    ","$"},
    *func_main2[]={"","RFSH","QUIT","COPY","MAP","FINL","LIST","LOAD","UNLD",
            "CLER","MECH","PSTUP","$"},
    *func_map[] ={"","RFSH","RETN","COPY","VERT","STNS","T-S","$"},
    *func_map2[]={"","OTHRS","$"},
    *func_map3[]={"","RATIO","$"},
    *func_psup[]={"","RFSH","RETN","COPY","MAP","$"},
    *func_mech[]={"","RFSH","RETN","COPY","MAP","STNS","UP/LO","$"};

#define put_reverse(bm,xzero,yzero,xsize,ysize) \
  put_bitblt(bm,xzero,yzero,xsize,ysize,bm,xzero,yzero,BF_DI)
#define put_white(bm,xzero,yzero,xsize,ysize) \
  put_fill(bm,xzero,yzero,xsize,ysize,0)
#define put_black(bm,xzero,yzero,xsize,ysize) \
  put_fill(bm,xzero,yzero,xsize,ysize,1)
#define put_fram(bm,xzero,yzero,xsize,ysize) \
  put_fill(bm,xzero,yzero,xsize,ysize,1); \
  put_fill(bm,xzero+1,yzero+1,xsize-2,ysize-2,0)
#define p2w(p)  ((p+15)/16)

/*
#if defined(SUNOS4)
#define mktime timelocal
#endif
*/
  static lBitmap dpy,*mon,info,cursor,cursor_mask,zoom,epi_s,epi_l,arrows_ud,
    arrows_lr,arrows_lr_zoom,arrows_leng,arrows_scale,bbuf,sym,sym_stn;
  static lPoint *points;

static char patterns[N_LPTN][2]={{1,0}, {1,1}, {2,2}, {4,4}, {1,7}, {1,15}};
  static GC gc_line[N_LPTN],gc_line_mem[N_LPTN],gc_fb,gc_mem,gc_fbi,gc_memi;
  static XID ttysw;
  static char *marks[]={"P","S","F","!"};
  static uint8_w bbm[16][N_BM];  /* text bitmap buffer */
  static float depsteps[41]=
    {-6400,-10,-5,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10,12,15,20,25,30,35,40,
     50,60,70,80,90,100,120,150,200,250,300,350,400,500,600,700,6400};
  static float magsteps[21]=
    {-9.0,-2.0,-1.5,-1.0,-0.5,0.0,0.5,1.0,1.5,2.0,2.5,3.0,3.5,
     4.0,4.5,5.0,5.5,6.0,6.5,7.0,9.5};
  static double mapsteps[11]={800.0,500.0,300.0,200.0,100.0,50.0,20.0,10.0,5.0,2.5,1.0};
  struct YMDhms {
    int ye,mo,da,ho,mi,se;
    };
static struct Sel {
    char o[8];
    int no_blast;
    long t1,t2;   /* 64bit ok */
    struct YMDhms time1,time2,time1_save,time2_save;
    float dep1,dep2;
    int dep1_idx,dep2_idx,deplen_idx;
    float mag1,mag2;
    int mag1_idx,mag2_idx,mag_ud;
    } sel= {
    {0,0,0,0,0,0,0,0},  0,    0,0,
    {80, 1, 1, 0, 0, 0},  {99,12,31,23,59, 0},
    {80, 1, 1, 0, 0, 0},  {99,12,31,23,59, 0},
    0.0,0.0,  0,0,0,
    0.0,0.0,  0,0,1};
  /* (X0,Y0,size,center-offset,radius) */
  static int size_sym[N_SYM][5]=
    {{36,23,3,1,1}, {54,7,3,1,1}, {41,25,4,1,2}, {31,23,5,2,3},
     {54,0,7,3,4}, {46,38,11,5,5}, {46,23,15,7,7}, {27,31,19,9,9},
     {31,0,23,11,11}, {0,31,27,13,13}, {0,0,31,15,15}};
  static int size_sym_stn[N_SYM_STN][5]=
    {{1,1,5,2,3}, {8,0,7,3,4}, {0,8,7,3,4}, {7,7,9,4,5}};
  static uint8_w
    buf_epi_s[32]=
      {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x80,0x07,0xc0,
       0x0f,0xe0,0x0f,0xe0,0x0f,0xe0,0x07,0xc0,0x03,0x80,0x00,0x00,
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    buf_epi_l[32]=
      {0x07,0xc0,0x1f,0xf0,0x3f,0xf8,0x7f,0xfc,0x7f,0xfc,0xff,0xfe,
       0xff,0xfe,0xff,0xfe,0xff,0xfe,0xff,0xfe,0x7f,0xfc,0x7f,0xfc,
       0x3f,0xf8,0x1f,0xf0,0x07,0xc0,0x00,0x00},
/*
   Note that 'buf_sym*', 'buf_arrows*' and 'buf_cursor*' were
  created using 'bitmap' of X11, so that they have the LSB-left
  bit order.
*/
    buf_sym_stn[32]=
      {0x00,0x63, 0x22,0x77, 0x14,0x3e, 0x08,0x1c, 0x14,0x3e, 0x22,0x77,
       0x00,0x63, 0x80,0xff, 0x9c,0xe3, 0x9c,0xf7, 0xff,0xbe, 0xff,0x9c,
       0xff,0xbe, 0x9c,0xf7, 0x9c,0xe3, 0x80,0xff},
    buf_sym[464]=
      {0x00,0xf0, 0x07,0x00, 0x80,0x3f, 0x00,0x07, 0x00,0x0e, 0x38,0x00,
       0x60,0xc0, 0x80,0x08, 0x80,0x01, 0xc0,0x00, 0x18,0x00, 0x43,0x10,
       0x40,0x00, 0x00,0x01, 0x04,0x00, 0x44,0x10, 0x20,0x00, 0x00,0x02,
       0x02,0x00, 0x48,0x10, 0x10,0x00, 0x00,0x04, 0x02,0x00, 0x88,0x08,
       0x08,0x00, 0x00,0x08, 0x01,0x00, 0x10,0x07, 0x04,0x00, 0x00,0x10,
       0x01,0x00, 0x90,0x00, 0x04,0x00, 0x00,0x90, 0x00,0x00, 0x60,0x01,
       0x02,0x00, 0x00,0xa0, 0x00,0x00, 0xa0,0x00, 0x02,0x00, 0x00,0xa0,
       0x00,0x00, 0x20,0x00, 0x02,0x00, 0x00,0xa0, 0x00,0x00, 0x20,0x00,
       0x01,0x00, 0x00,0xc0, 0x00,0x00, 0x20,0x00, 0x01,0x00, 0x00,0xc0,
       0x00,0x00, 0x20,0x00, 0x01,0x00, 0x00,0xc0, 0x00,0x00, 0x20,0x00,
       0x01,0x00, 0x00,0x40, 0x01,0x00, 0x10,0x00, 0x01,0x00, 0x00,0x40,
       0x01,0x00, 0x10,0x00, 0x01,0x00, 0x00,0x40, 0x02,0x00, 0x08,0x00,
       0x01,0x00, 0x00,0x40, 0x02,0x00, 0x08,0x00, 0x02,0x00, 0x00,0x20,
       0x04,0x00, 0x04,0x00, 0x02,0x00, 0x00,0x20, 0x18,0x00, 0x03,0x00,
       0x02,0x00, 0x00,0x20, 0x60,0xc0, 0x00,0x00, 0x04,0x00, 0x00,0x10,
       0x80,0x3f, 0x00,0x00, 0x04,0x00, 0x00,0x10, 0x77,0x00, 0xf8,0x00,
       0x08,0x00, 0x00,0x88, 0x78,0x00, 0x06,0x03, 0x10,0x00, 0x00,0x84,
       0x78,0x0c, 0x01,0x04, 0x20,0x00, 0x00,0x82, 0x08,0x92, 0x00,0x08,
       0x40,0x00, 0x00,0x01, 0x07,0x92, 0x00,0x08, 0x80,0x01, 0xc0,0x00,
       0x00,0x4c, 0x00,0x10, 0x00,0x0e, 0x38,0x00, 0x00,0x40, 0x00,0x10,
       0x00,0xf0, 0x07,0x00, 0x00,0x40, 0x00,0x10, 0x00,0xfc, 0x01,0x00,
       0xfe,0x40, 0x00,0x10, 0x80,0x03, 0x0e,0x80, 0x01,0x43, 0x00,0x10,
       0x40,0x00, 0x10,0x40, 0x00,0x84, 0x00,0x08, 0x30,0x00, 0x60,0x20,
       0x00,0x88, 0x00,0x08, 0x08,0x00, 0x80,0x10, 0x00,0x10, 0x01,0x04,
       0x08,0x00, 0x80,0x10, 0x00,0x10, 0x06,0x03, 0x04,0x00, 0x00,0x09,
       0x00,0x20, 0xf8,0x00, 0x02,0x00, 0x00,0x0a, 0x00,0x20, 0x3e,0x00,
       0x02,0x00, 0x00,0x0a, 0x00,0x20, 0x41,0x00, 0x02,0x00, 0x00,0x0a,
       0x00,0xa0, 0x80,0x00, 0x01,0x00, 0x00,0x0c, 0x00,0x60, 0x00,0x01,
       0x01,0x00, 0x00,0x0c, 0x00,0x60, 0x00,0x01, 0x01,0x00, 0x00,0x0c,
       0x06,0x00, 0x00,0x01, 0x01,0x00, 0x00,0x14, 0x00,0x50, 0x00,0x01,
       0x01,0x00, 0x00,0x14, 0x00,0x50, 0x00,0x01, 0x01,0x00, 0x00,0x24,
       0x00,0x88, 0x80,0x00, 0x01,0x00, 0x00,0x44, 0x00,0x04, 0x41,0x00,
       0x02,0x00, 0x00,0x82, 0x01,0x03, 0x3e,0x00, 0x02,0x00, 0x00,0x02,
       0xfe,0x00, 0x00,0x00, 0x02,0x00, 0x00,0x02, 0x00,0x00, 0x00,0x00,
       0x04,0x00, 0x00,0x01, 0x00,0x00, 0x00,0x00, 0x08,0x00, 0x80,0x00,
       0x00,0x00, 0x00,0x00, 0x08,0x00, 0x80,0x00, 0x00,0x00, 0x00,0x00,
       0x30,0x00, 0x60,0x00, 0x00,0x00, 0x00,0x00, 0x40,0x00, 0x10,0x00,
       0x00,0x00, 0x00,0x00, 0x80,0x03, 0x0e,0x00, 0x00,0x00, 0x00,0x00,
       0x00,0xfc, 0x01,0x00, 0x00,0x00, 0x00,0x00},
    buf_arrows_ud[64]=
      {0xc0,0x00, 0xc0,0x0f, 0xe0,0x01, 0xc0,0x0f, 0xf0,0x03, 0xc0,0x0f,
       0xf8,0x07, 0xc0,0x0f, 0xfc,0x0f, 0xc0,0x0f, 0xfe,0x1f, 0xc0,0x0f,
       0xff,0x3f, 0xc0,0x0f, 0xf0,0x03, 0xc0,0x0f, 0xf0,0x03, 0xc0,0x0f,
       0xf0,0x03, 0xfc,0xff, 0xf0,0x03, 0xf8,0x7f, 0xf0,0x03, 0xf0,0x3f,
       0xf0,0x03, 0xe0,0x1f, 0xf0,0x03, 0xc0,0x0f, 0xf0,0x03, 0x80,0x07,
       0xf0,0x03, 0x00,0x03},
    buf_arrows_lr[64]=
      {0x80,0x00, 0x00,0x01, 0xc0,0x00, 0x00,0x03, 0xe0,0x00, 0x00,0x07,
       0xf0,0x00, 0x00,0x0f, 0xf8,0x00, 0x00,0x1f, 0xfc,0x7f, 0xfe,0x3f,
       0xfe,0x7f, 0xfe,0x7f, 0xff,0x7f, 0xfe,0xff, 0xff,0x7f, 0xfe,0xff,
       0xfe,0x7f, 0xfe,0x7f, 0xfc,0x7f, 0xfe,0x3f, 0xf8,0x00, 0x00,0x1f,
       0xf0,0x00, 0x00,0x0f, 0xe0,0x00, 0x00,0x07, 0xc0,0x00, 0x00,0x03,
       0x80,0x00, 0x00,0x01},
    buf_arrows_lr_zoom[64]=
      {0x80,0x00, 0x00,0x01, 0xc0,0x80, 0x01,0x03, 0xe0,0x80, 0x01,0x07,
       0xf0,0x80, 0x01,0x0f, 0xf8,0x80, 0x01,0x1f, 0xfc,0x9f, 0xf9,0x3f,
       0xfe,0x9f, 0xf9,0x7f, 0xff,0x9f, 0xf9,0xff, 0xff,0x9f, 0xf9,0xff,
       0xfe,0x9f, 0xf9,0x7f, 0xfc,0x9f, 0xf9,0x3f, 0xf8,0x80, 0x01,0x1f,
       0xf0,0x80, 0x01,0x0f, 0xe0,0x80, 0x01,0x07, 0xc0,0x80, 0x01,0x03,
       0x80,0x00, 0x00,0x01},
    buf_arrows_leng[64]=
      {0x08,0xc1, 0x43,0x20, 0x08,0xc1, 0x43,0x20, 0x0c,0xc3, 0xc3,0x30,
       0x0c,0x83, 0xc1,0x30, 0x0e,0x87, 0xc1,0x39, 0xfe,0x07, 0xf0,0xf9,
       0xff,0x0f, 0xf0,0xff, 0xff,0x0f, 0xf0,0xff, 0xff,0x0f, 0xf0,0xff,
       0xff,0x0f, 0xf0,0xff, 0xfe,0x07, 0xf0,0xf9, 0x0e,0x87, 0xc1,0x39,
       0x0c,0x83, 0xc1,0x30, 0x0c,0xc3, 0xc3,0x30, 0x08,0xc1, 0x43,0x20,
       0x08,0xc1, 0x43,0x20},
    buf_arrows_scale[64]=
      {0x60,0x00, 0x87,0x1f, 0xf0,0x80, 0x81,0x1f, 0xf8,0xc1, 0xf0,0xff,
       0xfc,0xc3, 0xe0,0x7f, 0xfe,0xc7, 0xc0,0x3f, 0xff,0xcf, 0x80,0x1f,
       0xf8,0x81, 0x01,0x0f, 0xf8,0x81, 0x01,0x06, 0xf8,0x81, 0x01,0x06,
       0xf8,0x81, 0x01,0x0f, 0xff,0x0f, 0x83,0x1f, 0xfe,0x07, 0xc3,0x3f,
       0xfc,0x03, 0xe3,0x7f, 0xf8,0x01, 0xf3,0xff, 0xf0,0x80, 0x81,0x1f,
       0x60,0xe0, 0x80,0x1f};

/* (8 x 16) x (128-32) */
/* start = 32, end = 126, n of codes = 126-32+1 = 95 */
/* bitmap of 16(tate) x 96(yoko) bytes */
    static uint8_w font16[16][96]=
      {{0x00,0x00,0x6c,0x00,0x10,0x02,0x00,0xe0,0x02,0x80,0x00,0x00,0x00,0x00,0x00,0x02,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x80,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x00,0xf0,0x10,0x00,
	0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x06,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x10,0xc0,0xfe,0x00},
       {0x00,0x38,0x6c,0x12,0x38,0x62,0x30,0xe0,0x04,0x40,0x00,0x00,0x00,0x00,0x00,0x02,
	0x18,0x10,0x18,0x38,0x08,0xfc,0x3c,0xfe,0x38,0x38,0x00,0x00,0x04,0x00,0x40,0x38,
	0x3c,0x10,0xf8,0x3a,0xf8,0xfe,0xfe,0x1a,0xe7,0xfe,0x1f,0xe6,0xf0,0x82,0x87,0x38,
	0xf8,0x38,0xf8,0x34,0xfe,0xe7,0xc6,0xc6,0xee,0xc6,0xfe,0x10,0xc6,0x10,0x28,0x00,
	0x30,0x00,0xc0,0x00,0x06,0x00,0x0e,0x00,0xc0,0x18,0x06,0xc0,0x78,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x38,0x24,0x12,0x54,0x94,0x48,0x20,0x08,0x20,0x00,0x00,0x00,0x00,0x00,0x04,
	0x24,0x70,0x24,0x44,0x18,0x80,0x42,0x82,0x44,0x44,0x00,0x00,0x04,0x00,0x40,0x44,
	0x42,0x28,0x44,0x46,0x44,0x42,0x42,0x26,0x42,0x10,0x02,0x44,0x40,0xc6,0xc2,0x44,
	0x44,0x44,0x44,0x4c,0x92,0x42,0x82,0x82,0x44,0x82,0x84,0x10,0x82,0x10,0x44,0x00,
	0x20,0x00,0x40,0x00,0x04,0x00,0x11,0x00,0x40,0x00,0x00,0x40,0x08,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x38,0x24,0x12,0x92,0x94,0x48,0x20,0x08,0x20,0x10,0x10,0x00,0x00,0x00,0x04,
	0x24,0x10,0x42,0x82,0x28,0x80,0x46,0x82,0x82,0x82,0x00,0x00,0x08,0x00,0x20,0x82,
	0x82,0x28,0x42,0x42,0x44,0x42,0x42,0x42,0x42,0x10,0x02,0x44,0x40,0xaa,0xa2,0x82,
	0x42,0x44,0x42,0x84,0x92,0x42,0x82,0x82,0x44,0x44,0x88,0x10,0x44,0x10,0x82,0x00,
	0x20,0x00,0x40,0x00,0x04,0x00,0x10,0x00,0x40,0x00,0x00,0x40,0x08,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x38,0x48,0x7f,0x96,0x94,0x48,0xc0,0x10,0x10,0x38,0x10,0x00,0x00,0x00,0x08,
	0x42,0x10,0x62,0x82,0x28,0x80,0x80,0x04,0x82,0x82,0x00,0x00,0x08,0x00,0x20,0xc2,
	0x9a,0x28,0x42,0x80,0x42,0x40,0x40,0x40,0x42,0x10,0x02,0x48,0x40,0xaa,0xa2,0x82,
	0x42,0x82,0x42,0x80,0x10,0x42,0x82,0x82,0x28,0x44,0x08,0x10,0x44,0x10,0x00,0x00,
	0x10,0x00,0x40,0x00,0x04,0x00,0x10,0x00,0x40,0x00,0x00,0x40,0x08,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x38,0x00,0x24,0x90,0x98,0x50,0x00,0x10,0x10,0x92,0x10,0x00,0x00,0x00,0x08,
	0x42,0x10,0x02,0x02,0x48,0xb8,0x80,0x04,0x82,0x82,0x38,0x38,0x10,0xfe,0x10,0x02,
	0xa6,0x44,0x42,0x80,0x42,0x48,0x48,0x80,0x42,0x10,0x02,0x48,0x40,0xaa,0xa2,0x82,
	0x42,0x82,0x42,0x80,0x10,0x42,0x82,0x92,0x28,0x44,0x10,0x10,0x44,0x10,0x00,0x00,
	0x00,0x3c,0x78,0x3a,0x3c,0x38,0xfe,0x3b,0x5c,0x78,0x3e,0x42,0x08,0x6c,0xdc,0x38,
	0xf8,0x3e,0xec,0x3a,0xfc,0xc6,0xc6,0x92,0xee,0xe7,0x7e,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x38,0x00,0x24,0x50,0x68,0x20,0x00,0x10,0x10,0xd6,0x10,0x00,0x00,0x00,0x08,
	0x42,0x10,0x04,0x04,0x48,0xc4,0xb8,0x04,0x44,0x82,0x38,0x38,0x10,0x00,0x10,0x04,
	0xa2,0x44,0x44,0x80,0x42,0x48,0x48,0x80,0x7e,0x10,0x02,0x70,0x40,0x92,0x92,0x82,
	0x42,0x82,0x44,0x60,0x10,0x42,0x44,0x92,0x10,0x28,0x10,0x10,0x28,0x10,0x00,0x00,
	0x00,0x42,0x44,0x46,0x44,0x44,0x10,0x44,0x62,0x08,0x02,0x44,0x08,0x92,0x62,0x44,
	0x44,0x44,0x32,0x46,0x20,0x42,0x82,0x92,0x44,0x42,0x44,0x10,0x10,0x10,0x00,0x00},
       {0x00,0x10,0x00,0x24,0x38,0x10,0x2e,0x00,0x10,0x10,0x38,0xfe,0x00,0xfe,0x00,0x10,
	0x42,0x10,0x08,0x38,0x88,0x82,0xc4,0x08,0x38,0x46,0x00,0x00,0x20,0x00,0x08,0x04,
	0xa2,0x44,0x78,0x80,0x42,0x78,0x78,0x8f,0x42,0x10,0x02,0x50,0x40,0x92,0x92,0x82,
	0x44,0x82,0x78,0x18,0x10,0x42,0x44,0x92,0x28,0x28,0x10,0x10,0xfe,0x10,0x00,0x00,
	0x00,0x02,0x42,0x82,0x84,0x82,0x10,0x44,0x42,0x08,0x02,0x48,0x08,0x92,0x42,0x82,
	0x42,0x84,0x22,0x42,0x20,0x42,0x82,0x92,0x28,0x22,0x08,0x20,0x10,0x08,0x00,0x00},
       {0x00,0x10,0x00,0x24,0x14,0x10,0x54,0x00,0x10,0x10,0xd6,0x10,0x00,0x00,0x00,0x10,
	0x42,0x10,0x08,0x04,0x88,0x02,0x82,0x08,0x44,0x3a,0x00,0x00,0x20,0x00,0x08,0x08,
	0xa2,0x44,0x44,0x80,0x42,0x48,0x48,0x82,0x42,0x10,0x02,0x48,0x40,0x92,0x92,0x82,
	0x78,0x82,0x48,0x04,0x10,0x42,0x44,0xaa,0x28,0x10,0x20,0x10,0x10,0x10,0x00,0x00,
	0x00,0x3e,0x42,0x80,0x84,0xfe,0x10,0x44,0x42,0x08,0x02,0x58,0x08,0x92,0x42,0x82,
	0x42,0x84,0x20,0x40,0x20,0x42,0x44,0x92,0x28,0x24,0x08,0x10,0x10,0x10,0x00,0x00},
       {0x00,0x10,0x00,0x24,0x12,0x2c,0x54,0x00,0x10,0x10,0x92,0x10,0x00,0x00,0x00,0x10,
	0x42,0x10,0x10,0x02,0xfe,0x02,0x82,0x08,0x82,0x02,0x00,0x00,0x10,0xfe,0x10,0x10,
	0xa6,0x7c,0x42,0x80,0x42,0x48,0x48,0x82,0x42,0x10,0x82,0x48,0x40,0x82,0x8a,0x82,
	0x40,0x82,0x44,0x82,0x10,0x42,0x44,0xaa,0x28,0x10,0x20,0x10,0xfe,0x10,0x00,0x00,
	0x00,0x42,0x42,0x80,0x84,0x80,0x10,0x38,0x42,0x08,0x02,0x64,0x08,0x92,0x42,0x82,
	0x42,0x84,0x20,0x3c,0x20,0x42,0x44,0xaa,0x10,0x14,0x10,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x10,0x00,0xfe,0xd2,0x32,0x94,0x00,0x10,0x10,0x38,0x10,0x00,0x00,0x00,0x20,
	0x42,0x10,0x20,0x82,0x08,0xc2,0x82,0x08,0x82,0x02,0x00,0x00,0x10,0x00,0x10,0x10,
	0x9a,0x82,0x42,0x82,0x42,0x42,0x40,0x82,0x42,0x10,0x82,0x44,0x42,0x82,0x8a,0x82,
	0x40,0xba,0x44,0x82,0x10,0x42,0x28,0xaa,0x44,0x10,0x42,0x10,0x10,0x10,0x00,0x00,
	0x00,0x82,0x42,0x80,0x84,0x80,0x10,0x40,0x42,0x08,0x02,0x44,0x08,0x92,0x42,0x82,
	0x42,0x84,0x20,0x02,0x20,0x42,0x44,0xaa,0x28,0x08,0x10,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x00,0x00,0x48,0x92,0x52,0x88,0x00,0x10,0x10,0x10,0x10,0xe0,0x00,0x40,0x20,
	0x42,0x10,0x22,0x82,0x08,0x82,0x82,0x10,0x82,0x82,0x00,0x38,0x08,0x00,0x20,0x00,
	0x80,0x82,0x42,0x42,0x44,0x42,0x40,0x42,0x42,0x10,0x82,0x44,0x42,0x82,0x8a,0x82,
	0x40,0x44,0x44,0x82,0x10,0x42,0x28,0x44,0x44,0x10,0x42,0x10,0x10,0x10,0x00,0x00,
	0x00,0x82,0x42,0x82,0x84,0x82,0x10,0x78,0x42,0x08,0x02,0x42,0x08,0x92,0x42,0x82,
	0x44,0x44,0x20,0x82,0x22,0x42,0x28,0x44,0x28,0x08,0x22,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x00,0x00,0x48,0x94,0x52,0x8c,0x00,0x08,0x20,0x00,0x00,0xe0,0x00,0xe0,0x40,
	0x24,0x10,0x42,0x44,0x08,0x44,0x44,0x10,0x44,0x44,0x38,0x38,0x08,0x00,0x20,0x00,
	0x42,0x82,0x42,0x42,0x44,0x42,0x40,0x66,0x42,0x10,0x44,0x42,0x42,0x82,0x86,0x44,
	0x40,0x44,0x42,0xc4,0x10,0x42,0x10,0x44,0x82,0x10,0x82,0x10,0x10,0x10,0x00,0x00,
	0x00,0x86,0x44,0x42,0x44,0x42,0x10,0x84,0x42,0x08,0x82,0x42,0x08,0x92,0x42,0x44,
	0x78,0x3c,0x20,0xc2,0x22,0x46,0x28,0x44,0x44,0x10,0x42,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x10,0x00,0x48,0x78,0x52,0x72,0x00,0x08,0x20,0x00,0x00,0x20,0x00,0xe0,0x40,
	0x24,0x7c,0x7e,0x38,0x3c,0x38,0x38,0x10,0x38,0x38,0x38,0x18,0x04,0x00,0x40,0x10,
	0x3c,0xc6,0xfc,0x3c,0xf8,0xfe,0xf0,0x1a,0xe7,0xfe,0x38,0xe3,0xfe,0xc6,0xc2,0x38,
	0xf0,0x38,0xe3,0xb8,0x7c,0x3c,0x10,0x44,0xc6,0x7c,0xfe,0x10,0x7c,0x10,0x00,0x00,
	0x00,0x7b,0x78,0x3c,0x3e,0x3c,0x7c,0x82,0xe7,0xff,0x82,0xe3,0xff,0xdb,0xe7,0x38,
	0x40,0x04,0xfc,0xbc,0x1c,0x39,0x10,0x44,0xee,0x90,0xfe,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x38,0x00,0x48,0x10,0x8c,0x00,0x00,0x04,0x40,0x00,0x00,0x20,0x00,0x40,0x80,
	0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x10,0x04,0x00,0x40,0x38,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x10,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x00,0x00,0x44,0x00,0x00,0x00,0x00,0x00,
	0x40,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0x00,0x08,0x10,0x20,0x00,0x00},
       {0x00,0x10,0x00,0x00,0x10,0x80,0x00,0x00,0x02,0x80,0x00,0x00,0xc0,0x00,0x00,0x80,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x80,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x00,0xf0,0x00,0xfe,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7c,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x00,
	0xf0,0x1e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x06,0x10,0xc0,0x00,0x00}};
#if OLD_FORMAT
  static int16_w e_table[256]={
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002A,0x002C,0x002D,0x002E,0x0030,0x0031,
    0x0033,0x0034,0x0036,0x0037,0x0039,0x003A,0x003C,0x003E,
    0x0040,0x0042,0x0044,0x0046,0x0048,0x004A,0x004C,0x004E,
    0x0050,0x0053,0x0055,0x0058,0x005A,0x005D,0x0060,0x0062,
    0x0065,0x0068,0x006B,0x006E,0x0072,0x0075,0x0078,0x007C,
    0x0080,0x0083,0x0087,0x008B,0x008F,0x0093,0x0098,0x009C,
    0x00A1,0x00A6,0x00AA,0x00AF,0x00B5,0x00BA,0x00BF,0x00C5,
    0x00CB,0x00D1,0x00D7,0x00DD,0x00E4,0x00EA,0x00F1,0x00F8,
    0x0100,0x0107,0x010F,0x0117,0x011F,0x0127,0x0130,0x0139,
    0x0142,0x014C,0x0155,0x015F,0x016A,0x0174,0x017D,0x018A,
    0x0196,0x01A2,0x01AE,0x01BB,0x01C8,0x01D5,0x01E3,0x01F1,
    0xFE00,0xFE0F,0xFE1D,0xFE2B,0xFE38,0xFE45,0xFE52,0xFE5E,
    0xFE6A,0xFE75,0xFE81,0xFE8C,0xFE96,0xFEA1,0xFEAB,0xFEB4,
    0xFEBE,0xFEC7,0xFED0,0xFED9,0xFEE1,0xFEE9,0xFEF1,0xFEF9,
    0xFF00,0xFF08,0xFF0F,0xFF16,0xFF1C,0xFF23,0xFF29,0xFF2F,
    0xFF35,0xFF3B,0xFF41,0xFF46,0xFF4B,0xFF51,0xFF56,0xFF5A,
    0xFF5F,0xFF64,0xFF68,0xFF6D,0xFF71,0xFF75,0xFF79,0xFF7D,
    0xFF80,0xFF84,0xFF88,0xFF8B,0xFF8E,0xFF92,0xFF95,0xFF98,
    0xFF9B,0xFF9E,0xFFA0,0xFFA3,0xFFA6,0xFFA8,0xFFAB,0xFFAD,
    0xFFB0,0xFFB2,0xFFB4,0xFFB6,0xFFB8,0xFFBA,0xFFBC,0xFFBE,
    0xFFC0,0xFFC2,0xFFC4,0xFFC6,0xFFC7,0xFFC9,0xFFCA,0xFFCC,
    0xFFCD,0xFFCF,0xFFD0,0xFFD2,0xFFD3,0xFFD4,0xFFD6,0xFFD7,
    0xFFD8,0xFFD9,0xFFDA,0xFFDB,0xFFDC,0xFFDD,0xFFDE,0xFFDF,
    0xFFE0,0xFFE1,0xFFE2,0xFFE3,0xFFE4,0xFFE5,0xFFE6,0xFFE7,
    0xFFE8,0xFFE9,0xFFEA,0xFFEB,0xFFEC,0xFFED,0xFFEE,0xFFEF,
    0xFFF0,0xFFF1,0xFFF2,0xFFF3,0xFFF4,0xFFF5,0xFFF6,0xFFF7,
    0xFFF8,0xFFF9,0xFFFA,0xFFFB,0xFFFC,0xFFFD,0xFFFE,0xFFFF};
   /* moved to winlib.h */
/*  int e_ch[241]={
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
    0x0080,0x0081,0x0082,0x0083,0x0084,0x0085,0x0086,0x0087,
    0x0088,0x0089,0x008A,0x008B,0x008C,0x008D,0x008E,0x008F,
    0x0090,0x0091,0x0092,0x0093,0x0094,0x0095,0x0096,0x0097,
    0x0098,0x0099,0x009A,0x009B,0x009C,0x009D,0x009E,0x009F,
    0x00A0,0x00A1,0x00A2,0x00A3,0x00A4,0x00A5,0x00A6,0x00A7,
    0x00A8,0x00A9,0x00AA,0x00AB,0x00AC,0x00AD,0x00AE,0x00AF,
    0x00B0,0x00B1,0x00B2,0x00B3,0x00B4,0x00B5,0x00B6,0x00B7,
    0x00B8,0x00B9,0x00BA,0x00BB,0x00BC,0x00BD,0x00BE,0x00BF,
    0x00C0,0x00C1,0x00C2,0x00C3,0x00C4,0x00C5,0x00C6,0x00C7,
    0x00C8,0x00C9,0x00CA,0x00CB,0x00CC,0x00CD,0x00CE,0x00CF,
    0x00D0,0x00D1,0x00D2,0x00D3,0x00D4,0x00D5,0x00D6,0x00D7,
    0x00D8,0x00D9,0x00DA,0x00DB,0x00DC,0x00DD,0x00DE,0x00DF,
    0x00E5,0x00E6,0x00E7,0x00E8,0x00E9,0x00EA,0x00EB,0x00EC,
    0x00ED,0x00EE,0x00EF,0x00F0,0x00F1,0x00F2,0x00F3,0x00F4,
    0x00F5};*/
#endif

  struct File_Ptr
    {
    size_t        offset;  /* 64bit ok? */
    WIN_bs        size;  
    uint8_w       time[WIN_TM_LEN + 2];  /* last 2 bytes are padding */
    };
  struct Pick_Time
    {
    int valid;    /* 1:valid, 0:invalid / max_defl(float) */
    int sec1;     /* sec(1) / sec of max_defl */
    int msec1;    /* msec(1) / msec of max_defl */
    int sec2;     /* sec(2) */
    int msec2;    /* msec(2) */
    int polarity; /* polarity +1/0/-1 / unit(power of (s)) */
    int period;   /* period (ms) */
    int sec_sdp;  /* last quiet sec */
    };
  struct Filter
    {
    char kind[12];
    double fl,fh,fp,fs,ap,as;
    double *coef;
    int order;
    };
  struct Stn
    {
    char name[STNLEN],comp[CMPLEN];
    char unit[9];       /* "m", "m/s", "m/s/s" or "*****" */
    char invert;
    int16_w scale;
    uint16_w rflag;
    int order;
    int ch_order;
    int32_w offset;     /* offset */
    float north,east;   /* lat & long in deg */
    float x,y;          /* in km */
    long z;             /* in m */   /* 64bit ok */
    float stcp,stcs;    /* in s */
    float units_per_bit;/* units/LSB */
    int16_w psup;       /* 1 for paste-up, otherwise 0 */
    int16_w psup_done;  /* 1 for done */
    int16_w psup_scale; /* scale for paste-up */
    float delta;        /* epicentral distance (km) */
    int16_w azimuth;    /* azimuth from the epicentre */ 
    float ratio;        /* maximum STA/LTA */
    };
  struct Fnl {
    char stn[STNLEN],pol[5];
    double delta,azim,emerg,incid,pt,pe,pomc,st,se,somc,amp,mag;
    int idx;
    };
  struct Hypo {
    int valid,tm[5],tm_c[7],ndata,ellipse;
    char diag[10],textbuf[100];
    double se,se_c,alat,along,dep,mag,xe,ye,ze,c[3][3],
      alat0,along0,dep0,xe0,ye0,ze0,pomc_rms,somc_rms;
    struct Fnl *fnl;
    };
static struct File_Table     /* Data File Information */
    {
    char data_file[NAMLEN];   /* data file name */
    char dat_file[NAMLEN];    /* temporary data file name */
    char param_file[NAMLEN];  /* parameter file name */
    char filt_file[NAMLEN];   /* filter file name */
    char seis_file[NAMLEN];   /* seis file for hypo */
    char init_file[NAMLEN];   /* init file for hypo */
    char rept_file[NAMLEN];   /* rept file for hypo */
    char finl_file[NAMLEN];   /* finl file for hypo */
    char seis_file2[NAMLEN];  /* seis file for hypo (2) */
    char init_file2[NAMLEN];  /* init file for hypo (2) */
    char rept_file2[NAMLEN];  /* rept file for hypo (2) */
    char finl_file2[NAMLEN];  /* finl file for hypo (2) */
    char othrs_file[NAMLEN];  /* othrs file for map */
    char mech_file[NAMLEN];   /* mech file for mecha() */
    char save_file[NAMLEN];   /* pick/hypo/mech data file */
    char map_file[NAMLEN];    /* map data file */
    char mon_file[NAMLEN];    /* bitmap save file */
    char log_file[NAMLEN];    /* log file */
    char hypo_dir[NAMLEN];    /* pick dir */
    char hypo_dir1[NAMLEN];   /* pick dir(2) */
    char final_opt[NAMLEN];   /* final file(dir) by command line option */
    char pick_server[NAMLEN]; /* pick file server host name or IP address */
    unsigned short pick_server_port; /* pick file server TCP port */
    FILE *fp_log;             /* fp of log file */
    char mailer[NAMLEN];      /* mailer printer */
    struct Hypo hypo;         /* final data from hypo */
    struct Hypo hypoall;      /* final data from hypo (TT of all stations) */
    char label_file[NAMLEN];  /* label file */
    int fd;                   /* fd for data file */
    int fd_save;              /* fd for save file */
    int32_w len;              /* data length in sec (old: int) */
    int32_w n_ch;             /* number of channels (old: int) */
    int len_mon;              /* normal width of mon in sec */
    int w_mon;                /* normal width of mon in pixels */
    int w_mon_last;           /* w_mon of last bitmap */
    int n_mon;                /* n of mon bitmaps */
    uint8_w *secbuf;          /* one sec buffer */
    int32_w ptr_secbuf;       /* one sec buffer pointer */
    struct File_Ptr *ptr;     /* location & time of each sec */
    int32_w n_ch_ex;          /* number of channels (inc. no data ) (OLD:int) */
    WIN_sr sr_max;            /* max of sampling rate (old: int) */
    int n_filt;               /* n of filters */
    int n_label;              /* n of labels */
    int label_idx;            /* label index */
    int ch_exclusive;         /* label index */
    struct Filter filt[N_FILTERS];  /* filter parameters */
    int ch2idx[N_CH_NAME];    /* index for each sys_ch */
    int16_w ch_use[N_CH_NAME];  /* use(1) or not use(0) */
    WIN_ch *idx2ch;   /* sys_ch for each index
                  (from the order in the first sec) */
    struct Stn *stn;          /* station parameters */
    int16_w *pos2idx;           /* index for each position */
    int16_w *idx2pos;           /* position for each index */
    WIN_sr *sr;               /* SR (old: short!! .sv format changed!!) */
    char label[N_LABELS][20]; /* labels */
    struct Pick_Time (*pick)[4];  /* phase times : P,S,X times */
    struct Pick_Time (*pick_save)[4]; /* phase times : P,S,X times */
    struct Pick_Time (*pick_calc)[4]; /* phase times : P,S,X times */
    struct Pick_Time pick_calc_ot;    /* origin time for 'C' */
    int16_w *trigch;          /* idx list of chs for trig */
    int n_trigch;             /* n of chs for trig */
    int period;               /* averaged period */ 
    } ft;
  /*****************************************************************
  relations of sys_ch <-> index <-> position
  sys_ch  =ft.idx2ch[index] ( =ft.idx2ch[ft.pos2idx[position]] )
  index =ft.ch2idx[sys_ch]
  position=ft.idx2pos[index]  ( =ft.idx2pos[ft.ch2idx[sys_ch]] )
  index =ft.pos2idx[position]
   *****************************************************************/
  struct Filt
    {
    char tfilt[14];   /* filter text */
    int m_filt;       /* order of filter */
    int n_filt;       /* order of Butterworth function */
    double gn_filt;   /* gain factor of filter */
    double coef[MAX_FILT*4]; /* filter coefficients */
    };
static struct Psup
    {
    int valid;
    int x1,x2;
    int x1_init,x2_init;
    int t1,t2;
    double vred;
    int clip;
    int filt;       /* filter index */
    int xx1,yy1,xx2,yy2;  /* rectangle in display coordinate */
    double pixels_per_sec;
    double pixels_per_km;
    int ot[7];
    struct Filt f;
    } pu,pu_new;
static struct Zoom_Window
    {
    int valid;       /* 1: valid, 0:invalid */
    int length;      /* in sec */
    int sec;         /* counted from the top */
    int length_save; /* in sec (to be saved) */
    int sec_save;    /* counted from the top (to be saved) */
    int scale;       /* amplitude scale */
    int w_scale;     /* amplitude scale */
    int nounit;      /* if 1, show raw amplitude */
    int offset;      /* offset nulling,  1:on, 0:off */
    int32_w zero;    /* offset level */
    int integ;       /* integral flag */
    WIN_sr sr;       /* sampling rate (OLD : int) */
    WIN_ch sys_ch;   /* sys & ch (OLD : int) */
    int pos;         /* trace position in mon */
    int pos_save;    /* trace position in mon (to be saved) */
    int pixels;      /* pixels per sec */
    int shift;       /* time shift in zoom window (ms) */
    int filt;        /* filter index */
    struct Filt f;   /* filter structure */
    } zoom_win[N_ZOOM_MAX];
  struct ms_event event;
  typedef struct {
    float lat,lon,x,y,d;
    long t;  /* 64bit ok */
    char m,blast,o[4],s,ss;
    } HypoData;     /* 24 bytes / event */

typedef struct
    {
    int n_min_trig;
    int n_trig_off;
    int trigger;
    double dist1,dist2;
    int ms_on,ms_off;
    int sub_rate;
    double lt,st,ratio;
    double ap,as,fl,fh,fs;
    } Evdet;

typedef struct
    {
    double ttp;
    double wp,ws;
    } Aph;

typedef struct
    {
    int status;   /* 0:OFF, 1:ON but not comfirmed */
            /* 2:ON confirmed, 3:OFF but not confirmed */
    struct Pick_Time pt;  /* P time */
    int score;        /* trigger score */
    int dis;        /* channel that disabled this channel */
    int flag;       /* enable flag */
    int use;        /* 0:unuse 1:use */
    double sd;        /* SD for AR model */
    double sdp;       /* SD of last sec */
    int sec_sdp;      /* sec where SD decreased */
    double c[MAX_FILT*4]; /* AR coefficient */
    double rec[MAX_FILT*4]; /* buffer for digfil */
    int m;          /* order of AR model */
    double zero;      /* offset level */
    int on_sec;       /* time of on (sec) */
    int on_msec;      /* time of on (msec) */
    int count_on;     /* on sample counter */
    int count_off;      /* off sample counter */
    double res[4];      /* reserved sapmles to kill spikes */
    double lta;
    double lta_save;
    double sta;
    double al,bl;
    double as,bs;
    double ratio;     /* maximum ratio during event */
    struct Filt f;
    double uv[MAX_FILT*4];
    } Evdet_Tbl;

static int n_zoom_max,n_zoom,main_mode,
    loop,loop_stack[5],loop_stack_ptr,x_zero_max,
    y_zero_max,width_win_mon,height_win_mon,width_mon,mailer_flag,
    width_mon_max,height_mon,flag_change,width_info,height_info,
    not_save,other_epis,map_only,flag_save,
    auto_flag,auto_flag_hint,background,doing_auto_pick,x_win_mon,y_win_mon,
    x_win_info,y_win_info,x_zero,y_zero,expose_list,width_win_info,
    map_vert,width_horiz,height_horiz,map_dir,copy_file,ratio_vert,
    map_name,ppk_idx,map_true,map_vstn,map_ellipse,bye,got_hup,
    s_cursor,flag_hypo,flag_mech,nplane,black,pixels_per_trace,ppt_half,
    width_frame,height_frame,width_zoom,height_zoom,mech_name,
    map_mode,x_time_file,x_time_now,init_dep,
    init_depe,init_dep_init,init_depe_init,com_dep1,com_dep2,
    com_depe1,com_depe2,mec_xzero,mec_yzero,map_period,first_map,
    first_map_others,map_update,com_diag1,map_all,map_period_save,
    com_diag2,mecha_mode,hypo_use_ratio,map_f1x,map_f1y,map_f2x,map_f2y,
    map_n_find,map_line,fit_height,read_hypo,map_interval,mon_offset,
    just_hypo,just_hypo_offset,list_on_map,phypo_format,sec_block,autpk_but_off,calc_line_off;

/* removed by Uehira. (2010/06/23) */
/* static int eventmask,readfds,fd_mouse,fd_fb,x_cursor,y_cursor; */

static unsigned int  width_dpy,height_dpy;
static float init_lat_init,init_lon_init,init_late_init,init_lone_init;
static char mec_hemi[10],diagnos[50],apbuf[20],monbuf[20],mapbuf[20],
    map_period_unit,dot;
static double pixels_per_km,lat_cent,lon_cent,mec_rc,alat0,along0,pdpi;
static double  *dbuf,*dbuf2;
static int32_w *buf,*buf2;

static int  do_chck;   /* channel table check  1:check, 0:warning only, -1:no check(default) */

/*********************** prototypes **********************/
static void xgetorigin(Display *, Window, int *, int *, unsigned int *,
		       unsigned int *, unsigned int *, Window *, Window*);   /* check ok 2010.4.3 */
static char *get_time_win(struct YMDhms *, int);   /* check ok 2010.4.3 */
static int invert_dpy(int, int, int);   /* check ok 2010.4.3 */
static char *getname(uid_t);  /* check ok 2010.4.6 */
static FILE *open_file(char *, char *);  /* check ok 2010.4.6 */
static int get_func(int);  /* check ok 2010.4.6 */
static void put_funcs(char **, int);  /* check 2010.4.9 */
static void put_func(char *, int, int, int, int);  /* check 2010.6.10 */
static int x_func(int);  /* check 2010.6.10 */
static void put_fill(lBitmap *, int, int, int, int, int);  /* check 2010.6.10 */
static time_t time2lsec(int *);  /* check 2010.6.10 */
static void lsec2time(time_t, int *);  /* check 2010.6.10 */
static void make_sec_table(void);  /* check?? 2010.6.14 */
static WIN_sr read_one_sec(int32_w, WIN_ch, register int32_w *, int);  /* check?? 2010.6.15 */
static int read_one_sec_mon(int32_w, WIN_ch, register int32_w *, int32_w);  /* check?? 2010.6.15 */
static void print_usage(void);  /* check 2010.6.15 */
static int init_process(int, char *[], int);  /* check?? 2010.6.15 */
static int refresh(int);    /* Is return value really OK? */
static void set_period(int, struct Pick_Time *);   /* check?? 2010.6.15 */
static void set_pick(struct Pick_Time *, int, int, int, int);   /* check?? 2010.6.15 */
static int get_width(struct Pick_Time *);   /* check?? 2010.6.15 */
static void set_width(struct Pick_Time *pt, int, int);   /* check?? 2010.6.15 */
static int show_pick(int, struct Pick_Time *, int);   /* check?? 2010.6.15 */
/*- some structures & macros -*/
static void *alloc_mem(size_t, char *);   /* check?? 2010.6.15 */
static int getdata(int, struct Pick_Time, double **, int *);   /* check?? 2010.6.15 */
static int find_pick(double *, float *, float *, float *, int, int,
		     int, double *);   /* check?? 2010.6.15 */
static int get_range(int, int, float *, float *, float *, int, int, int,
		     int, struct Pick_Time *);   /* check?? 2010.6.15 */
static int pick_phase(int, int);   /* check?? 2010.6.15 */
static int cancel_picks(char *, int);   /* check?? 2010.6.15 */
static int cancel_picks_calc(void);   /* check?? 2010.6.15 */
static int get_pick(char *, int, struct Pick_Time *);   /* check?? 2010.6.15 */
static int auto_pick(int);   /* check?? 2010.6.15 */
static int set_max(double, int, int, int);   /* check?? 2010.6.15 */
static void auto_pick_range(Evdet_Tbl *);   /* check?? 2010.6.15 */
static void auto_pick_pick(int, int);   /* check?? 2010.6.15 */
static void auto_pick_hypo(char *, int);   /* check?? 2010.6.15 */
static int auto_pick_hint(int);   /* check?? 2010.6.15 */
static int auto_pick_single(Evdet_Tbl *, int, int);   /* check?? 2010.6.15 */
static int get_ratio(char *, double *);   /* check?? 2010.6.15 */
static void set_diagnos(char *, char *);   /* check?? 2010.6.15 */
static int pick_s(int, int, int);   /* check?? 2010.6.15 */
static void get_trigch(void);   /* check?? 2010.6.15 */
static int evdet(Evdet *, int);   /* check?? 2010.6.15 */
static void confirm_on(int, int, int, Evdet_Tbl *, Evdet *);   /* check?? 2010.6.15 */
static void confirm_off(int, int, int, Evdet_Tbl *, Evdet *);   /* check?? 2010.6.15 */
static void plot_mon(int, int, register int, uint8_w *);   /* check?? 2010.6.15 */
static void mapconv(int, char *[], int);   /* check?? 2010.6.15 */
static void bye_entry(void);   /* check?? 2010.6.15 */
static void end_process(int);   /* check?? 2010.6.15 */
static void set_geometry(void);   /* check?? 2010.6.15 */
/*--------------- main()--------------------*/
int main(int, char *[]);  /* check 2010.6.23 */
/*--------------- main()--------------------*/
static void bye_process(void);  /* check 2010.6.17 */
static void do_auto_pick(void);  /* check 2010.6.17 */
static void do_auto_pick_hint(void);  /* check 2010.6.17 */
static void do_just_hypo(void);  /* check 2010.6.17 */
static void proc_alarm(void);  /* check 2010.6.17 */
static void do_map(void);  /* check 2010.6.17 */
static void window_main_loop(void);  /* check 2010.6.17 */
static void open_save(void);  /* check 2010.6.17 */
static void plot_zoom(int, int, struct Pick_Time *, int);  /* check 2010.6.23 */
static void close_zoom(int);  /* check 2010.6.17 */
static void proc_main(void);  /* check 2010.6.23 */
static int measure_max_zoom(int);  /* check 2010.6.17 */
static int get_max(double *, int, int *, int *, int *, int *);  /* check 2010.6.17 */
static void put_function(void);  /* check 2010.6.17 */
static void put_function_map(void);  /* check 2010.6.17 */
static void put_init_depth(void);  /* check 2010.6.17 */
static void put_function_mecha(void);  /* check 2010.6.17 */
static void put_function_psup(void);  /* check 2010.6.17 */
static int put_main(void);  /* check 2010.6.17 */
static void get_screen_type(int *, unsigned int *, unsigned int *,
			    int *, int *);  /* check 2010.6.17 */
static void draw_ellipse(int, int, double, double, double, int,
			 int, lBitmap *, int, int, int, int);  /* check 2010.6.18 */
static void draw_circle(int, int, int, int, int, lBitmap *);  /* check 2010.6.18 */
static void draw_seg(int, int, int, int, int, int, lBitmap *);  /* check 2010.6.18 */
static void draw_rect(int, int, int, int, int, int, lBitmap *);  /* check 2010.6.18 */
static void draw_line(lPoint *, int, int,
		      int, lBitmap *, int, int, int, int, int);  /* check 2010.6.18 */
static void put_mark_zoom(int, int, struct Pick_Time *, int);  /* check 2010.6.17 */
static void put_mon(int, int);  /* check 2010.6.17 */
static void put_bitblt(lBitmap *, int, int, int, int,
 		       lBitmap *, int, int, int);  /* check 2010.6.18 */
static void define_bm(lBitmap *, char, unsigned int, unsigned int, char *);  /* check?? 2010.6.16 */
static void invert_bits(uint8_w *, register int);  /* check?? 2010.6.16 */
static void put_text(lBitmap *, int, int, char *, int);  /* check 2010.6.18 */
static int put_mark(int, int, int);  /* check 2010.6.17 */
static void put_mark_mon(int, int);  /* check 2010.6.17 */
static void make_visible(int);  /* check 2010.6.17 */
static void list_line(void);  /* check 2010.6.17 */
static void raise_ttysw(int);  /* check 2010.6.17 */
static void adj_sec_win(int *, double *, int *, double *);  /* check 2010.6.17 */
static void get_calc(void);  /* check 2010.6.17 */
#if HINET_EXTENTION_3>=2
static int load_data_prep(int);  /* check 2010.6.18 */
#endif  /* HINET_EXTENTION_3>=2 */
#if HINET_EXTENTION_3
static int replot_mon(int);   /* check?? 2010.6.16 */
static int reorder(void);   /* check?? 2010.6.16 */
static int get_delta(void);  /* check?? 2010.6.16 */
#endif  /* HINET_EXTENTION_3 */
static void locate(int, int);   /* check?? 2010.6.16 */
static void list_picks(int);   /* check?? 2010.6.16 */
static void list_finl(int);   /* check?? 2010.6.16 */
static void output_all(FILE *);   /* check?? 2010.6.16 */
static void output_pick(FILE *);   /* check?? 2010.6.16 */
static void wait_mouse(void);   /* check?? 2010.6.16 */
static void hard_copy(int);   /* check?? 2010.6.16 */
static void draw_ticks(int, int, int, int, int, int, int);   /* check?? 2010.6.16 */
/* some macros */
static void draw_coord(int, int, double, double, double, double, int, int,
		       int, double, double, double, double);  /* check 2010.6.18 */
static int km2pixel(int, int, int, int, int, int, int, double,
		    double, double, double, int *, int *, double, double);  /* check 2010.6.18 */
static void phypo(int, int, HypoData *);  /* check 2010.6.18 */
static void put_time1(long, long, long *, long *, int, int, int, int, int,
		      char *);   /* check?? 2010.6.16 */
static void put_time2(long, long, long *, long *, int, int, int, int, int,
		      char *);  /* check?? 2010.6.16 */
static int draw_ticks_ts(int, long, int, int, int);  /* check?? 2010.6.16 */
static int draw_ticks_ts2(int, long, int, int, int *, int);  /* check?? 2010.6.16 */
static int put_map(int);  /* check 2010.6.18 */
static void init_map(int);  /* check 2010.6.18 */
static int check(int, int *, int, int);  /* check 2010.6.18 */
static int check_year(int, int *, int, int);  /* check 2010.6.19 */
static void proc_map(void);  /* check 2010.6.18 */
static int open_sock(char *, unsigned short);  /* check 2010.6.17 */
static int load_data(int);  /* check 2010.6.18 */
static void init_mecha(void);  /* check 2010.6.18 */
static void proc_mecha(void);  /* check 2010.6.18 */
static void xy_pt(int *, int *, double *, double *, int);  /* check 2010.6.18 */
static int read_final(char *, struct Hypo *);  /* check 2010.6.18 */
/* static void str2double(char *, int, int, double *); */
static int put_mecha(void);  /* check 2010.6.18 */
static void switch_psup(int, int);  /* check 2010.6.18 */
static void init_psup(void);  /* check 2010.6.17 */
static void bell(void);  /* check 2010.6.17 */
/* some macros */
static void proc_psup(void);  /* check 2010.6.18 */
static int put_psup(void);  /* check 2010.6.18 */
static void plot_psup(int);  /* check 2010.6.18 */
static int save_data(int);  /* check 2010.6.18 */
static int read_parameter(int, char *);  /* check 2010.6.17 */
static int read_filter_file(void);  /* check 2010.6.17 */
static int read_label_file(void);  /* check 2010.6.17 */
static void get_filter(int, struct Filt *, WIN_sr, int);  /* check 2010.6.17 */
static int form2(double, char *);  /* check 2010.6.17 */
/* static void pltxy(double, double, double *, double *, double *, double *, int); */
static int getar(double *, int, double *, int *, double *, double *, int);  /* check?? 2010.6.16 */
static void digfil(double *, double *, int, double *, int, double *, double *);  /* check?? 2010.6.16 */
static void mat_sym(double (*)[3]);  /* check?? 2010.6.16 */
static void mat_copy(double (*)[3], double(*)[3]);  /* check?? 2010.6.16 */
static void mat_mul(double (*)[3], double (*)[3], double (*)[3]);  /* check?? 2010.6.16 */
/* static void mat_print(char *, double (*)[3]); */
static void get_mat(double, double, double (*)[3]);  /* check?? 2010.6.16 */
static long time2long(int, int, int, int, int);  /* check?? 2010.6.16 */
static void long2time(struct YMDhms *, long *);  /* check?? 2010.6.16 */
static int time_cmp_win(int *, int *, int);  /* check?? 2010.6.16 */
static void fill(int *, int, int);  /* check?? 2010.6.16 */
static void emalloc(char *);  /* check?? 2010.6.16 */
static void writelog(char *);  /* check?? 2010.6.16 */
/*********************** END prototypes **********************/

static void
xgetorigin(Display  *d, Window w, int *x, int *y, unsigned int *wi,
	   unsigned int *h, unsigned int *de, Window *ro, Window *pa)
  {
  Window child,*children;
  int xx,yy;
  unsigned int b,nchildren;

  XGetGeometry(d,(Drawable)w,ro,&xx,&yy,wi,h,&b,de);
  XQueryTree(d,w,ro,pa,&children,&nchildren);
  XFree((void *)children);
  XTranslateCoordinates(d,*pa,*ro,xx,yy,x,y,&child);
  }

static char *
get_time_win(struct YMDhms *rt, int addsec)
  {
  static char c[18];
  struct tm *nt;
  time_t ltime;

  time(&ltime);
  ltime+=addsec;
  nt=localtime(&ltime);
  if(rt)
    {
    rt->se=nt->tm_sec;
    rt->mi=nt->tm_min;
    rt->ho=nt->tm_hour;
    rt->da=nt->tm_mday;
    rt->mo=nt->tm_mon+1;
    rt->ye=nt->tm_year%100;
    }
  snprintf(c,sizeof(c),"%02d/%02d/%02d %02d:%02d:%02d",
	   nt->tm_year%100,nt->tm_mon+1,
	   nt->tm_mday,nt->tm_hour,nt->tm_min,nt->tm_sec);
  return (c);
  }

static int
invert_dpy(int sbmtype, int dbmtype, int func)
  {

  if(black) return (func);  /* if black>0, return */
  if(dbmtype==BM_FB)
    switch(func)
      {
      case BF_SDXI: return (BF_SDX);
      case BF_SDX:  return (BF_SDXI);
      case BF_SIDA: return (BF_SIDO);
      case BF_SDO:  return (BF_SDA);
      }
  return (func);
  }

/* alternative to 'getlogin()' which doesn't work in 'su' */
static char *
getname(uid_t  uid)
  {
  struct passwd *pwd;

  pwd=getpwuid(uid);
  if(pwd!=NULL) return (pwd->pw_name);
  else return (NULL);
  }

static FILE *
open_file(char *fn, char *fs)
  {
  FILE *fp;

  if((fp=fopen(fn,"r"))==NULL)
    fprintf(stderr,"%s file '%s' not found\007\n",fs,fn);
  return (fp);
  }

static int
get_func(int x)
  {
  int xx,i;

  if(x<WB) return (0);
  xx=width_dpy-WB;
  i=1;
  while(xx>0)
    {
    if(x>=xx) return (i);
    xx-=WB+HW;
    i++;
    }
  return (-1);
  }

static void
put_funcs(char **func, int y)
  {
  int i;

  i=0;
  while(*func[i]!='$')
    {
    put_func(func[i],i,y,0,0);
    i++;
    }
  }

static void
put_func(char *func, int n, int y, int idx, int h)
  /* int n;    func index */
  /* int idx;  0:white on black, 1:black on white, 2:reverse */
  /* int h;    0:MARGIN, 1:HEIGHT_TEXT */
  {
  int x,len,hi,yh;

  if((len=strlen(func))==0) return;
  if(n==0) x=0;
  else x=width_dpy-(WB+HW)*(n-1)-WB;
  if(h==0)
    {
    hi=MARGIN;
    yh=(MARGIN-HEIGHT_TEXT)/2;
    }
  else
    {
    hi=HEIGHT_TEXT;
    yh=0;
    }
  if(idx==0)
    {
    put_black(&dpy,x,y,WB,hi);
    put_text(&dpy,x+(WB-len*WIDTH_TEXT)/2,y+yh,func,BF_SI);
    }
  else if(idx==1)
    {
    put_white(&dpy,x,y,WB,hi);
    put_text(&dpy,x+(WB-strlen(func)*WIDTH_TEXT)/2,y+yh,func,BF_S);
    }
  else if(idx==2) put_reverse(&dpy,x,y,WB,hi);
  }

static int
x_func(int n)
  {

  if(n==0) return (0);
  else return (width_dpy-(WB+HW)*(n-1)-WB);
  }

static void
put_fill(lBitmap *bm, int xzero, int yzero, int xsize, int ysize, int blk)
  /*int blk;   if 1 then black else white */
  {
  GC *gc;

  if(background) return;
  if(bm->type==BM_FB)
    {   
    if(blk) gc=(&gc_fb); 
    else gc=(&gc_fbi);
    }                   
  else
    {
    if(blk) gc=(&gc_mem);
    else gc=(&gc_memi);
    }
  XSetFunction(disp,*gc,BF_S);
  XFillRectangle(disp,bm->drw,*gc,xzero,yzero,
		 (unsigned int)xsize,(unsigned int)ysize);
  XFlush(disp);
  }

static time_t
time2lsec(int *tarray)
{
  static struct tm tm;

  tm.tm_year=tarray[0];
  if(tm.tm_year<WIN_YEAR) tm.tm_year+=100;
  tm.tm_mon=tarray[1]-1;
  tm.tm_mday=tarray[2];
  tm.tm_hour=tarray[3];
  tm.tm_min=tarray[4];
  tm.tm_sec=tarray[5];
  return (mktime(&tm));
}

static void
lsec2time(time_t sec, int *tarray)
{
  struct tm *tm;

  tm=localtime(&sec);
  if((tarray[0]=tm->tm_year)>=100) tarray[0]-=100;
  tarray[1]=tm->tm_mon+1;
  tarray[2]=tm->tm_mday;
  tarray[3]=tm->tm_hour;
  tarray[4]=tm->tm_min;
  tarray[5]=tm->tm_sec;
}

static void
make_sec_table(void)
  {
  int i,ii,j,tm[WIN_TM_LEN];
  size_t  ptr;
  WIN_ch  chtmp;
  WIN_sr  sr;
  WIN_bs  size,size_max;
  time_t lsec_done=0,lsec;  /* just for suppress warning */
  uint8_w   c[4],t[WIN_TM_LEN];
#if OLD_FORMAT
   uint32_w gh;
#else
   uint8_w  gh[5];
#endif

  if(flag_save==1)  /* LOAD */
    {
    read(ft.fd_save,&ft.len,sizeof(ft.len));
    read(ft.fd_save,&ft.n_ch,sizeof(ft.n_ch));
    read(ft.fd_save,&size_max,sizeof(size_max));
    if((ft.idx2ch=(WIN_ch *)win_xmalloc(sizeof(*ft.idx2ch)*ft.n_ch))==NULL)
      emalloc("ft.idx2ch");
    if((ft.sr=(WIN_sr *)win_xmalloc(sizeof(*ft.sr)*ft.n_ch))==NULL) emalloc("ft.sr");
    if((ft.ptr=(struct File_Ptr *)win_xmalloc(sizeof(*ft.ptr)*ft.len))==NULL)
      emalloc("ft.ptr");
    read(ft.fd_save,ft.ptr,sizeof(*ft.ptr)*ft.len);
    read(ft.fd_save,&ft.sr_max,sizeof(ft.sr_max));
    read(ft.fd_save,ft.idx2ch,sizeof(*ft.idx2ch)*ft.n_ch);
    for(i=0;i<ft.n_ch;i++) ft.ch2idx[ft.idx2ch[i]]=i;
    read(ft.fd_save,ft.sr,sizeof(*ft.sr)*ft.n_ch);
    }
  else
    {
    if((ft.ptr=(struct File_Ptr *)win_xmalloc(sizeof(*(ft.ptr))*100))==NULL)
      emalloc("ft.ptr");
  /* get the number of sec blocks and make sec pointers */
reset_blockmode:
#if HINET_WIN32
    lseek(ft.fd,(off_t)4,0);  /* BOF */
    ft.len=ptr=size_max=i=ii=0;
    ptr+=4;
    while(read(ft.fd,c,1))
      {
      read(ft.fd,(uint8_w *)ft.ptr[ft.len].time,WIN_TM_LEN);
      lseek(ft.fd,(off_t)5,1);
      read(ft.fd,c,4);
      size=(WIN_bs)mkuint4(&c[0]);
      if(size==0) break;
      else if((size+16)>size_max) size_max=size+16;
      size+=16;
#else
    lseek(ft.fd,(off_t)0,0);  /* BOF */
    ft.len=ptr=size_max=i=ii=0;
    while(read(ft.fd,c,4))
      {
      size=(WIN_bs)mkuint4(&c[0]);
      if(size==0) break;
      else if(size>size_max) size_max=size;
      read(ft.fd,(uint8_w *)ft.ptr[ft.len].time,WIN_TM_LEN);
#endif
      bcd_dec(tm,ft.ptr[ft.len].time);
      if(sec_block && ft.ptr[ft.len].time[5]==0){
        if(i>=2 && i==ii){
          sec_block=0;
          writelog("not assuming second blocks");
          goto reset_blockmode;
        }
        ii++;
      }
      i++;
      lsec=time2lsec(tm);
      if(sec_block && ft.len>0)
        {
        if(lsec_done<lsec-1)
          {
          memcpy(t,ft.ptr[ft.len].time,WIN_TM_LEN);
          while(lsec_done<lsec-1)
            {
            lsec2time(++lsec_done,tm);
            dec_bcd(ft.ptr[ft.len].time,tm);
            ft.ptr[ft.len].size=0;
            ft.ptr[ft.len++].offset=ptr;
            if(ft.len%100==0)
              if((ft.ptr = (struct File_Ptr *)win_xrealloc(ft.ptr,sizeof(*(ft.ptr))*(ft.len+100)))==NULL)
		emalloc("ft.ptr");
            }
          memcpy(ft.ptr[ft.len].time,t,WIN_TM_LEN);
          }
        }
      lsec_done=lsec;
      ft.ptr[ft.len].size=size;
      ft.ptr[ft.len++].offset=ptr;
      if(ft.len%100==0)
        if((ft.ptr=(struct File_Ptr *)win_xrealloc(ft.ptr,sizeof(*(ft.ptr))*(ft.len+100)))==NULL)
	  emalloc("ft.ptr");
      lseek(ft.fd,(off_t)(ptr+=size),0);
      }
    if(ft.len==0)
      {
      fprintf(stderr,"data file empty !\007\n");
      end_process(1);
      }
  /* get structure of one sec from the first 3 sec */
    for(ii=0;ii<2;ii++)
      {
      ft.n_ch=0;
      for(j=0;j<3;j++)
        {
        if(j==ft.len) break;
        ptr=ft.ptr[j].offset+10;  /* locate to the first second */
#if HINET_WIN32
	ptr+=6;
#endif
        while(ptr<ft.ptr[j].offset+ft.ptr[j].size)
          {
#if HINET_WIN32
	  ptr+=2;
#endif
          lseek(ft.fd,(off_t)ptr,0);
#if OLD_FORMAT
          read(ft.fd,c,4);
          gh=(c[0]<<24)+(c[1]<<16)+(c[2]<<8)+c[3];
          i=gh>>16;
          if((i&0xff00)==0) i=e_ch[i%241];
          if((sr=gh&0xfff)>ft.sr_max) ft.sr_max=sr;
          ptr+=((gh>>12)&0xf)*sr+4;
#else  /* OLD_FORMAT */
	  read(ft.fd,&gh[0],5);
	  ptr += win_get_chhdr(&gh[0], &chtmp, &sr);
	  i = chtmp;
	  if (sr > ft.sr_max)
	    ft.sr_max=sr;
#endif  /* OLD_FORMAT */
          if(sr<SR_LOW) continue;   /* exclude ch with sr<SR_LOW */
          if(ft.ch_exclusive && ft.ch_use[i]==0) continue;
          if(ft.ch2idx[i]<0)
            {
            ft.ch2idx[i]=ft.n_ch;
            if(ii==1) {ft.sr[ft.n_ch]=sr;ft.idx2ch[ft.n_ch]=i;}
            ft.n_ch++;
            }
          }
        }
      if(ii==0)
        {
        for(i=0;i<N_CH_NAME;i++) ft.ch2idx[i]=(-1);
        if((ft.idx2ch=(WIN_ch *)win_xmalloc(sizeof(*ft.idx2ch)*ft.n_ch))==NULL)
          emalloc("ft.idx2ch");
        if((ft.sr=(WIN_sr *)win_xmalloc(sizeof(*ft.sr)*ft.n_ch))==NULL)
          emalloc("ft.sr");
        }
      }

    if(ft.n_ch==0)
      {
      fprintf(stderr,"data file empty !\007\n");
      end_process(1);
      }
    if(flag_save==2)  /* SAVE */
      {
      write(ft.fd_save,&ft.len,sizeof(ft.len));
      write(ft.fd_save,&ft.n_ch,sizeof(ft.n_ch));
      write(ft.fd_save,&size_max,sizeof(size_max));
      write(ft.fd_save,ft.ptr,sizeof(*ft.ptr)*ft.len);
      write(ft.fd_save,&ft.sr_max,sizeof(ft.sr_max));
      write(ft.fd_save,ft.idx2ch,sizeof(*ft.idx2ch)*ft.n_ch);
      write(ft.fd_save,ft.sr,sizeof(*ft.sr)*ft.n_ch);
      }
    }
  ft.pick=(struct Pick_Time (*)[4])
    win_xmalloc(sizeof(struct Pick_Time)*4*ft.n_ch);
  ft.pick_save=(struct Pick_Time (*)[4])
    win_xmalloc(sizeof(struct Pick_Time)*4*ft.n_ch);
  ft.pick_calc=(struct Pick_Time (*)[4])
    win_xmalloc(sizeof(struct Pick_Time)*4*ft.n_ch);
  for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++)
    ft.pick[i][j].valid=ft.pick_calc[i][j].valid=0;
  ft.pick_calc_ot.valid=0;

/* make one second buffer */
  if((ft.secbuf=(uint8_w *)win_xmalloc(size_max))==NULL) emalloc("ft.secbuf");
  ft.ptr_secbuf=(-1);
  }

static WIN_sr
read_one_sec(int32_w ptr, WIN_ch sys_ch, register int32_w *abuf, int spike)
  /* long ptr,sys_ch;    sys_ch = sys*256 + ch */
  /* int spike;           if 1, eliminate spikes */
  {
#define N_SBUF   600
#define SR_SBUF  100
  static int32_w *sbuf[N_SBUF];
  static struct Sbuf_Index {
    int32_w sec;  /* OLD : unsigned short */
    WIN_sr sr;    /* OLD : short */
    WIN_ch ch;
    } *sbuf_index;
  static int n_sbuf,i_sbuf;
#if OLD_FORMAT
  int sys_channel;
#else
  WIN_ch sys_channel;
#endif
  uint32_w g_size;  /* OLD: int */
  register int i,j;
  WIN_sr s_rate;
  register uint8_w *dp;
#if OLD_FORMAT
  uint32_w gh;
#endif
  uint8_w *ddp;
  int32_w dmax,dmin,drange;

  if(n_sbuf==0)
    {
    sbuf_index=(struct Sbuf_Index *)win_xmalloc(sizeof(*sbuf_index)*N_SBUF);
    for(i=0;i<N_SBUF;i++)
      {
      sbuf_index[i].sr=0;  /* OLD : (short)(-1) */
      if((sbuf[i]=(int32_w *)win_xmalloc(sizeof(int32_w)*SR_SBUF))==NULL) break;
      }
    n_sbuf=i;
    }
  else if(ft.sr[ft.ch2idx[sys_ch]]<=SR_SBUF)
    {
    if((i=i_sbuf-1)<0) i=n_sbuf-1;
    while(i!=i_sbuf && sbuf_index[i].sr>0)
      {
      if(sbuf_index[i].ch==sys_ch && sbuf_index[i].sec==ptr)
        {
        s_rate=sbuf_index[i].sr;
        for(j=0;j<s_rate;j++) abuf[j]=sbuf[i][j];
        return (s_rate);
        }
      if(--i<0) i=n_sbuf-1;
      }
    }

  if(ptr>=ft.len || ft.ptr[ptr].size==0) return (0);
  if(ft.ptr_secbuf!=ptr)
    {
    lseek(ft.fd,(off_t)(ft.ptr[ptr].offset),0);
    read(ft.fd,ft.secbuf,ft.ptr[ptr].size);
    ft.ptr_secbuf=ptr;
    }
  ddp=ft.secbuf+ft.ptr[ptr].size;
  dp=ft.secbuf+10;
#if HINET_WIN32
  dp+=6;
#endif
#if OLD_FORMAT
  for(;;)
    {
    gh=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
    dp+=4;
    s_rate=gh&0xfff;
    g_size=(b_size=(gh>>12)&0xf)*s_rate;
    sys_channel=gh>>16;
    if((sys_channel&0xff00)==0) sys_channel=e_ch[sys_channel%241];
    if(sys_channel==sys_ch) break;
    else if((dp+=g_size)>=ddp) return (0);
    }
/*  if(s_rate!=ft.sr[ft.ch2idx[sys_ch]]) return 0;*/
  /* read group */
  switch(b_size)
    {
    case 1:
      if((sys_channel&0xff00)==0)
        for(i=0;i<s_rate;i++) abuf[i]=e_table[*dp++];
      else
        for(i=0;i<s_rate;i++) abuf[i]=(*(int8_w *)(dp++));
      break;
    case 2:
      for(i=0;i<s_rate;i++)
        {
        shreg=(dp[0]<<8)+dp[1];
        dp+=2;
        abuf[i]=shreg;
        }
      break;
    case 3:
      for(i=0;i<s_rate;i++)
        {
        inreg=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8);
        dp+=3;
        abuf[i]=inreg>>8;
        }
      break;
    case 4:
      for(i=0;i<s_rate;i++)
        {
        abuf[i]=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
        dp+=4;
        }
      break;
    default:
      return (0); /* bad header */
    }
#else  /* from "#if OLD_FORMAT" */
  for(;;)
    {
#if HINET_WIN32
    dp+=2;
#endif
    g_size = get_sysch(dp, &sys_channel);
    
    /* advance pointer or break */
    if (sys_channel == sys_ch)
      break;
    else if ((dp += g_size) >= ddp)
      return (0);
    }
  /* read group */
  if (win2fix(dp, abuf, &sys_channel, &s_rate) == 0)    /* bad header */
    return (0);
  /*  if(s_rate!=ft.sr[ft.ch2idx[sys_ch]]) return 0;*/

  if(spike) for(i=2;i<s_rate-2;i++)
    {
    dmax=dmin=abuf[i-2];
    if(abuf[i-1]>dmax) dmax=abuf[i-1];
    else if(abuf[i-1]<dmin) dmin=abuf[i-1];
    if(abuf[i+1]>dmax) dmax=abuf[i+1];
    else if(abuf[i+1]<dmin) dmin=abuf[i+1];
    if(abuf[i+2]>dmax) dmax=abuf[i+2];
    else if(abuf[i+2]<dmin) dmin=abuf[i+2];
    drange=(dmax-dmin+1)*4;
    if(abuf[i]>dmax+drange || abuf[i]<dmin-drange)
      abuf[i]=(abuf[i-1]+abuf[i+1])/2;
    }

#endif  /* OLD_FORMAT */

  if(ft.stn[ft.ch2idx[sys_ch]].invert)
    for(i=0;i<s_rate;i++) abuf[i]=(-abuf[i]);
  if(s_rate<=SR_SBUF && n_sbuf>0)
    {
    sbuf_index[i_sbuf].ch=sys_ch;
    sbuf_index[i_sbuf].sec=ptr;
    sbuf_index[i_sbuf].sr=s_rate;
    for(j=0;j<s_rate;j++) sbuf[i_sbuf][j]=abuf[j];
    if(++i_sbuf==n_sbuf) i_sbuf=0;
    }
  return (s_rate);  /* normal return */
  }

static int
read_one_sec_mon(int32_w ptr, WIN_ch sys_ch, register int32_w *abuf, int32_w ppsm)
  /* long ptr,sys_ch,ppsm;   sys_ch = sys*256 + ch */
  {
  register int i,k;
  register int32_w  now,y_min,y_max;
  register WIN_sr  sub_rate;  /* OLD : int */
  WIN_sr  s_rate;  /* OLD : int */
  register uint8_w *dp;
  int32_w *abuf_save;
#if OLD_FORMAT
  int sys_channel;
#else
  WIN_ch sys_channel;
#endif
  int b_size;
  uint32_w  g_size;  /* OLD: int */
#if OLD_FORMAT
  uint32_w gh;
#else
  uint8_w gh[5];
#endif
  uint8_w *ddp;
  int16_w shreg;
  int32_w inreg;

  if(ptr>=ft.len || ft.ptr[ptr].size==0) return (0);
  abuf_save=abuf;
  if(ft.ptr_secbuf!=ptr)
    {
    lseek(ft.fd,(off_t)(ft.ptr[ptr].offset),0);
    read(ft.fd,ft.secbuf,ft.ptr[ptr].size);
    ft.ptr_secbuf=ptr;
    }
  dp=ft.secbuf+10;
#if HINET_WIN32
  dp+=6;
#endif
  ddp=ft.secbuf+ft.ptr[ptr].size;

#if OLD_FORMAT
  gh=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
  dp+=4;
  s_rate=gh&0xfff;
  g_size=(b_size=(gh>>12)&0xf)*s_rate;
  sys_channel=gh>>16;
  if((sys_channel&0xff00)==0) sys_channel=e_ch[sys_channel%241];
  if(sys_channel!=sys_ch)
    {
    dp=ft.secbuf+10;
    for(;;)
      {
      gh=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
      dp+=4;
      s_rate=gh&0xfff;
      g_size=(b_size=(gh>>12)&0xf)*s_rate;
      sys_channel=gh>>16;
      if((sys_channel&0xff00)==0) sys_channel=e_ch[sys_channel%241];
      if(sys_channel==sys_ch) break;
      else if((dp+=g_size)>=ddp) return (0);
      }
    }
  /* read group */
  sub_rate=s_rate/ppsm;

  k=0;
  switch(b_size)
    {
    case 1:
      if((sys_channel&0xff00)==0)
        {
        for(i=0;i<s_rate;i++)
          {
          now=e_table[*dp++];
          if(k++==0) y_max=y_min=now;
          else
            {
            if(now>y_max) y_max=now;
            else if(now<y_min) y_min=now;
            if(k==sub_rate)
              {
              *abuf++=y_min;
              *abuf++=y_max;
              k=0;
              }
            }
          }
        }
      else
        {
        for(i=0;i<s_rate;i++)
          {
          now=(*(int8_w *)(dp++));
          if(k++==0) y_max=y_min=now;
          else
            {
            if(now>y_max) y_max=now;
            else if(now<y_min) y_min=now;
            if(k==sub_rate)
              {
              *abuf++=y_min;
              *abuf++=y_max;
              k=0;
              }
            }
          }
        }
      break;
    case 2:
      for(i=0;i<s_rate;i++)
        {
        shreg=(dp[0]<<8)+dp[1];
        dp+=2;
        now=shreg;
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    case 3:
      for(i=0;i<s_rate;i++)
        {
        inreg=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8);
        dp+=3;
        now=inreg>>8;
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    case 4:
      for(i=0;i<s_rate;i++)
        {
        now=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
        dp+=4;
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    default:
      return (0); /* bad header */
    }
#else  /* from "#if OLD_FORMAT" */
#if HINET_WIN32
    dp+=2;
#endif
  gh[0]=dp[0];
  gh[1]=dp[1];
  gh[2]=dp[2];
  gh[3]=dp[3];
  gh[4]=dp[4];
  g_size = win_chheader_info(&gh[0], &sys_channel, &s_rate, &b_size);

  if ((gh[2] & 0x80) == 0x0)
    dp += 4;  /* channel header = 4 byte */
  else
    dp += 5;  /* channel header = 5 byte */

  /*    printf("mon: sys_ch=%04X ch=%04X ch_1=%02X%02X sr=%d gsize=%d\n", */
  /* 	  sys_ch,gh[1]+gh[0]*256,gh[0],gh[1],s_rate,g_size); */
  
  if(sys_channel!=sys_ch) {
    dp=ft.secbuf+10;
#if HINET_WIN32
    dp+=6;
#endif
    for(;;) {
#if HINET_WIN32
      dp+=2;
#endif
      gh[0]=dp[0];
      gh[1]=dp[1];
      gh[2]=dp[2];
      gh[3]=dp[3];
      gh[4]=dp[4];

      g_size = win_chheader_info(&gh[0], &sys_channel, &s_rate, &b_size);
      if (sys_channel == sys_ch) { /* advance pointer and break */
	if ((gh[2] & 0x80) == 0x0)
	  dp += 4;    /* channel header = 4 byte */
	else
	  dp += 5;    /* channel header = 5 byte */
	break;
      } else if ((dp += g_size) >= ddp)
	return (0);
    }  /* for(;;) */
  }  /* if(sys_channel!=sys_ch) */
  /* read group */
  sub_rate=s_rate/ppsm;

  y_min=y_max=now=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
  dp+=4;
  k=1;
  if(sub_rate==0) for(i=0;i<ppsm;i++) {*abuf++=now;*abuf++=now;}
  else switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        now+=((*(int8_w *)dp)>>4);
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
	if (i == s_rate -1)
	  break;
        now+=(((int8_w)(*(dp++)<<4))>>4);
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    case 1:
      for(i=1;i<s_rate;i++)
        {
        now+=(*(int8_w *)(dp++));
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    case 2:
      for(i=1;i<s_rate;i++)
        {
        shreg=(dp[0]<<8)+dp[1];
        dp+=2;
        now+=shreg;
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        inreg=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8);
        dp+=3;
        now+=(inreg>>8);
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        inreg=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
        dp+=4;
        now+=inreg;
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
    case 5:
      for(i=1;i<s_rate;i++)
        {
        now=(dp[0]<<24)+(dp[1]<<16)+(dp[2]<<8)+dp[3];
        dp+=4;
        if(k++==0) y_max=y_min=now;
        else
          {
          if(now>y_max) y_max=now;
          else if(now<y_min) y_min=now;
          if(k==sub_rate)
            {
            *abuf++=y_min;
            *abuf++=y_max;
            k=0;
            }
          }
        }
      break;
    default:
      return (0); /* bad header */
    }
#endif  /* OLD_FORMAT */
  if(ft.stn[ft.ch2idx[sys_ch]].invert) while((abuf-=2)>=abuf_save)
    {
    now=(-(*abuf));
    *abuf=(-(*(abuf+1)));
    *(abuf+1)=now;
    }
  return (1); /* normal return */
  }

static void
print_usage(void)
  {
  fprintf(stderr,"usage of '%s' :\n",NAME_PRG);
  fprintf(stderr,"   %s ([-options]) ([data file name])\n",NAME_PRG);
  fprintf(stderr,"       options:   a    - do 'auto-pick'\n");
  fprintf(stderr,"                  b    - background mode\n");
  fprintf(stderr,"                  c    - mapconv (text -> binary)\n");
  fprintf(stderr,"                  d [file] - hypo data file\n");
  fprintf(stderr,"                  e [file] - exclusively use ch file\n");
  fprintf(stderr,"                  f    - fit window to screen (X only)\n");
  fprintf(stderr,"                  g    - check channel table\n");
  fprintf(stderr,"                  h    - print usage\n");
  fprintf(stderr,"                  i [ch# initial] - exclusively use ch\n");
  fprintf(stderr,"                  m ([period]h/d) ([interval]m) - 'just map' mode\n");
  fprintf(stderr,"                  n    - 'no save' in auto-pick\n");
  fprintf(stderr,"                  o    - remove offset from MON traces\n");
  fprintf(stderr,"                  p [parameter file]\n");
  fprintf(stderr,"                  q    - 'quit' mode\n");
  fprintf(stderr,"                  r    - do 'auto-pick' with prelim. hypo\n");
  fprintf(stderr,"                  s [server(:port)] - pick file server & port\n");
  fprintf(stderr,"                  t    - copy data-file to local\n");
  /* fprintf(stderr,"                  w    - write bitmap (.sv) file\n"); */
  fprintf(stderr,"                  x [pick file] - just calculate hypocenter\n");
  fprintf(stderr,"                  z [mode] - Expert mode\n");
  fprintf(stderr,"                     a     - AUTPK/EVDET Button OFF\n");
  fprintf(stderr,"                     c     - CALC. LINE OFF\n");
  /* fprintf(stderr,"                  B    - don't use bitmap (.sv) file\n"); */
  fprintf(stderr,"                  C [color] - set color of cursor\n");
  fprintf(stderr,"                  S    - don't assume second blocks\n");
  fprintf(stderr,"                  _    - use '_' in pick file names\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"`ee` command may show you a list of event files, but it depends on installation\n");
  }

static int
init_process(int argc, char *argv[], int args)
  {
  struct YMDhms tm;
  FILE *fp;
  DIR *dir_ptr;
  int i,j,jj,k,kk,sys_ch,height,dum1,dum2,use_default_chfile,n_stations=0, /* just for suppress warning */
    tmd[6];
  uint16_w *rflag;
  float north,east,stcp,stcs,dm1,dm2,sens,to,h,g,adc;
  double alat,along,x,y;
  struct {float x,y;} md;
  char text_buf[LINELEN],fname[NAMLEN],fname1[NAMLEN],name[STNLEN],unit[7],
    dpath[NAMLEN],comp[CMPLEN],*ptr,fname2[NAMLEN],sname[STNLEN],c,*cp,
    fname00[NAMLEN],fname01[NAMLEN],fname02[NAMLEN],(*stations)[STNLEN];
#if HINET_WIN32
  char text_buf2[LINELEN],text_buf3[LINELEN];
#endif
  union Swp {
    float f;
    uint32_w i;
    uint8_w  c[4];
    } *swp;
  /* added by Uehira */
  int  *sys_ch_list;
  struct ch_check_list {
    char  code[STNLEN];
    float north;   /* latitude */
    float east;    /* longitude */
    int   height;  /* height */
    float stcp;    /* station correction of P */
    float stcs;    /* station correction of S */
  } *chcheck;
  int  itemnum;
  int  stnum;
  int  old_ch_flag;
  int  mm, nn;

  /* open parameter file */
  if((fp=open_file(ft.param_file,"parameter"))) fclose(fp);
  else return (0);
  read_parameter(PARAM_PATH,dpath); /* default data file path */
  read_parameter(PARAM_CHS,fname);  /* channels table file */
  if((ptr=strchr(fname,'*')))
    {
    *ptr=0;
    use_default_chfile=1;
    }
  else use_default_chfile=0;
  *ft.hypo_dir1=0;
  read_parameter(PARAM_OUT,text_buf);
  if((ptr=strchr(text_buf,':'))==0) strcpy(ft.hypo_dir,text_buf);
  else
    {
    *ptr=0;
    strcpy(ft.hypo_dir,text_buf);
    strcpy(ft.hypo_dir1,ptr+1);
    }
  read_parameter(PARAM_ZONES,fname1); /* zones table file */
  read_parameter(PARAM_PRINTER,text_buf);
  if((ptr=strchr(text_buf,':'))==0) mailer_flag=0;
  else
    {
    strcpy(ft.mailer,ptr+1);  /* secondly specified printer */
    mailer_flag=1;
    }

/* make temporary file names */
  if(read_parameter(PARAM_TEMP,text_buf)==0) strcpy(text_buf,".");
  sprintf(ft.seis_file,"%s/%s.seis.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.init_file,"%s/%s.init.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.finl_file,"%s/%s.final.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.rept_file,"%s/%s.report.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.seis_file2,"%s/%s.seis2.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.init_file2,"%s/%s.init2.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.finl_file2,"%s/%s.final2.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.rept_file2,"%s/%s.report2.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.othrs_file,"%s/%s.othrs.%d",text_buf,NAME_PRG,getpid());
  sprintf(ft.log_file,"%s/%s.log.%d",text_buf,NAME_PRG,getpid());

  for(i=0;i<N_CH_NAME;i++) ft.ch2idx[i]=(-1);
  ft.sr_max=250;
  ft.period=0;
  ft.n_ch=0;
  map_interval=0;
  map_all=0;
  map_period=1;
  map_period_unit='d';
  map_update=1;
  stations=NULL;
  rflag=NULL;
  if(map_only)
    {
    map_update=1;
    while(args<argc)
      {
      ptr=argv[args++];
      i=0;
      while(*ptr) text_buf[i++]=tolower(*ptr++);
      text_buf[i]=0;
      ptr=text_buf+strlen(text_buf)-1;
      if(*ptr=='m')
        {
        *ptr=0;
        map_interval=strtol(text_buf,(char **)NULL,10);
        }
      else if(*ptr=='d' || *ptr=='h')
        {
        map_period_unit=(*ptr);
        *ptr=0;
        map_period=strtol(text_buf,(char **)NULL,10);
        }
      else
        {
        *ptr=0;
        map_period=strtol(text_buf,(char **)NULL,10);
        }
      }
    if(map_period==0) map_all=1;
    map_period_save=map_period;
/*    strcpy(func_map[RETN],"QUIT");*/
    func_map[RETN]="QUIT";
/*    strcpy(func_map2[OTHRS],"UPDAT");*/
    func_map2[OTHRS]="UPDAT";

    i=strlen(ft.hypo_dir);
    get_time_win(&tm,0);
    if(ft.hypo_dir[i-1]=='/') for(;;)
      {
      sprintf(ft.hypo_dir+i,"%02d%02d",tm.ye,tm.mo);
      if((dir_ptr=opendir(ft.hypo_dir)))
        {
        closedir(dir_ptr);
        break;
        }
      if(--tm.mo==0)
        {
        if(--tm.ye==(-1)) tm.ye=99;
        if(tm.ye==50) /* i.e. 1950 */
          {
          ft.hypo_dir[i]=0;
          fprintf(stderr,"pick directory in '%s' not found\007\n",ft.hypo_dir);
          *ft.hypo_dir=0;
          break;
          }
        tm.mo=12;
        }
      }
    goto just_map;
    }
  else /*strcpy(func_map[TMSP],"$");*/
    func_map[TMSP]="$";
  if(just_hypo)
    {
    if(args<argc) just_hypo_offset=atoi(argv[args]);
    sprintf(text_buf,"%s/%s",ft.hypo_dir,ft.save_file);
    if((fp=open_file(text_buf,"pick"))==NULL) return (0);
    fgets(text_buf,LINELEN,fp);
    sscanf(text_buf,"%s%s",fname00,fname01);
    strcpy(fname02,fname01);
    if(fname01[6]=='.') fname02[6]='_';
    else if(fname01[6]=='_') fname02[6]='.';
    if(strcmp(fname00,"#p"))
      {
      fprintf(stderr,"pick file '%s' illegal\n",ft.save_file);
      return (0);
      }
    /* data file name in "fname01/fname02" */
    sprintf(ft.data_file,"%s/%s",dpath,fname01);
    goto just_map;
    }
  else if(!(args<argc))
    {
    print_usage();
    exit(1);
    }
  else
    {
    strcpy(text_buf,argv[args]);
    if((ptr=strrchr(text_buf,'/'))!=0)  /* path is specified */
      {
      strcpy(ft.data_file,text_buf);
      if((ft.fd=open(ft.data_file,O_RDONLY))==-1
          && strlen(ptr+1)==11) for(i=0;i<60;i++)
        {
        sprintf(ft.data_file,"%s%02d",text_buf,i);
        if((ft.fd=open(ft.data_file,O_RDONLY))!=-1) break;
        }
      }
    else                /* path is not specified */
      {
      strcpy(fname2,text_buf);
      if(*fname2=='.' || *fname2=='_') /* YYMMDD abbreviated */
        {
        c=(*fname2);
        for(i=0;i<strlen(fname2);i++) if(fname2[i+1]!=c) break;
        get_time_win(&tm,-(60*60*24)*i);
        sprintf(text_buf,"%02d%02d%02d%1c%s",tm.ye,tm.mo,tm.da,c,fname2+i+1);
        strcpy(fname2,text_buf);
        }
      else if(fname2[2]=='.' || fname2[2]=='_') /* YYMM abbreviated */
        {
        get_time_win(&tm,0);
        sprintf(text_buf,"%02d%02d%s",tm.ye,tm.mo,fname2);
        strcpy(fname2,text_buf);
        }
      /* default data path */
      sprintf(ft.data_file,"%s/%s",dpath,fname2); 
      if((ft.fd=open(ft.data_file,O_RDONLY))==-1
          && strlen(fname2)==11) for(i=0;i<60;i++)
        {
        sprintf(ft.data_file,"%s/%s%02d",dpath,fname2,i);
        if((ft.fd=open(ft.data_file,O_RDONLY))!=-1) break;
        }
      /* current directory */
      if(ft.fd==-1)
        {
        sprintf(ft.data_file,"%s",fname2);
        if((ft.fd=open(ft.data_file,O_RDONLY))==-1
            && strlen(fname2)==11) for(i=0;i<60;i++)
          {
          sprintf(ft.data_file,"%s/%s%02d",dpath,fname2,i);
          if((ft.fd=open(ft.data_file,O_RDONLY))!=-1) break;
          }
        }
      }
    }
  *ft.save_file=0;
  if(ft.fd==-1)
    {
    fprintf(stderr,"data file not found : %s\n",ft.data_file);
    print_usage();
    exit(1);
    }
  if(copy_file) /* use temp data file */
    {
    if(read_parameter(PARAM_TEMP,text_buf)==0) strcpy(text_buf,".");
    sprintf(ft.dat_file,"%s/%s.dat.%d",text_buf,NAME_PRG,getpid());
    sprintf(text_buf,"cp %s %s",ft.data_file,ft.dat_file);
    if(system(text_buf))  /* error */
      {
      fprintf(stderr,"'%s' failed\007\007\n",text_buf);
      unlink(ft.dat_file);
      copy_file=0;
      }
    else
      {
      close(ft.fd);
      ft.fd=open(ft.dat_file,O_RDONLY);
      }
    }
  i=lseek(ft.fd,(off_t)0,2);      /* file size */
  fprintf(stderr,"data file='%s'   %d bytes\n",ft.data_file,i);
  /* make second table */
  if(flag_save) open_save();
  make_sec_table();
  /* ft.n_ch is fixed */
  if(ft.n_ch*(HEIGHT_TEXT+1)>32767) pixels_per_trace=32767/ft.n_ch;
  else pixels_per_trace=HEIGHT_TEXT+1;
  ppt_half=pixels_per_trace/2;
  if((ft.pos2idx=(int16_w *)win_xmalloc(sizeof(*ft.pos2idx)*ft.n_ch))==NULL)
    emalloc("ft.pos2idx");
  if((ft.idx2pos=(int16_w *)win_xmalloc(sizeof(*ft.idx2pos)*ft.n_ch))==NULL)
    emalloc("ft.idx2pos");

  bcd_dec(tmd,ft.ptr[0].time);
  i=strlen(ft.hypo_dir);
  if(ft.hypo_dir[i-1]=='/') for(;;)
    {
    sprintf(ft.hypo_dir+i,"%02d%02d",tmd[0],tmd[1]);
    if((dir_ptr=opendir(ft.hypo_dir)))
      {
      closedir(dir_ptr);
      break;
      }
    if(--tmd[1]==0)
      {
      if(--tmd[0]==(-1)) tmd[0]=99;
      if(tmd[0]==50)  /* i.e. 1950 */
        {
        ft.hypo_dir[i]=0;
        fprintf(stderr,"pick directory in '%s' not found\007\n",ft.hypo_dir);
        *ft.hypo_dir=0;
        break;
        }
      tmd[1]=12;
      }
    }
  bcd_dec(tmd,ft.ptr[0].time);
  i=strlen(ft.hypo_dir1);
  if(*ft.hypo_dir1 && ft.hypo_dir1[i-1]=='/') for(;;)
    {
    sprintf(ft.hypo_dir1+i,"%02d%02d",tmd[0],tmd[1]);
    if((dir_ptr=opendir(ft.hypo_dir1)))
      {
      closedir(dir_ptr);
      break;
      }
    if(--tmd[1]==0)
      {
      if(--tmd[0]==(-1)) tmd[0]=99;
      if(tmd[0]==50)  /* i.e. 1950 */
        {
        ft.hypo_dir1[i]=0;
        fprintf(stderr,"pick directory in '%s' not found\007\n",ft.hypo_dir1);
        *ft.hypo_dir1=0;
        break;
        }
      tmd[1]=12;
      }
    }

  read_parameter(PARAM_FILT,ft.filt_file);  /* filter file */
  if((ft.n_filt=read_filter_file()))
    fprintf(stderr,"%d filters installed from '%s'\n",ft.n_filt-1,ft.filt_file);

  read_parameter(PARAM_LABELS,ft.label_file); /* label file */
  if((ft.n_label=read_label_file()))
    fprintf(stderr,"%d labels installed from '%s'\n",ft.n_label-1,ft.label_file);
  ft.label_idx=0;

  /* read zone table file */
  i=0;
  if((fp=open_file(fname1,"zone table")))
    {
    while(fscanf(fp,"%s",text_buf)!=EOF)
      if(*text_buf=='#') continue;
      else i++;
    fclose(fp);
    }
  n_stations=(++i);

  if((rflag=(uint16_w *)win_xmalloc(sizeof(*rflag)*i))==NULL)
    emalloc("rflag");
  if((stations=(char (*)[STNLEN])win_xmalloc(sizeof(char)*STNLEN*i))==NULL)
    emalloc("stations");

  if((fp=open_file(fname1,"zone table"))==NULL) stations[0][0]=0;
  else
    {
    i=j=0;
    while(fscanf(fp,"%s",text_buf)!=EOF)
      {
      if(*text_buf=='#')
        {
        j++;
        continue;
        }
      rflag[i]=j;
      strcpy(stations[i++],text_buf);
      }
    stations[i][0]=0;
    fprintf(stderr,"%d stations (%d groups) in '%s'\n",i,j,fname1);
    fclose(fp);
    }

just_map:

  read_parameter(PARAM_DPI,text_buf);
  if(sscanf(text_buf,"%lf",&pdpi)!=1) pdpi=PDPI;
  else if(pdpi<10.0 || pdpi>1000.0) pdpi=PDPI;
  pixels_per_km=pdpi/2.54/(mapsteps[ppk_idx=PPK_INIT]*0.1);

  /* get origin of coordinate */
  read_parameter(PARAM_MAP,ft.map_file);
  if((fp=open_file(ft.map_file,"map")))
    {
    fread(&md,sizeof(md),1,fp);
    fclose(fp);
    swp=(union Swp *)&md.x;
    swp->i=(swp->c[0]<<24)+(swp->c[1]<<16)+(swp->c[2]<<8)+swp->c[3];
    swp=(union Swp *)&md.y;
    swp->i=(swp->c[0]<<24)+(swp->c[1]<<16)+(swp->c[2]<<8)+swp->c[3];
    alat0=(double)md.x;
    along0=(double)md.y;
    }
  else alat0=100.0;

  /* initialize station table "ft.stn" */
  if (!map_only && !just_hypo) {  /* ft.n_ch = 0 */
    if((ft.stn=(struct Stn *)win_xmalloc(sizeof(*ft.stn)*ft.n_ch))==NULL)
      emalloc("ft.stn");
    for(i=0;i<ft.n_ch;i++)
      {
      strcpy(ft.stn[i].name,"****");
      strcpy(ft.stn[i].comp,"**");
      ft.stn[i].scale=ft.stn[i].psup_scale=4;
      strcpy(ft.stn[i].unit,"*****");
      ft.stn[i].units_per_bit=1.0;
      ft.stn[i].invert=ft.stn[i].offset=0;
      ft.stn[i].rflag=ft.stn[i].order=0;
      ft.stn[i].north=ft.stn[i].east=ft.stn[i].x=ft.stn[i].y=0.0;
      ft.stn[i].z=0;
      ft.stn[i].stcp=ft.stn[i].stcs=0.0;
      ft.stn[i].psup=0;
      ft.pos2idx[i]=ft.idx2pos[i]=(-1);
      }
  }

  /* open channel table file */
  if(map_only || use_default_chfile) fp=open_file(fname,"channel table");
  else
    {
#if HINET_WIN32
      /* for Hi-net Channels table */
      text_buf[0]='\0';
      text_buf2[0]='\0';
      text_buf3[0]='\0';
      /* find directory name */
      if(strlen(ft.data_file)>23) {
	strncpy(text_buf2,ft.data_file,(int)(strlen(ft.data_file)-23));
	text_buf2[strlen(ft.data_file)-23]='\0';
      }
      /* find win32file name */
      if((ptr=strrchr(ft.data_file,'/'))){
	ptr++;
	strncpy(text_buf3,ptr,19);
	text_buf3[19]='\0';
      } else {
	strncpy(text_buf3,ft.data_file,19);
	text_buf3[19]='\0';
      }
      sprintf(text_buf,"%sU%s.ch",text_buf2,text_buf3);
#if 0
      fprintf(stderr,"rslt:%s\n",text_buf);
      fprintf(stderr,"rslt:%s\n",text_buf2);
      fprintf(stderr,"rslt:%s\n",text_buf3);
#endif
      if((fp=fopen(text_buf,"r"))==NULL)
	{
	  sprintf(text_buf,"%s.ch",ft.data_file);
	  if((fp=fopen(text_buf,"r"))==NULL)
	    {
	      sprintf(text_buf,"%s.CH",ft.data_file);
	      if((fp=fopen(text_buf,"r"))==NULL)
		fp=open_file(fname,"channel table");
	      else strcpy(fname,text_buf);
	    }
	  else strcpy(fname,text_buf);
	}
      else strcpy(fname,text_buf);
#else

    sprintf(text_buf,"%s.ch",ft.data_file);
    if((fp=fopen(text_buf,"r"))==NULL)
      {
      sprintf(text_buf,"%s.CH",ft.data_file);
      if((fp=fopen(text_buf,"r"))==NULL)
        fp=open_file(fname,"channel table");
      else strcpy(fname,text_buf);
      }
    else strcpy(fname,text_buf);
#endif
    }
  if(fp!=NULL)
    {
    kk=ft.n_ch;
    while(fgets(text_buf,LINELEN,fp)!=NULL)
      {
      if(*text_buf=='#') continue;
      sscanf(text_buf,"%s",name);
      if(strlen(name)<3)  /* old channel table format */
        {
        sscanf(text_buf,"%x%x%d%d%s%s%d%s%f%s%f%f%f%f%f%f%d%f%f",
          &i,&j,&dum1,&dum2,name,comp,&k,sname,&sens,unit,
          &to,&h,&g,&adc,&north,&east,&height,&stcp,&stcs);
        sys_ch=(i<<8)+j;
        }
      else sscanf(text_buf,"%x%d%d%s%s%d%s%f%s%f%f%f%f%f%f%d%f%f",
             &sys_ch,&dum1,&dum2,name,comp,&k,sname,&sens,unit,
             &to,&h,&g,&adc,&north,&east,&height,&stcp,&stcs);
      if(ft.ch2idx[sys_ch]<0) /* not contained in data file */
        if(just_hypo || (north!=0.0 && east!=0.0)) kk++;
      }
    rewind(fp);

    if(just_hypo)
      if((ft.idx2ch=(WIN_ch *)win_xmalloc(sizeof(*ft.idx2ch)*kk))==NULL)
        emalloc("ft.idx2ch");

    if(kk>ft.n_ch)  /* if larger than already allocated */
      {
      if((ft.stn=(struct Stn *)win_xrealloc(ft.stn,sizeof(*ft.stn)*kk))==NULL)
      for(i=ft.n_ch;i<kk;i++)
        {
        strcpy(ft.stn[i].name,"****");
        strcpy(ft.stn[i].comp,"**");
        ft.stn[i].scale=ft.stn[i].psup_scale=4;
        strcpy(ft.stn[i].unit,"*****");
        ft.stn[i].units_per_bit=1.0;
        ft.stn[i].invert=ft.stn[i].offset=0;
        ft.stn[i].rflag=ft.stn[i].order=0;
        ft.stn[i].north=ft.stn[i].east=ft.stn[i].x=ft.stn[i].y=0.0;
        ft.stn[i].z=0;
        ft.stn[i].stcp=ft.stn[i].stcs=0.0;
        ft.stn[i].psup=0;
        }
      }

    if(do_chck>=0) {
    /***** check channel table *****/
    if ((sys_ch_list = MALLOC(int, kk)) == NULL)
      emalloc("sys_ch_list");
    if ((chcheck = MALLOC(struct ch_check_list, kk)) == NULL)
      emalloc("sys_ch_list");
    for (nn = 0; nn < kk; ++nn) {
      /* chcheck[nn].north = chcheck[nn].east = 400.0; */
      chcheck[nn].height = 10000000;
      chcheck[nn].stcp = chcheck[nn].stcs = 1.1e+38;  /* big value */
    }
    nn = 0;
    stnum = 0;
    while (fgets(text_buf, LINELEN, fp) != NULL) {
      if(text_buf[0] == '#') continue;
      sscanf(text_buf, "%s", name);
      if (strlen(name) < 3) { /* old channel table format */
	itemnum = 
	  sscanf(text_buf, "%x%x%d%d%s%s%d%s%f%s%f%f%f%f%f%f%d%f%f",
		 &i, &j, &dum1, &dum2, name, comp, &k, sname, &sens, unit,
		 &to, &h, &g, &adc, &north, &east, &height, &stcp, &stcs);
	if (itemnum <= 0) {
	  fprintf(stderr, "There is a blank line in channels table.\n");
	  if (do_chck)
	    return (0);
	}
	/* if (!(itemnum == 14 || 17 <= itemnum)) { */
	/*   fprintf(stderr,"invalid item number:\n%s\n", text_buf); */
	/*   return (0); */
	/* } */
	sys_ch = (i << 8) + j;
	old_ch_flag = 1;
      } else {
	itemnum =
	  sscanf(text_buf, "%x%d%d%s%s%d%s%f%s%f%f%f%f%f%f%d%f%f",
		 &sys_ch, &dum1, &dum2, name, comp, &k, sname, &sens, unit,
		 &to, &h, &g, &adc, &north, &east, &height, &stcp, &stcs);
	if (itemnum <= 0) {
	  fprintf(stderr, "There is a blank line in channels table.\n");
	  if (do_chck)
	    return (0);
	}
	/* if (!(itemnum == 13 || 16 <= itemnum)) { */
	/*   fprintf(stderr,"invalid item number:\n%s\n", text_buf); */
	/*   return (0); */
	/* } */
	old_ch_flag = 0;
      }

      /** check station proper parameters **/
      for (mm = 0; mm < stnum; ++mm) {
	if (strcmp(name, chcheck[mm].code) == 0)
	  break;
      }
      if (mm == stnum) {
	strcpy(chcheck[mm].code, name);
	stnum++;
      }
      /* position check */
      if ((old_ch_flag && itemnum >= 17) || (!old_ch_flag && itemnum >= 16)) {
	if (chcheck[mm].height == 10000000) {
	  chcheck[mm].north = north;
	  chcheck[mm].east = east;
	  chcheck[mm].height = height;
	} else {
	  if (chcheck[mm].north != north || chcheck[mm].east != east || chcheck[mm].height != height) {
	    fprintf(stderr, "Inconsistent position information: %s[%s]\n",
		    chcheck[mm].code, comp);
	    if (do_chck)
	      return (0);
	  }
	}
      }

      /* station correction of P check */
      if ((old_ch_flag && itemnum >= 18) || (!old_ch_flag && itemnum >= 17)) {
	/* printf("%s\t%f\n", text_buf, stcp); */
	if (chcheck[mm].stcp > 1.0e+38)
	  chcheck[mm].stcp = stcp;
	else
	  if (chcheck[mm].stcp != stcp) {
	    fprintf(stderr, "Inconsistent station correction of P : %s\n",
		    chcheck[mm].code);
	    if (do_chck)
	      return (0);
	  }
      }
      
      /* station correction of S check */
      if ((old_ch_flag && itemnum >= 19) || (!old_ch_flag && itemnum >= 18)) {
	/* printf("%s\t%f\n", text_buf, stcs); */
	if (chcheck[mm].stcs > 1.0e+38)
	  chcheck[mm].stcs = stcs;
	else
	  if (chcheck[mm].stcs != stcs) {
	    fprintf(stderr, "Inconsistent station correction of S : %s\n",
		    chcheck[mm].code);
	    if (do_chck)
	      return (0);
	  }
      }

      /** check duplicated channel number **/
      sys_ch_list[nn] = sys_ch;
      for (mm = 0; mm < nn; ++mm)
	if (sys_ch_list[mm] == sys_ch_list[nn]) {
	  fprintf(stderr, "Duplicate sys_ch list in channels table: %X\n",
		  sys_ch_list[mm]);
	  if (do_chck)
	    return (0);
	}
      nn++;
    }
    FREE(chcheck);
    FREE(sys_ch_list);
    rewind(fp);
    }

    ft.n_ch_ex=ft.n_ch;
    kk=0;
    /* read channels table */
    while(fgets(text_buf,LINELEN,fp)!=NULL)
      {
      if(*text_buf=='#') continue;
      height=0;
      north=east=stcp=stcs=0.0;
      bzero(comp,CMPLEN);
      sscanf(text_buf,"%s",name);
      if(strlen(name)<3)  /* old channel table format */
        {
        sscanf(text_buf,"%x%x%d%d%s%s%d%s%f%s%f%f%f%f%f%f%d%f%f",
          &i,&j,&dum1,&dum2,name,comp,&k,sname,&sens,unit,
          &to,&h,&g,&adc,&north,&east,&height,&stcp,&stcs);
        sys_ch=(i<<8)+j;
        }
      else sscanf(text_buf,"%x%d%d%s%s%d%s%f%s%f%f%f%f%f%f%d%f%f",
             &sys_ch,&dum1,&dum2,name,comp,&k,sname,&sens,unit,
             &to,&h,&g,&adc,&north,&east,&height,&stcp,&stcs);
      if(just_hypo)
        {
        ft.ch2idx[sys_ch]=kk;
        ft.idx2ch[ft.n_ch]=sys_ch;
        ft.n_ch_ex=ft.n_ch=kk+1;
        }
      if(ft.ch2idx[sys_ch]>=0) /* i.e. contained in data file */
        {
        ft.stn[ft.ch2idx[sys_ch]].scale=
          ft.stn[ft.ch2idx[sys_ch]].psup_scale=k;
        strcpy(ft.stn[ft.ch2idx[sys_ch]].name,name);
        strcpy(ft.stn[ft.ch2idx[sys_ch]].comp,comp);
        jj=0;
        if(stations) while(stations[jj][0])
          {
          if(strcmp(stations[jj],name)==0)
            {
            cp=comp;
            ft.stn[ft.ch2idx[sys_ch]].order=(n_stations-jj)*300+200;
            if(*cp=='w') {ft.stn[ft.ch2idx[sys_ch]].order-=40;cp++;}
            else if(*cp=='s') {ft.stn[ft.ch2idx[sys_ch]].order-=80;cp++;}
            if(*cp=='V' || *cp=='U' || *cp=='D')
              ft.stn[ft.ch2idx[sys_ch]].order+=35;
            else if(*cp=='N' || *cp=='S') ft.stn[ft.ch2idx[sys_ch]].order+=30;
            else if(*cp=='E' || *cp=='W') ft.stn[ft.ch2idx[sys_ch]].order+=25;
            else if(*cp=='H') ft.stn[ft.ch2idx[sys_ch]].order+=20;
            else if(*cp=='X') ft.stn[ft.ch2idx[sys_ch]].order+=15;
            else if(*cp=='Y') ft.stn[ft.ch2idx[sys_ch]].order+=10;
            else if(*cp=='Z') ft.stn[ft.ch2idx[sys_ch]].order+=5;
            else ft.stn[ft.ch2idx[sys_ch]].order-=120;
            if(strchr(cp+1,'1')!=0 || strchr(cp+1,'L')!=0)
              ft.stn[ft.ch2idx[sys_ch]].order+=4;
            if(strchr(cp+1,'2')!=0 || strchr(cp+1,'M')!=0)
              ft.stn[ft.ch2idx[sys_ch]].order+=3;
            if(strchr(cp+1,'3')!=0 || strchr(cp+1,'H')!=0)
              ft.stn[ft.ch2idx[sys_ch]].order+=2;
            break;
            }
          jj++;
          }
        strcpy(ft.stn[ft.ch2idx[sys_ch]].unit,unit);
        if((adc<0.0 && sens>=0.0) || (adc>=0.0 && sens<0.0))
          ft.stn[ft.ch2idx[sys_ch]].invert=1;
        if(*unit!='*') ft.stn[ft.ch2idx[sys_ch]].units_per_bit=
            (float)(fabs(adc/sens)/pow(10.0,(g/20.0)));
        if(rflag) ft.stn[ft.ch2idx[sys_ch]].rflag=rflag[jj];  /* zone# */
        if(north!=0.0 && east!=0.0)
          {
          ft.stn[ft.ch2idx[sys_ch]].north=north;
          ft.stn[ft.ch2idx[sys_ch]].east=east;
          ft.stn[ft.ch2idx[sys_ch]].z=(long)height;
          alat=(double)north;
          along=(double)east;
          if(alat0>99.0)
            {
            alat0=rint(alat);
            along0=rint(along);
            }
          pltxy(alat0,along0,&alat,&along,&x,&y,0);
          ft.stn[ft.ch2idx[sys_ch]].x=(float)x;
          ft.stn[ft.ch2idx[sys_ch]].y=(float)y;
          }
        ft.stn[ft.ch2idx[sys_ch]].stcp=stcp;
        ft.stn[ft.ch2idx[sys_ch]].stcs=stcs;
        kk++;
        }
      else /* ch not contained in the data file */
        {
        if(north!=0.0 && east!=0.0)  /* dum1 is record flag */
          {
          strcpy(ft.stn[ft.n_ch_ex].name,name);
          for(i=0;i<ft.n_ch_ex;i++)
            if(strcmp(ft.stn[i].name,ft.stn[ft.n_ch_ex].name)==0) break;
          if(i<ft.n_ch_ex) continue;
          strcpy(ft.stn[ft.n_ch_ex].comp,comp);
          ft.stn[ft.n_ch_ex].north=north;
          ft.stn[ft.n_ch_ex].east=east;
          ft.stn[ft.n_ch_ex].z=(long)height;
          alat=(double)north;
          along=(double)east;
          pltxy(alat0,along0,&alat,&along,&x,&y,0);
          ft.stn[ft.n_ch_ex].x=(float)x;
          ft.stn[ft.n_ch_ex].y=(float)y;
          ft.stn[ft.n_ch_ex].stcp=stcp;
          ft.stn[ft.n_ch_ex].stcs=stcs;
          ft.n_ch_ex++;
          }
        }
      }  /* end of read channnels table */

    for(i=0;i<ft.n_ch;i++)  /* fill north/east/z/stcp/stcs/x/y */
      {
      if(ft.stn[i].north==0.0 || ft.stn[i].east==0.0)
        for(j=0;j<ft.n_ch_ex;j++)
          if(strcmp(ft.stn[j].name,ft.stn[i].name)==0 &&
              (ft.stn[j].north!=0.0 && ft.stn[j].east!=0.0))
            {
            ft.stn[i].north=ft.stn[j].north;
            ft.stn[i].east=ft.stn[j].east;
            ft.stn[i].x=ft.stn[j].x;
            ft.stn[i].y=ft.stn[j].y;
            ft.stn[i].z=ft.stn[j].z;
            break;
            }
      if(ft.stn[i].stcp==0.0) for(j=0;j<ft.n_ch_ex;j++)
        if(strcmp(ft.stn[j].name,ft.stn[i].name)==0 &&
            ft.stn[j].stcp!=0.0)
          {
          ft.stn[i].stcp=ft.stn[j].stcp;
          break;
          }
      if(ft.stn[i].stcs==0.0) for(j=0;j<ft.n_ch_ex;j++)
        if(strcmp(ft.stn[j].name,ft.stn[i].name)==0 &&
            ft.stn[j].stcs!=0.0)
          {
          ft.stn[i].stcs=ft.stn[j].stcs;
          break;
          }
      }
    fprintf(stderr,"%d data chs (%d chs + %d extra stations from '%s')\n",
      ft.n_ch,kk,ft.n_ch_ex-ft.n_ch,fname);
    fclose(fp);
    }
  else fprintf(stderr,"%d data chs\n",ft.n_ch);

  if(rflag) FREE(rflag);
  if(stations) FREE(stations);

  for(i=0;i<ft.n_ch;i++) ft.stn[i].ch_order=ft.stn[i].order;

  k=0;
  if(!just_hypo) for(;;)  /* arrange channls in descending order of 'order' */
    {
    j=kk=0;
    for(i=0;i<ft.n_ch;i++)
      {
      if(ft.idx2pos[i]>=0) continue;
      if(j==0 || ft.stn[i].order>ft.stn[kk].order) kk=i;
      j=1;
      }
    if(j==0) break;
    ft.idx2pos[kk]=k;
    ft.pos2idx[k++]=kk;
    }

  /* read initial value for depth */
  read_parameter(PARAM_STRUCT,fname);
  if((ptr=strchr(fname,'*'))) *ptr=0;
  if((fp=open_file(fname,"structure")))
    {
    fgets(text_buf,LINELEN,fp);
    sscanf(text_buf,"%f%f%f",&init_lat_init,&init_lon_init,&dm1);
    init_dep=init_dep_init=(int)dm1;
    fgets(text_buf,LINELEN,fp);
    sscanf(text_buf,"%d",&i); /* N of layers */
    j=(i+2+6)/7+(i+1+6)/7+1;  /* (N of lines to skip) +1 */
    while(j-->0) fgets(text_buf,LINELEN,fp);
    sscanf(text_buf,"%f%f%f%f",&dm1,&init_late_init,&init_lone_init,&dm2);
    init_depe=init_depe_init=(int)dm2;
/*    if(init_depe>init_dep) init_depe=init_depe_init=init_dep;*/
    fclose(fp);
    }   

  if(background) goto bg;

  if((disp=XOpenDisplay(NULL))==0)
    {
    fprintf(stderr,"X : display not open\n");
    exit(1);
    }
  if((ptr=(char *)getenv("WINDOWID"))) ttysw=strtol(ptr,(char **)NULL,10);
  else if(map_only) ttysw=0;
  else
    {
    fprintf(stderr,"Please set environmental variable 'WINDOWID'\n");
    fprintf(stderr,"     (use 'xwininfo -int' and 'setenv')\007\n");
    return (0);
    }
bg: return (1);
  }

static int
refresh(int idx) /* Is return value really OK? */
  {
  Window root;
  int x,y;
  unsigned int bw,d;

  if(!background)
    {
    XGetGeometry(disp,dpy.drw,&root,&x,&y,&width_dpy,&height_dpy,&bw,&d);
    set_geometry();
    }
  switch(loop)
    {
    case LOOP_MAIN:   return (put_main());
    case LOOP_MAP:    return (put_map(idx));
    case LOOP_MECHA:  return (put_mecha());
    case LOOP_PSUP:   return (put_psup());
    }
  }

static void
set_period(int idx, struct Pick_Time *pt)
  {
  struct Pick_Time pt1;
  int n,i;
  double *db,zero,dj;

  /* length= 2 s */
  pt1=(*pt);
  pt1.sec2=pt1.sec1+2;
  pt1.msec2=pt1.msec1;
  n=getdata(idx,pt1,&db,&i);
  /* measure FREQUENCY from 2 sec */
  smeadl(db,n,&zero);
  dj=0.0;
  for(i=0;i<n;i++) if((db[i]>=0.0 && db[i-1]<0.0) ||
    (db[i]<=0.0 && db[i-1]>0.0)) dj+=0.5;
  if((pt->period=1000.0*(double)n/(dj*(double)ft.sr[idx]))>500) pt->period=500;
  FREE(db);
  }

static void
set_pick(struct Pick_Time *pt, int sec, int msec, int ms_width1, int ms_width2)
  { /* negative time is permitted only for ft.pick_calc_ot */

  if(pt!=&ft.pick_calc_ot &&(sec<0 || sec>=ft.len)) pt->valid=0;
  else
    {
    pt->sec1=pt->sec2=sec;
    pt->msec1=msec-ms_width1;
    while(pt->msec1<0) {pt->msec1+=1000;pt->sec1--;}
    if(pt->sec1<0) pt->msec1=pt->sec1=0;
    pt->msec2=msec+ms_width2;
    while(pt->msec2>=1000) {pt->msec2-=1000;pt->sec2++;}
    if(pt->sec2>=ft.len) {pt->msec2=1000;pt->sec2=ft.len-1;}
    pt->valid=1;
    }
  pt->polarity=0;
  }

static int
get_width(struct Pick_Time *pt)
  {

  return (((pt->sec2-pt->sec1)*1000+pt->msec2-pt->msec1)/2);
  }

static void
set_width(struct Pick_Time *pt, int ms_width1, int ms_width2)
  {
  int msec;

  msec=((pt->sec1+pt->sec2)*1000+pt->msec1+pt->msec2)/2;
  set_pick(pt,msec/1000,msec%1000,ms_width1,ms_width2);
  }

/* set and show pick time */
static int
show_pick(int idx, struct Pick_Time *pt, int i)
  {

  if(pt->valid)
    {
    cancel_picks(ft.stn[idx].name,i);
    ft.pick[idx][i]=(*pt);
    put_mark(i,ft.idx2pos[idx],0);
    make_visible(idx);
    put_mon(x_zero,y_zero);
    return (1);
    }
  else return (0);
  }

static void *
alloc_mem(size_t size, char *mes)   /* 64bit OK */
  {
  char *almem,tb[100];

  if((almem=(void *)win_xmalloc(size))==NULL)
    {
    snprintf(tb,sizeof(tb),"%s(%zu)",mes,size);
    emalloc(tb);
    }
  return (almem);
  }

static int
getdata(int idx, struct Pick_Time pt, double **dbp, int *ip)
  /* int idx,*ip;    if interpolated, *ip=1 (for measure MAX) */
  {
  /*  int sec,n1,n2,n3,n,i,j,sr,i1,ii;*/
  int n1,n2,n3,n,i,j,i1=0,ii;  /* just for suppress warning */
  WIN_sr sr;  /* OLD : int */
  double dmax,dmin,drange,*db;
  int32_w sec;

  *ip=0;
  if(pt.sec1<0) pt.sec1=pt.msec1=0;
  sec=pt.sec1;
  sr=ft.sr[idx];
  n1=pt.msec1*sr/1000;
  n3=pt.msec2*sr/1000;
  n2=(pt.sec2-pt.sec1-1)*sr;
  if(n2<0) n=n3-n1;
  else n=(sr-n1)+n2+n3;
  *dbp=(double *)alloc_mem((size_t)sizeof(double)*n,"dbp");
  db=(*dbp);
  if(read_one_sec(sec++,ft.idx2ch[idx],buf2,NOT_KILL_SPIKE)==0)
    fill((int *)buf2,ft.sr[idx],0);
  j=0;
  if(n2<0) for(i=n1;i<n3;i++) db[j++]=(double)buf2[i];
  else
    {
    for(i=n1;i<sr;i++) db[j++]=(double)buf2[i];
    while(sec<pt.sec2)
      {
      if(read_one_sec(sec++,ft.idx2ch[idx],buf2,NOT_KILL_SPIKE)==0)
        fill((int *)buf2,ft.sr[idx],0);
      for(i=0;i<sr;i++) db[j++]=(double)buf2[i];
      }
    if(read_one_sec(sec++,ft.idx2ch[idx],buf2,NOT_KILL_SPIKE)==0)
      fill((int *)buf2,ft.sr[idx],0);
    for(i=0;i<n3;i++) db[j++]=(double)buf2[i];
    }
  n=j;  /* possibly j<n */
  dmax=dmin=db[0];
  j=0;
  for(i=1;i<n;i++)  /* kill line failure by linear interpolation */
    {
    if(db[i]==dmax && ((dmin-dmax)>20.0 || (dmin-dmax)<-20.0))
      {
      if(++j==3) i1=i-3;
      }
    else
      {
      if(j>=3)
        {
        for(ii=i1;ii<i;ii++) db[ii]=db[i1-1]+
          (db[i]-db[i1-1])*(double)(ii-i1+1)/(double)(i-i1+1);
        *ip=1;
        }
      dmin=dmax;
      dmax=db[i];
      j=0;
      }
    }
/*if(ft.idx2ch[idx]==0x61)for(i=430;i<460;i++)
{fprintf(stderr,"i=%d buf=%.1f\n",i,db[i]);usleep(100000);}*/
  for(i=2;i<n-2;i++)    /* kill spikes */
    {
    dmax=dmin=db[i-2];
    if(db[i-1]>dmax) dmax=db[i-1];
    else if(db[i-1]<dmin) dmin=db[i-1];
    if(db[i+1]>dmax) dmax=db[i+1];
    else if(db[i+1]<dmin) dmin=db[i+1];
    if(db[i+2]>dmax) dmax=db[i+2];
    else if(db[i+2]<dmin) dmin=db[i+2];
    drange=(dmax-dmin+1.0)*4.0;
    if(db[i]>dmax+drange || db[i]<dmin-drange) db[i]=(db[i-1]+db[i+1])/2.0;
    }
  return (n);
  }

static int
find_pick(double *db, float *aic, float *sd1, float *sd2, int i0, int n,
	  int m, double *aic_all)
  {
  double aic_min,s1,s2,dn1,dn2;
  int i,j,i_min;
#if DEBUG_AP>=2
  double sd;
#endif

  s2=s1=dn1=0.0;
  for(j=i0;j<n;j++) s2+=db[j];
  dn2=(double)(n-i0);
  aic[i0]=aic_min=(*aic_all)=dn2*log(s2/dn2);
#if DEBUG_AP>=2
  sd=s2/dn2;
  fprintf(stderr,"n-i0=%d sd=%f aic_all=%f m=%d\n",n-i0,sd,*aic_all,m);
#endif
  i_min=0;
  j=i0;
  for(i=i0+1;i<n;i++)
    {
    sd1[i]=(s1+=db[j])/(dn1+=1.0);
    sd2[i]=(s2-=db[j])/(dn2-=1.0);
    aic[i]=dn1*log(sd1[i])+dn2*log(sd2[i]);
    if(sd1[i]/sd2[i]<0.5 && aic[i]<aic_min) aic_min=aic[i_min=i];
    j++;
/* AIC = n1*log(sd1) + n2*log(sd2) +2*(m1+m2+4) */
/* AIC = (n1+n2)*log(sd12) +2*(m12+2) */
/*fprintf(stderr,"i=%d db=%.3f s1=%.3f s2=%.3f dn1=%.1f dn2=%.1f sd1=%.3f sd2=%.3f aic=%.3f\n",
i,db[i],s1,s2,dn1,dn2,sd1[i],sd2[i],aic[i]);*/
    }
#if DEBUG_AP>=2
fprintf(stderr,"i_min=%d %f - %f - %f (%f)\n",i_min,aic[i0]/(dn1+dn2),
aic_min/(dn1+dn2),aic[n-1]/(dn1+dn2),*aic_all/(dn1+dn2));
#endif
  return (i_min);
  }

static int
get_range(int sec, int msec, float *aic, float *sd1, float *sd2, int nm,
	  int n, int i_min, int sr, struct Pick_Time *pt)
  {
  int ms,i,ms1,ms2,width;
  double dj,sum,nn,d;

  nn=(double)(n-nm);
  ms=((i_min-1)*1000-500)/sr;   /* -1 from experience */
#if DEBUG_AP>=2
fprintf(stderr,"sec=%d msec=%d i_min=%d n=%d nm=%d nn=%f ms=%d\n",
sec,msec,i_min,n,nm,nn,ms);
#endif
  dj=sum=0.0;
  for(i=i_min-1;i>nm;i--)
    {
    d=(aic[i]-aic[i_min])/nn;
    if(d>=0.0) sum+=d;
    dj+=1.0;
    if(d>LEVEL_2 || sd2[i]==0.0 || sd1[i]/sd2[i]>1.0) break;
    }
  if((sum*=2.0/dj)==0.0||sum<dj*0.00001) return (0);
  ms1=(int)((LEVEL_1*dj/sum+0.5)*1000.0)/sr;
#if DEBUG_AP>=2
fprintf(stderr,"sum=%f dj=%f ms1=%d\n",sum,dj,ms1);
#endif
  dj=sum=0.0;
  for(i=i_min+1;i<n;i++)
    {
    d=(aic[i]-aic[i_min])/nn;
    if(d>=0.0) sum+=d;
    dj+=1.0;
    if(d>LEVEL_2 || sd1[i]/sd2[i]>1.0) break;
    }
  if((sum*=2.0/dj)==0.0||sum<dj*0.00001) return (0);
  ms2=(int)((LEVEL_1*dj/sum+0.5)*1000.0)/sr;
#if DEBUG_AP>=1
fprintf(stderr,"sum=%f dj=%f ms2=%d\n",sum,dj,ms2);
for(i=i_min-10;i<i_min+10;i++)
{
fprintf(stderr,"%.4f(%.2f %.2f) ",(aic[i]-aic[i_min])/nn,sd1[i],sd2[i]);
if(i%5==0) fprintf(stderr,"\n");
}
fprintf(stderr,"\n");
#endif
  if((width=(ms2+ms1)/2)>1000) return (0);
  msec+=(ms-ms1+ms+ms2)/2;
  sec+=msec/1000;
  msec=msec%1000;
#if DEBUG_AP>=2
fprintf(stderr,"min=%d(%d.%03d) 1=%d 2=%d w=%d\n",i_min,sec,msec,ms1,ms2,width);
#endif
  set_pick(pt,sec,msec,width,width);
  return (1);
  }

static int
pick_phase(int idx, int iph)   /* pick an onset in a range of time */
  /* int idx;      channel index */
  /* int iph;      P/S/X */
  {
  int n,n1,i,i_min,width,sec,msec,
    ret,nm,period,m,width_ar;
  WIN_sr sr; /* OLD : int */
  double *db,aic_all,d=0.0,sum,dj,zero,freq,level,sd,sdd,sump; /* just for suppress warning */
  struct Pick_Time pt,pt1,pt2;
  static double *db2,*c,*rec;
  static float *aic,*sd1,*sd2;

  if(doing_auto_pick && background==0)
    {
    sprintf(apbuf," '%s' in %-.4s-%-.2s   ",marks[iph],
      ft.stn[idx].name,ft.stn[idx].comp);
    put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
    }
  ret=0;
  pt=ft.pick[idx][iph];
#if DEBUG_AP>=2
fprintf(stderr,"in : %04X(%d) %2d.%03d-%2d.%03d pol=%d\n",ft.idx2ch[idx],iph,
pt.sec1,pt.msec1,pt.sec2,pt.msec2,pt.polarity);
#endif
  if(pt.valid==0) return (ret);
  if(pt.sec2>=ft.len)
    {
    pt.sec2=ft.len-1;
    pt.msec2=1000;
    }
  sr=ft.sr[idx];
  width=get_width(&pt);
  if((period=pt.period)==0 && (period=ft.period)==0) period=4*1000/sr;
  width_ar=period*10;
  if(pt.sec1*1000+pt.msec1<width_ar) width_ar=pt.sec1*1000+pt.msec1;
  if(width_ar<period*3) return (ret);
  nm=width_ar*sr/1000;      /* get length to make AR (10T) */
  set_width(&pt,width+width_ar,width);  /* extend range by -10T */
  sec=pt.sec1;
  msec=pt.msec1;
  n=getdata(idx,pt,&db,&i);         /* get raw data */
  db2=(double *)alloc_mem((size_t)(n*sizeof(double)),"db2"); /* AR resid squared */
  aic=(float *)alloc_mem((size_t)(n*sizeof(float)),"aic");  /* AIC */
  sd1=(float *)alloc_mem((size_t)(n*sizeof(float)),"sd1");  /* sd1 */
  sd2=(float *)alloc_mem((size_t)(n*sizeof(float)),"sd2");  /* sd2 */
  c=(double *)alloc_mem((size_t)(nm*sizeof(double)),"c"); /* AR filter coef */
  rec=(double *)alloc_mem((size_t)(nm*sizeof(double)),"rec"); /* rsvd data for filt */
  smeadl(db,n,&zero);         /* remove offset */
  getar(db,nm,&sd,&m,c,&zero,0);    /* get AR from first NM data */
#if DEBUG_AP>=2
fprintf(stderr,"nm=%d n=%d m=%d sd=%f zero=%f\n",nm,n,m,sd,zero);
#endif
  if(m==0) goto fail;
  for(i=0;i<m;i++) rec[i]=0;      /* clear reserved data */
  digfil(db,db2,n,c,m,rec,&sd);   /* apply AR filter (db->db2) */
  for(i=0;i<n;i++) db2[i]-=db[i];   /* take residuals (db2-db) */
  for(i=0;i<n;i++) db2[i]*=db2[i];  /* take squares */
  FREE(db);

  if(iph==P) level=LEVEL_3;
  else level=0.0;
  i_min=find_pick(db2,aic,sd1,sd2,nm,n,m,&aic_all);
  if(i_min>nm+10 && i_min<n-1-10 && aic[i_min]<aic_all-level*(double)(n-nm))
    {
    if(get_range(sec,msec,aic,sd1,sd2,nm,n,i_min,sr,&pt)==0) goto fail;
    if(iph==P)  /* search in the first part */
      {
#if DEBUG_AP>=2
i=0;
#endif
      pt2=pt;
      do
        {
        sd=sd1[n=i_min];
        sdd=sd2[n=i_min];
        i_min=find_pick(db2,aic,sd1,sd2,nm,n,m,&aic_all);
#if DEBUG_AP>=2
fprintf(stderr,"fp#%d i_min=%d nm=%d n=%d m=%d\n",i,i_min,nm,n,m);
i++;
#endif
        if(i_min>nm+10 && i_min<n-1-10 &&
            aic[i_min]<aic_all-level*(double)(n-nm) &&
            get_range(sec,msec,aic,sd1,sd2,nm,n,i_min,sr,&pt2)==1
            && sd1[i_min]<sd && sd2[i_min]>sdd*0.04 && sd2[i_min]>3.0)
          {
          pt=pt2;
#if DEBUG_AP>=2
fprintf(stderr,"repl\n");
#endif
          }
        else break;
        } while(i_min-nm>20);
    /* measure FREQUENCY (from 1 sec) */
      pt1=pt;
      set_width(&pt1,0,1000);
      n=getdata(idx,pt1,&db,&i);    /* get raw data */
      smeadl(db,n,&zero);       /* remove offset */
      dj=0.0;
      for(i=1;i<n;i++)
        if((db[i]>=0.0 && db[i-1]<0.0) || (db[i]<=0.0 && db[i-1]>0.0)) dj+=0.5;
      if((freq=dj*(double)sr/(double)n)<2.0) freq=2.0;
#if DEBUG_AP>=2
fprintf(stderr,"n=%d sr=%d dj=%f freq=%f\n",n,sr,dj,freq);
#endif
    /* measure POLARITY */
      n1=(int)((1.0/freq*0.4)*(double)sr+1.0);
      dj=sum=sump=0.0;
      for(i=0;i<=n1;i++)
        {
        sum+=db[i]-db[0];
        dj+=sum*sum;
        sump+=sum;
        if(dj!=0.0) d=sump/sqrt(dj);
        else d=0.0;
        if(d<-2.0 || d>2.0) break;
#if DEBUG_AP>=2
fprintf(stderr,"%d %f %f %f %f %f\n",i,db[i],sqrt(dj),sum,sump,d);
#endif
        }
      if(d>1.0) pt.polarity=1;
      else if(d<-1.0) pt.polarity=(-1);
      else pt.polarity=0;
#if DEBUG_AP>=2
fprintf(stderr,"f=%f dj=%f n1=%d pol=%d rat=%f\n",
freq,dj,n1,pt.polarity,sum);
#endif
      FREE(db);
      }
    show_pick(idx,&pt,iph);
    ret=1;
    }
fail:
#if DEBUG_AP>=2
fprintf(stderr,"out: %04X(%d) %2d.%03d-%2d.%03d pol=%d\n",ft.idx2ch[idx],iph,
pt.sec1,pt.msec1,pt.sec2,pt.msec2,pt.polarity);
#endif
  FREE(db2);
  FREE(aic);
  FREE(sd1);
  FREE(sd2);
  FREE(c);
  FREE(rec);
  return (ret);
  }

static int
cancel_picks(char *name, int idx)  /* cancel picks */
  /* char *name;    station name or NULL */
  /* int idx;       P/S/X/-1 */
  {
  int i,j;

  j=0;
  if(idx<0)
    {
    for(idx=0;idx<4;idx++) for(i=0;i<ft.n_ch;i++)
      if((name==NULL || strcmp(ft.stn[i].name,name)==0)
          && ft.pick[i][idx].valid)
        {
        put_mark(idx,ft.idx2pos[i],0);
        ft.pick[i][idx].valid=0;
        j=1;
        }
    }
  else
    {
    for(i=0;i<ft.n_ch;i++)
      if((name==NULL || strcmp(ft.stn[i].name,name)==0)
          && ft.pick[i][idx].valid)
        {
        put_mark(idx,ft.idx2pos[i],0);
        ft.pick[i][idx].valid=0;
        j=1;
        }
    }
  return (j);
  }

/* cancel calculated picks */
static int
cancel_picks_calc(void)
  {
  int i,j,jj,k,idx;

  j=0;
  for(i=0;i<ft.n_ch;i++)
    {
    jj=0;
    for(idx=0;idx<4;idx++) if(ft.pick_calc[i][idx].valid)
      {
      ft.pick_calc[i][idx].valid=0;
      j=jj=1;
      }
    if(jj) for(k=0;k<n_zoom;k++)
      if(zoom_win[k].valid && i==ft.ch2idx[zoom_win[k].sys_ch])
        plot_zoom(k,0,0,0);
    j=1;
    }
  ft.pick_calc_ot.valid=0;
  return (j);
  }

/* get picks */
static int
get_pick(char *name, int idx, struct Pick_Time *pt)
  /* char *name;            station name or NULL */
  /* int idx;               P/S/X/-1 */
  /* struct Pick_Time *pt;  pt to return, or NULL */
  {
  int i,j;
  j=0;
  for(i=0;i<ft.n_ch;i++)
    if(strcmp(ft.stn[i].name,name)==0 && ft.pick[i][idx].valid)
      {
      if(pt!=0) *pt=ft.pick[i][idx];
      j=1;
      }
  return (j);
  }

/* automatic pick & locate routine */
static int
auto_pick(int singl)
  /* int singl;   process just one event */
  {
  static Evdet ev={
    3,0,0,        /* int n_min_trig,n_trig_off,trigger, */
    1.0,50.0,       /* double dist1,dist2, */
    1000,5000,5,      /* int ms_on,ms_off,sub_rate, */  
    10.0,1.0,1.25,      /* double lt,st,ratio, */
    0.5,5.0,1.0,20.0,30.0 /* double ap,as,fl,fh,fs */
    };

#if 0
/* Example of winap.prm.  Just 15 lines with no comment line. */
3      /* n_min_trig */
0      /* n_trig_off */
1.0    /* dist1 */
100.0  /* dist2 */
1000   /* ms_on */
5000   /* ms_off */
5      /* sub_rate, */
10.0   /* lt */
1.0    /* st */
1.25   /* ratio */
0.5    /* ap */
5.0    /* as */
1.0    /* fl */
20.0   /* fh */
30.0   /* fs */
#endif

  int done;
  char tbuf[LINELEN];
  FILE *fp;

  doing_auto_pick=1;
  cancel_picks(NULL,-1);
  fprintf(stderr,"canceled all picks\n");
  fprintf(stderr,"event detection, auto-pick and locate ");
  if(singl) fprintf(stderr,"(single event)\n");
  else fprintf(stderr,"(entire file)\n");
  put_mon(x_zero,y_zero);
  /* some 'pick' file may have been loaded, but it will be deleted  */
  if((fp=fopen("winap.prm","r")))
    {
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%d",&ev.n_min_trig);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%d",&ev.n_trig_off);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.dist1);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.dist2);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%d",&ev.ms_on);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%d",&ev.ms_off);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%d",&ev.sub_rate);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.lt);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.st);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.ratio);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.ap);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.as);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.fl);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.fh);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&ev.fs);
    fclose(fp);
    }
  ft.fp_log=fopen(ft.log_file,"a+");
  ev.trigger=0;
  fprintf(ft.fp_log,"n_min_trig = %d\n",ev.n_min_trig);
  fprintf(ft.fp_log,"n_trig_off = %d\n",ev.n_trig_off);
  fprintf(ft.fp_log,"dist1      = %lf\n",ev.dist1);
  fprintf(ft.fp_log,"dist2      = %lf\n",ev.dist2);
  fprintf(ft.fp_log,"ms_on      = %d\n",ev.ms_on);
  fprintf(ft.fp_log,"ms_off     = %d\n",ev.ms_off);
  fprintf(ft.fp_log,"sub_rate   = %d\n",ev.sub_rate);
  fprintf(ft.fp_log,"lt         = %lf\n",ev.lt);
  fprintf(ft.fp_log,"st         = %lf\n",ev.st);
  fprintf(ft.fp_log,"ratio      = %lf\n",ev.ratio);
  fprintf(ft.fp_log,"ap         = %lf\n",ev.ap);
  fprintf(ft.fp_log,"as         = %lf\n",ev.as);
  fprintf(ft.fp_log,"fl         = %lf\n",ev.fl);
  fprintf(ft.fp_log,"fh         = %lf\n",ev.fh);
  fprintf(ft.fp_log,"fs         = %lf\n",ev.fs);
  done=evdet(&ev,singl);
  if(done && background==0 && singl==0)
    load_data(MSE_BUTNM); /* load the earliest 'pick' file */
  doing_auto_pick=0;
  fclose(ft.fp_log);
  ft.fp_log=0;
  return (done);
  }

static int
set_max(double d, int sec, int msec, int ch)
  {
  int i,idx;
  char tbuf1[10];
  struct Pick_Time pt;

  idx=ft.ch2idx[ch];
  pt.sec2=pt.sec1=sec;
  pt.msec2=pt.msec1=msec;
  *(float *)&(pt.valid)=d*ft.stn[idx].units_per_bit;
  strcpy(tbuf1,ft.stn[idx].unit);
  if(*tbuf1=='m')
    {
    if(strcmp(tbuf1,"m")==0) pt.polarity=0;
    else if(strcmp(tbuf1,"m/s")==0) pt.polarity=(-1);
    else if(strcmp(tbuf1,"m/s/s")==0) pt.polarity=(-2);
    else pt.polarity=1;
    }
  else pt.polarity=1;

  /* if same as before, cancel it */
  if(ft.pick[idx][MD].valid==pt.valid &&
    ft.pick[idx][MD].sec1==pt.sec1 &&
    ft.pick[idx][MD].msec1==pt.msec1) i=1;
  else i=0;

  /* cancel others for the same stn */
  cancel_picks(ft.stn[idx].name,MD);
  if(i) return(1);

  ft.pick[idx][MD]=pt;
  put_mark(MD,ft.idx2pos[idx],0);
  return(1);
  }

static void
auto_pick_range(Evdet_Tbl *tbl)
  {
  struct Pick_Time *p;
  int i,j,sec,msec,period;

  sec = msec = 0; /* for supress warnings */

  /* (1) measure predominant period and set P range */
  period=j=0;
  for(i=0;i<ft.n_trigch;i++)
    {
    if(ft.pick[ft.trigch[i]][P].valid)
      {
      p=(&tbl[i].pt);
      set_period(ft.trigch[i],p);       /* set P period */
      set_width(p,30*p->period,10*p->period); /* range of P */
#if DEBUG_AP>=2
fprintf(stderr,"%04X %d %d %d %d p=%d\n",ft.idx2ch[ft.trigch[i]],
p->sec1,p->msec1,p->sec2,p->msec2,p->period);
#endif
      period+=p->period;
      if(j==0 || p->sec1*1000+p->msec1<sec*1000+msec)
        {   /* get earliest time of P range */
        sec=p->sec1;
        msec=p->msec1;
        }
      j++;
      }
    }
  if(j==0) return;
  ft.period=(period/=j);  /* averaged period (for default) */
#if DEBUG_AP>=2
fprintf(stderr,"np=%d p=%d earliest=%2d.%03d\n",j,ft.period,sec,msec);
#endif
  if(--sec<2) /* earliest time: "2" at earliest, however */
    {
    sec=2;
    msec=0;
    }

  /* (2) adjust P range for 'sdp' */
  for(i=0;i<ft.n_trigch;i++)
    {
    if(ft.pick[ft.trigch[i]][P].valid)
      {
      p=(&tbl[i].pt);
#if DEBUG_AP>=2
fprintf(stderr,"ch=%04X p=%d sdps=%d rat=%.3f",ft.idx2ch[ft.trigch[i]],
p->period,p->sec_sdp,ft.stn[ft.trigch[i]].ratio);
#endif
      if((p->sec_sdp+1)*1000<p->sec1*1000+p->msec1)
        {
        if((p->sec_sdp+1)*1000<sec*1000+msec)
          {
          p->sec1=sec;
          p->msec1=msec;
          }
        else
          {
          p->sec1=p->sec_sdp+1;
          p->msec1=0;
          }
        }
      if(p->sec1==0)
        {
        p->sec1=1;
        p->msec1=0;
        }
      show_pick(ft.trigch[i],p,P);
#if DEBUG_AP>=2
fprintf(stderr,"%d %d %d %d\n",p->sec1,p->msec1,p->sec2,p->msec2);
#endif
      }
    }
  }

static void
auto_pick_pick(int sec_now, int hint)
  /* int sec_now;  limit of sec < ft.len */
  {
  struct Pick_Time pt;
  int i,j,k,sec,msec,idx,idx_s,n,n_max,n_min,c_max,c_min,ip;
  double *db,zero;

  /* (3) pick each station (P, and then S) */
  for(i=0;i<ft.n_trigch;i++)
    {
    if(ft.pick[idx=ft.trigch[i]][P].valid)
      {
      if(pick_phase(idx,P)) /* pick P phase */
        { /* if P is picked, then pick S and measure max ampl. */
        idx_s=pick_s(idx,sec_now,hint); /* pick S phase for the station */
        j=1;
        if(!hint || (hint && idx_s>=0 && ft.pick[idx_s][S].valid))
          { /* measure max in the range from P to 10 sec after S, or 'pt' */
          pt.sec1=ft.pick[idx][P].sec1;
          pt.msec1=ft.pick[idx][P].msec1;
          if(hint)
            {
            pt.sec2=ft.pick[idx][S].sec2;
            pt.msec2=ft.pick[idx][S].msec2;
            }
          else if(idx_s>=0 && ft.pick[idx_s][S].valid &&
                 ft.pick[idx_s][S].sec2+10<=sec_now)
            {
            pt.sec2=ft.pick[idx_s][S].sec2+10;
            pt.msec2=ft.pick[idx_s][S].msec2;
            }
          else
            {
            pt.msec2=1000;
            pt.sec2=sec_now;
            }
          pt.valid=1;
          k=ft.idx2pos[idx];
          for(;;)
            {
            n=getdata(idx,pt,&db,&ip);
            if(ip==0) /* not interpolated */
              {
              smeadl(db,n,&zero);
              if((j=get_max(db,n,&n_max,&n_min,&c_max,&c_min))==0) break;
              }
            FREE(db);
            if(--k<0 || strcmp(ft.stn[ft.pos2idx[k]].name,ft.stn[idx].name))
              break;
            idx=ft.pos2idx[k];
            }
          }
        if(j==0)  /* 'max' has been read */
          {
          if(db[n_max]<-db[n_min]) n_max=n_min;
          msec=(n_max*1000)/ft.sr[idx]+pt.msec1;
          sec=pt.sec1+msec/1000;
          msec%=1000;
          set_max(fabs(db[n_max]),sec,msec,ft.idx2ch[idx]);
          FREE(db);
          }
        }
      else cancel_picks(ft.stn[idx].name,-1);
      }
    }
  }

static void
auto_pick_hypo(char *tbuf, int hint)
  {
  int i,j,k;
  double tlim,omc,rat;
  char *name,tb[80];

  /************** hypocenter determination ***************/
  for(j=0;;j++)
    {
    k=0;
    for(i=0;i<ft.n_trigch;i++) if(get_pick(ft.stn[ft.trigch[i]].name,P,0)) k++;
    if(k<3)
      {
      strcpy(tbuf,"FAIL");
      fprintf(stderr,"location failed (less than three 'P' data)\n");
      break;  /* if less than three P data, quit location */
      }
    if(hint && j==0) j++;
    if(j==0)
      {
      raise_ttysw(1);
      fprintf(stderr,"PRELIMINARY TRY ...\n");
      hypo_use_ratio=1;
      locate(1,hint);
      raise_ttysw(0);
    /* omit 2*RMS(O-C) or 3.0 s for P */ 
    /* omit 2*RMS(O-C) or 5.0 s for S */
      for(i=0;i<ft.hypo.ndata;i++)
        {
        omc=ft.hypo.fnl[i].pomc;
        if((tlim=ft.hypo.pomc_rms*2.0)>=3.0)
          if(ft.hypo.fnl[i].pt!=0.0 && (omc>tlim || omc<-tlim))
            cancel_picks(ft.hypo.fnl[i].stn,P);
        omc=ft.hypo.fnl[i].somc;
        if((tlim=ft.hypo.somc_rms*2.0)>=5.0)
          if(ft.hypo.fnl[i].st!=0.0 && (omc>tlim || omc<-tlim))
            cancel_picks(ft.hypo.fnl[i].stn,S);
        }
      for(i=0;i<ft.n_trigch;i++)
        { /* if no P, cancel S, F and MD too. */
        name=ft.stn[ft.trigch[i]].name;
        if(get_pick(name,P,0)==0) cancel_picks(name,-1);
        }
      if(flag_hypo==0) j=(-1);
      else hypo_use_ratio=0;  /* proceed to 'j=1' */
      continue;
      }
    else
      {
      raise_ttysw(1);
      if(j==1) fprintf(stderr,"FIRST TRY ...\n");
      else if(j==2) fprintf(stderr,"SECOND TRY ...\n");
      else if(j==3) fprintf(stderr,"THIRD TRY ...\n");
      else fprintf(stderr,"%dTH TRY ...\n",j);
      locate(1,hint);
      raise_ttysw(0);
      }
    /* check O-C (TT/2 or 3 s (5 s for S)) and omit bad data */
    for(i=0;i<ft.hypo.ndata;i++)
      {
      omc=ft.hypo.fnl[i].pomc;
    /* if RMS>=0.5s, it must be (O-C)<RMS*2 for P */
      if((tlim=ft.hypo.pomc_rms*2.0)>=1.0)
        if(ft.hypo.fnl[i].pt!=0.0 && (omc>tlim || omc<-tlim))
          {
          cancel_picks(ft.hypo.fnl[i].stn,P);
          sprintf(tb,"1:canceled %s(%d)",ft.hypo.fnl[i].stn,P);
          writelog(tb);
          }
      omc=ft.hypo.fnl[i].somc;
    /* if RMS>=1.0s, it must be (O-C)<RMS*2 for S */
      if((tlim=ft.hypo.somc_rms*2.0)>=2.0)
        if(ft.hypo.fnl[i].st!=0.0 && (omc>tlim || omc<-tlim))
          {
          cancel_picks(ft.hypo.fnl[i].stn,S);
          sprintf(tb,"1:canceled %s(%d)",ft.hypo.fnl[i].stn,S);
          writelog(tb);
          }
      }
    if(flag_hypo && hint==0) for(i=0;i<ft.hypo.ndata;i++)
      {
      get_ratio(ft.hypo.fnl[i].stn,&rat);
      omc=ft.hypo.fnl[i].pomc/(rat/3.0);
      tlim=5.0;
    /* (O-C)<5.0 for P */
      if(ft.hypo.fnl[i].pt!=0.0 && (omc>tlim || omc<-tlim))
        {
        cancel_picks(ft.hypo.fnl[i].stn,P);
        sprintf(tb,"2:canceled %s(%d)",ft.hypo.fnl[i].stn,P);
        writelog(tb);
        }
      omc=ft.hypo.fnl[i].somc/(rat/3.0);
      tlim=8.0;
    /* (O-C)<8.0 for S */
      if(ft.hypo.fnl[i].st!=0.0 && (omc>tlim || omc<-tlim))
        {
        cancel_picks(ft.hypo.fnl[i].stn,S);
        sprintf(tb,"2:canceled %s(%d)",ft.hypo.fnl[i].stn,S);
        writelog(tb);
        }
      }
    if(flag_hypo && hint==0) for(i=0;i<ft.hypo.ndata;i++)
      {
      /* (O-C)<0.05*(TT/2)+(2*error)) */ 
      if(ft.hypo.fnl[i].pt!=0.0)
        {
        tlim=0.05*(ft.hypo.fnl[i].pt-ft.hypo.fnl[i].pomc-ft.hypo.se)
          +ft.hypo.fnl[i].pe*2.0;
        if(tlim>3.0) tlim=3.0;
        else if(tlim<0.3) tlim=0.3;
        if(ft.hypo.fnl[i].pomc>tlim || ft.hypo.fnl[i].pomc<-tlim)
          {
          cancel_picks(ft.hypo.fnl[i].stn,P);
          sprintf(tb,"3:canceled %s(%d)",ft.hypo.fnl[i].stn,P);
          writelog(tb);
          }
        }
      if(ft.hypo.fnl[i].st!=0.0)
        {
        tlim=0.05*(ft.hypo.fnl[i].st-ft.hypo.fnl[i].somc-ft.hypo.se)
          +ft.hypo.fnl[i].se*2.0;
        if(tlim>5.0) tlim=5.0;
        else if(tlim<0.5) tlim=0.5;
        if(ft.hypo.fnl[i].somc>tlim || ft.hypo.fnl[i].somc<-tlim)
          {
          cancel_picks(ft.hypo.fnl[i].stn,S);
          sprintf(tb,"3:canceled %s(%d)",ft.hypo.fnl[i].stn,S);
          writelog(tb);
          }
        }
      }
    for(i=0;i<ft.n_trigch;i++)
      { /* if no P, cancel S, F and MD too. */
      name=ft.stn[ft.trigch[i]].name;
      if(get_pick(name,P,0)==0) cancel_picks(name,-1);
      }
    if(flag_hypo) /* no data has been deleted */
      {
      if(ft.hypo.dep<-3.0)
        {
        raise_ttysw(1);
        fprintf(stderr,"AIR FOCUS - DEPTH FIXED ...\n");
        init_dep =0;
        init_depe=0;
        locate(1,hint);
        init_dep =init_dep_init;
        init_depe=init_depe_init;
        raise_ttysw(0);
        strcpy(tbuf,"Z-FIX");
        }
      else sprintf(tbuf,"M%.1f",ft.hypo.mag);
      if(background) break;
      /* go to MAP */
      raise_ttysw(0);
      loop_stack[loop_stack_ptr++]=loop;
      loop=LOOP_MAP;
      first_map=1;
      init_map(MSE_BUTNR);
      sleep(1);
      /* return to LIST */
      loop=loop_stack[--loop_stack_ptr];
      if(!background) refresh(0);
      raise_ttysw(1);
      break;
      }
    else if(j==10)    /* try location upto 10 times */
      {
      strcpy(tbuf,"FAIL");
      break;
      }
    }
  /************** end of hypocenter determination ***************/
  }

/* automatic pick & locate with a preliminary hypocenter */
static int
auto_pick_hint(int save)
  {
  static Aph aph={
  0.05,    /* ratio to travel time for allowance */
  0.5,     /* width for search P (sec) */
  0.5      /* width for search S (sec) */
  };
#if 0
/* Example of winaph.prm.  Just 3 lines with no comment line. */
0.05   /* ratio to travel time for allowance */
0.5    /* width for search P (sec) */
0.5    /* width for search S (sec) */
#endif

  int i,width;
  char tbuf[20];
  struct Pick_Time pt;
  double tt;
  FILE *fp;

  if(ft.pick_calc_ot.valid==0)
    {
    fprintf(stderr,"no preliminary hypocenter\n");
    return (0);
    }
  doing_auto_pick=1;
  cancel_picks(NULL,-1);    /* cancel all picks */
  fprintf(stderr,"canceled all picks\n");
  fprintf(stderr,"auto-pick and locate with a preliminary hypocenter\n");
  put_mon(x_zero,y_zero);

  ft.fp_log=fopen(ft.log_file,"a+");
  if((fp=fopen("winaph.prm","r")))
    {
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&aph.ttp);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&aph.wp);
    fgets(tbuf,LINELEN,fp);sscanf(tbuf,"%lf",&aph.ws);
    fclose(fp);
    fprintf(ft.fp_log,"winaph.prm read\n");
    }
  else fprintf(ft.fp_log,"winaph.prm not found\n");

  fprintf(ft.fp_log,"ttp = %lf\n",aph.ttp);
  fprintf(ft.fp_log,"wp  = %lf\n",aph.wp);
  fprintf(ft.fp_log,"ws  = %lf\n",aph.ws);
  fclose(ft.fp_log);
  ft.fp_log=0;

  get_trigch();
  /* set P & S range */
  for(i=0;i<ft.n_trigch;i++)
    {
    if(ft.pick_calc[ft.trigch[i]][P].valid)
      {
      tt=(double)(ft.pick_calc[ft.trigch[i]][P].sec1-ft.pick_calc_ot.sec1)+
      (double)(ft.pick_calc[ft.trigch[i]][P].msec1-ft.pick_calc_ot.msec1)*0.001;
      pt=ft.pick_calc[ft.trigch[i]][P];
      width=(int)((tt*aph.ttp+aph.wp)*1000); /* default: 5% of TT plus 0.5 sec */
      set_width(&pt,width,width);
      show_pick(ft.trigch[i],&pt,P);
      }
    if(ft.pick_calc[ft.trigch[i]][S].valid)
      {
      tt=(double)(ft.pick_calc[ft.trigch[i]][S].sec1-ft.pick_calc_ot.sec1)+
      (double)(ft.pick_calc[ft.trigch[i]][S].msec1-ft.pick_calc_ot.msec1)*0.001;
      pt=ft.pick_calc[ft.trigch[i]][S];
      width=(int)((tt*aph.ttp+aph.ws)*1000); /* default: 5% of TT plus 0.5 sec */
      set_width(&pt,width,width);
      show_pick(ft.trigch[i],&pt,S);
      }
    }

  auto_pick_pick(ft.len-1,1); /* pick each station (P, and then S) */
  auto_pick_hypo(tbuf,1); /* hypocenter determination */
  get_calc();           /* calculate theoretical travel times for all stns */

  /* save result to a pick file */
  set_diagnos(tbuf,getname(geteuid()));
  if(save) save_data(0);

  /* return to MAIN */
  main_mode=MODE_NORMAL;
  raise_ttysw(0);
  if(!background) refresh(0);
  doing_auto_pick=0;
  return (1);
  }

/* automatic pick & locate (single) */
static int
auto_pick_single(Evdet_Tbl *tbl, int sec_now, int save)
  /* int save;   if 1, save result */
  {
  int i;
  char tbuf[20],tb[80];

  for(i=0;i<ft.n_trigch;i++) if(ft.pick[ft.trigch[i]][P].valid)
    {
    if(ft.pick[ft.trigch[i]][X].valid==0)
      ft.stn[ft.trigch[i]].ratio=tbl[i].ratio;
    }

  auto_pick_range(tbl);    /* measure predominant period and set P range */
  auto_pick_pick(sec_now,0); /* pick each station (P, and then S) */
  for(i=0;i<ft.n_trigch;i++)
    {
    sprintf(tb,"%s %f",ft.stn[ft.trigch[i]].name,ft.stn[ft.trigch[i]].ratio);
    writelog(tb);
    }
  auto_pick_hypo(tbuf,0); /* hypocenter determination */
  get_calc();           /* calculate theoretical travel times for all stns */

  /* save result to a pick file */
  set_diagnos(tbuf,getname(geteuid()));
  if(save)
    {
    save_data(0);
    if(auto_flag) if(load_data(MSE_BUTNR)==0)
      {  /* load the next 'pick' file */
      *ft.save_file=0;
      cancel_picks(NULL,-1);
      }
    }
  /* return to MAIN */
  main_mode=MODE_NORMAL;
  raise_ttysw(0);
  if(!background) refresh(0);
  return (1);
  }

static int
get_ratio(char *stn, double *rat)
  {
  int i;

  for(i=0;i<ft.n_trigch;i++) if(ft.pick[ft.trigch[i]][P].valid)
    {
    if(strcmp(ft.stn[ft.trigch[i]].name,stn)==0)
      {
      *rat=ft.stn[ft.trigch[i]].ratio;
      break;
      }
    }
  if(i==ft.n_trigch) return (0);
  else return (1);
  }

static void
set_diagnos(char *tb, char *ub)
  {
  char *ptr;
  int i;

  strcpy(diagnos,"                             ");
  ptr=diagnos+1;
  if(tb)
    {
    if((i=strlen(tb))>strlen(ptr)-1) i=strlen(ptr)-1;
    memcpy(ptr,tb,i);
    ptr+=i+1;
    }
  if(ub)
    {
    if((i=strlen(ub))>strlen(ptr)-1) i=strlen(ptr)-1;
    if(i>0) memcpy(ptr+strlen(ptr)-i-1,ub,i);
    }
  }

static int
pick_s(int idx, int sec_now, int hint)
  {
  struct Pick_Time pt,pt_s,pt1,pt_ss;
  int n,done,sec,msec,idxs,width_s=0,idx_s,period,ms,n_max,n_min,
    c_max,c_min,i,j;
  char *name,*comp;
  double *db,zero;

  memset(&pt_ss, 0, sizeof(pt_ss));  /* for supress warning */
  done=0;
  idx_s=(-1);
  if(ft.pick[idx][P].valid==0) return (-1);
  if(!hint)
    {
    if((period=ft.pick[idx][P].period)==0)
      if((period=ft.period)==0) period=4*1000/ft.sr[idx];
#if DEBUG_AP>=2
pt=ft.pick[idx][P];
fprintf(stderr,"%2d.%03d - %2d.%03d %d %d %d\n",pt.sec1,pt.msec1,pt.sec2,pt.msec2,
pt.period,ft.period,period);
#endif
    /* from 10T after P time, */
    pt1.sec1=ft.pick[idx][P].sec2;
    pt1.msec1=ft.pick[idx][P].msec2+period*10;
    while(pt1.msec1>=1000) {pt1.msec1-=1000;pt1.sec1++;}
    /* to 'sec_now' */
    pt1.sec2=sec_now;
    pt1.msec2=0;
    pt1.valid=1;
    /* pt1 is the range to search the MAXIMUM AMPLITUDE */
#if DEBUG_AP>=2
fprintf(stderr,"%2d.%03d - %2d.%03d\n",pt1.sec1,pt1.msec1,pt1.sec2,pt1.msec2);
#endif
  if(get_width(&pt1)<1000) return (-1);  /* too short, something wrong. */
    }
  else
    {
    if(ft.pick[idx][S].valid==0) return (-1);
    pt_ss=ft.pick[idx][S];
    if(ft.pick[idx][P].valid &&
        ((double)pt_ss.sec1+(double)pt_ss.msec1*0.001 <
         (double)ft.pick[idx][P].sec2+(double)ft.pick[idx][P].msec2*0.001))
      {
      pt_ss.sec1=ft.pick[idx][P].sec2;
      pt_ss.msec1=ft.pick[idx][P].msec2;
      }
    }
  name=ft.stn[idx].name;
  for(j=0;j<2;j++) /* j==0 - horizontal, j==1 - vertical */
    {
    for(idxs=0;idxs<ft.n_ch;idxs++)
      {
      if(strcmp(ft.stn[idxs].name,name)) continue;
      comp=ft.stn[idxs].comp;
      if((j==0 && (*comp!='N' && *comp!='S' && *comp!='E' &&
        *comp!='W' && *comp!='H')) || (j==1 &&
        (*comp!='V' && *comp!='U' && *comp!='D'))) continue;
      if(!hint)
        {
        n=getdata(idxs,pt1,&db,&i); /* get data of the range pt1 */ 
        smeadl(db,n,&zero);     /* offset nulling */
        get_max(db,n,&n_max,&n_min,&c_max,&c_min);  /* search MAX */
        if(db[n_max]==db[n_min]) {FREE(db);continue;}
#if DEBUG_AP>=2
fprintf(stderr,"n=%d n_max=%d n_min=%d\n",n,n_max,n_min);
#endif
        if(db[n_max]<-db[n_min]) n_max=n_min;
        FREE(db);
        msec=(n_max*1000)/ft.sr[idxs]+pt1.msec1;
        sec=pt1.sec1+msec/1000;
        msec%=1000;     /* sec.msec is the time of MAX */
      /* set 'pt' range to start from midpoint of P and MAX */
        pt=ft.pick[idx][P]; /* this is 'valid',  of course. */
        ms=(msec+pt.msec1+(sec+pt.sec1)*1000)/2;
        pt.msec1=ms%1000;
        pt.sec1=ms/1000;
        pt.msec2=msec;
        pt.sec2=sec+1;  /* set pt range to end at 1 s after MAX */
        }
      else pt=pt_ss;
#if DEBUG_AP>=2
fprintf(stderr,"%2d.%03d : %2d.%03d\n",pt.sec1,pt.msec1,pt.sec2,pt.msec2);
#endif
      show_pick(idxs,&pt,S);
      if(pick_phase(idxs,S))
        {
        if(done==0 || get_width(&ft.pick[idxs][S])<width_s)
          {
          width_s=get_width(&ft.pick[idxs][S]);
          pt_s=ft.pick[idxs][S];
          idx_s=idxs;
          }
        done=1;
        }
      }
    if(done) break;
    }
  if(done) show_pick(idx_s,&pt_s,S);
  else cancel_picks(name,S);
  return (idx_s);
  }

static void
get_trigch(void)
  {
  int i,k,j,jj;

  if(ft.trigch==NULL)
    if((ft.trigch=(int16_w *)win_xmalloc(sizeof(*ft.trigch)*ft.n_ch))==NULL)
      emalloc("ft.trigch");
  /* select V/VH chs */
  k=j=0;
  while(j<ft.n_ch)
    {
    i=0;
    for(jj=j;jj<ft.n_ch;jj++)
      {
      if(strcmp(ft.stn[ft.pos2idx[j]].name,
        ft.stn[ft.pos2idx[jj]].name)) break;
      if(*ft.stn[ft.pos2idx[jj]].comp=='V' ||
          *ft.stn[ft.pos2idx[jj]].comp=='U' ||
          *ft.stn[ft.pos2idx[jj]].comp=='D')
        {
        ft.trigch[k]=ft.pos2idx[jj];
        i=1;
        }
      }
    if(i && ft.stn[ft.trigch[k]].north!=0.0
                         && ft.stn[ft.trigch[k]].east!=0.0) k++;
    if(jj==ft.n_ch) break;
    j=jj;
    }
  ft.n_trigch=k;
  }

static int
evdet(Evdet *ev, int singl)
  /* int singl;  process just one event */
  {
  static Evdet_Tbl *tbl;
  int i,k,j,jj,sec,ch,trig,m,done,sub,trig_off,sec_on,
    sec_start,save;
  WIN_sr sr;  /* OLD : int */
  double sd,zero,c[MAX_FILT*4],dmin,dmax,drange,dd,lta,ratio;

  if(singl) {sec_start=x_zero/PIXELS_PER_SEC_MON;save=0;}
  else {sec_start=0;save=1;}
  get_trigch(); /* select vertical component chs -> ft.trigch */
  done=0;
  if(tbl==NULL && (tbl=(Evdet_Tbl *)win_xmalloc(sizeof(*tbl)*ft.n_trigch))==NULL)
    emalloc("tbl");
/* get AR models from one of the first 5 sec */
/* get first trig level from RMS of AR filter output */
  for(ch=0;ch<ft.n_trigch;ch++) tbl[ch].use=0;
  for(sec=sec_start+1;sec<sec_start+5;sec++)
    {
    for(ch=0;ch<ft.n_trigch;ch++)
      {
      if((sr=read_one_sec(sec,ft.idx2ch[ft.trigch[ch]],buf,KILL_SPIKE))==0)
        continue;
      for(j=0;j<sr;j++) dbuf[j]=(double)buf[j];
      if(getar(dbuf,sr,&sd,&m,c,&zero,0)>=0 && m>0 && sd>0.0)
        {
        if(tbl[ch].use==0 || sd<tbl[ch].sd)
          {
          tbl[ch].use=1;
          tbl[ch].sd=sd;
          tbl[ch].m=m;
          for(j=0;j<m;j++) tbl[ch].c[j]=c[j];
          tbl[ch].zero=zero;
          }
        }
      }
    }

  for(ch=0;ch<ft.n_trigch;ch++) if(tbl[ch].use)
    {
    tbl[ch].al=1.0/((double)ev->sub_rate*ev->lt);
    tbl[ch].bl=1.0-tbl[ch].al;
    tbl[ch].as=1.0/((double)ev->sub_rate*ev->st);
    tbl[ch].bs=1.0-tbl[ch].as;
    tbl[ch].sta=tbl[ch].lta=tbl[ch].ratio=0.0;
    tbl[ch].count_on=tbl[ch].count_off=0;
    for(i=0;i<tbl[ch].m;i++) tbl[ch].rec[i]=0.0;
    for(i=0;i<4;i++) tbl[ch].res[i]=0.0;
    tbl[ch].flag=1;
    tbl[ch].dis=(-1);
    tbl[ch].score=tbl[ch].status=tbl[ch].pt.valid=0;
    tbl[ch].sec_sdp=0;
    tbl[ch].sdp=tbl[ch].sd*2.0;
    sr=ft.sr[ft.trigch[ch]];
    butpas(tbl[ch].f.coef,&tbl[ch].f.m_filt,&tbl[ch].f.gn_filt,
      &tbl[ch].f.n_filt,ev->fl/(double)sr,ev->fh/(double)sr,
      ev->fs/(double)sr,ev->ap,ev->as);
    if(tbl[ch].f.m_filt>MAX_FILT)
      {
      fprintf(stderr,"*** filter order exceeded limit %d ch=%04X ***\007\007\n",
        tbl[ch].f.m_filt,ft.idx2ch[ft.trigch[ch]]);
      tbl[ch].use=0;
      }
    for(j=0;j<tbl[ch].f.m_filt*4;j++) tbl[ch].uv[j]=0.0;
#if DEBUG_AP>=1
fprintf(stderr,"%3d ch=%04X m=%2d zero=%.2f BPF=%d\n",ch,ft.idx2ch[ft.trigch[ch]],
tbl[ch].m,tbl[ch].zero,tbl[ch].f.m_filt);
#endif
    }
#if DEBUG_AP>=1
fprintf(stderr,"n=%d\n",ft.n_trigch);
#endif
/* scan ft.n_trigch channels data, applying AR filter, to find trigger */
  for(sec=sec_start+1;sec<ft.len;sec++)
    {
    if(background==0)
      {
      sprintf(apbuf,"%4d s (%02x:%02x:%02x)",sec,ft.ptr[sec].time[3],
        ft.ptr[sec].time[4],ft.ptr[sec].time[5]);
      put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
      }
    trig=0;
    trig_off=1;
#if DEBUG_AP>=3
fprintf(stderr,"%2d ",sec);if((sec+1)%20==0) fprintf(stderr,"\n");fflush(stderr);
#endif
    for(ch=0;ch<ft.n_trigch;ch++) if(tbl[ch].use)
      {
      if((sr=read_one_sec(sec,ft.idx2ch[ft.trigch[ch]],buf,KILL_SPIKE))==0)
        continue;
      jj=k=buf[0];
#if DEBUG_AP>=6
#define SSS 0
#define CCC 0x119
if(sec>=SSS && ft.idx2ch[ft.trigch[ch]]==CCC){
fprintf(stderr,"RAW  ");for(i=0;i<sr;i++) fprintf(stderr,"%d ",buf[i]);
fprintf(stderr,"\n");getchar();}
#endif
      j=0;
      for(i=1;i<sr;i++) /* kill line failure by replacing with '0' */
        {
        if(buf[i]==k && (k>20 || k<-20))
          {
          if(++j==3) buf[i]=buf[i-1]=buf[i-2]=buf[i-3]=jj;
          else if(j>3) buf[i]=jj;
          }
        else
          {
          jj=k;
          k=buf[i];
          j=0;
          }
        }
#if DEBUG_AP>=6
if(sec>=SSS && ft.idx2ch[ft.trigch[ch]]==CCC){
fprintf(stderr,"LINE ");for(i=0;i<sr;i++) fprintf(stderr,"%d ",buf[i]);
fprintf(stderr,"\n");getchar();}
#endif
      for(j=0;j<4;j++) dbuf[j]=tbl[ch].res[j];
      i=0;
      jj=(int)tbl[ch].zero;
      for(;j<sr;j++) dbuf[j]=(double)(buf[i++]-jj);
#if DEBUG_AP>=6
if(sec>=SSS && ft.idx2ch[ft.trigch[ch]]==CCC){
fprintf(stderr,"RES ");for(j=0;j<sr;j++) fprintf(stderr,"%.0f ",dbuf[j]);
fprintf(stderr,"\n");getchar();}
#endif
      for(j=0;j<4;j++) tbl[ch].res[j]=(double)(buf[i++]-jj);
      for(i=2;i<6;i++)  /* kill spike */
        {
        dmax=dmin=dbuf[i-2];
        if(dbuf[i-1]>dmax) dmax=dbuf[i-1];
        else if(dbuf[i-1]<dmin) dmin=dbuf[i-1];
        if(dbuf[i+1]>dmax) dmax=dbuf[i+1];
        else if(dbuf[i+1]<dmin) dmin=dbuf[i+1];
        if(dbuf[i+2]>dmax) dmax=dbuf[i+2];
        else if(dbuf[i+2]<dmin) dmin=dbuf[i+2];
        drange=(dmax-dmin+1.0)*4.0;
        if(dbuf[i]>dmax+drange || dbuf[i]<dmin-drange)
          dbuf[i]=(dbuf[i-1]+dbuf[i+1])/2.0;
        }
#if DEBUG_AP>=6
if(sec>=SSS && ft.idx2ch[ft.trigch[ch]]==CCC){
fprintf(stderr,"KILL ");for(j=0;j<sr;j++) fprintf(stderr,"%.0f ",dbuf[j]);
fprintf(stderr,"\n");getchar();}
#endif
      /* AR filter */
      digfil(dbuf,dbuf2,sr,tbl[ch].c,tbl[ch].m,tbl[ch].rec,&sd);
      for(j=0;j<sr;j++) dbuf[j]-=dbuf2[j];
#if DEBUG_AP>=6
if(sec>=SSS && ft.idx2ch[ft.trigch[ch]]==CCC){
fprintf(stderr,"AR  ");for(j=0;j<sr;j++) fprintf(stderr,"%.0f ",dbuf[j]);
fprintf(stderr,"\n");getchar();}
#endif
      /* BPF */
      tandem(dbuf,dbuf,sr,tbl[ch].f.coef,tbl[ch].f.m_filt,1,tbl[ch].uv);
      for(j=0;j<sr;j++) dbuf[j]*=tbl[ch].f.gn_filt;
      if(tbl[ch].pt.valid==0 && sd<tbl[ch].sd*2.0) tbl[ch].sec_sdp=sec;
      tbl[ch].sdp=sd;
#if DEBUG_AP>=6
if(sec>=SSS && ft.idx2ch[ft.trigch[ch]]==CCC){
fprintf(stderr,"BPF ");for(j=0;j<sr;j++) fprintf(stderr,"%.0f ",dbuf[j]);
fprintf(stderr,"\n");getchar();}
#endif
    /* find trigger */
      sub=sr/ev->sub_rate;
      k=0;
      for(j=0;j<sr;j++)
        {
        if(k++==0) dmax=dmin=dbuf[j];
        else if(dbuf[j]>dmax) dmax=dbuf[j];
        else if(dbuf[j]<dmin) dmin=dbuf[j];
        if(k<sub) continue;
        dd=dmax-dmin; /* '+3.0' worked good, but why ? */
        tbl[ch].sta=tbl[ch].as*dd+tbl[ch].bs*tbl[ch].sta;
        if(sec==1)
          {
          tbl[ch].lta=tbl[ch].sta;
          continue;
          }
        tbl[ch].lta=tbl[ch].al*dd+tbl[ch].bl*tbl[ch].lta;
        if(tbl[ch].status==0) lta=tbl[ch].lta;
        else lta=tbl[ch].lta_save;
#if DEBUG_AP>=5
if(ft.idx2ch[ft.trigch[ch]]==0x40 || ft.idx2ch[ft.trigch[ch]]==0x128
|| ft.idx2ch[ft.trigch[ch]]==0x8f){
fprintf(stderr,"ch=%04X j=%d (%.3f %.3f %.3f)\n",ft.idx2ch[ft.trigch[ch]],j,
dd,tbl[ch].sta,tbl[ch].lta);}
#endif
        if(lta==0.0 || (sec>=(int)ev->lt/2 && tbl[ch].sta/lta<0.1))
          {  /* data abnormal */
          tbl[ch].use=0;
          fprintf(ft.fp_log," %02d %03d   %04X %s ABNORM\n",
            sec,j*1000/sr,ft.idx2ch[ft.trigch[ch]],
            ft.stn[ft.trigch[ch]].name);
          if(ft.pick[ft.trigch[ch]][P].valid && tbl[ch].status>=2)
            confirm_off(ch,sec,j*1000/sr,tbl,ev);
          break;
          }
        ratio=(tbl[ch].sta-1.0)/lta;
        if(ratio>ev->ratio)
          {
          if(tbl[ch].status==0)
            {
            tbl[ch].status=1;
            tbl[ch].on_sec=sec;
            tbl[ch].on_msec=j*1000/sr;
            tbl[ch].lta_save=tbl[ch].lta;
            }
          else if(tbl[ch].status==3) tbl[ch].status=2;
          tbl[ch].count_on++;
          tbl[ch].count_off=0;
          if(tbl[ch].status==1 && tbl[ch].count_on*
              1000/ev->sub_rate > ev->ms_on)
            confirm_on(ch,sec,j*1000/sr,tbl,ev);
          if(tbl[ch].status>0 && ratio>tbl[ch].ratio)
            tbl[ch].ratio=ratio;  /* record max ratio */
          }
        else
          {
          if(tbl[ch].status==1) tbl[ch].status=0;
          tbl[ch].count_on=0;
          if(tbl[ch].status==2) tbl[ch].status=3;
          tbl[ch].count_off++;
          if(tbl[ch].status==3 && tbl[ch].count_off*
              1000/ev->sub_rate > ev->ms_off &&
              tbl[ch].lta<tbl[ch].lta_save)
            confirm_off(ch,sec,j*1000/sr,tbl,ev);
          if(tbl[ch].status==0) tbl[ch].ratio=0.0;
          }
        k=0;
#if DEBUG_AP>=5
if(ft.idx2ch[ft.trigch[ch]]==0x40 || ft.idx2ch[ft.trigch[ch]]==0x128
|| ft.idx2ch[ft.trigch[ch]]==0x8f){
fprintf(stderr,"ch=%04X s=%d j=%d %.2f %d d=%.2f on=%d of=%d %.2f %.2f %.2f (%.2f-%.2f) sd=%.2f\n",
ft.idx2ch[ft.trigch[ch]],sec,j,dbuf[j],tbl[ch].status,dd,
tbl[ch].count_on,tbl[ch].count_off,
tbl[ch].sta,tbl[ch].lta,ratio,dmax,dmin,sd);}
#endif
        }
      if(tbl[ch].flag)
        {
        if(tbl[ch].score>=ev->n_min_trig) trig=1;
        if(tbl[ch].score>ev->n_trig_off) trig_off=0;
        }
      }

    if(trig && ev->trigger==0)
      {
      ev->trigger=1;
      fprintf(ft.fp_log,"%02d +\n",sec_on=sec);fflush(ft.fp_log);
      for(ch=0;ch<ft.n_trigch;ch++)
        if(tbl[ch].use && tbl[ch].status>1 && tbl[ch].pt.valid)
          show_pick(ft.trigch[ch],&tbl[ch].pt,P);
      }
    else if(ev->trigger && trig_off)
      {
      ev->trigger=0;
      fprintf(ft.fp_log,"%02d -\n",sec);fflush(ft.fp_log);
      done=auto_pick_single(tbl,sec,save);
      if(singl) break;
      }
#if DEBUG_AP>=4
for(ch=0;ch<ft.n_trigch;ch++)
if(ft.idx2ch[ft.trigch[ch]]==0x40 || ft.idx2ch[ft.trigch[ch]]==0x128
|| ft.idx2ch[ft.trigch[ch]]==0x8f){
fprintf(stderr,"ch=%04X sd=%.3f\n",ft.idx2ch[ft.trigch[ch]],tbl[ch].sdp);}
#endif
    }
#if DEBUG_AP>=3
fprintf(stderr,"\n");
#endif
  if(ev->trigger)
    {
    fprintf(ft.fp_log,"%02d - EOF\n",sec);fflush(ft.fp_log);
    done=auto_pick_single(tbl,sec-1,save);
    }
  if(done==0)
    {
    set_diagnos("NOTRG",getname(geteuid()));
    if(singl==0) save_data(0);
    }
  return (done);
  }

static void
confirm_on(int ch, int sec, int msec, Evdet_Tbl *tbl, Evdet *ev)
  {
  int j;
  double x1,x2,y1,y2,d;

  tbl[ch].status=2;
  if(ev->trigger==0 || (ev->trigger && ft.pick[ft.trigch[ch]][P].valid==0))
    {
    cancel_picks(ft.stn[ft.trigch[ch]].name,-1);
    j=1000/ev->sub_rate;
    set_pick(&tbl[ch].pt,tbl[ch].on_sec,tbl[ch].on_msec,j,j);
    tbl[ch].pt.sec_sdp=tbl[ch].sec_sdp;
    if(ev->trigger) show_pick(ft.trigch[ch],&tbl[ch].pt,P);
    }
#if DEBUG_AP>=1
fprintf(stderr,"ch=%04X sr=%d\n",ft.idx2ch[ft.trigch[ch]],ft.sr[ft.trigch[ch]]);
#endif
  x1=(double)ft.stn[ft.trigch[ch]].x;
  y1=(double)ft.stn[ft.trigch[ch]].y;
  for(j=0;j<ft.n_trigch;j++)
    {
    if(j==ch) tbl[ch].score++;        /* +1 to myself */
    else if(tbl[ch].flag)
      {
      x2=(double)ft.stn[ft.trigch[j]].x;
      y2=(double)ft.stn[ft.trigch[j]].y;
      /* d=sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)); */
      d = hypot(x2-x1, y2-y1);
      if(d<ev->dist1 && tbl[j].dis==(-1)) /* 'too near' station */
        {
        tbl[j].flag=0;        /*  is disabled */
        tbl[j].dis=ch;        /*  by channel 'ch' */
        fprintf(ft.fp_log,"  %s DIS < %s\n",ft.stn[ft.trigch[j]].name,
          ft.stn[ft.trigch[ch]].name);
        }
      else if(d<ev->dist2) tbl[j].score++;  /* +1 within DIST2 km */
      }
    }
  fprintf(ft.fp_log," %02d %03d + %04X %s (%.3f)",tbl[ch].on_sec,
    tbl[ch].on_msec,ft.idx2ch[ft.trigch[ch]],
    ft.stn[ft.trigch[ch]].name,tbl[ch].ratio);
  if(tbl[ch].flag) fprintf(ft.fp_log,"\n");
  else fprintf(ft.fp_log," (NC)\n");
  fflush(ft.fp_log);
  }

static void
confirm_off(int ch, int sec, int msec, Evdet_Tbl *tbl, Evdet *ev)
  {
  int j;
  double x1,x2,y1,y2,d;
  struct Pick_Time pt;

  tbl[ch].status=0;
  if(tbl[ch].pt.valid)
    {
    tbl[ch].pt.valid=0;
    if(ft.pick[ft.trigch[ch]][P].valid)
      {
      j=1000/ev->sub_rate;
      set_pick(&pt,sec,msec,j,j);
      show_pick(ft.trigch[ch],&pt,X); /* 'F' time */
      ft.stn[ft.trigch[ch]].ratio=tbl[ch].ratio;
      }
    }
  x1=(double)ft.stn[ft.trigch[ch]].x;
  y1=(double)ft.stn[ft.trigch[ch]].y;
  for(j=0;j<ft.n_trigch;j++)
    {
    if(j==ch) tbl[ch].score--;          /* -1 to myself */
    else
      {
      x2=(double)ft.stn[ft.trigch[j]].x;
      y2=(double)ft.stn[ft.trigch[j]].y;
      /* d=sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)); */
      d = hypot(x2-x1, y2-y1);
      if(d<ev->dist1 && tbl[j].dis==ch) /* 'too near' station */
        {
        tbl[j].flag=1;      /* is enabled */
        tbl[j].dis=(-1);    /* by whom having disabled */
        fprintf(ft.fp_log,"  %s EN  < %s\n",ft.stn[ft.trigch[j]].name,
          ft.stn[ft.trigch[ch]].name);
        }
      else if(d<ev->dist2) tbl[j].score--;  /* -1 within DIST2 km */
      }
    }
  fprintf(ft.fp_log," %02d %03d - %04X %s (%.3f)\n",sec,msec,
    ft.idx2ch[ft.trigch[ch]],ft.stn[ft.trigch[ch]].name,tbl[ch].ratio);
  fflush(ft.fp_log);
  }

static void
plot_mon(int base_sec, int mon_len, register int wmb, uint8_w *buf_mon)
  {
  register int32_w *ptr;
  register int x_byte,y,y_min,y_max,x,yy;
  register int32_w ofs;
  double dy;
  int i,j,kk,xx;
  uint8_w ymask;
  static uint8_w bit_mask[8]=
    {0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80};

  /* plot mon traces */
  xx=0;
  for(i=0;i<mon_len;i++)
    {
    for(j=0;j<ft.n_ch;j++)
      {
      x=xx;
      yy=ppt_half+ft.idx2pos[j]*pixels_per_trace;
      if(read_one_sec_mon(i+base_sec,ft.idx2ch[j],buf,PIXELS_PER_SEC_MON)>0)
        {
        if(mon_offset && base_sec==0 && i==0)
          {
          dy=0.0;
          ptr=buf;
          for(kk=0;kk<PIXELS_PER_SEC_MON*2;kk++) dy+=(double)(*ptr++);
          ofs=ft.stn[j].offset=(int32_w)(dy/(double)kk);
          }
        else ofs=ft.stn[j].offset;
        ptr=buf;
        for(kk=0;kk<PIXELS_PER_SEC_MON;kk++)
          {
          y=ppt_half;
          y_min=(-(((*ptr++)-ofs)>>ft.stn[j].scale));
          if(y_min>=y)     y_min=yy+y-1;
          else if(y_min<(-y)) y_min=yy-y;
          else        y_min+=yy;
          y_max=(-(((*ptr++)-ofs)>>ft.stn[j].scale));
          if(y_max>=y)     y_max=yy+y-1;
          else if(y_max<(-y)) y_max=yy-y;
          else        y_max+=yy;
          ymask=bit_mask[x&0x7];
          x_byte=wmb*y_max+((x++)>>3);
          while(y_max++<=y_min) /* y_min>=y_max */
            {
            buf_mon[x_byte]|=ymask;
            x_byte+=wmb;
            }
          }
        }
      }
    xx+=PIXELS_PER_SEC_MON;
    fprintf(stderr,".");
    fflush(stderr);
    if((i+1)%60==0) fprintf(stderr,"%d\n",i+1);
    }
  if(i%60!=0) fprintf(stderr,"%d\n",i);
  }

static void
mapconv(int argc, char *argv[], int args)
  {
  struct {float x,y;} a;
  int flag;
  double alat,along,x,y;
  char tb[80];
  uint8_w c[4];
  union Swp {
    float f;
    uint32_w i;
    /* uint8_w c[4]; */
    } swp;

  if(args+1<argc)
    {
    sscanf(argv[args],"%lf",&alat0);
    sscanf(argv[args+1],"%lf",&along0);
    }
  else
    {
    fprintf(stderr,"usage of 'mapconv' (or '%s -c') :\n",NAME_PRG);
    fprintf(stderr,"   mapconv [lat. of orgin] [long. of orgin] < [infile] > [outfile]\n");
    return;
    }
  fprintf(stderr,"***** mapconv ***** (origin=%8.4fN %8.4fE)\n",alat0,along0);
  swp.f=(float)alat0;
  c[0]=swp.i>>24;
  c[1]=swp.i>>16;
  c[2]=swp.i>>8;
  c[3]=swp.i;
  fwrite(c,1,4,stdout);
  swp.f=(float)along0;
  c[0]=swp.i>>24;
  c[1]=swp.i>>16;
  c[2]=swp.i>>8;
  c[3]=swp.i;
  fwrite(c,1,4,stdout);

  flag=0;
  while(fgets(tb,sizeof(tb),stdin)!=NULL && flag==0)
    {
    if(*tb=='#') continue;
    sscanf(tb,"%lf%lf",&alat,&along);
    if(alat<999.0 && along<999.0) /* coast line data */
      {
      pltxy(alat0,along0,&alat,&along,&x,&y,0);
      a.x=(float)x;
      a.y=(float)y;
      }
    else if(along>999.0)  /* end of map */
      {
      a.x=10000.0;
      a.y=10000.0;
      flag=1;
      }
    else if(alat>99999.0) /* begin trench axes */
      {
      a.x=1000000.0;
      a.y=0.0;
      }
    else if(alat>9999.0)  /* begin prefectural borders */
      {
      a.x=100000.0;
      a.y=0.0;
      }
    else          /* pen up to move */
      {
      a.x=10000.0;
      a.y=0.0;
      }
    swp.f=a.x;
    c[0]=swp.i>>24;
    c[1]=swp.i>>16;
    c[2]=swp.i>>8;
    c[3]=swp.i;
    fwrite(c,1,4,stdout);
    swp.f=a.y;
    c[0]=swp.i>>24;
    c[1]=swp.i>>16;
    c[2]=swp.i>>8;
    c[3]=swp.i;
    fwrite(c,1,4,stdout);
    }
  }

static void
bye_entry(void)
 {

   got_hup=1;
 }

static void
end_process(int ret)
  {

  raise_ttysw(1);
  /* delete temporary files */
  unlink(ft.seis_file);
  unlink(ft.init_file);
  unlink(ft.finl_file);
  unlink(ft.rept_file);
  unlink(ft.seis_file2);
  unlink(ft.init_file2);
  unlink(ft.finl_file2);
  unlink(ft.rept_file2);
  unlink(ft.othrs_file);
  unlink(ft.dat_file);
  unlink(ft.log_file);
/*  auto_wrap_on();*/
  exit(ret);
  }

static void
set_geometry(void)
  {

  /* geometry for MECHA */
  mec_xzero=width_dpy/2;
  mec_yzero=HEIGHT_TEXT*3+(height_dpy-HEIGHT_TEXT*3)/2;
  if(mec_xzero<mec_yzero) mec_rc=0.8*(double)mec_xzero;
  else mec_rc=0.8*(double)(height_dpy-mec_yzero);

  com_dep1=0;
  com_dep2=com_dep1+WIDTH_TEXT*12;
  com_depe1=com_dep2;
  com_depe2=com_depe1+WIDTH_TEXT*6;
  x_time_file=x_func(OPEN)+WB+HW;
  com_diag2=x_func(UD)-HW;
  com_diag1=x_time_now=com_diag2-WIDTH_TEXT*strlen(diagnos);

/* window position & size */
  x_win_mon=WIDTH_INFO;
  y_win_mon=MARGIN+HEIGHT_FUNC;
  if((n_zoom_max=(height_dpy-1-MARGIN-HEIGHT_FUNC)/HEIGHT_ZOOM)>N_ZOOM_MAX)
    n_zoom_max=N_ZOOM_MAX;
  while(n_zoom>n_zoom_max) close_zoom(n_zoom-1);
  height_win_mon=height_dpy-1-MARGIN-HEIGHT_FUNC-n_zoom*HEIGHT_ZOOM;
  width_win_mon =width_dpy-WIDTH_INFO;
  if((x_zero_max=ft.len*PIXELS_PER_SEC_MON-width_win_mon)<0) x_zero_max=0;
  if((y_zero_max=height_mon-height_win_mon)<0) y_zero_max=0;
  x_win_info=0;
  y_win_info=MARGIN+HEIGHT_FUNC;
  width_win_info =WIDTH_INFO;
  width_zoom=width_dpy+1;
  height_zoom=HEIGHT_ZOOM+1;
  }

int
main(int argc, char *argv[])
  {
  FILE *fp;
  int yy,i,j,k,base_sec,mon_len,i_mon,c,mc;
  char textbuf[LINELEN],tbuf[LINELEN],chstr[100],file_exclusive[LINELEN];
  uint8_w *buf_mon;
  char  *ptr;
  int16_w i2p;
  int x,y;
  unsigned int w,h,d;  /* 64bit ok */
  unsigned int ui, uj;  /* 64bit ok */
  Window root,parent;
/*   extern int optind; */
/*   extern char *optarg; */

#if (defined(__FreeBSD__) && (__FreeBSD__ < 4))
#include <floatingpoint.h>
#endif

#if (defined(__FreeBSD__) && (__FreeBSD__ < 4))
  /* allow divide by zero -- Inf */
  fpsetmask(fpgetmask() & ~(FP_X_DZ|FP_X_INV));
#endif

  if((ptr=getenv("WIN_PICK_SERVER"))) strcpy(ft.pick_server,ptr);
  else *ft.pick_server=0;
  if((ptr=getenv("WIN_PICK_SERVER_PORT"))) ft.pick_server_port=(unsigned short)atoi(ptr);
  else ft.pick_server_port=PICK_SERVER_PORT;
  sprintf(ft.param_file,"%s.prm",NAME_PRG);
  background=map_only=mc=bye=auto_flag=auto_flag_hint=not_save=autpk_but_off=calc_line_off=0;
  copy_file=got_hup=ft.ch_exclusive=0;
  mon_offset=just_hypo=just_hypo_offset=0;
  do_chck =(-1);
  /* flag_save=sec_block=1; */
  sec_block=1;
  flag_save=0;  /* .sv obsolete. */
  *ft.final_opt=0;
  dot='.';
  *chstr=0;
  while((c=getopt(argc,argv,"abcd:e:fghi:mnop:qrs:twx:z:BC:S_"))!=-1)
    {
    switch(c)
      {
      case 'a':   /* auto pick */
        auto_flag=1;
        break;
      case 'b':   /* background */
        background=bye=1;
        break;
      case 'c':   /* works as 'mapconv' */
        mc=1;
        break;
      case 'd':   /* final (hypocenter) data file or dir */
        strcpy(ft.final_opt,optarg);
        break;
      case 'e':   /* ch exclusive */
        strcpy(file_exclusive,optarg);
        ft.ch_exclusive|=1;
        break;
      case 'f':   /* fit height */
        fit_height=1;
        break;
      case 'g':   /* channels table check */
        do_chck=1;
/*        do_chck = 0;
	fprintf(stderr, "\n\n**** Ignore results of channels table check!!\n");
	fprintf(stderr, "**** Use '-g' option AT YOUR OWN RISK!!\n\n\n");
*/
        break;
      case 'i':   /* ch inclusive */
        strcpy(chstr,optarg);
        ft.ch_exclusive|=2;
        break;
      case 'm':   /* just map */
        map_only=1;
        ft.n_ch=0;
        break;
      case 'n':   /* not-save mode; not to save pick data */
        not_save=1; /* this is valid only in the auto-pick */
        break;
      case 'o':   /* remove offset in MON traces */
        mon_offset=1;
        flag_save=0;   /* don't use MON bitmap */
        break;
      case 'p':   /* specify parameter file name */
        strcpy(ft.param_file,optarg);
        break;
      case 'q':   /* 'bye' mode */
        bye=1;
        break;
      case 'r':   /* auto pick (with hint) */
        auto_flag_hint=1;
        break;
      case 's':   /* specify find_picks server & port */
        strcpy(tbuf,optarg);
        if((ptr=strchr(tbuf,':')))
          {
          *ptr=0;
          ft.pick_server_port=(unsigned short)atoi(ptr+1);
          }
        strcpy(ft.pick_server,tbuf);
        break;
      case 't':   /* use temporary data file in temp dir */
        copy_file=1;
        break;
      case 'w':   /* save MON bitmap */
        /* flag_save=2; */
	flag_save=0;  /* obsolete */
	fprintf(stderr, "'-w' option is obsolete.\n");
        break;
      case 'x':   /* just calculate hypocenter */
        strcpy(ft.save_file,optarg);
        just_hypo=background=bye=1;
        break;
      case 'z':   /* auto-pick and event-detect button on */
	if(strchr(optarg,'a')) autpk_but_off=1;
	if(strchr(optarg,'c')) calc_line_off=1;
        break;
      case 'B':   /* don't use MON bitmap */
        flag_save=0;
	fprintf(stderr, "'-B' option is obsolete.\n");
        break;
      case 'C':   /* cursor color */
        strcpy(cursor_color,optarg);
        break;
      case 'S':   /* don't assume second block */
        sec_block=0;
        break;
      case '_':   /* use '_' instead '.' for pick file */
        dot='_';
        break;
      case 'h':   /* show 'help' */
      default:
        print_usage();
        exit(0);
      }
    }
  if(auto_flag==0 || auto_flag_hint==0) not_save=0;
  if(mc)
    {
    mapconv(argc,argv,optind);
    exit(0);
    }
  signal(SIGINT,(void *)end_process);
  signal(SIGTERM,(void *)end_process);
  signal(SIGHUP,(void *)bye_entry);
  fprintf(stderr,"***  %s  (%s)  ***\n",NAME_PRG,WIN_VERSION);
  WIN_version();
  /* fprintf(stderr, "%s\n", rcsid); */

  lat_cent=lon_cent=200.0;  /* unrealistic position */
  first_map=first_map_others=1;
  ratio_vert=3; /* 2,3 or 5 */
  map_dir=0;    /* -45 -> +45 */
  map_vert=1;   /* draw vertical cross section ? */
  map_vstn=0;   /* plot stations on vertical cross section ? */
  map_true=0;   /* hypocenter symbol size in empirical fault size ? */
  map_ellipse=0;  /* draw error ellipse ? */
  map_name=0;   /* print stations & their name on map ? */
  other_epis=0; /* plot other hypoceters ? */
  flag_hypo=flag_mech=mech_name=flag_change=0;
  *mec_hemi=0;
  pu.valid=doing_auto_pick=0;
/* initialization process */
  if(ft.ch_exclusive)
    {
    flag_save=0;
    if(ft.ch_exclusive&1)
      {
      for(i=0;i<N_CH_NAME;i++) ft.ch_use[i]=0;
      if(*file_exclusive)
        {
        if((fp=fopen(file_exclusive,"r"))!=NULL)
          {
	   while(fgets(tbuf,sizeof(tbuf),fp))
            {
            if(*tbuf=='#') continue;
            if(sscanf(tbuf,"%x",&k)!=1) continue;
            k&=0xffff;
            ft.ch_use[k]=1;
            }
          fclose(fp);
          }
        else
          {
          fprintf(stderr,"exclusive ch file '%s' not open\n",file_exclusive);
          exit(1); 
          }
        }
      }
    else for(i=0;i<N_CH_NAME;i++) ft.ch_use[i]=1;
    if(ft.ch_exclusive&2)
      {
      j=strlen(chstr);
      for(i=0;i<N_CH_NAME;i++)
        {
        sprintf(tbuf,"%04X",i);
        if(strncasecmp(tbuf,chstr,j)==0) ft.ch_use[i]&=1;
        else ft.ch_use[i]=0;
        } 
      }
    }
  if(init_process(argc,argv,optind)==0) {
    if(copy_file)
      unlink(ft.dat_file);
    exit(1);
  }
  if(NULL == (dbuf  =(double *)win_xmalloc((ft.sr_max+1)*sizeof(double)))){
     fprintf(stderr, "Cannot malloc dbuf\n");
     exit(1);
  }
  if(NULL == (dbuf2 =(double *)win_xmalloc((ft.sr_max+1)*sizeof(double)))){
     fprintf(stderr, "Cannot malloc dbuf2\n");
     exit(1);
  }
  if(NULL == (buf   =  (int32_w *)win_xmalloc((ft.sr_max+1)*sizeof(int32_w)))){
     fprintf(stderr, "Cannot malloc buf\n");
     exit(1);
  }
  if(NULL == (buf2  =  (int32_w *)win_xmalloc((ft.sr_max+1)*sizeof(int32_w)))){
     fprintf(stderr, "Cannot malloc buf2\n");
     exit(1);
  }
  if(NULL == (points=(lPoint *)win_xmalloc((ft.sr_max+1)*sizeof(lPoint)))){
     fprintf(stderr, "Cannot malloc points\n");
     exit(1);
  }
  writelog(NAME_PRG);
  writelog(WIN_VERSION);
  writelog(ft.data_file);
  writelog(ft.param_file);
/* physical display size */
  get_screen_type(&nplane,&width_dpy,&height_dpy,&width_frame,&height_frame);
  set_diagnos("","");
  n_zoom=0;
  set_geometry();

/* define bitmap dpy */
  if(map_only) sprintf(textbuf," %s map",NAME_PRG);
  else sprintf(textbuf," %s %s",NAME_PRG,ft.data_file);
  if(background) goto bg;
  black=BlackPixel(disp,0);
  dpy.drw=XCreateSimpleWindow(disp,DefaultRootWindow(disp),
    fit_height?0:(width_frame-width_dpy)/2,
    fit_height?0:(height_frame-(height_dpy+30))/2,
    width_dpy,height_dpy,0,BlackPixel(disp,0),WhitePixel(disp,0));
  XStoreName(disp,dpy.drw,textbuf);
  sizehints.flags=PPosition|PResizeInc;
  XSetWMNormalHints(disp,dpy.drw,&sizehints);
  sprintf(textbuf,"black=%lu(%08lX) white=%lu(%08lX)",  /* 64bit ok */
    (unsigned long)BlackPixel(disp,0),(unsigned long)BlackPixel(disp,0),
    (unsigned long)WhitePixel(disp,0),(unsigned long)WhitePixel(disp,0));
  writelog(textbuf);
  invert_bits(buf_epi_s,sizeof(buf_epi_s));
  invert_bits(buf_epi_l,sizeof(buf_epi_l));
  invert_bits((uint8_w *)font16,sizeof(font16));
  define_bm(&dpy,BM_FB,width_dpy,height_dpy,0);

/* make patterns */
  define_bm(&sym,BM_MEM,16*4,58,(char *)buf_sym);  
  define_bm(&sym_stn,BM_MEM,16*1,16,(char *)buf_sym_stn);
  define_bm(&arrows_ud,BM_MEM,16*2,16,(char *)buf_arrows_ud);
  define_bm(&arrows_lr,BM_MEM,16*2,16,(char *)buf_arrows_lr);
  define_bm(&arrows_lr_zoom,BM_MEM,16*2,16,(char *)buf_arrows_lr_zoom);
  define_bm(&arrows_leng,BM_MEM,16*2,16,(char *)buf_arrows_leng);
  define_bm(&arrows_scale,BM_MEM,16*2,16,(char *)buf_arrows_scale);
  define_bm(&epi_s,BM_MEM,16*1,16,(char *)buf_epi_s);
  define_bm(&epi_l,BM_MEM,16*1,16,(char *)buf_epi_l);

/* create GCs */
  for(i=0;i<N_LPTN;i++)
    {
    gc_line[i]=XCreateGC(disp,dpy.drw,0,0);
    gc_line_mem[i]=XCreateGC(disp,sym.drw,0,0);
    XSetPlaneMask(disp,gc_line[i],BlackPixel(disp,0)^WhitePixel(disp,0));
    XSetForeground(disp,gc_line[i],BlackPixel(disp,0));
    XSetBackground(disp,gc_line[i],WhitePixel(disp,0));
    XSetForeground(disp,gc_line_mem[i],1);
    XSetBackground(disp,gc_line_mem[i],0);
    if(i==0)
      {
      XSetLineAttributes(disp,gc_line[i],0,LineSolid,CapButt,JoinMiter);
      XSetLineAttributes(disp,gc_line_mem[i],0,LineSolid,CapButt,JoinMiter);
      }
    else
      {
      XSetLineAttributes(disp,gc_line[i],0,LineOnOffDash,CapButt,JoinMiter);
      XSetDashes(disp,gc_line[i],0,patterns[i],2);
      XSetLineAttributes(disp,gc_line_mem[i],0,LineOnOffDash,CapButt,JoinMiter);
      XSetDashes(disp,gc_line_mem[i],0,patterns[i],2);
      }
    }

  gc_fb=XCreateGC(disp,dpy.drw,0,0);  /* depth=(display's depth) */
  XSetPlaneMask(disp,gc_fb,BlackPixel(disp,0)^WhitePixel(disp,0));
  XSetForeground(disp,gc_fb,BlackPixel(disp,0));
  XSetBackground(disp,gc_fb,WhitePixel(disp,0));
  XSetGraphicsExposures(disp,gc_fb,False);

  gc_fbi=XCreateGC(disp,dpy.drw,0,0);  /* depth=(display's depth) */
  XSetPlaneMask(disp,gc_fbi,BlackPixel(disp,0)^WhitePixel(disp,0));
  XSetForeground(disp,gc_fbi,WhitePixel(disp,0));
  XSetBackground(disp,gc_fbi,BlackPixel(disp,0));
  XSetGraphicsExposures(disp,gc_fbi,False);

  gc_mem=XCreateGC(disp,sym.drw,0,0);  /* depth=1 */
  XSetForeground(disp,gc_mem,1);
  XSetBackground(disp,gc_mem,0);
  XSetGraphicsExposures(disp,gc_mem,False);

  gc_memi=XCreateGC(disp,sym.drw,0,0);  /* depth=1 */
  XSetForeground(disp,gc_memi,0);
  XSetBackground(disp,gc_memi,1);
  XSetGraphicsExposures(disp,gc_memi,False);

/* define cursor */
  XQueryBestCursor(disp,dpy.drw,SIZE_CURSOR,SIZE_CURSOR,&ui,&uj);
  if((s_cursor=ui)>uj) s_cursor=uj;
  if(s_cursor%2==0) s_cursor--;
  define_bm(&cursor,BM_MEM,s_cursor,s_cursor,0);
  put_bitblt(&cursor,0,0,s_cursor,s_cursor,&cursor,0,0,BF_SDX);
  define_bm(&cursor_mask,BM_MEM,s_cursor,s_cursor,0);
  put_bitblt(&cursor_mask,0,0,s_cursor,s_cursor,&cursor_mask,0,0,BF_SDX);
  draw_seg(s_cursor/2,0,s_cursor/2,s_cursor-1,LPTN_FF,BF_1,&cursor);
  draw_seg(0,s_cursor/2,s_cursor-1,s_cursor/2,LPTN_FF,BF_1,&cursor);
  XSetLineAttributes(disp,gc_line_mem[LPTN_FF],3,LineSolid,CapButt,JoinMiter);
  draw_circle(s_cursor/2,s_cursor/2,s_cursor/2,LPTN_FF,BF_1,&cursor);
  XSetLineAttributes(disp,gc_line_mem[LPTN_FF],0,LineSolid,CapButt,JoinMiter);
  colormap=DefaultColormap(disp,0);
  c_black.pixel=BlackPixel(disp,0);
  XQueryColor(disp,colormap,&c_black);
  c_white.pixel=WhitePixel(disp,0);
  XQueryColor(disp,colormap,&c_white);
  x11_cursor=XCreatePixmapCursor(disp,cursor.drw,cursor.drw,
    &c_black,&c_white,s_cursor/2,s_cursor/2);
  if(XAllocNamedColor(disp,colormap,cursor_color,&c_cursor,&c_c)==True)
    XRecolorCursor(disp,x11_cursor,&c_cursor,&c_white);
  XDefineCursor(disp,dpy.drw,x11_cursor);

bg: if(map_only) goto skip_mon;
  else if(just_hypo) goto skip_mon_just_hypo;
#if 0 /* HINET_EXTENTION_3 */
  load_data_prep(MSE_BUTNL);
  /*get_delta();*/
#endif
/* define bitmap info */
  height_mon=ft.n_ch*pixels_per_trace;
  width_info=WIDTH_INFO;
  height_info=height_mon;
  define_bm(&info,BM_MEM,width_info,height_info,0);
  put_white(&info,0,0,width_info,height_info);

/* define bitmap zoom */
  define_bm(&zoom,BM_MEM,WIDTH_INFO_ZOOM+1,height_zoom,0);
  put_white(&zoom,0,0,WIDTH_INFO_ZOOM+1,height_zoom);
  /* make zoom info format */
  put_fram(&zoom,0,0,WIDTH_INFO_ZOOM+1,height_zoom);
  put_fram(&zoom,0,1,WIDTH_INFO_ZOOM+1,height_zoom-1);
  put_black(&zoom,X_Z_CHN,Y_Z_CHN,W_Z_CHN+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_POL,Y_Z_POL,W_Z_POL+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_SCL,Y_Z_SCL,W_Z_SCL+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_TSC,Y_Z_TSC,W_Z_TSC+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_SFT,Y_Z_SFT,W_Z_SFT+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_FLT,Y_Z_FLT,W_Z_FLT+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_GET,Y_Z_GET,W_Z_GET+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_PUT,Y_Z_PUT,W_Z_PUT+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_CLS,Y_Z_CLS,W_Z_CLS+1,PIXELS_PER_LINE+1);
  put_bitblt(&arrows_scale,0,0,32,16,&zoom,X_Z_SCL+X_Z_ARR,Y_Z_SCL+Y_Z_ARR,BF_SDO);
  put_bitblt(&arrows_leng,0,0,32,16,&zoom,X_Z_TSC+X_Z_ARR,Y_Z_TSC+Y_Z_ARR,BF_SDO);
  put_bitblt(&arrows_lr_zoom,0,0,32,16,&zoom,X_Z_SFT+X_Z_ARR,
    Y_Z_SFT+Y_Z_ARR,BF_SDO);
  put_text(&zoom,X_Z_POL+(W_Z_POL-WIDTH_TEXT*3)/2,Y_Z_POL+Y_Z_OFS,"+/-",BF_SDO);
  put_text(&zoom,X_Z_FLT+WIDTH_TEXT,Y_Z_FLT+Y_Z_OFS,"   NO FILTER     ",BF_SDO);
  put_text(&zoom,X_Z_GET+(W_Z_GET-WIDTH_TEXT*3)/2,Y_Z_GET+Y_Z_OFS,"GET",BF_SDO);
  put_text(&zoom,X_Z_PUT+(W_Z_PUT-WIDTH_TEXT*3)/2,Y_Z_PUT+Y_Z_OFS,"PUT",BF_SDO);
  put_text(&zoom,X_Z_CLS+(W_Z_CLS-WIDTH_TEXT*3)/2,Y_Z_CLS+Y_Z_OFS,"CLS",BF_SDO);

/* print info lines */
  yy=k=0;
  for(j=0;j<ft.n_ch;j++)
    {
    sprintf(textbuf,"%04X",ft.idx2ch[ft.pos2idx[j]]);
    for(i=0;i<WIDTH_INFO_C-4;i++) textbuf[4+i]=' ';
    sprintf(tbuf,"%d",ft.stn[ft.pos2idx[j]].scale);
    if(strlen(tbuf)==1) strcpy(textbuf+WIDTH_INFO_C-1,tbuf);
    else strcpy(textbuf+WIDTH_INFO_C-2,tbuf);
    sprintf(tbuf,"%s-%s",ft.stn[ft.pos2idx[j]].name,ft.stn[ft.pos2idx[j]].comp);
    if(!(j==0 || strcmp(ft.stn[ft.pos2idx[j]].name,
        ft.stn[ft.pos2idx[j-1]].name)))
      for(i=0;i<strlen(ft.stn[ft.pos2idx[j]].name)+1;i++) tbuf[i]=' ';
    if(WIDTH_INFO_C-5-1>=strlen(tbuf))
      for(i=0;i<strlen(tbuf);i++) textbuf[5+i]=tbuf[i];
    else for(i=0;i<strlen(tbuf);i++) textbuf[4+i]=tbuf[i]; 
    if(j>0 && ft.stn[ft.pos2idx[j]].rflag!=ft.stn[ft.pos2idx[j-1]].rflag) k^=1;
    if(k) put_text(&info,0,yy,textbuf,BF_SI);
    else put_text(&info,0,yy,textbuf,BF_S);
    yy+=pixels_per_trace;
    }

  /* make mon */
  ft.len_mon=(MEMORY_LIMIT/height_mon)/PIXELS_PER_SEC_MON;
  if((ft.w_mon=ft.len_mon*PIXELS_PER_SEC_MON)>MAX_SHORT)
    {
    ft.len_mon=MAX_SHORT/PIXELS_PER_SEC_MON;
    ft.w_mon=ft.len_mon*PIXELS_PER_SEC_MON;
    }
  ft.n_mon=(ft.len-1)/ft.len_mon+1;   /* n of bitmaps */
  fprintf(stderr,"%d sec x %d chs (%d bitmap(s))\n",ft.len,ft.n_ch,ft.n_mon);
  if(background && flag_save!=2) goto bg1;
  if((mon=(lBitmap *)win_xmalloc(sizeof(lBitmap)*ft.n_mon))==NULL) emalloc("mon");
  base_sec=0;
  width_mon_max=0;
  for(i_mon=0;i_mon<ft.n_mon;i_mon++)
    {
    if((mon_len=ft.len-base_sec)>ft.len_mon) mon_len=ft.len_mon;
    if((width_mon=mon_len*PIXELS_PER_SEC_MON)>width_mon_max)
      width_mon_max=width_mon;
    if((buf_mon=(uint8_w *)win_xmalloc(p2w(width_mon)*height_mon*2))
      ==NULL) break;
    for(i=0;i<p2w(width_mon)*height_mon*2;i++) buf_mon[i]=0x00;
    fprintf(stderr,"map #%d/%d : %d sec x %d chs (%d x %d)",
      i_mon+1,ft.n_mon,mon_len,ft.n_ch,width_mon,height_mon);
    fprintf(stderr,"   %d bytes\n",p2w(width_mon)*height_mon*2);
    ft.w_mon_last=width_mon;
    /* plot mon */
    if(flag_save==1 && mon_offset==0)
      {
      if(i_mon==0) for(j=0;j<ft.n_ch;j++)
        read(ft.fd_save,(char *)&(ft.stn[j].offset),sizeof(int32_w));
      for(j=0;j<ft.n_ch;j++)
        {
        read(ft.fd_save,&i2p,2);
        if(i2p!=ft.idx2pos[j]) break;
        }
      if(j==ft.n_ch)
        {
        fprintf(stderr,"loading saved bitmap file '%s'\n",ft.mon_file);
        read(ft.fd_save,buf_mon,p2w(width_mon)*height_mon*2);
        invert_bits(buf_mon,p2w(width_mon)*height_mon*2);
        }
      else
        {
        fprintf(stderr,"saved bitmap file '%s' inconsistent\007\n",ft.mon_file);
        for(j=0;j<ft.n_ch;j++) ft.stn[j].offset=0;
        plot_mon(base_sec,mon_len,p2w(width_mon)*2,buf_mon);
        }
      }
    else plot_mon(base_sec,mon_len,p2w(width_mon)*2,buf_mon);
    define_bm(&mon[i_mon],BM_MEM,p2w(width_mon)*16,height_mon,(char *)buf_mon);
    if(flag_save==2)
      {
      if(i_mon==0) for(j=0;j<ft.n_ch;j++)
        write(ft.fd_save,(char *)&(ft.stn[j].offset),sizeof(int32_w));
      write(ft.fd_save,ft.idx2pos,2*ft.n_ch);
      invert_bits(buf_mon,p2w(width_mon)*height_mon*2);
      write(ft.fd_save,buf_mon,(width_mon+15)/16*height_mon*2);
      }
    FREE(buf_mon);
    base_sec+=mon_len;
    }
  if(i_mon<ft.n_mon)
    {
    fprintf(stderr,"*** Data truncated at %d sec ***\n",base_sec);
    ft.n_mon=i_mon;
    ft.len=ft.n_mon*ft.len_mon;
    }
#if HINET_EXTENTION_3>=2
  if(load_data_prep(MSE_BUTNL)) replot_mon(0);
#endif
bg1:if(flag_save) close(ft.fd_save);
  if((x_zero_max=ft.len*PIXELS_PER_SEC_MON-width_win_mon)<0) x_zero_max=0;
  if((y_zero_max=height_mon-height_win_mon)<0) y_zero_max=0;
  loop=LOOP_NO;
skip_mon_just_hypo:
  load_data(MSE_BUTNL);
  if(just_hypo) do_just_hypo();
skip_mon:
  if(background==0)
    {
    XMapRaised(disp,dpy.drw);
    if(fit_height)
      {
      XMoveWindow(disp,dpy.drw,0,0);
      XSync(disp,False);
      usleep(100000);
      xgetorigin(disp,dpy.drw,&x,&y,&w,&h,&d,&root,&parent);
      XMoveWindow(disp,dpy.drw,-x,-y);
      fit_height=y;
      }
    XSync(disp,False);
    usleep(100000);
    XSelectInput(disp,dpy.drw,ButtonPressMask|ButtonReleaseMask|
      PointerMotionMask|ExposureMask);
    }
  x_zero=y_zero=0;
/* Loop forever, examining each event */
  loop_stack_ptr=1;
  main_mode=MODE_NORMAL;
  loop=LOOP_MAIN;
/*  auto_wrap_off();*/
  if(map_only) do_map();
  else if(auto_flag) do_auto_pick();
  else if(auto_flag_hint) do_auto_pick_hint();
  else if(background) end_process(0);
  else
    {
    bell();
    refresh(0);
    if(bye) end_process(0);
    else bye_process();
    }
  window_main_loop();
  }
/***** end of main() *****/

static void
bye_process(void)
  {

  signal(SIGHUP,(void *)end_process);
  if(got_hup) end_process(0);
  }

static void
do_auto_pick(void)
  {

  if(background) fprintf(stderr,"AUTO-PICK mode\n");
  else
    {
    refresh(0);
    strcpy(apbuf,"EVDET & AUTO-PICK");
    put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
    }
  auto_pick(0);
  if(bye) end_process(0);
  else bye_process();
  auto_flag=0;
  }

static void
do_auto_pick_hint(void)
  {

  if(background) fprintf(stderr,"AUTO-PICK W/HINT mode\n");
  else
    {
    refresh(0);
    strcpy(apbuf," AUTOPICK W/HINT ");
    put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
    }
  do /* repeat auto_pick_hint() as many as pick files */
    {
    auto_pick_hint(1);
    } while(load_data(MSE_BUTNR));

  if(bye) end_process(0);
  else bye_process();
  auto_flag_hint=0;
  }

static void
do_just_hypo(void)
  {

  fprintf(stderr,"JUST-HYPO mode\n");
  load_data(MSE_BUTNL);

  locate(1,0);
  save_data(0);
  end_process(0);
  }

static void
proc_alarm(void)
  {

  read_hypo=1;
  raise_ttysw(0);
  refresh(0);
  alarm(map_interval*60);
  signal(SIGALRM,(void *)proc_alarm);
  }

static void
do_map(void)
  {

  other_epis=1;
  loop=LOOP_MAP;
  if(map_interval)
    {
    alarm(map_interval*60);
    signal(SIGALRM,(void *)proc_alarm);
    }
  init_map(MSE_BUTNL);
  if(bye) end_process(0);
  else bye_process();
  }

static void
window_main_loop(void)
  {

  while (1)
    {
    wait_mouse(); /* Get the next event */
    switch(loop)
      {
      case LOOP_MAIN: proc_main();break;
      case LOOP_MAP:  proc_map();break;
      case LOOP_MECHA:proc_mecha();break;
      case LOOP_PSUP: proc_psup();break;
      }
    }
  }

/*
auto_wrap_on()
  {
  fprintf(stderr,"\033[?7h");
  fflush(stderr);
  }

auto_wrap_off()
  {
  fprintf(stderr,"\033[?7l");
  fflush(stderr);
  }
*/

static void
open_save(void)
  {
#define MAGIC 601
  int32_w magic;

  /* flag_save: 0-none, 1-load, 2-save */
  sprintf(ft.mon_file,"%s.sv",ft.data_file);
  if(flag_save==2 && (ft.fd_save=open(ft.mon_file,
      O_WRONLY|O_CREAT|O_TRUNC,0664))>=0)
    {
    magic=MAGIC;
    write(ft.fd_save,&magic,sizeof(magic));
    }
  else if((ft.fd_save=open(ft.mon_file,O_RDONLY))>=0)
    {
    magic=0;
    read(ft.fd_save,&magic,sizeof(magic));
    if(magic==MAGIC) flag_save=1;
    else
      {
      close(ft.fd_save);
      flag_save=0;
      fprintf(stderr,"ignore bitmap file '%s' (different endian ?)\007\n",
        ft.mon_file);
      }
    }
  else flag_save=0;
  }

static void
plot_zoom(int izoom, int leng, struct Pick_Time *pt, int put)
  {
  FILE *fp=NULL;
  char path[NAMLEN],filename[NAMLEN],text_buf[LINELEN], fmt[5];
  int8_w cc;
  int16_w ss;
  int32_w ll;
  int xzero,yzero,i,j,k,buf0=0,i_map,xz,join,start,np,np_last=0,x,y,xp=0,ymax=0,ymin=0;  /* just for suppress warning */
  WIN_sr sr;
  char tbuf[100],tbuf1[STNLEN+CMPLEN];
  double uv[MAX_FILT*4],rec[MAX_FILT*4],sd,dk;
  lPoint pts[5];
  int  over_flow;

  if(put)
    {
    read_parameter(PARAM_WAVE,path);
    read_parameter(PARAM_FMT,text_buf);
    sscanf(text_buf,"%4s",fmt);
    fmt[0]=(char)toupper((unsigned char)fmt[0]);
    }
  /* reverse mon */
  if(zoom_win[izoom].valid)
    {
    i_map=(xz=zoom_win[izoom].sec_save*PIXELS_PER_SEC_MON)/ft.w_mon;
    xz-=i_map*ft.w_mon;
    put_reverse(&mon[i_map],xz,zoom_win[izoom].pos_save*pixels_per_trace,
      zoom_win[izoom].length_save*PIXELS_PER_SEC_MON,pixels_per_trace);
    if(++i_map<ft.n_mon &&
        xz+zoom_win[izoom].length_save*PIXELS_PER_SEC_MON>ft.w_mon)
      {
      xz-=ft.w_mon;
      put_reverse(&mon[i_map],xz,zoom_win[izoom].pos_save*pixels_per_trace,
        zoom_win[izoom].length_save*PIXELS_PER_SEC_MON,pixels_per_trace);
      }
    }
  if(leng) zoom_win[izoom].length=leng;
  i_map=(xz=zoom_win[izoom].sec*PIXELS_PER_SEC_MON)/ft.w_mon;
  xz-=i_map*ft.w_mon;
  put_reverse(&mon[i_map],xz,zoom_win[izoom].pos*pixels_per_trace,
    zoom_win[izoom].length*PIXELS_PER_SEC_MON,pixels_per_trace);
  if(++i_map<ft.n_mon &&
      xz+zoom_win[izoom].length*PIXELS_PER_SEC_MON>ft.w_mon)
    put_reverse(&mon[i_map],xz-ft.w_mon,
      zoom_win[izoom].pos*pixels_per_trace,
      zoom_win[izoom].length*PIXELS_PER_SEC_MON,pixels_per_trace);
  zoom_win[izoom].sec_save=zoom_win[izoom].sec;
  zoom_win[izoom].length_save=zoom_win[izoom].length;
  zoom_win[izoom].pos_save=zoom_win[izoom].pos;
  put_mon(x_zero,y_zero);
  /* clear zoom window */
  put_white(&dpy,WIDTH_INFO_ZOOM+1,height_dpy-1-(izoom+1)*HEIGHT_ZOOM+2,
    width_dpy-WIDTH_INFO_ZOOM-1,HEIGHT_ZOOM-2);
  /* print info */
  xzero=WIDTH_INFO_ZOOM;
  yzero=height_dpy-1-(izoom+1)*HEIGHT_ZOOM;
  sprintf(tbuf1," %04X %s-%s",zoom_win[izoom].sys_ch,
    ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].name,
    ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].comp);
  if(strlen(tbuf1)>WIDTH_INFO_C)
    sprintf(tbuf1,"%04X %s-%s",zoom_win[izoom].sys_ch,
      ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].name,
      ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].comp);
  sprintf(tbuf,"%-18s",tbuf1); /* write WIDTH_INFO_C here */
  put_text(&dpy,X_Z_CHN,yzero+Y_Z_CHN+Y_Z_OFS,tbuf,BF_SI);
  sprintf(tbuf,"%2d",zoom_win[izoom].scale);
  if(zoom_win[izoom].integ) put_text(&dpy,X_Z_SCL+X_Z_ARR+32+WIDTH_TEXT*1,
    yzero+Y_Z_SCL+Y_Z_OFS,tbuf,BF_SI);
  else put_text(&dpy,X_Z_SCL+X_Z_ARR+32+WIDTH_TEXT*1,yzero+Y_Z_SCL+Y_Z_OFS,
    tbuf,BF_S);
  /* plot trace */
  zoom_win[izoom].pixels=(width_dpy-WIDTH_INFO_ZOOM+
    (zoom_win[izoom].length>>1))/zoom_win[izoom].length;
  zoom_win[izoom].valid=1;
  join=0;
  start=1;
  if(pt) pt->valid=0;
  for(i=0;i<zoom_win[izoom].length;i++)
    {
    sr=read_one_sec((int32_w)zoom_win[izoom].sec+i,(WIN_ch)zoom_win[izoom].sys_ch,
      buf,NOT_KILL_SPIKE);
    if(i==0)
      {
      if(put)
        {
        j=zoom_win[izoom].sec+i;
        if(!strncmp(path,"/dev/",5)) sprintf(filename,"%s",path); /* device */
        else /* path contains directory name */
          {
          sprintf(filename,"%s/%02x%02x%02x.%02x%02x%02x.%d.%04X.%s",
            path,ft.ptr[j].time[0],ft.ptr[j].time[1],ft.ptr[j].time[2],
            ft.ptr[j].time[3],ft.ptr[j].time[4],ft.ptr[j].time[5],
            sr,zoom_win[izoom].sys_ch,fmt);
          if(zoom_win[izoom].filt)
            sprintf(filename+strlen(filename),".F%d",zoom_win[izoom].filt);
          if(ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].name[0]!='*')
            sprintf(filename+strlen(filename),".%s-%s",
            ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].name,
            ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].comp);
          if(fmt[0]=='A') strcat(filename,".au");
          }
        if((fp=fopen(filename,"w+"))==NULL) goto write_error;
        if(fmt[0]=='A') /* write an SUN audio file header */
          {
          fprintf(fp,".snd");
          fputc(0,fp);                       /* Data offset */
          fputc(0,fp);
          fputc(0,fp);
          fputc(24,fp);
          fputc(0xff,fp);                    /* Length of data (unknown) */
          fputc(0xff,fp);
          fputc(0xff,fp);
          fputc(0xff,fp);
          fputc(0,fp);                       /* Encoding */
          fputc(0,fp);
          fputc(0,fp);
          if(!strcmp(fmt,"A4")) fputc(5,fp); /* 32 bit linear PCM */
          if(!strcmp(fmt,"A3")) fputc(4,fp); /* 24 bit linear PCM */
          if(!strcmp(fmt,"A2")) fputc(3,fp); /* 16 bit linear PCM */
          if(!strcmp(fmt,"A1")) fputc(2,fp); /*  8 bit linear PCM */
          if(!strcmp(fmt,"A"))  fputc(1,fp); /* u-law */
          fputc(((sr*80) & 0xff000000) >> 24,fp);  /* Sample frequency (Hz) */
          fputc(((sr*80) & 0x00ff0000) >> 16,fp);
          fputc(((sr*80) & 0x0000ff00) >> 8,fp);
          fputc(((sr*80) & 0x000000ff),fp);
          fputc(0,fp);                             /* Channels */
          fputc(0,fp);
          fputc(0,fp);
          fputc(1,fp);
          }
        }
      xzero+=zoom_win[izoom].shift*zoom_win[izoom].pixels/1000;
      /* time */
      sprintf(tbuf,"%02x:%02x",ft.ptr[zoom_win[izoom].sec].time[4],
        ft.ptr[zoom_win[izoom].sec].time[5]);
      put_text(&dpy,xzero+2,yzero+MIN_ZOOM,tbuf,BF_S);
      }
    /* tick marks */
    pts[0].x=pts[1].x=pts[2].x=pts[3].x=xzero;
    pts[0].y=yzero+(BORDER+1);
    pts[1].y=yzero+(BORDER+1+HEIGHT_ZOOM/12);
    pts[2].y=yzero+(HEIGHT_ZOOM-2-HEIGHT_ZOOM/12);
    pts[3].y=yzero+(HEIGHT_ZOOM-2);
    draw_line(pts,4,LPTN_55,BF_SDO,&dpy,0,0,width_dpy,height_dpy,1);
    if(sr>0)
      {
      if(start)
        {
        /* get filter coefs */
        get_filter(zoom_win[izoom].filt,&zoom_win[izoom].f,sr,izoom);
        put_text(&dpy,X_Z_FLT+WIDTH_TEXT,yzero+Y_Z_FLT+Y_Z_OFS,
          zoom_win[izoom].f.tfilt,BF_S);
        /* get zero-level */
        buf0=0;
        dk=0.0;
        for(j=0;j<sr;j++) dk+=(double)buf[j];
        zoom_win[izoom].zero=(int32_w)(dk/(double)sr+0.5);
        /* set up filter memory */
        if(zoom_win[izoom].filt>0 && zoom_win[izoom].f.n_filt>0)
          {
          for(j=0;j<zoom_win[izoom].f.m_filt*4;j++) uv[j]=0.0;
          }
        else if(zoom_win[izoom].filt!=0)
          {
          for(j=0;j<zoom_win[izoom].f.m_filt;j++) rec[j]=0.0;
          sd=0.0;
          }
        zoom_win[izoom].sr=sr;
        /* scale */
        strcpy(tbuf1,ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].unit);
        if(pt)
          {
          if(*tbuf1=='m')
            {
            if(strcmp(tbuf1,"m")==0) pt->polarity=0;
            else if(strcmp(tbuf1,"m/s")==0) pt->polarity=(-1);
            else if(strcmp(tbuf1,"m/s/s")==0) pt->polarity=(-2);
            else pt->polarity=1;
            if(zoom_win[izoom].integ) pt->polarity++;
            }
          else pt->polarity=1;
          }
        if(*tbuf1!='*' && zoom_win[izoom].nounit==0)
          {
          if(zoom_win[izoom].integ)
            {
            if(strcmp(tbuf1,"m/s")==0) strcpy(tbuf1,"m");
            else if(strcmp(tbuf1,"m/s/s")==0) strcpy(tbuf1,"m/s");
            else strcpy(tbuf1,"?");
            sprintf(tbuf,"%.2e%s",
              ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].units_per_bit
              *(float)(HEIGHT_ZOOM<<zoom_win[izoom].scale)/(float)sr,tbuf1);
            }
          else sprintf(tbuf,"%.2e%s",
            ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].units_per_bit*
            (float)(HEIGHT_ZOOM<<zoom_win[izoom].scale),
            ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].unit);
          }
        else sprintf(tbuf,"%d",HEIGHT_ZOOM<<zoom_win[izoom].scale);
        if(zoom_win[izoom].nounit)
          put_text(&dpy,xzero+2,yzero+MAX_ZOOM-HEIGHT_TEXT,tbuf,BF_SI);
        else put_text(&dpy,xzero+2,yzero+MAX_ZOOM-HEIGHT_TEXT,tbuf,BF_S);
        zoom_win[izoom].w_scale=strlen(tbuf)*WIDTH_TEXT;
        start=0;
        }
      if(join) points[0]=points[np_last-1];
      np=join;
      /* remove offset */
      over_flow = 0;
      for (j =0; j < sr; j++)
	if (check_4byte_diff(buf[j], zoom_win[izoom].zero)) {
	  over_flow = 1;
	  break;
	}
      if (!over_flow) for(j=0;j<sr;j++) buf[j]-=zoom_win[izoom].zero;
      /* integration */
      if(zoom_win[izoom].integ) for(j=0;j<sr;j++)
        {
        buf[j]+=buf0;
        buf0=buf[j];
        }
      /* filtering */
      if(zoom_win[izoom].filt)
        {
        for(j=0;j<sr;j++) dbuf[j]=(double)buf[j];
        if(zoom_win[izoom].filt>0 && zoom_win[izoom].f.n_filt>0)
          { /* normal filter */
          tandem(dbuf,dbuf,sr,zoom_win[izoom].f.coef,
            zoom_win[izoom].f.m_filt,1,uv);
          for(j=0;j<sr;j++)
            buf[j]=(int32_w)(dbuf[j]*zoom_win[izoom].f.gn_filt+0.5);
          }
        else if(zoom_win[izoom].filt>0 && zoom_win[izoom].f.n_filt==0)
          {           /* coefs given */
          digfil(dbuf,dbuf2,sr,zoom_win[izoom].f.coef,
            zoom_win[izoom].f.m_filt,rec,&sd);
          for(j=0;j<sr;j++) buf[j]=(int32_w)(dbuf2[j]+0.5);
          }
        else            /* AR filter */
          {
          digfil(dbuf,dbuf2,sr,zoom_win[izoom].f.coef,
            zoom_win[izoom].f.m_filt,rec,&sd);
          for(j=0;j<sr;j++) buf[j]=(int32_w)(dbuf[j]-dbuf2[j]+0.5);
          }
        }
      /* plot */
      if(pt) for(j=0;j<sr;j++)
        { /* search maximum deflection */
        if(buf[j]>pt->valid)
          {
          pt->valid=buf[j];
          pt->sec1=zoom_win[izoom].sec+i;
          pt->msec1=(j*1000)/sr;
          }
        else if((-buf[j])>pt->valid)
          {
          pt->valid=(-buf[j]);
          pt->sec1=zoom_win[izoom].sec+i;
          pt->msec1=(j*1000)/sr;
          }
        }

      if(zoom_win[izoom].pixels*4<sr) for(j=0;j<sr;j++)
        {
        x=xzero+(zoom_win[izoom].pixels*j+(sr>>1))/sr;
	if (check_4byte_diff(CENTER_ZOOM, buf[j]>>zoom_win[izoom].scale))
	  y = WIN_AMP_MAX;
	else
	  y=CENTER_ZOOM-(buf[j]>>zoom_win[izoom].scale);
        if(y<MIN_ZOOM) y=MIN_ZOOM+yzero;
        else if(y>MAX_ZOOM) y=MAX_ZOOM+yzero;
        else y+=yzero;
        if(j==0)
          {
          xp=x;
          ymin=ymax=y;
          }
        else if(x!=xp || j==sr-1)
          {
          points[np].x=xp;
          points[np++].y=ymin;
          points[np].x=xp;
          points[np++].y=ymax;
          points[np].x=xp=x;
          points[np++].y=ymin=ymax=y;
          }
        else
          {
          if(y>ymax) ymax=y;
          else if(y<ymin) ymin=y;
          }
        }
      else for(j=0;j<sr;j++)
        {
        points[np].x=xzero+(zoom_win[izoom].pixels*j+(sr>>1))/sr;
	if (check_4byte_diff(CENTER_ZOOM, buf[j]>>zoom_win[izoom].scale))
	  y = WIN_AMP_MAX;
	else
	  y=CENTER_ZOOM-(buf[j]>>zoom_win[izoom].scale);
        if(y<MIN_ZOOM)    points[np++].y=MIN_ZOOM+yzero;
        else if(y>MAX_ZOOM) points[np++].y=MAX_ZOOM+yzero;
        else points[np++].y=y+yzero;
        }
      if(np>1) draw_line(points,np,LPTN_FF,BF_SDO,&dpy,0,0,width_dpy,
            height_dpy,0);
      join=1;
      np_last=np;
      if(put)
        {
        if(!strcmp(fmt,"B4"))    /* 4-byte binary */
          for(j=0;j<sr;j++)
            {
            ll=buf[j];
            if(!fwrite(&ll,4,1,fp)) goto write_error;
            }
        else if(!strcmp(fmt,"A4"))    /* 4-byte big-endian (32 bit PCM audio) */
          for(j=0;j<sr;j++)
            {
            ll=buf[j];
            if(fputc(ll>>24,fp)==EOF) goto write_error;
            if(fputc(ll>>16,fp)==EOF) goto write_error;
            if(fputc(ll>>8,fp)==EOF) goto write_error;
            if(fputc(ll,fp)==EOF) goto write_error;
            }
        else if(!strcmp(fmt,"A3")) /* 3-byte big-endian (24 bit PCM audio) */
          for(j=0;j<sr;j++)
            {
            if(buf[j]>(2<<23)-1) ll=(2<<23)-1;
            else if(buf[j]<(-(2<<23))) ll=(-(2<<23));
            else ll=buf[j];
            if(fputc(ll>>16,fp)==EOF) goto write_error;
            if(fputc(ll>>8,fp)==EOF) goto write_error;
            if(fputc(ll,fp)==EOF) goto write_error;
            }
        else if(!strcmp(fmt,"B2")) /* 2-byte binary */
          for(j=0;j<sr;j++)
            {
            if(buf[j]>32767) ss=32767;
            else if(buf[j]<(-32768)) ss=(-32768);
            else ss=buf[j];
            if(!fwrite(&ss,2,1,fp)) goto write_error;
            }
        else if(!strcmp(fmt,"A2")) /* 2-byte big-endian (16 bit PCM audio) */
          for(j=0;j<sr;j++)
            {
            if(buf[j]>32767) ll=32767;
            else if(buf[j]<(-32768)) ll=(-32768);
            else ll=buf[j];
            if(fputc(ll>>8,fp)==EOF) goto write_error;
            if(fputc(ll,fp)==EOF) goto write_error;
            }
        else if(!strcmp(fmt,"C"))  /* numerical characters */
          for(j=0;j<sr;j++)
            {
            if(fprintf(fp,"%d\n",buf[j])==EOF) goto write_error;
            }
        else if(!strcmp(fmt,"A"))    /* u-law */
          {
          if(put==MSE_BUTNL+1) k=8;
          else if(put==MSE_BUTNM+1) k=4;
          else k=0;
          for(j=0;j<sr;j++)
            {
            cc=(int8_w)ulaw(buf[j]<<k);
            if(!fwrite(&cc,1,1,fp)) goto write_error;
            }
          }
        else if(!strcmp(fmt,"B1") || !strcmp(fmt,"A1"))
                                      /* 1 byte binary scaled */
          {
          for(j=0;j<sr;j++)
            {
            ll=buf[j]>>zoom_win[izoom].scale;
            if(ll>127) cc=127;
            else if(ll<(-128)) cc=(-128);
            else cc=ll;
            if(!fwrite(&cc,1,1,fp)) goto write_error;
            }
          }
        else
          {
          fprintf(stderr,"unknown data format '%s'\007\n",fmt);
          break;
          }
        }
      }
    else join=0;
    xzero+=zoom_win[izoom].pixels;
    }
  if(put) fclose(fp);
  if(zoom_win[izoom].integ || zoom_win[izoom].filt)
    zoom_win[izoom].zero=0;
  if(pt)
    {
    pt->sec2=pt->sec1;
    pt->msec2=pt->msec1;
    *(float *)&(pt->valid)=(float)pt->valid*
        ft.stn[ft.ch2idx[zoom_win[izoom].sys_ch]].units_per_bit;
    if(zoom_win[izoom].integ)
      *(float *)&(pt->valid)/=(float)zoom_win[izoom].sr;
    }

  /* put marks */
  for(j=0;j<4;j++) if(ft.pick[ft.ch2idx[zoom_win[izoom].sys_ch]][j].valid)
    put_mark_zoom(j,izoom,&ft.pick[ft.ch2idx[zoom_win[izoom].sys_ch]][j],0);
  for(j=0;j<2;j++) if(ft.pick_calc[ft.ch2idx[zoom_win[izoom].sys_ch]][j].valid)
    put_mark_zoom(j,izoom,&ft.pick_calc[ft.ch2idx[zoom_win[izoom].sys_ch]][j],1);
  if(put && strcmp(fmt,"A")) sleep(1);
  return;

write_error:
  fprintf(stderr,"file write error in '%s'\007\n",filename);
  fclose(fp);
  return;
  }

static void
close_zoom(int izoom)
  {
  int xz,k,i_map;

  if(zoom_win[izoom].valid)
    {
    i_map=(xz=zoom_win[izoom].sec_save*PIXELS_PER_SEC_MON)/ft.w_mon;
    xz-=i_map*ft.w_mon;
    put_reverse(&mon[i_map],xz,zoom_win[izoom].pos_save*pixels_per_trace,
      zoom_win[izoom].length_save*PIXELS_PER_SEC_MON,pixels_per_trace);
    if(++i_map<ft.n_mon &&
        xz+zoom_win[izoom].length_save*PIXELS_PER_SEC_MON>ft.w_mon)
      {
      xz-=ft.w_mon;
      put_reverse(&mon[i_map],xz,zoom_win[izoom].pos_save*pixels_per_trace,
        zoom_win[izoom].length_save*PIXELS_PER_SEC_MON,pixels_per_trace);
      }
    }
  if(izoom<n_zoom-1) for(k=izoom;k<n_zoom-1;k++)
    {
    zoom_win[k]=zoom_win[k+1];
    put_bitblt(&dpy,0,height_dpy-1-(k+2)*HEIGHT_ZOOM,width_zoom,height_zoom,
      &dpy,0,height_dpy-1-(k+1)*HEIGHT_ZOOM,BF_S);
    }   /* move k+1 to k */
  n_zoom--;
  height_win_mon+=HEIGHT_ZOOM;
  if((y_zero_max=height_mon-height_win_mon)<0) y_zero_max=0;
  }

static void
proc_main(void)
  {
  static struct Pick_Time pt;
  int xx,yy,x,y,i,j,k,kk,ring_bell,plot_flag,xshift,yshift;
  int find_wchid,ret;
  static int pos_zoom;
  char textbuf[LINELEN],textbuf1[LINELEN],tbuf[20],unit[10];
  char cmdbuf[LINELEN];

  j=kk=0;  /* for supress warnings */
  xx=x_zero;
  yy=y_zero;
  x=event.mse_data.md_x;
  y=event.mse_data.md_y;
  ring_bell=plot_flag=0;
  if(event.mse_trig==MSE_MOTION)
    {
    strcpy(textbuf,diagnos);
    strcpy(textbuf1,monbuf);
    if(y>=YBASE_MON+height_win_mon)
      {
      i=(height_dpy-1-y)/HEIGHT_ZOOM;   /* zoom no. */
      if(x>=WIDTH_INFO_ZOOM && zoom_win[i].valid)  /* zoom trace */
        {
        kk=((height_dpy-1-(i+1)*HEIGHT_ZOOM+CENTER_ZOOM-y)<<
          zoom_win[i].scale)+zoom_win[i].zero;
        if(*ft.stn[ft.ch2idx[zoom_win[i].sys_ch]].unit!='*' &&
             zoom_win[i].nounit==0)
          {
          if(zoom_win[i].integ)
            {
            strcpy(unit,ft.stn[ft.ch2idx[zoom_win[i].sys_ch]].unit);
            if(strcmp(unit,"m/s")==0) strcpy(unit,"m");
            else if(strcmp(unit,"m/s/s")==0) strcpy(unit,"m/s");
            else strcpy(unit,"?");
            sprintf(tbuf,"%+.2e%-5s",
              ft.stn[ft.ch2idx[zoom_win[i].sys_ch]].units_per_bit
              *(float)kk/(float)zoom_win[i].sr,unit);
            }
          else sprintf(tbuf,"%+.2e%-5s",
            ft.stn[ft.ch2idx[zoom_win[i].sys_ch]].units_per_bit*(float)kk,
            ft.stn[ft.ch2idx[zoom_win[i].sys_ch]].unit);
          }
        else sprintf(tbuf,"%+9d     ",kk);
        xshift=zoom_win[i].shift*zoom_win[i].pixels/1000;
        j=(((x-WIDTH_INFO_ZOOM-xshift)%zoom_win[i].pixels)*1000+
          (zoom_win[i].pixels>>1))/zoom_win[i].pixels;
        k=zoom_win[i].sec+(x-WIDTH_INFO_ZOOM-xshift)/zoom_win[i].pixels;
        if(k<ft.len) sprintf(textbuf,"%04X %02x:%02x.%03d %s",
                zoom_win[i].sys_ch,ft.ptr[k].time[4],ft.ptr[k].time[5],j,tbuf);
        }
      }
    else if(y<MARGIN) /* function(1) area - LIST */
      {
      ; /* nothing associated to MOTION for LIST */
      }
    else if(y<YBASE_MON) /* function(2) area */
      {
      if(x>x_time_file && x<x_time_file+WIDTH_TEXT*17)
        if(!autpk_but_off||(autpk_but_off&&(event.mse_key==MSE_SHIFT))){
          strcpy(textbuf1,"    AUTO-PICK    ");
        }
      }
    else if(y<YBASE_MON+height_mon)
      {
      k=(y_zero+y-y_win_mon)/pixels_per_trace;
      if(x>=WIDTH_INFO && k<ft.n_ch)     /* mon traces */
        {
        kk=(((pixels_per_trace*k+ppt_half)-(y+y_zero-y_win_mon))
          <<ft.stn[ft.pos2idx[k]].scale)+ft.stn[ft.pos2idx[k]].offset;
        if(*ft.stn[ft.pos2idx[k]].unit!='*')
          sprintf(tbuf,"%+.2e%-5s",
            ft.stn[ft.pos2idx[k]].units_per_bit*(float)kk,
            ft.stn[ft.pos2idx[k]].unit);
        else sprintf(tbuf,"%+9d     ",kk);
        j=(((x_zero+x-x_win_mon)%PIXELS_PER_SEC_MON)*10+
          (PIXELS_PER_SEC_MON>>1))/PIXELS_PER_SEC_MON;
        i=(x_zero+x-x_win_mon)/PIXELS_PER_SEC_MON;
        if(i<ft.len) sprintf(textbuf,"%04X %02x:%02x.%d   %s",
          ft.idx2ch[ft.pos2idx[k]],ft.ptr[i].time[4],ft.ptr[i].time[5],j,tbuf);
        }
      }
    put_text(&dpy,x_time_now,Y_TIME,textbuf,BF_SI);
    put_text(&dpy,x_time_file,Y_TIME,textbuf1,BF_SI);
    }
  else if(event.mse_trig==MSE_BUTTON && event.mse_dir==MSE_DOWN
         && event.mse_code>=0)
    {
    ring_bell=1;
    if(y>=YBASE_MON+height_win_mon)  /* zoom area */
      {
      i=(height_dpy-1-y)/HEIGHT_ZOOM;   /* zoom no. */
      if(x<WIDTH_INFO_ZOOM) /* zoom info */
        {
        j=(i+1)*HEIGHT_ZOOM-(height_dpy-1-y); /* relative y */
        if((height_dpy-1-y)%HEIGHT_ZOOM && j%PIXELS_PER_LINE)
            switch(j/PIXELS_PER_LINE) /* line no */
          {
          case 0:
            if(zoom_win[i].valid==0) break;
            if(x<WIDTH_TEXT*5)  /* auto pick */
              {
              switch(event.mse_code)
                {
                case MSE_BUTNL: kk=P;break;
                case MSE_BUTNM: kk=S;break;
                case MSE_BUTNR: kk=X;break;
                }
              if(pick_phase(ft.pos2idx[zoom_win[i].pos],kk))
                {
                plot_flag=1;
                ring_bell=0;
                }
              }
            else   /* CH */
              {
              switch(event.mse_code)
                {
                case MSE_BUTNL:
                  if(zoom_win[i].pos>0)
                    {
                    zoom_win[i].pos--;
                    zoom_win[i].sys_ch=ft.idx2ch[ft.pos2idx[zoom_win[i].pos]];
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                case MSE_BUTNM: break;
                case MSE_BUTNR:
                  if(zoom_win[i].pos<ft.n_ch-1)
                    {
                    zoom_win[i].pos++;
                    zoom_win[i].sys_ch=ft.idx2ch[ft.pos2idx[zoom_win[i].pos]];
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                }
              }
            break;
          case 1:
            if(zoom_win[i].valid==0) break;
            if(x<W_Z_POL)    /* POLARITY */
              {
              if(ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].valid==0) break;
              put_mark(P,zoom_win[i].pos,0);
              switch(event.mse_code)
                {
                case MSE_BUTNL:
                  ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].polarity=1;
                  break;
                case MSE_BUTNM:
                  ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].polarity=0;
                  break;
                case MSE_BUTNR:
                  ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].polarity=(-1);
                  break;
                }
              put_mark(P,zoom_win[i].pos,0);
              ring_bell=0;
              }
            else        /* SCALE */
              {
              switch(event.mse_code)
                {
                case MSE_BUTNL:
                  if(zoom_win[i].scale>0)
                    {
                    zoom_win[i].scale--;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                case MSE_BUTNM:
                  zoom_win[i].integ=(++zoom_win[i].integ)&1;
                  plot_zoom(i,0,0,0);
                  ring_bell=0;
                  break;
                case MSE_BUTNR:
                  if(zoom_win[i].scale<SCALE_MAX)
                    {
                    zoom_win[i].scale++;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                }
              }
            break;
          case 2:
            if(zoom_win[i].valid==0) break;
            if(x<W_Z_TSC)    /* LENG */
              {
              switch(event.mse_code)
                {
                case MSE_BUTNL:
                  if(zoom_win[i].length>ZOOM_LENGTH_MIN)
                    {
                    zoom_win[i].length/=2;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                case MSE_BUTNM:
                  /* measure maximum amplitude */
                  if(measure_max_zoom(i))
                    {
                    plot_flag=1;
                    ring_bell=0;
                    }
                  break;
                case MSE_BUTNR:
                  if(zoom_win[i].length<ZOOM_LENGTH_MAX &&
                      zoom_win[i].sec+zoom_win[i].length<ft.len)
                    {
                    zoom_win[i].length*=2;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                }
              }
            else        /* L/R */
              {
              switch(event.mse_code)
                {
                case MSE_BUTNL:
                  if(zoom_win[i].sec>0)
                    {
                    if((kk=zoom_win[i].length/SHIFT)==0) kk=1;
                    zoom_win[i].sec-=kk;
                    if(zoom_win[i].sec<0) zoom_win[i].sec=0;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                case MSE_BUTNM:
                  if(ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].valid==0 &&
                      zoom_win[i].shift)
                    {
                    zoom_win[i].shift=0;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  else if(ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].valid &&
                      zoom_win[i].sec<=ft.pick[ft.ch2idx[zoom_win[i].
                        sys_ch]][P].sec1 && zoom_win[i].sec+zoom_win[i].length>
                        ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].sec1)
                    {
                    zoom_win[i].shift=1000-
                      ft.pick[ft.ch2idx[zoom_win[i].sys_ch]][P].msec1%1000;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                case MSE_BUTNR:
                  if(zoom_win[i].sec+zoom_win[i].length<ft.len)
                    {
                    if((kk=zoom_win[i].length/SHIFT)==0) kk=1;
                    zoom_win[i].sec+=kk;
                    if(zoom_win[i].sec+zoom_win[i].length>ft.len)
                      zoom_win[i].sec=ft.len-zoom_win[i].length;
                    plot_zoom(i,0,0,0);
                    ring_bell=0;
                    }
                  break;
                }
              }
            break;
          case 3:        /* FILTER */
            if(zoom_win[i].valid==0) break;
            switch(event.mse_code)
              {
              case MSE_BUTNL:
                if(++zoom_win[i].filt==ft.n_filt) zoom_win[i].filt=0;
                break;
              case MSE_BUTNM:
                if(zoom_win[i].filt>0) zoom_win[i].filt=0;
                else if(zoom_win[i].filt==0) zoom_win[i].filt=(-1);
                else zoom_win[i].filt=0;
                break;
              case MSE_BUTNR:
                if(zoom_win[i].filt<0) zoom_win[i].filt=0;
                else if(--zoom_win[i].filt==(-1)) zoom_win[i].filt=ft.n_filt-1;
                break;
              }
            plot_zoom(i,0,0,0);
            ring_bell=0;
            break;
          case 4:
          case 5:
            if(x<W_Z_GET)    /* GET */
              {
              ring_bell=0;
              if(main_mode==MODE_GET)
                {
                if(i==pos_zoom) /* cancel get mode */
                  {
                  put_reverse(&dpy,X_Z_GET+1,
                    Y_Z_GET+1+height_dpy-1-(i+1)*HEIGHT_ZOOM,
                    W_Z_GET-1,PIXELS_PER_LINE-1);
                  main_mode=MODE_NORMAL;
                  break;
                  }
                else      /* change window */
                  put_reverse(&dpy,X_Z_GET+1,
                    Y_Z_GET+1+height_dpy-1-(pos_zoom+1)*HEIGHT_ZOOM,
                    W_Z_GET-1,PIXELS_PER_LINE-1);
                }
              else main_mode=MODE_GET;
              pos_zoom=i;
              put_reverse(&dpy,X_Z_GET+1,
                Y_Z_GET+1+height_dpy-1-(i+1)*HEIGHT_ZOOM,
                W_Z_GET-1,PIXELS_PER_LINE-1);
              }
            else if(x>X_Z_CLS)      /* CLOSE */
              {
              if(main_mode==MODE_GET && i==pos_zoom)
                {
                put_reverse(&dpy,X_Z_GET+1,
                  Y_Z_GET+1+height_dpy-1-(i+1)*HEIGHT_ZOOM,
                  W_Z_GET-1,PIXELS_PER_LINE-1);
                main_mode=MODE_NORMAL;
                }
              close_zoom(i);
              if(y_zero>y_zero_max) yy=y_zero_max;
              plot_flag=1;
              ring_bell=0;
              }
            else      /* PUT */
              {
              if(main_mode==MODE_GET && i==pos_zoom)
                {
                put_reverse(&dpy,X_Z_GET+1,
                  Y_Z_GET+1+height_dpy-1-(i+1)*HEIGHT_ZOOM,
                  W_Z_GET-1,PIXELS_PER_LINE-1);
                main_mode=MODE_NORMAL;
                }
              if(zoom_win[i].valid)
                {
                put_reverse(&dpy,X_Z_PUT+1,
                  Y_Z_PUT+1+height_dpy-1-(i+1)*HEIGHT_ZOOM,
                  W_Z_PUT-1,PIXELS_PER_LINE-1);
                plot_zoom(i,0,0,event.mse_code+1);
                put_reverse(&dpy,X_Z_PUT+1,
                  Y_Z_PUT+1+height_dpy-1-(i+1)*HEIGHT_ZOOM,
                  W_Z_PUT-1,PIXELS_PER_LINE-1);
                ring_bell=0;
                }
              }
            break;
          }
        }
      else if((height_dpy-1-y)%HEIGHT_ZOOM &&
             (height_dpy-1-y)%HEIGHT_ZOOM!=HEIGHT_ZOOM-1) /* zoom trace */
        {
        if(main_mode==MODE_GET && zoom_win[i].valid)
          {
          zoom_win[pos_zoom].scale=zoom_win[i].scale;
          zoom_win[pos_zoom].w_scale=zoom_win[i].w_scale;
          zoom_win[pos_zoom].nounit=zoom_win[i].nounit;
          zoom_win[pos_zoom].offset=zoom_win[i].offset;
          zoom_win[pos_zoom].integ=zoom_win[i].integ;
          zoom_win[pos_zoom].zero=zoom_win[i].zero;
          zoom_win[pos_zoom].sys_ch=zoom_win[i].sys_ch;
          zoom_win[pos_zoom].sec=zoom_win[i].sec;
          zoom_win[pos_zoom].shift=zoom_win[i].shift;
          zoom_win[pos_zoom].pos=zoom_win[i].pos;
          zoom_win[pos_zoom].filt=zoom_win[i].filt;
          plot_zoom(pos_zoom,zoom_win[i].length,0,0);
          main_mode=MODE_NORMAL;
          put_reverse(&dpy,X_Z_GET+1,
            Y_Z_GET+1+height_dpy-1-(pos_zoom+1)*HEIGHT_ZOOM,
            W_Z_GET-1,PIXELS_PER_LINE-1);
          ring_bell=0;
          }
        else if(zoom_win[i].valid)
          {
          if(x<WIDTH_INFO_ZOOM+zoom_win[i].w_scale &&
              ((i+1)*HEIGHT_ZOOM-(height_dpy-1-y))/PIXELS_PER_LINE==4)
            { /* use raw amplitude */
            if(ft.stn[ft.ch2idx[zoom_win[i].sys_ch]].unit[0]!='*')
              {
              zoom_win[i].nounit=(++zoom_win[i].nounit)&1;
              plot_zoom(i,0,0,0);
              ring_bell=0;
              }
            }
          else
            {
            xshift=zoom_win[i].shift*zoom_win[i].pixels/1000;
            pt.msec1=(((x-WIDTH_INFO_ZOOM-xshift)%zoom_win[i].pixels)*1000+
              (zoom_win[i].pixels>>1))/zoom_win[i].pixels;
            pt.sec1=zoom_win[i].sec+
              (x-WIDTH_INFO_ZOOM-xshift)/zoom_win[i].pixels;
            pt.valid=i;  /* zoom window no. */
            main_mode=MODE_PICK;
            ring_bell=0;
            }
          }
        }
      }
    else if(y<MARGIN) /* command area (1) - LIST */
      {
      if(com_dep1<=x && x<=com_dep2)
        {
        ring_bell=0;
        switch(event.mse_code)
          {
          case MSE_BUTNL:
            if(init_dep<10) init_dep+=1;
            else if(init_dep< 50) init_dep=(init_dep/5+1)*5;
            else if(init_dep<100) init_dep=(init_dep/10+1)*10;
            else if(init_dep<700) init_dep=(init_dep/50+1)*50;
            else ring_bell=1;
            break;
          case MSE_BUTNM:
            init_dep =init_dep_init;
            init_depe=init_depe_init;
            break;
          case MSE_BUTNR:
            if(init_dep==(-3)) ring_bell=1;
            else if(init_dep<= 10) init_dep-=1;
            else if(init_dep<= 50) init_dep=(init_dep/5-1)*5;
            else if(init_dep<=100) init_dep=(init_dep/10-1)*10;
            else init_dep=(init_dep/50-1)*50;
            break;
          }
/*        if(init_depe>init_dep) init_depe=init_dep;*/
        if(ring_bell==0) put_init_depth();
        }
      else if(com_depe1<=x && x<=com_depe2)
        {
        ring_bell=0;
        switch(event.mse_code)
          {
          case MSE_BUTNL:
            if(init_depe<10) init_depe+=1;
            else if(init_depe< 50) init_depe=(init_depe/5+1)*5;
            else if(init_depe<100) init_depe=(init_depe/10+1)*10;
            else if(init_depe<700) init_depe=(init_depe/50+1)*50;
            else ring_bell=1;
            break;
          case MSE_BUTNM:
            init_depe=init_dep;
            break;
          case MSE_BUTNR:
            if(init_depe==0) ring_bell=1;
            else if(init_depe<= 10) init_depe-=1;
            else if(init_depe<= 50) init_depe=(init_depe/5-1)*5;
            else if(init_depe<=100) init_depe=(init_depe/10-1)*10;
            else init_depe=(init_depe/50-1)*50;
            break;
          }
/*
        if(init_depe>init_dep)
          {
          init_depe=init_dep;
          ring_bell=1;
          }
*/
        if(ring_bell==0) put_init_depth();
        }
      else switch(get_func(x))
        {
        case QUIT:
          if(event.mse_code==MSE_BUTNR) end_process(0);
          k=0;
          if(diagnos[1]!=' ') k=1;
          else
            {
            for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++)
              if(ft.pick[ft.pos2idx[i]][j].valid) k=1;
            }
          if(k==0 || flag_change==0) end_process(0);
              /* no picks or no changes */
          break;
        case RFSH:
          raise_ttysw(0);
#if HINET_EXTENTION_3
	  if (event.mse_code==MSE_BUTNR&&ft.hypo.valid==1) {
	    put_reverse(&dpy,x_func(RFSH),0,WB,MARGIN);
	    replot_mon(1);
	    put_reverse(&dpy,x_func(RFSH),0,WB,MARGIN);
	  }
	  if (event.mse_code==MSE_BUTNM&&ft.hypo.valid==1){
	    put_reverse(&dpy,x_func(RFSH),0,WB,MARGIN);
	    replot_mon(-1);
	    put_reverse(&dpy,x_func(RFSH),0,WB,MARGIN);
	  }
#endif
          refresh(1);
          ring_bell=0;
          break;
        case MAP:
          raise_ttysw(0);
          loop_stack[loop_stack_ptr++]=loop;
          loop=LOOP_MAP;
          init_map(event.mse_code);
          return;
        case MECH:
          raise_ttysw(0);
          loop_stack[loop_stack_ptr++]=loop;
          loop=LOOP_MECHA;
          init_mecha();
          return;
        case PSTUP:
          raise_ttysw(0);
          loop_stack[loop_stack_ptr++]=loop;
          loop=LOOP_PSUP;
          init_psup();
          return;
        case COPY:
          put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
          switch(event.mse_code)
            {
            case MSE_BUTNL: hard_copy(3);break;
            case MSE_BUTNM: hard_copy(2);break;
            case MSE_BUTNR: hard_copy(1);break;
            }
          put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
          ring_bell=0;
          break;
        case LIST:
          raise_ttysw(1);
          put_reverse(&dpy,x_func(LIST),0,WB,MARGIN);
          list_picks(1);
          put_reverse(&dpy,x_func(LIST),0,WB,MARGIN);
          ring_bell=0;
          break;
        case FINL:
          raise_ttysw(1);
          put_reverse(&dpy,x_func(FINL),0,WB,MARGIN);
          list_finl(1);
          put_reverse(&dpy,x_func(FINL),0,WB,MARGIN);
          ring_bell=0;
          break;
        case LOAD:
          raise_ttysw(1);
          put_reverse(&dpy,x_func(LOAD),0,WB,MARGIN);
          if(load_data(event.mse_code))
            {
            list_picks(0);
            plot_flag=1;
            ring_bell=0;
            }
#if HINET_EXTENTION_3>=2
          replot_mon(1);
#endif
          put_reverse(&dpy,x_func(LOAD),0,WB,MARGIN);
          break;
        case UNLD:
          raise_ttysw(1);
          put_reverse(&dpy,x_func(UNLD),0,WB,MARGIN);
          if(cancel_picks(NULL,-1)) plot_flag=1;
          flag_hypo=0;
          cancel_picks_calc();
          set_diagnos("",getname(geteuid()));
          flag_change=0;
          list_picks(0);
          if(*ft.save_file)
            {
            *ft.save_file=0;
            fprintf(stderr,"using no pick file\n");
            }
          put_reverse(&dpy,x_func(UNLD),0,WB,MARGIN);
          ring_bell=0;
          break;
        case CLER:
          raise_ttysw(1);
          put_reverse(&dpy,x_func(CLER),0,WB,MARGIN);
          if(cancel_picks(NULL,-1)) plot_flag=1;
          flag_hypo=0;
          cancel_picks_calc();
          set_diagnos("",getname(geteuid()));
          flag_change=0;
          list_picks(0);
          if(*ft.save_file)
            fprintf(stderr,"using pick file '%s'\n",ft.save_file);
          put_reverse(&dpy,x_func(CLER),0,WB,MARGIN);
          ring_bell=0;
          break;
        }
      }
    else if(y<YBASE_MON) /* command area (2) */
      {
      if(x_time_file<=x && x<=x_time_file+WIDTH_TEXT*17)
        {
          if(!autpk_but_off||(autpk_but_off&&(event.mse_key==MSE_SHIFT))){
            strcpy(apbuf," DOING AUTO-PICK ");
            put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
            if(auto_pick(0)) ring_bell=0;
          }
        }
      if(com_diag1<=x && x<=com_diag2)
        {
        switch(event.mse_code)
          {
          case MSE_BUTNL:
            if(++ft.label_idx==ft.n_label) ft.label_idx=0;
            break;
          case MSE_BUTNM: ft.label_idx=0;break;
          case MSE_BUTNR:
            if(--ft.label_idx==(-1)) ft.label_idx=ft.n_label-1;
            break;
          }
        set_diagnos(ft.label[ft.label_idx],getname(geteuid()));
        put_text(&dpy,x_time_now,Y_TIME,diagnos,BF_SI);
        flag_change=1;
        ring_bell=0;
        }
      else switch(get_func(x))
        {
        case OPEN:
          if(n_zoom<n_zoom_max)
            {
            n_zoom++;
            height_win_mon-=HEIGHT_ZOOM;
            if((y_zero_max=height_mon-height_win_mon)<0) y_zero_max=0;
          /* open a new zoom window */
            put_white(&dpy,0,height_dpy-1-n_zoom*HEIGHT_ZOOM,width_zoom,
              height_zoom);
            put_fram(&dpy,0,height_dpy-1-n_zoom*HEIGHT_ZOOM,width_zoom,
              height_zoom);
            put_fram(&dpy,0,height_dpy-1-n_zoom*HEIGHT_ZOOM+1,width_zoom,
              height_zoom-1);
            put_bitblt(&zoom,0,0,WIDTH_INFO_ZOOM+1,height_zoom,&dpy,0,
              height_dpy-1-n_zoom*HEIGHT_ZOOM,BF_S);
            zoom_win[n_zoom-1].length=ZOOM_LENGTH;
            zoom_win[n_zoom-1].offset=1;  /* always 1 */
            zoom_win[n_zoom-1].integ=0;
            zoom_win[n_zoom-1].filt=0;
            zoom_win[n_zoom-1].valid=0;
            zoom_win[n_zoom-1].shift=0;
            zoom_win[n_zoom-1].nounit=0;
            zoom_win[n_zoom-1].w_scale=0;
            if(n_zoom==1) draw_seg(0,height_dpy-1,width_dpy,
              height_dpy-1,LPTN_FF,BF_SDO,&dpy);
            ring_bell=0;
            }
          break;
        case UD:
          if(event.mse_key==MSE_CTRL) yshift=pixels_per_trace*50;
          else if(event.mse_key==MSE_SHIFT) yshift=pixels_per_trace*500;
          else yshift=height_win_mon/SHIFT;
          switch(event.mse_code)
            {
#if HINET_EXTENTION_1
            case MSE_BUTNL:
              if((yy-=yshift)<0) {
		/* yy=0; */
		bell();
		yy = y_zero_max;
	      }
              break;
            case MSE_BUTNM: 
	      bell();
	      yy = 0;
	      break;
            case MSE_BUTNR:
              if((yy+=yshift)>y_zero_max) {
		/* yy=y_zero_max; */
		bell();
		yy = 0 ;
	      }
              break;
#else
            case MSE_BUTNL:
              if((yy-=yshift)<0) yy=0;
              break;
            case MSE_BUTNM: break;
            case MSE_BUTNR:
              if((yy+=yshift)>y_zero_max) yy=y_zero_max;
              break;
#endif
            }
          break;
        case LR:
          if(event.mse_key==MSE_CTRL) xshift=PIXELS_PER_SEC_MON*60;
          else if(event.mse_key==MSE_SHIFT) xshift=PIXELS_PER_SEC_MON*600;
          else xshift=width_win_mon/SHIFT;
          switch(event.mse_code)
            {
            case MSE_BUTNL:
              if((xx-=xshift)<0) xx=0;
              break;
            case MSE_BUTNM: 
#if HINET_EXTENTION_2
		fflush(stdin);
		raise_ttysw(1);
		while (1) {
		  find_wchid=-1; ret=0;
		  fprintf(stderr,"Please Input Station Chnannel ID ...>");
		  fgets(cmdbuf,LINELEN,stdin);
		  cmdbuf[strlen(cmdbuf)-1]='\0';
		  if(cmdbuf[0]=='#') ret=sscanf(cmdbuf+1,"%x",&find_wchid);
		  if(ret!=1) {
		    /*fprintf(stderr,":%s:",cmdbuf);*/
		    for(i=0;i<ft.n_ch;i++){
		      /*fprintf(stderr,"%s:%s:",ft.stn[i].name,cmdbuf);*/
		      if(strcmp(ft.stn[i].name,cmdbuf)==0) {
			find_wchid=ft.idx2ch[i];
			break;
		      }
		    }
		    if(find_wchid>0) break ;
		    else fprintf(stderr,"Cannot Find  %s....\n",cmdbuf);
		    continue ;
		  }
		  if(find_wchid<0) {
		    fprintf(stderr,"Channel ID  Select Mode End.\n");
		    break ;
		  }
		  if(find_wchid<=0||find_wchid>=0xffff) {
                    fprintf(stderr,"Channel ID  Rang is 0x0001 --> 0xffff\n");
		    continue ;  
		  } else {
		    break;
		  }
		}
		if(find_wchid>0){
		  if(ft.ch2idx[find_wchid]<0) {
		    fprintf(stderr,"Cannot Find Channel ID: %04x....\n",find_wchid);
		    yy=0;
		  } else {
		    yy=ft.idx2pos[ft.ch2idx[find_wchid]]*pixels_per_trace;
		    if(yy<0) yy=0;
		    if(yy>y_zero_max) yy=y_zero_max;
		  }
		} else {
		  /*yy=0;*/
		}
		raise_ttysw(0);
		refresh(0);
#endif
		break ;
            case MSE_BUTNR:
              if((xx+=xshift)>x_zero_max) xx=x_zero_max;
              break;
            }
          break;
        case SAVE:
          raise_ttysw(1);
          put_reverse(&dpy,x_func(SAVE),HEIGHT_FUNC,WB,MARGIN);
          switch(event.mse_code)
            {
            case MSE_BUTNL:
            case MSE_BUTNM: if(save_data(0)) ring_bell=0;break;
            case MSE_BUTNR: if(save_data(1)) ring_bell=0;break;
            }
          put_reverse(&dpy,x_func(SAVE),HEIGHT_FUNC,WB,MARGIN);
          break;
        case HYPO:
          raise_ttysw(1);
          put_reverse(&dpy,x_func(HYPO),HEIGHT_FUNC,WB,MARGIN);
          locate(1,0);
          get_calc();
          put_reverse(&dpy,x_func(HYPO),HEIGHT_FUNC,WB,MARGIN);
          ring_bell=0;
          break;
        case EVDET:
          if(!autpk_but_off||(autpk_but_off&&(event.mse_key==MSE_SHIFT))){
            strcpy(apbuf,"EVDET & AUTO-PICK");
            put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
            put_reverse(&dpy,x_func(EVDET),HEIGHT_FUNC,WB,MARGIN);
            if(auto_pick(1)) ring_bell=0;
          }
          break;
        case AUTPK:
          if(!autpk_but_off||(autpk_but_off&&(event.mse_key==MSE_SHIFT))){
            strcpy(apbuf," AUTOPICK W/HINT ");
            put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
            put_reverse(&dpy,x_func(AUTPK),HEIGHT_FUNC,WB,MARGIN);
            if(auto_pick_hint(0)) ring_bell=0;
          }
          break;
        }
      }
    else if(y<YBASE_MON+height_mon)  /* mon area */
      {
      i=(x_zero+x-x_win_mon)/PIXELS_PER_SEC_MON;
      j=(((x_zero+x-x_win_mon)%PIXELS_PER_SEC_MON)*10+
          (PIXELS_PER_SEC_MON>>1))/PIXELS_PER_SEC_MON;
      k=(y_zero+y-y_win_mon)/pixels_per_trace;
      if(x<WIDTH_TEXT*4)      /* select paste-up */
        {
        if(ft.stn[ft.pos2idx[k]].psup==0)
          {
          for(kk=0;kk<ft.n_ch;kk++) if(ft.stn[kk].psup)
            {
            if(strcmp(ft.stn[kk].name,ft.stn[ft.pos2idx[k]].name)==0)
              switch_psup(kk,0);
            }
          switch_psup(ft.pos2idx[k],1);
          }
        else switch_psup(ft.pos2idx[k],0);
        plot_flag=1;
        ring_bell=0;
        }
      else if(x<WIDTH_INFO)   /* mon info : cancel mark */
        {
        switch(event.mse_code)
          {
          case MSE_BUTNL: break;
          case MSE_BUTNM: break;
          case MSE_BUTNR: /* cancel picks for the station */
            if(cancel_picks(ft.stn[ft.pos2idx[k]].name,-1))
              {
              plot_flag=1;
              ring_bell=0;
              }
          }
        }
      else if(i<ft.len)    /* mon trace */
        {
        if(main_mode==MODE_GET)  /* get zoom */
          {
          if(zoom_win[pos_zoom].valid==0)
            {
            if((zoom_win[pos_zoom].scale=ft.stn[ft.pos2idx[k]].scale-2)<0)
              zoom_win[pos_zoom].scale=0;
            }
          zoom_win[pos_zoom].sys_ch=ft.idx2ch[ft.pos2idx[k]];
          zoom_win[pos_zoom].sec=i;
          zoom_win[pos_zoom].pos=k;
          plot_zoom(pos_zoom,0,0,0);
          main_mode=MODE_NORMAL;
          put_reverse(&dpy,X_Z_GET+1,
            Y_Z_GET+1+height_dpy-1-(pos_zoom+1)*HEIGHT_ZOOM,
            W_Z_GET-1,PIXELS_PER_LINE-1);
          ring_bell=0;
          }
        else  /* pick P, S or X time */
          {
          set_pick(&pt,i,j*100,500/PIXELS_PER_SEC_MON,500/PIXELS_PER_SEC_MON);
          switch(event.mse_code)
            {
            case MSE_BUTNL: i=P;break;
            case MSE_BUTNM: i=S;break;
            case MSE_BUTNR: i=X;break;
            }
          cancel_picks(ft.stn[ft.pos2idx[k]].name,i);
          ft.pick[ft.pos2idx[k]][i]=pt;
          put_mark(i,k,0);
          plot_flag=1;
          ring_bell=0;
          }
        }
      }
    }
  else if(event.mse_trig==MSE_BUTTON && event.mse_dir==MSE_UP
         && event.mse_code>=0)
    {
    if(main_mode==MODE_PICK)
      {
      ring_bell=1;
      i=(height_dpy-1-y)/HEIGHT_ZOOM;   /* zoom no. */
      if(i==pt.valid)  /* in the same zoom window ? */
        {
        xshift=zoom_win[i].shift*zoom_win[i].pixels/1000;
        pt.msec2=(((x-WIDTH_INFO_ZOOM-xshift)%zoom_win[i].pixels)*1000+
          (zoom_win[i].pixels>>1))/zoom_win[i].pixels;
        pt.sec2=zoom_win[i].sec+(x-WIDTH_INFO_ZOOM-xshift)/zoom_win[i].pixels;
        k=zoom_win[i].pos;
        switch(event.mse_code)
          {
          case MSE_BUTNL: j=P;break;
          case MSE_BUTNM: j=S;break;
          case MSE_BUTNR: j=X;break;
          }
        if(pt.sec1*1000+pt.msec1<=pt.sec2*1000+pt.msec2)
          {
          if((pt.msec1-=500/zoom_win[i].pixels)<0)
            {pt.msec1+=1000;pt.sec1-=1;}
          if(pt.sec1<0) pt.msec1=pt.sec1=0;
          if((pt.msec2+=500/zoom_win[i].pixels)>=1000)
            {pt.msec2-=1000;pt.sec2+=1;}
          if(pt.sec2>ft.len-1) {pt.msec2=999;pt.sec2=ft.len-1;}
          pt.valid=1;
          pt.polarity=0;
          cancel_picks(ft.stn[ft.pos2idx[k]].name,j);
          ft.pick[ft.pos2idx[k]][j]=pt;
          put_mark(j,k,0);
          plot_flag=1;
          ring_bell=0;
          }
        else
          {
          if(ft.pick[ft.pos2idx[k]][j].valid)
            {
            put_mark(j,k,0);
            ft.pick[ft.pos2idx[k]][j].valid=0;
            plot_flag=1;
            ring_bell=0;
            }
          }
        }
      main_mode=MODE_NORMAL;
      }
    }
  else if(event.mse_trig==MSE_EXP)
    {
    main_mode=MODE_NORMAL;
    refresh(0);
    ring_bell=0;
    }
  if(!(xx==x_zero && yy==y_zero) || plot_flag) put_mon(x_zero=xx,y_zero=yy);
  else if(ring_bell) bell();
  }

static int
measure_max_zoom(int izoom)
  {
  int i,ch;
  struct Pick_Time pt;

  pt.valid=0;

  plot_zoom(izoom,zoom_win[izoom].length,&pt,0);
  ch=zoom_win[izoom].sys_ch;

  /* if same as before, cancel it */
  if(ft.pick[ft.ch2idx[ch]][MD].valid==pt.valid &&
    ft.pick[ft.ch2idx[ch]][MD].sec1==pt.sec1 &&
    ft.pick[ft.ch2idx[ch]][MD].msec1==pt.msec1) i=1;
  else i=0;

  /* cancel others for the same stn */
  cancel_picks(ft.stn[ft.ch2idx[ch]].name,MD);
  if(i) return(1);

  ft.pick[ft.ch2idx[ch]][MD]=pt;
  put_mark(MD,ft.idx2pos[ft.ch2idx[ch]],0);
  return(1);
  }

static int
get_max(double *db, int n, int *n_max, int *n_min, int *c_max, int *c_min)
  /* int *c_max,*c_min;   number of the same values */
  {
  int i;
  double d_max,d_min;

  d_max=d_min=db[0];
  *n_max=(*n_min)=0;
  *c_max=(*c_min)=1;
  for(i=1;i<n;i++)
    {
    if(db[i]>=d_max)
      {
      if(db[*n_max=i]==d_max) (*c_max)++;
      else
        {
        *c_max=1;
        d_max=db[i];
        }
      }
    else if(db[i]<=d_min)
      {
      if(db[*n_min=i]==d_min) (*c_min)++;
      else
        {
        *c_min=1;
        d_min=db[i];
        }
      }
    }
  if((d_max>100.0 && *c_max>=5) || (d_min<-100.0 && *c_min>=5)) return (1);
  else return (0);
  }

static void
put_function(void)
  {
  int y;

  put_init_depth();
  put_funcs(func_main2,0);
  y=HEIGHT_FUNC;
  put_funcs(func_main,y);
  put_bitblt(&arrows_ud,0,0,32,16,&dpy,x_func(UD)+(WB-32)/2,
    (MARGIN-16)/2+y,BF_SI);
  put_bitblt(&arrows_lr,0,0,32,16,&dpy,x_func(LR)+(WB-32)/2,
    (MARGIN-16)/2+y,BF_SI);
  put_text(&dpy,x_time_now,Y_TIME,diagnos,BF_SI);
  }

static void
put_function_map(void)
  {
  char textbuf[10];

  put_funcs(func_map,0);
  if(map_mode==MODE_TS3) put_reverse(&dpy,x_func(TMSP),0,WB,MARGIN);
  put_funcs(func_map2,height_dpy-MARGIN);
  if(mapsteps[ppk_idx]<10.0) sprintf(textbuf,"%3.1f",mapsteps[ppk_idx]);
  else sprintf(textbuf,"%3d",(int)mapsteps[ppk_idx]);
  put_func(textbuf,QUIT,height_dpy-MARGIN+Y_LINE1,0,1);
  if(map_vert && map_mode!=MODE_TS3)
    {
    put_funcs(func_map3,height_dpy-MARGIN*2-HW);
    if(map_dir>=0) sprintf(textbuf,"N%2dE",map_dir);
    else sprintf(textbuf,"N%2dW",(-map_dir));
    put_func(textbuf,QUIT,height_dpy-MARGIN*2-HW+Y_LINE1,0,1);
    }
  }

static void
put_init_depth(void)
  {
  char textbuf[LINELEN];

  sprintf(textbuf,"DEPTH=>%3dkm+%3dkm",init_dep,init_depe);
  put_black(&dpy,0,0,WIDTH_TEXT*(strlen(textbuf)+1),MARGIN);
  put_text(&dpy,HW,Y_LINE1,textbuf,BF_SI);
  put_text(&dpy,WIDTH_TEXT*12+HW,Y_LINE1,"_",BF_SIDA);
  }

static void
put_function_mecha(void)
  {
  char textbuf[LINELEN];
  put_funcs(func_mech,0);
  sprintf(textbuf,"%s HEMISPHERE PROJECTION",mec_hemi);
  put_black(&dpy,0,0,WIDTH_TEXT*(strlen(textbuf)+1),MARGIN);
  put_text(&dpy,HW,Y_LINE1,textbuf,BF_SI);
  }

static void
put_function_psup(void)
  {

  put_funcs(func_psup,0);
  }

static int
put_main(void)
  {
  int i;

  fflush(stderr);
  put_white(&dpy,0,0,width_dpy,height_dpy); /* clear */
  put_function();
  if(n_zoom)
    draw_seg(0,height_dpy-1,width_dpy,height_dpy-1,LPTN_FF,BF_SDO,&dpy);
  for(i=0;i<n_zoom;i++) /* put zoom */
    {
    put_white(&dpy,0,height_dpy-1-(i+1)*HEIGHT_ZOOM,width_zoom,height_zoom);
    put_fram(&dpy,0,height_dpy-1-(i+1)*HEIGHT_ZOOM,width_zoom,height_zoom);
    put_fram(&dpy,0,height_dpy-1-(i+1)*HEIGHT_ZOOM+1,width_zoom,height_zoom-1);
    put_bitblt(&zoom,0,0,WIDTH_INFO_ZOOM+1,height_zoom,&dpy,0,
      height_dpy-1-(i+1)*HEIGHT_ZOOM,BF_S);
    if(zoom_win[i].valid) plot_zoom(i,0,0,0);
    }
  put_mon(x_zero,y_zero);
#if DEBUG_AP>=1
if(background==0) XSync(disp,0);
#endif
 return (0);
  }

static void
get_screen_type(int *np, unsigned int *w_dpy, unsigned int *h_dpy, int *w_frame, int *h_frame)
  {

  if(background)
    {
    *np=1;
    *w_dpy=W_WIDTH;
    *h_dpy=W_HEIGHT;
    return;
    }
  *np=DefaultDepth(disp,0);
  *w_frame=DisplayWidth(disp,0);
  *h_frame=DisplayHeight(disp,0);
  if((*w_dpy=W_WIDTH)>(*w_frame)-10) *w_dpy=((*w_frame)-10)/8*8;
  if(fit_height) *h_dpy=(*h_frame);
  else *h_dpy=((*h_frame)-10-35); /* 30 for title bar */
  }

static void
draw_ellipse(int xzero, int yzero, double s1, double s2, double roh, int lptn,
	     int func, lBitmap *bm, int x1, int x2, int y1, int y2)
  {
  lPoint pts[200];
  int i,j,jj;
  double u=0.0,v,a=0.0,s1km,s2km,dx,dy,dxp=0.0,dyp=0.0;

  if(xzero>=x1 && xzero<=x2 && yzero>=y1 && yzero<=y2)
    {
    jj=2;
    put_bitblt(&sym_stn,size_sym_stn[jj][0],size_sym_stn[jj][1],
      size_sym_stn[jj][2],size_sym_stn[jj][2],bm,
      xzero-size_sym_stn[jj][3],yzero-size_sym_stn[jj][3],func);
    }
  s1km=s1*pixels_per_km/sqrt(2.0);
  s2km=s2*pixels_per_km/sqrt(2.0);
  if(roh>0.0)
    {
    a=sqrt(2.0*roh);
    u=atanh(sqrt((1.0-roh)/(1.0+roh)));
    }
  else if(roh<0.0)
    {
    a=sqrt(-2.0*roh);
    u=atanh(sqrt((1.0+roh)/(1.0-roh)));
    }
  v=0.0;
  i=0;
  for(j=0;j<200;j++)
    {
    if(roh==0.0)
      {
      pts[i].x=xzero;
      pts[i].y=yzero;
      }
    else
      {
      if(roh>0.0)
        {
        dxp=a*cosh(u)*cos(v);
        dyp=a*sinh(u)*sin(v);
        }
      else if(roh<0.0)
        {
        dxp=a*sinh(u)*cos(v);
        dyp=a*cosh(u)*sin(v);
        }
      dx=s1km*(dxp+dyp);
      dy=s2km*(dxp-dyp);
      pts[i].x=(int)dx+xzero;
      pts[i].y=(int)dy+yzero;
      }
    v+=PI*2.0/199.0;
    if(pts[i].x<x1 || pts[i].x>x2 || pts[i].y<y1 || pts[i].y>y2)
      {
      if(i>1) draw_line(pts,i,lptn,func,bm,bm->rect.origin.x,
        bm->rect.origin.y,bm->rect.extent.x,bm->rect.extent.y,0);
      i=0;
      }
    else i++;
    }
  if(i>1) draw_line(pts,i,lptn,func,bm,bm->rect.origin.x,
    bm->rect.origin.y,bm->rect.extent.x,bm->rect.extent.y,0);
  }

static void
draw_circle(int xzero, int yzero, int r,
	    int lptn, int func, lBitmap *bm)
  {
  lPoint pts[100];
  int i;
  double rr,a;

  i=0;
  rr=(double)r;
  a=0.0;
  for(i=0;i<100;i++)
    {
    pts[i].x=(int)(rr*sin(a))+xzero;
    pts[i].y=(int)(rr*cos(a))+yzero;
    a+=PI*2.0/99.0;
    }
  draw_line(pts,i,lptn,func,bm,bm->rect.origin.x,bm->rect.origin.y,
    bm->rect.extent.x,bm->rect.extent.y,0);
  }

static void
draw_seg(int x1, int y1, int x2, int y2,
	 int lptn, int func, lBitmap *bm)
  {
  lPoint pts[2];

  pts[0].x=x1;  pts[0].y=y1;
  pts[1].x=x2;  pts[1].y=y2;
  draw_line(pts,2,lptn,func,bm,bm->rect.origin.x,bm->rect.origin.y,
    bm->rect.extent.x,bm->rect.extent.y,0);
  }

static void
draw_rect(int x1, int y1, int x2, int y2, int lptn,
	  int func, lBitmap *bm)
  {
  lPoint pts[5];

  pts[0].x=x1;pts[0].y=y1;
  pts[1].x=x1;pts[1].y=y2;
  pts[2].x=x2;pts[2].y=y2;
  pts[3].x=x2;pts[3].y=y1;
  pts[4].x=x1;pts[4].y=y1;
  draw_line(pts,5,lptn,func,bm,bm->rect.origin.x,bm->rect.origin.y,
    bm->rect.extent.x,bm->rect.extent.y,0);
  }

static void
draw_line(lPoint *pts, int np, int lptn, int func,
	  lBitmap *bm, int xzero, int yzero, int xsize, int ysize, int disjoin)
  {
  GC *gc;

  if(background) return;
  if(bm->type==BM_FB) gc=(&gc_line[lptn]);
  else gc=(&gc_line_mem[lptn]);
  XSetFunction(disp,*gc,invert_dpy(BM_MEM,bm->type,func));
  if(disjoin) XDrawSegments(disp,bm->drw,*gc,(XSegment *)pts,np/2);
  else XDrawLines(disp,bm->drw,*gc,pts,np,CoordModeOrigin);
  XFlush(disp);
  }

static void
put_mark_zoom(int idx, int izoom, struct Pick_Time *pt, int mode)
  /* int mode;   0 for observed, 1 for calculated */
  {
  int x,y,i,xshift;

  if(calc_line_off&&(mode==1)) return;
  if(zoom_win[izoom].sec<=pt->sec2 &&
      zoom_win[izoom].sec+zoom_win[izoom].length>pt->sec1)
    {
    y=height_dpy-1-(izoom+1)*HEIGHT_ZOOM+MIN_ZOOM;
    xshift=zoom_win[izoom].shift*zoom_win[izoom].pixels/1000;
    x=WIDTH_INFO_ZOOM+zoom_win[izoom].pixels*(pt->sec1-zoom_win[izoom].sec)+
        (zoom_win[izoom].pixels*pt->msec1+500)/1000+xshift;
    i=WIDTH_INFO_ZOOM+zoom_win[izoom].pixels*(pt->sec2-zoom_win[izoom].sec)+
        (zoom_win[izoom].pixels*pt->msec2+500)/1000+xshift;
    if(x>WIDTH_INFO_ZOOM)
      {
      if(mode==0) put_reverse(&dpy,x,y,i-x+1,MAX_ZOOM-MIN_ZOOM+1);
      else draw_seg(x,y,x,y+MAX_ZOOM-MIN_ZOOM+1,LPTN_33,BF_SDO,&dpy);
      if(x>WIDTH_INFO_ZOOM+WIDTH_TEXT)
        {
        if(mode==1) y+=MAX_ZOOM-HEIGHT_TEXT-1;
        put_text(&dpy,x-WIDTH_TEXT,y,marks[idx],BF_SDXI);
        if(idx<MD && pt->polarity>0)
          put_text(&dpy,x-WIDTH_TEXT,y+HEIGHT_TEXT,"+",BF_SDXI);
        if(idx<MD && pt->polarity<0)
          put_text(&dpy,x-WIDTH_TEXT,y+HEIGHT_TEXT,"-",BF_SDXI);
        }
      }
    else
      put_reverse(&dpy,WIDTH_INFO_ZOOM+1,y,i-WIDTH_INFO_ZOOM,MAX_ZOOM-MIN_ZOOM+1);
    }
  }

static void
put_mon(int xzero, int yzero)
  {
  int i,j,xwm,xz,w_mon;

  i=xzero/ft.w_mon;
  xz=xzero-i*ft.w_mon;
  xwm=x_win_mon;
  /* put info */
  put_bitblt(&info,0,yzero,width_win_info,height_win_mon,&dpy,
    x_win_info,y_win_info,BF_S);
  /* put mon */
  for(;i<ft.n_mon && xwm<width_dpy;i++)
    {
    if(i==ft.n_mon-1) w_mon=ft.w_mon_last;
    else w_mon=ft.w_mon;
    if(width_dpy-xwm>w_mon-xz) j=w_mon-xz;
    else j=width_dpy-xwm;
    put_bitblt(&mon[i],xz,yzero,j,height_win_mon,&dpy,xwm,y_win_mon,BF_S);
/*
printf("%d:%d,%d(%d,%d)->%d,%d xwm=%d width_dpy=%d w_mon=%d\n",i,xz,yzero,j,height_win_mon,xwm,y_win_mon,
xwm,width_dpy,w_mon);
*/
    xwm+=j;
    if(i==ft.n_mon-1 && xwm<width_dpy)
      put_white(&dpy,xwm,y_win_mon,width_dpy-xwm,height_win_mon);
    xz=0;
    }
  if(height_win_mon>height_mon) put_white(&dpy,0,y_win_mon+height_mon,
    width_dpy,height_win_mon-height_mon);
  /* print time */
  i=x_zero/PIXELS_PER_SEC_MON;
  sprintf(monbuf,"%02x/%02x/%02x %02x:%02x:%02x",
    ft.ptr[i].time[0],ft.ptr[i].time[1],ft.ptr[i].time[2],
    ft.ptr[i].time[3],ft.ptr[i].time[4],ft.ptr[i].time[5]);
  if(doing_auto_pick==0) put_text(&dpy,x_time_file,Y_TIME,monbuf,BF_SI);
  else put_text(&dpy,x_time_file,Y_TIME,apbuf,BF_S);
  }

static void
put_bitblt(lBitmap *sbm, int xzero, int yzero, int xsize, int ysize,
	   lBitmap *dbm, int x, int y, int func)
  {
  GC *gc;

  if(background) return;
  if(dbm->type==BM_FB) gc=(&gc_fb);
  else gc=(&gc_mem);
  XSetFunction(disp,*gc,invert_dpy(sbm->type,dbm->type,func));
  if(sbm->type==BM_FB  && dbm->type==BM_FB)
    XCopyArea(disp,sbm->drw,dbm->drw,*gc,xzero,yzero,xsize,ysize,x,y);
  else XCopyPlane(disp,sbm->drw,dbm->drw,*gc,xzero,yzero,xsize,ysize,x,y,1);
  XFlush(disp);
  }

static void
define_bm(lBitmap *bm, char type, unsigned int xsize, unsigned int ysize,
	  char *base)
  {

  if(background) return;
  bm->type=type;
  bm->rect.origin.x=0;
  bm->rect.origin.y=0;
  bm->rect.extent.x=(short)xsize;   /* warning : will over flow? */
  bm->rect.extent.y=(short)ysize;   /* warning : will over flow? */
/* X11 type ; Window (BM_FB) or Pixmap (BM_MEM) */
  if(bm->type==BM_MEM)
    {
    if(base==0) bm->drw=XCreatePixmap(disp,dpy.drw,xsize,ysize,1);
    else bm->drw=XCreateBitmapFromData(disp,dpy.drw,base,xsize,ysize);
    bm->depth=1;
    }
  else if(bm->type==BM_FB)
    {
/*  if(base) bm->drw=XCreatePixmap(disp,dpy.drw,xsize,ysize,8);*/
    bm->depth=DefaultDepth(disp,0);
    }
  }

static void
invert_bits(uint8_w *base, register int bytes)
  {
  static uint8_w bit_conv[256]; /* conversion table */
  static int flag=0;
  static uint8_w bit_mask[8]={0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};
  register int i,j;

  if(flag==0)   /* for the first call */
    {
    for(i=0;i<256;i++) for(j=0;j<8;j++)
        if((int8_w)i&bit_mask[j]) bit_conv[i]|=bit_mask[7-j];  /* uint8_w?? */
    flag=1;
    }
  for(i=0;i<bytes;i++) base[i]=bit_conv[base[i]];
  }

static void
put_text(lBitmap *bm, int xzero, int yzero, char *text, int func)
  {
  register int i,j,code,len;

  if(background) return;
  if((len=strlen(text))>N_BM) len=N_BM;
  for(i=0;i<len;i++)
    {
    code=text[i]-CODE_START;
    for(j=0;j<HEIGHT_TEXT;j++) bbm[j][i]=font16[j][code];
    }
  define_bm(&bbuf,BM_MEM,16*N_BM/2,HEIGHT_TEXT,(char *)bbm);
  put_bitblt(&bbuf,0,0,WIDTH_TEXT*len,HEIGHT_TEXT,bm,xzero,yzero,func);
  XFreePixmap(disp,bbuf.drw);
  }

static int
put_mark(int idx, int pos, int loaded)
  {
  int i;
  char tb1[20],tb2[20];

  if(ft.pick[ft.pos2idx[pos]][idx].valid==0) return (0);
  /* mon */
  put_mark_mon(idx,pos);
  if(flag_change==0)
    {
    flag_change=1;
    if(!loaded)
      {
      *tb1=(*tb2)=0;
      sscanf(diagnos,"%s%s",tb1,tb2);
      if(strcmp(getname(geteuid()),tb1) && strcmp(getname(geteuid()),tb2))
        set_diagnos("",getname(geteuid()));
      }
    }
  flag_hypo=0;  /* calling put_mark_mon() means change of pick data */
  /* zoom */
  if(loop==LOOP_MAIN) for(i=0;i<n_zoom;i++)
    if(zoom_win[i].valid && zoom_win[i].pos==pos)
      put_mark_zoom(idx,i,&ft.pick[ft.pos2idx[pos]][idx],0);
#if DEBUG_AP>=1
if(background==0) XSync(disp,0);
#endif
  return (1);
  }

static void
put_mark_mon(int idx, int pos)
  {
  int x,y,i_map,xz;
  struct Pick_Time *pt;

  pt=(&(ft.pick[ft.pos2idx[pos]][idx]));
  x=((pt->sec1+pt->sec2)*PIXELS_PER_SEC_MON+
    ((pt->msec1+pt->msec2)*PIXELS_PER_SEC_MON+500)/1000)/2;
  y=pos*pixels_per_trace;
  i_map=(xz=x-(WIDTH_TEXT-1))/ft.w_mon;
  put_text(&mon[i_map],xz-=i_map*ft.w_mon,y-(HEIGHT_TEXT-ppt_half),
    marks[idx],BF_SDXI);
  if(++i_map<ft.n_mon && xz+WIDTH_TEXT>ft.w_mon)
    put_text(&mon[i_map],xz-=ft.w_mon,y-(HEIGHT_TEXT-ppt_half),
      marks[idx],BF_SDXI);
  }

static void
make_visible(int idx)
  {
  int pos,y;

  pos=ft.idx2pos[idx];
  y=pos*pixels_per_trace;
  if(y<y_zero || y_zero+height_win_mon<y+pixels_per_trace)
    {
    y_zero=y-height_win_mon/2;
    if(y_zero<0) y_zero=0;
    else if(y_zero>y_zero_max) y_zero=y_zero_max;
    }
  }

static void
list_line(void)
  {
  int i;

  for(i=0;i<80;i++) fprintf(stderr,"=");
  fprintf(stderr,"\n");
  }

static void
raise_ttysw(int idx)
  {
  XEvent xevent;
  int x,y,xt,yt;
  unsigned int w,h,d;
  Window root,parent;

  if(background) return;
  if(idx)
    {
    if(fit_height)
      {
      xgetorigin(disp,dpy.drw,&x,&y,&w,&h,&d,&root,&parent);
      xgetorigin(disp,ttysw,&xt,&yt,&w,&h,&d,&root,&parent);
      if(y+MARGIN+fit_height+3>yt) XMoveWindow(disp,ttysw,xt,y+MARGIN+3);
      }
    XRaiseWindow(disp,ttysw);
    XSync(disp,False);
    }
  else
    {
    XRaiseWindow(disp,dpy.drw);
    /* get an Expose event and throw it away */
    XSync(disp,False);
    for(x=0;x<10;x++) /* wait upto 1 sec (10x100000 usec) */ 
      if(XCheckTypedEvent(disp,Expose,&xevent)==True) break;
      else usleep(100000); /* wait for an Expose event to occur */
    }
  expose_list=idx;
  }

static void
adj_sec_win(int *tm, double *se, int *tmc, double *sec)
  {
  int i;
  double f;

  for(i=0;i<5;i++) tmc[i]=tm[i]; /* copy YMDhm */
  tmc[5]=(int)(f=floor(*se));
  f=(*se)-f;
  lsec2time(time2lsec(tmc),tmc);
  tmc[6]=(int)(f*1000.0);
  *sec=(double)tmc[5]+f;
  }

/* get calculated arrival times for all stations */
static void
get_calc(void)
{
  int iz,i,j,tm_p[7],tm_s[7],tm_ot[7],tm_base[6];
  time_t lsec_bs;
  double sec,sec_ot;
  FILE *fp;
  char prog[NAMLEN],stan[NAMLEN],text_buf[LINELEN];

  read_parameter(PARAM_HYPO,prog);
  read_parameter(PARAM_STRUCT,stan);
  /* write seis file */
  fp=fopen(ft.seis_file2,"w+");
  output_all(fp);
  fclose(fp);
  /* make init file */
  fp=fopen(ft.init_file2,"w+");
  fprintf(fp,"%10.4f%10.4f%8.2f\n",ft.hypo.alat,ft.hypo.along,ft.hypo.dep);
  fprintf(fp,"  5.0       5.0       5.0\n");
  /* above two lines are dummy */
  fprintf(fp,"%d %d %d %d %d %.3f %.5f %.5f %.3f %.1f\n",
    ft.hypo.tm[0],ft.hypo.tm[1],ft.hypo.tm[2],ft.hypo.tm[3],ft.hypo.tm[4],
    ft.hypo.se,ft.hypo.alat,ft.hypo.along,ft.hypo.dep,ft.hypo.mag);
  fclose(fp);
  /* run hypo program */
  sprintf(text_buf,"%s %s %s %s %s %s > /dev/null",
    prog,stan,ft.seis_file2,ft.finl_file2,ft.rept_file2,ft.init_file2);
  system(text_buf);
  read_final(ft.finl_file2,&ft.hypoall);
  bcd_dec(tm_base,ft.ptr[0].time);
  lsec_bs=time2lsec(tm_base);
  adj_sec_win(ft.hypoall.tm,&ft.hypoall.se,tm_ot,&sec_ot);
  cancel_picks_calc();
  set_pick(&ft.pick_calc_ot,(int)(time2lsec(tm_ot)-lsec_bs),tm_ot[6],0,0);
  for(i=0;i<ft.hypoall.ndata;i++){ /* station loop */
    adj_sec_win(ft.hypoall.tm,&ft.hypoall.fnl[i].pt,tm_p,&sec);
    adj_sec_win(ft.hypoall.tm,&ft.hypoall.fnl[i].st,tm_s,&sec);
    for(j=0;j<ft.n_ch;j++) {
      if(strcmp(ft.hypoall.fnl[i].stn,ft.stn[j].name)) continue;
      /* station name = ft.stn[j].name, idx=j */
      set_pick(&ft.pick_calc[j][P],(int)(time2lsec(tm_p)-lsec_bs),tm_p[6],0,0);
      set_pick(&ft.pick_calc[j][S],(int)(time2lsec(tm_s)-lsec_bs),tm_s[6],0,0);
      for(iz=0;iz<n_zoom;iz++) if(j==ft.ch2idx[zoom_win[iz].sys_ch]) {
        put_mark_zoom(P,iz,&ft.pick_calc[j][P],1);
        put_mark_zoom(S,iz,&ft.pick_calc[j][S],1);
      }
    }
  }
  fprintf(stderr,
   "calculated for '%02d %02d %02d %02d %02d %02.3f %.5f %.5f %.3f %.1f'\n",
       tm_ot[0],tm_ot[1],tm_ot[2],tm_ot[3],tm_ot[4],sec_ot,ft.hypoall.alat,
       ft.hypoall.along,ft.hypoall.dep,ft.hypoall.mag);
}

#if HINET_EXTENTION_3>=2
static int
load_data_prep(int btn) /* return=1 means success */
  /* int btn;     MSE_BUTNL, MSE_BUTNM or MSE_BUTNR */
  {
  FILE *fp,*fq;
  struct dirent *dir_ent;
  DIR *dir_ptr;
  int re,ii,i,j,k1,k2,k3,k4,k5,find_file,tm_begin[6],tm[6],sec_max,sockfd;
  float k6;
  time_t lsec_begin;
  char text_buf[LINELEN],*ptr,name1[NAMLEN],name2[NAMLEN],pickfile[NAMLEN],
    name_low[NAMLEN],name_high[NAMLEN],filename[NAMLEN],
    namebuf[NAMLEN],namebuf1[NAMLEN],diagbuf[50],userbuf[50];

  *name_low=(*name_high)=0;
  if(*ft.save_file) find_file=0;  /* pick file name already fixed */
  else find_file=1;               /* search file name here */
  if(btn==MSE_BUTNM) find_file=1; /* discard present pick file name */
                                  /* and search again */
  else if(btn==MSE_BUTNR && find_file==0) /* search the "next" pick file */
    {
    strcpy(name_low,ft.save_file);
    find_file=1;
    }

  if(find_file) /* search in a directory */
    {
    /* get time range */
    i=ft.len-1;
    sprintf(name1,"%02x%02x%02x%1c%02x%02x%02x",ft.ptr[0].time[0],
      ft.ptr[0].time[1],ft.ptr[0].time[2],dot,ft.ptr[0].time[3],
      ft.ptr[0].time[4],ft.ptr[0].time[5]);
    sprintf(name2,"%02x%02x%02x%1c%02x%02x%02x",ft.ptr[i].time[0],
      ft.ptr[i].time[1],ft.ptr[i].time[2],dot,ft.ptr[i].time[3],
      ft.ptr[i].time[4],ft.ptr[i].time[5]);

    re=0;
    if(*ft.pick_server) /* try pick file server */
      {
      if((ptr=strrchr(ft.data_file,'/'))==NULL) ptr=ft.data_file;
      else ptr++;
      if((sockfd=open_sock(ft.pick_server,ft.pick_server_port))!=-1)
        {
        fprintf(stderr,"connected to pick file server %s - ",ft.pick_server);
        fp=fdopen(sockfd,"r+");
        while(fgets(text_buf,LINELEN,fp)) if(strncmp(text_buf,"PICKS OK",8)==0)
          {
          re=1;
          rewind(fp);
          break;
          }
        if(re && (re=fprintf(fp,"%s %s %s %s\n",name1,name2,ptr,ft.hypo_dir))>0)
          {
          fflush(fp);
          rewind(fp);
          fprintf(stderr,"OK\n");
          while(fgets(text_buf,LINELEN,fp))
            {
            sscanf(text_buf,"%s",pickfile);
            /* find the earliest (but later than "name_low") pick file */
            if(*name_low && strncmp2(pickfile,name_low,17)<=0) continue;
            if(*name_high && strncmp2(pickfile,name_high,17)>=0) continue;
            strcpy(name_high,pickfile);
            }
          }
        close(sockfd);
        if(*name_high) strcpy(ft.save_file,name_high);
        else if(re) {fprintf(stderr,"NG\n");return (0);}
        }
      }
    if(re==0) /* search pick directory */
      { 
      if((dir_ptr=opendir(ft.hypo_dir))==NULL)
        {
        fprintf(stderr,"directory '%s' not open\007\007\n",ft.hypo_dir);
        return (0)(;
        }
      while((dir_ent=readdir(dir_ptr))!=NULL)
        {
        if(*dir_ent->d_name=='.') continue; /* skip "." & ".." */
      /* pick file name must be in the time range of data file */
        if(strncmp2(dir_ent->d_name,name1,13)<0 ||
          strncmp2(dir_ent->d_name,name2,13)>0) continue;
      /* read the first line */
        sprintf(filename,"%s/%s",ft.hypo_dir,dir_ent->d_name);
        if((fp=fopen(filename,"r"))==NULL) continue;
        *text_buf=0;
        fgets(text_buf,LINELEN,fp);
        fclose(fp);
      /* first line must be "#p [data file name] ..." */
        if((ptr=strrchr(ft.data_file,'/'))==NULL) ptr=ft.data_file;
        else ptr++;
        sscanf(text_buf,"%s%s",filename,namebuf);
        strcpy(namebuf1,namebuf);
        if(namebuf[6]=='.') namebuf1[6]='_';
        else if(namebuf[6]=='_') namebuf1[6]='.';
        if(strcmp(filename,"#p") ||
          (strcmp(namebuf,ptr) && strcmp(namebuf1,ptr))) continue;
      /* find the earliest (but later than "name_low") pick file */
        if(*name_low && strncmp2(dir_ent->d_name,name_low,17)<=0) continue;
        if(*name_high && strncmp2(dir_ent->d_name,name_high,17)>=0) continue;
        strcpy(name_high,dir_ent->d_name);
        }
      closedir(dir_ptr);
      if(*name_high) strcpy(ft.save_file,name_high);
      else return (0);
      }
    }
  /* file name fixed */
  sprintf(filename,"%s/%s",ft.hypo_dir,ft.save_file);
  if((fp=fopen(filename,"r"))==NULL) return (0);
  if(fgets(text_buf,LINELEN,fp)==NULL)
    {
    fclose(fp);
    return (0);
    }
  /* picks */
  *diagbuf=(*userbuf)=0;
  sscanf(text_buf+3,"%s %s %s",namebuf,diagbuf,userbuf);
  if(strcmp(diagbuf,".")==0) *diagbuf=0;
  if(just_hypo)
    {
    ft.pick=(struct Pick_Time (*)[4])
      win_xmalloc(sizeof(struct Pick_Time)*4*ft.n_ch);
    for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++) ft.pick[i][j].valid=0;
    }
  /*cancel_picks(NULL,-1);*/    /* cancel all picks */
  set_diagnos(diagbuf,userbuf);
  /* read picks */
  flag_hypo=flag_mech=sec_max=0;
  for(ii=0;;ii++)
    {
    *text_buf=0;
    if(fgets(text_buf,LINELEN,fp)==NULL || strncmp(text_buf,"#p",2)) break;
    if(ii==0)
      {
      if(strlen(text_buf)<25) /* for compatibility to old format */
        /* new format has time of beginning of data */
        {
        sscanf(text_buf+3,"%d%d%d%d%d%d",&tm_begin[0],&tm_begin[1],&tm_begin[2],
          &tm_begin[3],&tm_begin[4],&tm_begin[5]);
        if(!just_hypo)
          {
          bcd_dec(tm,ft.ptr[0].time);
          if(time_cmp_win(tm,tm_begin,6))
            {
            *ft.save_file=0;
            fclose(fp);
            return (0); /* incorrect time */
            }
          }
        continue;
        }
      else if(just_hypo)
        {
        fprintf(stderr,"time offset : 'FileName - %d sec'\n",just_hypo_offset);
        sscanf(ft.data_file+strlen(ft.data_file)-13,"%2d%2d%2d.%2d%2d%2d",
          &tm_begin[0],&tm_begin[1],&tm_begin[2],
          &tm_begin[3],&tm_begin[4],&tm_begin[5]);
        lsec2time(time2lsec(tm_begin)-just_hypo_offset,tm_begin);
        }
      }
    sscanf(text_buf+3,"%x%d%d%d%d%d%d%e",&i,&j,&k1,&k2,&k3,&k4,&k5,&k6);
    /* 2001.6.7. if data file exists, don't accept out-of-range picks */
    if(!just_hypo && (k1<0 || k1>=ft.len || k3<0 || k3>=ft.len))
      {
      fprintf(stderr,"out-of-range pick ignored - %s",text_buf);
      continue;
      }
    if(ft.ch2idx[i]<0) continue;
    if(j>MD) continue;
    ft.pick[ft.ch2idx[i]][j].valid=0;
    ft.pick[ft.ch2idx[i]][j].sec1 =k1;
    ft.pick[ft.ch2idx[i]][j].msec1=k2;
    ft.pick[ft.ch2idx[i]][j].sec2 =k3;
    ft.pick[ft.ch2idx[i]][j].msec2=k4;
    ft.pick[ft.ch2idx[i]][j].polarity=k5;
    if(j==MD) *(float *)&ft.pick[ft.ch2idx[i]][j].valid=k6;
    if(k3>sec_max) sec_max=k3;
    /*if(!just_hypo) put_mark(j,ft.idx2pos[ft.ch2idx[i]],1);*/
    }
  fprintf(stderr,"loaded from pick file '%s'\n",filename);
  if(just_hypo)
    {
    ft.len=sec_max+1;
    if((ft.ptr=(struct File_Ptr *)win_xmalloc(sizeof(*(ft.ptr))*ft.len))==NULL)
      emalloc("ft.ptr");
    lsec_begin=time2lsec(tm_begin);
    for(i=0;i<ft.len;i++)
      {
      dec_bcd(ft.ptr[i].time,tm_begin);
      lsec2time(++lsec_begin,tm_begin);
      }
    return (1);
    }
  /* read seis */
  if(strncmp(text_buf,"#s",2)==0)
    {
    fq=fopen(ft.seis_file,"w+");
    do
      {
      fprintf(fq,"%s",text_buf+3);
      *text_buf=0;
      if(fgets(text_buf,LINELEN,fp)==NULL) break;
      } while(strncmp(text_buf,"#s",2)==0);
    fclose(fq);
    }
  /* read final */
  if(strncmp(text_buf,"#f",2)==0)
    {
    fq=fopen(ft.finl_file,"w+");
    do
      {
      fprintf(fq,"%s",text_buf+3);
      *text_buf=0;
      if(fgets(text_buf,LINELEN,fp)==NULL) break;
      } while(strncmp(text_buf,"#f",2)==0);
    fclose(fq);
    read_final(ft.finl_file,&ft.hypo);
    /*flag_hypo=1;*/
    get_delta();
    }
  /* read mecha */
  if(strncmp(text_buf,"#m",2)==0)
    {
    fq=fopen(ft.mech_file,"w+");
    do
      {
      fprintf(fq,"%s",text_buf+3);
      *text_buf=0;
      if(fgets(text_buf,LINELEN,fp)==NULL) break;
      } while(strncmp(text_buf,"#m",2)==0);
    fclose(fq);
    flag_mech=1;
    }
  fclose(fp);
  flag_change=0;
  if(!ft.hypo.valid) return (0);
  return (1);
  }
#endif  /* #if HINET_EXTENTION_3>=2  */

#if HINET_EXTENTION_3
static int
replot_mon(int replot_locate)
{
  int yy,i,j,k,base_sec,mon_len,i_mon;
  char textbuf[LINELEN],tbuf[20];
  uint8_w *buf_mon;
  int save_flag_change;

  save_flag_change=flag_change;
/* define bitmap info */
  /* define_bm(&info,BM_MEM,width_info,height_info,0); */
  put_white(&info,0,0,width_info,height_info);

/* define bitmap zoom */
  /*define_bm(&zoom,BaM_MEM,WIDTH_INFO_ZOOM+1,height_zoom,0);*/
  n_zoom=0;
  put_white(&zoom,0,0,WIDTH_INFO_ZOOM+1,height_zoom);
  /* make zoom info format */
#if 1
  put_fram(&zoom,0,0,WIDTH_INFO_ZOOM+1,height_zoom);
  put_fram(&zoom,0,1,WIDTH_INFO_ZOOM+1,height_zoom-1);
  put_black(&zoom,X_Z_CHN,Y_Z_CHN,W_Z_CHN+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_POL,Y_Z_POL,W_Z_POL+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_SCL,Y_Z_SCL,W_Z_SCL+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_TSC,Y_Z_TSC,W_Z_TSC+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_SFT,Y_Z_SFT,W_Z_SFT+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_FLT,Y_Z_FLT,W_Z_FLT+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_GET,Y_Z_GET,W_Z_GET+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_PUT,Y_Z_PUT,W_Z_PUT+1,PIXELS_PER_LINE+1);
  put_fram(&zoom,X_Z_CLS,Y_Z_CLS,W_Z_CLS+1,PIXELS_PER_LINE+1);
  put_bitblt(&arrows_scale,0,0,32,16,&zoom,X_Z_SCL+X_Z_ARR,Y_Z_SCL+Y_Z_ARR,BF_SDO);
  put_bitblt(&arrows_leng,0,0,32,16,&zoom,X_Z_TSC+X_Z_ARR,Y_Z_TSC+Y_Z_ARR,BF_SDO);
  put_bitblt(&arrows_lr_zoom,0,0,32,16,&zoom,X_Z_SFT+X_Z_ARR,
    Y_Z_SFT+Y_Z_ARR,BF_SDO);
  put_text(&zoom,X_Z_POL+(W_Z_POL-WIDTH_TEXT*3)/2,Y_Z_POL+Y_Z_OFS,"+/-",BF_SDO);
  put_text(&zoom,X_Z_FLT+WIDTH_TEXT,Y_Z_FLT+Y_Z_OFS,"   NO FILTER     ",BF_SDO);
  put_text(&zoom,X_Z_GET+(W_Z_GET-WIDTH_TEXT*3)/2,Y_Z_GET+Y_Z_OFS,"GET",BF_SDO);
  put_text(&zoom,X_Z_PUT+(W_Z_PUT-WIDTH_TEXT*3)/2,Y_Z_PUT+Y_Z_OFS,"PUT",BF_SDO);
  put_text(&zoom,X_Z_CLS+(W_Z_CLS-WIDTH_TEXT*3)/2,Y_Z_CLS+Y_Z_OFS,"CLS",BF_SDO);
#endif  /* #if 1 */
  if(replot_locate==-1) reorder();
  else get_delta();
  /*flag_hypo=1;*/
  /* print info lines */
  yy=k=0;
  for(j=0;j<ft.n_ch;j++)
    {
    sprintf(textbuf,"%04X",ft.idx2ch[ft.pos2idx[j]]);
    for(i=0;i<WIDTH_INFO_C-4;i++) textbuf[4+i]=' ';
    sprintf(tbuf,"%d",ft.stn[ft.pos2idx[j]].scale);
    if(strlen(tbuf)==1) strcpy(textbuf+WIDTH_INFO_C-1,tbuf);
    else strcpy(textbuf+WIDTH_INFO_C-2,tbuf);
    sprintf(tbuf,"%s-%s",ft.stn[ft.pos2idx[j]].name,ft.stn[ft.pos2idx[j]].comp);
    if(!(j==0 || strcmp(ft.stn[ft.pos2idx[j]].name,
        ft.stn[ft.pos2idx[j-1]].name)))
      for(i=0;i<strlen(ft.stn[ft.pos2idx[j]].name)+1;i++) tbuf[i]=' ';
    if(WIDTH_INFO_C-5-1>=strlen(tbuf))
      for(i=0;i<strlen(tbuf);i++) textbuf[5+i]=tbuf[i];
    else for(i=0;i<strlen(tbuf);i++) textbuf[4+i]=tbuf[i]; 
    if(j>0 && ft.stn[ft.pos2idx[j]].rflag!=ft.stn[ft.pos2idx[j-1]].rflag) k^=1;
    if(k) put_text(&info,0,yy,textbuf,BF_SI);
    else put_text(&info,0,yy,textbuf,BF_S);
    yy+=pixels_per_trace;
    }

  /* make mon */
  /*ft.len_mon=(MEMORY_LIMIT/height_mon)/PIXELS_PER_SEC_MON;
    if((ft.w_mon=ft.len_mon*PIXELS_PER_SEC_MON)>MAX_SHORT)
    {
    ft.len_mon=MAX_SHORT/PIXELS_PER_SEC_MON;
    ft.w_mon=ft.len_mon*PIXELS_PER_SEC_MON;
      }
    ft.n_mon=(ft.len-1)/ft.len_mon+1;*/   /* n of bitmaps */
  fprintf(stderr,"%d sec x %d chs (%d bitmap(s))\n",ft.len,ft.n_ch,ft.n_mon);
  /*if(background && flag_save!=2) goto bg1;
    if((mon=(lBitmap *)win_xmalloc(sizeof(lBitmap)*ft.n_mon))==0) emalloc("mon");*/
  base_sec=0;
  width_mon_max=0;
  for(i_mon=0;i_mon<ft.n_mon;i_mon++)
    {
    if((mon_len=ft.len-base_sec)>ft.len_mon) mon_len=ft.len_mon;
    if((width_mon=mon_len*PIXELS_PER_SEC_MON)>width_mon_max)
      width_mon_max=width_mon;
    if((buf_mon=(uint8_w *)win_xmalloc(p2w(width_mon)*height_mon*2))
      ==NULL) break;
    for(i=0;i<p2w(width_mon)*height_mon*2;i++) buf_mon[i]=0x00;
    fprintf(stderr,"map #%d/%d : %d sec x %d chs (%d x %d)",
      i_mon+1,ft.n_mon,mon_len,ft.n_ch,width_mon,height_mon);
    fprintf(stderr,"   %d bytes\n",p2w(width_mon)*height_mon*2);
    /* plot mon */
    plot_mon(base_sec,mon_len,p2w(width_mon)*2,buf_mon);
    define_bm(&mon[i_mon],BM_MEM,p2w(width_mon)*16,height_mon,(char *)buf_mon);
    base_sec+=mon_len;
    }
  if(i_mon<ft.n_mon)
    {
    fprintf(stderr,"*** Data truncated at %d sec ***\n",base_sec);
    ft.n_mon=i_mon;
    ft.len=ft.n_mon*ft.len_mon;
    }
  /* plot mark for P,S,etc */
  for(i=0;i<ft.n_ch;i++){
    for(j=0;j<4;j++){
      if(ft.pick[i][j].valid)
        {
	  put_mark(j,ft.idx2pos[i],1);
        }
    }
  }
  if(replot_locate==1) locate(0,0);
  if(replot_locate>=0) get_calc();
  flag_change=save_flag_change;
  return (0);
}

static int
reorder(void)
{
  int i,j,k,kk;

  k=0;
  for(j=0;j<ft.n_ch;j++) {
    ft.idx2pos[j]=-1;
  }
  for(;;){  /* arrange channls in descending order of 'order' */
    j=kk=0;
    for(i=0;i<ft.n_ch;i++){
      if(ft.idx2pos[i]>=0) continue;
      if(j==0 || ft.stn[i].ch_order>ft.stn[kk].ch_order) kk=i;
      j=1;
    }
    if(j==0) break;
    ft.idx2pos[kk]=k;
    ft.pos2idx[k++]=kk;
  }
  for(j=0;j<ft.n_ch;j++) {
    if(ft.idx2pos[j]<0) {
      ft.idx2pos[j]=k;
      ft.pos2idx[k]=j;
      k++;
    }
  }
  fprintf(stderr,"re-order by default.\n");
  return (0);
}

/* get calculated delta  for all stations */
static int
get_delta(void)
{
  int i,j,k,tm_ot[7],tm_base[6],b,kk,ll;
  time_t lsec_bs;
  double sec_ot,a;
  FILE *fp;
  char prog[NAMLEN],stan[NAMLEN],text_buf[LINELEN];

  read_parameter(PARAM_HYPO,prog);
  read_parameter(PARAM_STRUCT,stan);
  /* write seis file */
  fp=fopen(ft.seis_file2,"w+");
  output_all(fp);
  fclose(fp);
  /* make init file */
  fp=fopen(ft.init_file2,"w+");
  fprintf(fp,"%10.4f%10.4f%8.2f\n",ft.hypo.alat,ft.hypo.along,ft.hypo.dep);
  fprintf(fp,"  5.0       5.0       5.0\n");
  /* above two lines are dummy */
  fprintf(fp,"%d %d %d %d %d %.3f %.5f %.5f %.3f %.1f\n",
	  ft.hypo.tm[0],ft.hypo.tm[1],ft.hypo.tm[2],ft.hypo.tm[3],ft.hypo.tm[4],
	  ft.hypo.se,ft.hypo.alat,ft.hypo.along,ft.hypo.dep,ft.hypo.mag);
  fclose(fp);
  /* run hypo program */
  sprintf(text_buf,"%s %s %s %s %s %s > /dev/null",
	  prog,stan,ft.seis_file2,ft.finl_file2,ft.rept_file2,ft.init_file2);
  system(text_buf);
  read_final(ft.finl_file2,&ft.hypoall);
  bcd_dec(tm_base,ft.ptr[0].time);
  lsec_bs=time2lsec(tm_base);
  adj_sec_win(ft.hypoall.tm,&ft.hypoall.se,tm_ot,&sec_ot);
  /* sort by delta */
  for(i=0;i<ft.hypoall.ndata;i++){ /* station loop */
    a=ft.hypoall.fnl[i].delta;
    b=ft.hypoall.fnl[i].idx;
    j=i-1;
    while(j>=0&&ft.hypoall.fnl[j].delta>a){
      ft.hypoall.fnl[j+1].delta=ft.hypoall.fnl[j].delta;
      ft.hypoall.fnl[j+1].idx=ft.hypoall.fnl[j].idx;
      j--;
    }
    ft.hypoall.fnl[j+1].delta=a;
    ft.hypoall.fnl[j+1].idx=b;
  }
#if 0
  for(i=0;i<ft.hypoall.ndata;i++){ /* station loop */
    fprintf(stderr,"%10s %4d %7.1f\n",
	    ft.hypoall.fnl[ft.hypoall.fnl[i].idx].stn,ft.hypoall.fnl[i].idx,ft.hypoall.fnl[i].delta);
  }
#endif  /* #if 0 */
  for(j=0;j<ft.n_ch;j++) {
    ft.idx2pos[j]=-1;
  }
  /* determined pos of plot_mon*/
  k=0;kk=0;
  for(i=0;i<ft.hypoall.ndata;i++){ /* station loop */
    for(;;){
      ll=0;
      for(j=0;j<ft.n_ch;j++) {
        if(ft.idx2pos[j]>=0) continue;
        if(strcmp(ft.hypoall.fnl[ft.hypoall.fnl[i].idx].stn,ft.stn[j].name)==0){
          /*fprintf(stderr,"%10s pos %d idx %d \n", ft.hypoall.fnl[ft.hypoall.fnl[i].idx].stn,k,j);*/
          if(ll==0||ft.stn[j].order>ft.stn[kk].order) kk=j;
          ll ++;
        }
      }
      if(ll==0) break;
      ft.idx2pos[kk]=k;
      ft.pos2idx[k]=kk;
      k++;
    }
  }
  /* no station infomation */
  for(j=0;j<ft.n_ch;j++) {
    if(ft.idx2pos[j]<0) {
      ft.idx2pos[j]=k;
      ft.pos2idx[k]=j;
      k++;
    }
  }  
  fprintf(stderr,
	  "calculated delta for '%02d %02d %02d %02d %02d %02.3f %.5f %.5f %.3f %.1f'\n",
	  tm_ot[0],tm_ot[1],tm_ot[2],tm_ot[3],tm_ot[4],sec_ot,ft.hypoall.alat,
	  ft.hypoall.along,ft.hypoall.dep,ft.hypoall.mag);
  return (0);
}
#endif  /* #if HINET_EXTENTION_3 */

static void
locate(int flag, int hint)
  /* int flag;    1:output on display */
  /* int hint;    1:use present hypocenter as the initial value */
  {
  FILE *fp;
  float init_lat,init_lon;
  char prog[NAMLEN],stan[NAMLEN],text_buf[LINELEN],*ptr;

  read_parameter(PARAM_HYPO,prog);
  read_parameter(PARAM_STRUCT,stan);
  /* write and read seis file */
  fp=fopen(ft.seis_file,"w+");
  output_pick(fp);
  fseek(fp,0L,0);   /* rewind file */
  fgets(text_buf,LINELEN,fp);
  init_lat=init_lat_init;
  init_lon=init_lon_init;
  if((ptr=strchr(stan,'*'))) *ptr=0;
  else while(fgets(text_buf,LINELEN,fp)!=NULL)
    {
    if(strlen(text_buf)>60)
      {
      sscanf(text_buf,"%*s%*s%*s%*s%*s%*s%*s%*s%f%f",&init_lat,&init_lon);
      if((init_lat==0.0)&&(init_lon==0.0)) continue;
      init_lat+=0.0001;
      init_lon+=0.0001;
      break;
      }
    }
  fclose(fp);
  /* make init file */
  fp=fopen(ft.init_file,"w+");
  if(hint)
    {
    fprintf(fp,"%10.4f%10.4f%8.2f\n",
      ft.hypoall.alat,ft.hypoall.along,ft.hypoall.dep);
    if(init_depe==0) fprintf(fp,"  5.0       5.0       0.1\n");
    else fprintf(fp,"  5.0       5.0       5.0\n");
    }
  else
    {
    fprintf(fp,"%10.4f%10.4f%6d.0\n",init_lat,init_lon,init_dep);
    if(init_depe==0) fprintf(fp,"  %6.1f     %6.1f        0.1\n",
      init_late_init,init_lone_init);
    else fprintf(fp,"  %6.1f     %6.1f     %4d.0\n",
      init_late_init,init_lone_init,init_depe);
    }
  fclose(fp);
  /* run hypo program */
  sprintf(text_buf,"%s %s %s %s %s %s",prog,stan,ft.seis_file,ft.finl_file,
    ft.rept_file,ft.init_file);
  if(flag==0) strcat(text_buf," > /dev/null");
  system(text_buf);
  bell();
  read_final(ft.finl_file,&ft.hypo);
  flag_hypo=flag_change=1;
  }

static void
list_picks(int more)
  /* int more;    if 1, use more */
  {
  char textbuf[LINELEN];
  FILE *fp;
  int i,j,k;

  list_line();
  if(diagnos[1]!=' ' || diagnos[strlen(diagnos)-2]!=' ')
    fprintf(stderr,"%s\n",diagnos);
  k=0;
  for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++)
    if(ft.pick[i][j].valid) k=1;
  if(doing_auto_pick) more=0;
  if(k)
    {
    fp=fopen(ft.seis_file,"w+");
    output_pick(fp);
    fclose(fp);
    if(more) sprintf(textbuf,"more -f %s",ft.seis_file);
    else sprintf(textbuf,"cat %s",ft.seis_file);
    system(textbuf);
    }
  else fprintf(stderr,"no picks\n");
  }

static void
list_finl(int more)
  /* int more;  if 1, use more */
  {
  char textbuf[LINELEN];

  list_line();
  if(flag_hypo==1)
    {
    if(doing_auto_pick) more=0;
    if(more) sprintf(textbuf,"more -f %s",ft.finl_file);
    else sprintf(textbuf,"cat %s",ft.finl_file);
    system(textbuf);
    }
  else fprintf(stderr,"no hypocenter\n");
  }

static void
output_all(FILE *fp)
  {
  int i,idx,tm_base[6];
  char stn[STNLEN];

  bcd_dec(tm_base,ft.ptr[0].time);
  fprintf(fp,"%02d/%02d/%02d %02d:%02d                   %14s\n",
    tm_base[0],tm_base[1],tm_base[2],tm_base[3],tm_base[4],get_time_win(0,0));
  *stn=0;
  for(i=0;i<ft.n_ch;i++) /* i : pos */
    {
    idx=ft.pos2idx[i];
    if(ft.stn[idx].north==0.0 || ft.stn[idx].east==0.0) continue;
    if(strcmp(stn,ft.stn[idx].name)==0) continue;
    fprintf(fp,"%-10s ",ft.stn[idx].name);
    fprintf(fp,".   0.000 1.000   0.000 1.000   0.0 0.00e+00");
    fprintf(fp," %10.5f %10.5f %6ld",ft.stn[idx].north,ft.stn[idx].east,
      ft.stn[idx].z);
    if(ft.stn[idx].stcp!=0.0 || ft.stn[idx].stcs!=0.0)
      fprintf(fp," %6.3f %6.3f\n",ft.stn[idx].stcp,ft.stn[idx].stcs);
    else fprintf(fp,"\n");
    strcpy(stn,ft.stn[idx].name);
    }
  fprintf(fp,"\n");
  }
 
static void
output_pick(FILE *fp)
  {
  long minu,time1,time2,sec,msec,sec_err,msec_err;  /* 64bit ok */
  int i,j,k,init,tm_base[6],tm[6],pos[4],sys_ch;
  double err,rat;
  time_t lsec_base;
  struct Pick_Time *pt;

  for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++)
    if(ft.pick[i][j].valid) ft.pick[i][j].valid=(-ft.pick[i][j].valid);
  init=1;
  lsec_base=0;  /* just for suppress warning */
  for(j=0;j<4;j++) for(;;)
    {
    minu=ft.len*1000;
    pos[P]=pos[S]=pos[X]=pos[MD]=ft.n_ch;
    sys_ch=(-1);
    for(i=0;i<ft.n_ch;i++)  /* get the earliest */
      {
      if((pt=(&ft.pick[i][j]))->valid>=0) continue;
      if(((pt->sec1+pt->sec2)*1000+pt->msec1+pt->msec2)/2<minu)
        {
        minu=((pt->sec1+pt->sec2)*1000+pt->msec1+pt->msec2)/2;
        pos[j]=i;
        sys_ch=ft.idx2ch[i];
        }
      }
    if(pos[j]==ft.n_ch) break;  /* no "j" time left */
    /* pos[j] fixed */

    for(i=0;i<ft.n_ch;i++)
      if(strcmp(ft.stn[i].name,ft.stn[pos[j]].name)==0)
        {
        for(k=j+1;k<4;k++) if(ft.pick[i][k].valid<0) pos[k]=i;
        }
    /* output time */
    if(init)
      {
      if((k=ft.pick[pos[j]][j].sec1)<0) k=0;
      bcd_dec(tm_base,ft.ptr[k].time);
      tm_base[5]=0;
      lsec_base=time2lsec(tm_base);
      fprintf(fp,"%02d/%02d/%02d %02d:%02d",tm_base[0],tm_base[1],tm_base[2],
        tm_base[3],tm_base[4]);
      fprintf(fp,"                   %14s\n",get_time_win(0,0));
      init=0;
      }
    fprintf(fp,"%-10s ",ft.stn[pos[j]].name);
    if(ft.pick[pos[j]][P].valid<0)
      {
      if(ft.pick[pos[j]][P].polarity>0)
        {
        if(*ft.stn[pos[j]].comp=='D') fprintf(fp,"D");
        else fprintf(fp,"U");
        }
      else if(ft.pick[pos[j]][P].polarity<0)
        {
        if(*ft.stn[pos[j]].comp=='D') fprintf(fp,"U");
        else fprintf(fp,"D");
        }
      else fprintf(fp,".");
      }
    else fprintf(fp,".");
    for(i=0;i<2;i++)
      {
      if(pos[i]<ft.n_ch)
        {
        time1=ft.pick[pos[i]][i].sec1*1000+ft.pick[pos[i]][i].msec1;
        time2=ft.pick[pos[i]][i].sec2*1000+ft.pick[pos[i]][i].msec2;
        sec=((time1+time2)/2)/1000;
        msec=((time1+time2)/2)%1000;
        sec_err=((time2-time1)/2)/1000;
        if((msec_err=((time2-time1)/2)%1000)==0) msec_err=1;
        err=(double)sec_err+0.001*(double)msec_err;
        if(doing_auto_pick && hypo_use_ratio)
          {
          get_ratio(ft.stn[pos[j]].name,&rat);
          err+=6.0/rat;
          if(i==S) err*=1.73;
          }
        if(err>99.0) err=99.0;
        bcd_dec(tm,ft.ptr[sec].time);
        fprintf(fp," %3d.%03ld%6.3f",(int)(time2lsec(tm)-lsec_base),msec,err);
        ft.pick[pos[i]][i].valid=(-ft.pick[pos[i]][i].valid);
        }
      else fprintf(fp,"   0.000 0.000");
      }
    i=X;
    if(pos[i]<ft.n_ch)
      {
      time1=ft.pick[pos[i]][i].sec1*1000+ft.pick[pos[i]][i].msec1;
      time2=ft.pick[pos[i]][i].sec2*1000+ft.pick[pos[i]][i].msec2;
      sec=((time1+time2)/2)/1000;
      msec=((time1+time2)/2)%1000;
      sec_err=((time2-time1)/2)/1000;
      if((msec_err=((time2-time1)/2)%1000)==0) msec_err=1;
      bcd_dec(tm,ft.ptr[sec].time);
      fprintf(fp," %3d.%1ld",(int)(time2lsec(tm)-lsec_base),msec/100);
      ft.pick[pos[i]][i].valid=(-ft.pick[pos[i]][i].valid);
      }
    else fprintf(fp,"   0.0");
    i=MD;   /* maximum deflection */
    if(pos[i]<ft.n_ch)
      {
      ft.pick[pos[i]][i].valid=(-ft.pick[pos[i]][i].valid);
      if(ft.pick[pos[i]][i].polarity==(-1))
        { /* velocity sensor (m/s) */
        fprintf(fp," %.2e",*(float *)&ft.pick[pos[i]][i].valid);
        }
      else fprintf(fp," 0.00e+00");
      }
    else fprintf(fp," 0.00e-00");
    /* output coordinates */
    if(sys_ch<0) fprintf(fp,"\n");
    else
      {
      fprintf(fp," %10.5f %10.5f %6ld",ft.stn[ft.ch2idx[sys_ch]].north,
        ft.stn[ft.ch2idx[sys_ch]].east,ft.stn[ft.ch2idx[sys_ch]].z);
      if(ft.stn[ft.ch2idx[sys_ch]].stcp!=0.0 ||
          ft.stn[ft.ch2idx[sys_ch]].stcs!=0.0)
        fprintf(fp," %6.3f %6.3f\n",ft.stn[ft.ch2idx[sys_ch]].stcp,
          ft.stn[ft.ch2idx[sys_ch]].stcs);
      else fprintf(fp,"\n");
      }
    }  /* for(;;) */
  fprintf(fp,"\n");
  }

static void
wait_mouse(void)
  {
  XEvent xevent;

  XNextEvent(disp,&xevent);
  event.mse_data.md_x=xevent.xbutton.x;
  event.mse_data.md_y=xevent.xbutton.y;
  if(xevent.type==ButtonPress || xevent.type==ButtonRelease)
    event.mse_trig=MSE_BUTTON;
  else if(xevent.type==MotionNotify) event.mse_trig=MSE_MOTION;
  else if(xevent.type==Expose) event.mse_trig=MSE_EXP;
  else event.mse_trig=(-1);
  if(xevent.type==ButtonRelease) event.mse_dir=MSE_UP;
  else if(xevent.type==ButtonPress) event.mse_dir=MSE_DOWN;
  else event.mse_dir=MSE_UNKOWN;

  switch(xevent.xbutton.button)
    {
    case Button1: event.mse_code=MSE_BUTNL;break;
    case Button2: event.mse_code=MSE_BUTNM;break;
    case Button3: event.mse_code=MSE_BUTNR;break;
    default:    event.mse_code=(-1);break;
    }
  if(xevent.xbutton.state & ShiftMask) event.mse_key=MSE_SHIFT;
  else if(xevent.xbutton.state & ControlMask) event.mse_key=MSE_CTRL;
  else event.mse_key=MSE_NONE;
  if(xevent.type==Expose) while(XCheckTypedEvent(disp,Expose,&xevent)==True);
  XSync(disp,False);
  }

static void
hard_copy(int ratio)
  {
  int x,y,ratio1,format;
  unsigned int i,j,d;  /* 64bit ok */
  char textbuf[200],printer[30],*ptr;
#define FMT_XWD   1
#define FMT_RASTER  2
#define FMT_PS    3
  Window root,parent,hardcopy_id;

  xgetorigin(disp,dpy.drw,&x,&y,&i,&j,&d,&root,&parent);
  if(parent!=ttysw && parent!=root) hardcopy_id=parent;
  else hardcopy_id=dpy.drw;
  if((ptr=(char *)getenv("DISPLAY"))) sprintf(textbuf,"xwd -display %s ",ptr);
  else strcpy(textbuf,"xwd ");
  if(ratio<1) ratio=1;
  if(ratio>3) ratio=3;
  ratio1=ratio-1;
  read_parameter(PARAM_PRINTER,printer);
  if((ptr=strchr(printer,':'))) /* second printer is specified */
    *ptr=0;
  if((ptr=strchr(printer,'*')))
    {
    *ptr=0;
    format=FMT_XWD;   /* '*' -> xwd */
    }
  else if((ptr=strchr(printer,'&')))
    {
    *ptr=0;
    format=FMT_PS;    /* '&' -> PostScript */
    }
  else format=FMT_PS;

  if(format==FMT_PS)  /* PostScript */
    {
    if(*printer) sprintf(textbuf+strlen(textbuf),
      "-id %ld | xpr -device ps -scale %d | lpr -P%s",
        hardcopy_id,ratio,printer);
    else sprintf(textbuf+strlen(textbuf),
      "-id %ld | xpr -device ps -scale %d > %s.ps",
      hardcopy_id,ratio,NAME_PRG);
    }
  else if(format==FMT_XWD)  /* XWD format */
    {
    if(*printer) sprintf(textbuf+strlen(textbuf),
      "-id %ld | lpr -P%s -x",hardcopy_id,printer);
    else sprintf(textbuf+strlen(textbuf),
      "-id %ld > %s.xwd",hardcopy_id,NAME_PRG);
    }
  system(textbuf);
  return;
  }

static void
draw_ticks(int x_y, int yx, int xy1, int xylen, int num1, int num2, int dir)
  /* int x_y;        0:X axis, 1:Y axis */
  /* int yx;         pos of axis */
  /* int xy1,xylen;  start and end of axis */
  /* int num1,num2;  range of values */
  /* int dir;        +1/-1, direction of tick mark */
  {
  static int steps[5]={1,5,10,50,100};
  int i,nsteps,m,n,maxx,xy,tlen;

  maxx=(xylen)/(WIDTH_TEXT*2);
  nsteps=sizeof(steps)/sizeof(*steps);
  for(m=0;m<nsteps;m++) if((num2-num1)/steps[m]<=maxx) break;
  if((n=m+1)==nsteps) n=0;
  if((i=(num2/steps[m])*steps[m])==num2) i-=steps[m];
  for(;i>num1;i-=steps[m])
    {
    if(n && i%steps[n]==0) tlen=WIDTH_TEXT;
    else tlen=WIDTH_TEXT/2;
    xy=xy1+xylen*(i-num1)/(num2-num1);
    if(x_y) draw_seg(yx,xy,yx+(dir)*tlen,xy,LPTN_FF,BF_SDO,&dpy);
    else draw_seg(xy,yx,xy,yx+(dir)*tlen,LPTN_FF,BF_SDO,&dpy);
    }
  }

/* magnitude-radius(km) relation */
#define   MAG2RAD(mag)  (pow(10.0,(double)mag*0.5-2.0)*0.5)
#define   MAG2RAD2(mag) (1+(int)(mag+(mag-4.0)*(mag-4.0)*(int)(mag/4.0)))
#define   XCONV(x,y)  ((x-x_cent)*cs-(y-y_cent)*sn)
#define   YCONV(x,y)  ((x-x_cent)*sn+(y-y_cent)*cs)
#define   XINV(x,y) ((x-xzero)*cs-(y-yzero)*sn)
#define   YINV(x,y) ((x-xzero)*sn+(y-yzero)*cs)
#define   MAT2ELLIPSE(i,j) \
        s1=sqrt(mat_error[i][i]);\
        s2=sqrt(mat_error[j][j]);\
        roh=mat_error[i][j]/(s1*s2)

static void
draw_coord(int idx, int conv, double along, double alat1, double alat2,
	   double step, int lptn, int xzero, int yzero, double x_cent,
	   double y_cent, double cs, double sn)
  /* int idx;     0:lat, 1:long */
  /* int conv;    1:conv, 0:no conv */ 
  {
  int j,x1,x2,y1,y2,xi,yi;
  double  alat,xd,yd;

  j=(int)(step*120.0*pixels_per_km);
  x1=(-j);
  x2=width_horiz+j;
  y1=MARGIN-j;
  y2=height_horiz+j;
  j=0;
  for(alat=floor(alat1);alat<alat2+step;alat+=step)
    {
    if(idx) pltxy(alat0,along0,&alat,&along,&xd,&yd,0);
    else pltxy(alat0,along0,&along,&alat,&xd,&yd,0);
    if(km2pixel(conv,xzero,yzero,x1,y1,x2,y2,x_cent,y_cent,xd,yd,&xi,&yi,cs,sn))
      continue;
    points[j].x=xi;
    points[j].y=yi;
    j++;    
    }
  if(j) draw_line(points,j,lptn,BF_SDO,&dpy,0,MARGIN,width_horiz,height_horiz,0);
  }

static int
km2pixel(int conv, int xzero, int yzero, int x1, int y1, int x2, int y2,
	 double x_cent, double y_cent, double xd, double yd, int *xi, int *yi,
	 double cs, double sn)
{

  if(conv)
    {
    *xi=xzero+(int)(pixels_per_km*XCONV(xd,yd));
    *yi=yzero-(int)(pixels_per_km*YCONV(xd,yd));
    }
  else
    {
    *xi=xzero+(int)(pixels_per_km*(xd-x_cent));
    *yi=yzero-(int)(pixels_per_km*(yd-y_cent));
    }
  if(*xi<x1 || *xi>x2 || *yi<y1 || *yi>y2) return (1);
  return (0);
  }

static void
phypo(int x, int y, HypoData *h)
  {
  int yy,tmc[7];
  double mag,sec,lat,lon;
  char textbuf[LINELEN],ulat,ulon;

  if(map_f1x<x && x<map_f2x && map_f1y<y && y<map_f2y)
    {
    long2time((struct YMDhms *)tmc,&h->t);
    sec=(double)(h->s)+(double)(h->ss)*0.1;
/*  long2time((struct YMDhms *)tm,&h->t);
    se=(double)(h->s)+(double)(h->ss)*0.1;
    adj_sec_win(tm,&se,tmc,&sec);*/
    mag=(double)(h->m)*0.1;
    map_n_find++;
    if(h->lat<0.0) {lat=(-h->lat);ulat='S';}
    else {lat=h->lat;ulat='N';}
    if(h->lon<0.0) {lon=(-h->lon);ulon='W';}
    else {lon=h->lon;ulon='E';}
    if(phypo_format==1) sprintf(textbuf,
      "%02d %02d %02d %02d %02d %04.1f %.4f %.4f %5.1f %.1f %.4s",
      tmc[0],tmc[1],tmc[2],tmc[3],tmc[4],sec,h->lat,h->lon,h->d,mag,h->o);
    else sprintf(textbuf,
      "%3d %02d/%02d/%02d %02d:%02d:%04.1f %.4f%1c %.4f%1c %5.1fkm M%.1f %.4s",
      map_n_find,tmc[0],tmc[1],tmc[2],tmc[3],tmc[4],sec,
      lat,ulat,lon,ulon,h->d,mag,h->o);
    printf("%s\n",textbuf);
    if(list_on_map)
      {
      yy=MARGIN+BORDER+1+map_line*HEIGHT_TEXT;
      put_text(&dpy,BORDER+1,yy,textbuf,BF_S);
      if(yy+HEIGHT_TEXT<height_dpy-MARGIN-HEIGHT_TEXT) map_line++;
      else map_line=0;
      }
    }
  }

static void
put_time1(long itv, long t, long *t1, long *t1a,
	  int ye, int mo, int da, int ho, int mi, char *tbuf)  /* 64bit ok */
  {

  if(t<(*t1a)-itv)
    {
    *t1a=(*t1);
    sprintf(tbuf,"%02d/%02d/%02d %02d:%02d -",ye,mo,da,ho,mi);
    put_text(&dpy,WIDTH_TEXT*6,height_dpy-MARGIN+Y_LINE1,tbuf,BF_SI);
    }
  *t1=t;
  }

static void
put_time2(long itv, long t, long *t2, long *t2a,
	  int ye, int mo, int da, int ho, int mi, char *tbuf)  /* 64bit ok */
  {

  if(t>(*t2a)+itv)
    {
    *t2a=(*t2);
    sprintf(tbuf,"- %02d/%02d/%02d %02d:%02d",ye,mo,da,ho,mi);
    put_text(&dpy,WIDTH_TEXT*(6+15),height_dpy-MARGIN+Y_LINE1,tbuf,BF_SI);
    }
  *t2=t;
  }

static int
draw_ticks_ts(int j, long jj, int it, int step, int k)
  /* long jj; */  /* 64bit ok */
  {
  char textbuf[10];
  int x,tim[6],t=0; /* just for suppress warning */

  if(it==2 && k>0)
    {
    long2time((struct YMDhms *)tim,&sel.t1);
    sprintf(textbuf,"%d",tim[k-1]);
    put_text(&dpy,map_f2x+MARGIN*3/2-strlen(textbuf)*WIDTH_TEXT/2,
      map_f1y-HEIGHT_TEXT-Y_LINE1,textbuf,BF_SDO);
    t=tim[k-1];
    }
  if(jj<sel.t1) jj+=step;
  do
    {
    x=map_f2x+MARGIN*3/2+(int)
      ((double)j*(double)(jj-sel.t1)/(double)(sel.t2-sel.t1));
    draw_seg(x,map_f1y,x,map_f1y+HW*it,LPTN_FF,BF_SDO,&dpy);
    draw_seg(x,map_f2y,x,map_f2y-HW*it,LPTN_FF,BF_SDO,&dpy);
    if(it==2)
      {
      long2time((struct YMDhms *)tim,&jj);
      sprintf(textbuf,"%d",tim[k]);
      put_text(&dpy,x-WIDTH_TEXT*strlen(textbuf)/2,
        map_f2y+Y_LINE1,textbuf,BF_SDO);
      if(k>0 && t!=tim[k-1])
        {
        sprintf(textbuf,"%d",tim[k-1]);
        put_text(&dpy,x+HW,map_f1y-HEIGHT_TEXT-Y_LINE1,textbuf,BF_SDO);
        t=tim[k-1];
        }
      }
    if(map_vert) draw_seg(x,height_dpy-MARGIN,x,
      height_dpy-MARGIN-HW*it,LPTN_FF,BF_SDO,&dpy);
    } while((jj+=step)<=sel.t2);
  return (it+1);
  }

static int
draw_ticks_ts2(int j, long jj, int it, int k, int *tim, int t)
  /* long jj; */   /* 64bit ok */
  {
  char textbuf[10];
  int x;

  x=map_f2x+MARGIN*3/2+(int)
      ((double)j*(double)(jj-sel.t1)/(double)(sel.t2-sel.t1));
  draw_seg(x,map_f1y,x,map_f1y+HW*it,LPTN_FF,BF_SDO,&dpy);
  draw_seg(x,map_f2y,x,map_f2y-HW*it,LPTN_FF,BF_SDO,&dpy);
  if(it==2 || k==0)
    {
    sprintf(textbuf,"%d",tim[k]);
    put_text(&dpy,x-WIDTH_TEXT*strlen(textbuf)/2,map_f2y+Y_LINE1,
      textbuf,BF_SDO);
    if(k>0 && t!=tim[k-1])
      {
      sprintf(textbuf,"%d",tim[k-1]);
      put_text(&dpy,x+HW,map_f1y-HEIGHT_TEXT-Y_LINE1,textbuf,BF_SDO);
      t=tim[k-1];
      }
    }
  if(map_vert) draw_seg(x,height_dpy-MARGIN,x,
    height_dpy-MARGIN-HW*it,LPTN_FF,BF_SDO,&dpy);
  return (t);
  }

static int
put_map(int idx)  /* 0:redraw all, 1:plot only hypocenters, */
  {
  FILE *fp,*fp_othrs;
  struct dirent *dir_ent;
  DIR *dir_ptr;
  struct stat st_buf;
  char textbuf[LINELEN],filename[NAMLEN],final_dir[NAMLEN],
    *ptr,tbuf1[20],tbuf2[20];
  typedef struct {float x,y;} MapData;
  static MapData *mapdata;
  int i,j,xzero,yzero,xi,yi,zi_y=0,zi_x=0,ye,mo,da,ho,mi,farout,
    x1,x2,y1,y2,lptn,conv,radi,x,y,xt=0,yt,it,tim[6],t=0,istep,
    width_map,height_map,length_map,length_vert=0,length_horiz;
  long jj;    /* 64bit ok */
  long size,t1,t2,t1a,t2a;   /* 64bit ok */
  double xd,yd,alat,along,ala[6],alo[6],se,x_cent,y_cent,
    alat1=0.0,alat2=0.0,along1=0.0,along2=0.0,cs,sn,arg,step,s1,s2,roh,step0;
  float mag,dep_base=0.0;
  HypoData hypo;
  double mat_r[3][3],mat_ri[3][3],mat_error[3][3];
  static int n_hypo;
  /* struct HypoB { */
  /*   int8_w time[8]; /\* Y,M,D,h,m,s,s10,mag10 (in binary, not in BCD) *\/ */
  /*   float alat,along,dep; */
  /*   char diag[4],owner[4]; */
  /*   } hypob;  */
  struct FinalB hypob;   /* 28 bytes / event */
  struct YMDhms tim1;
  union Swp {
    float f;
    uint32_w i;
    uint8_w  c[4];
    } *swp;

  /* read map data file */
  if(mapdata==NULL)
    {
    if((fp=fopen(ft.map_file,"r")))
      {
      fseek(fp,0L,2);
      if((mapdata=(MapData *)win_xmalloc(size=ftell(fp)))==NULL)
        emalloc("mapdata");
      fseek(fp,0L,0);
      fread(mapdata,1,size,fp);
      fclose(fp); 
      for(i=0;i<size/sizeof(MapData);i++)
        {
        swp=(union Swp *)&mapdata[i].x;
        swp->i=(swp->c[0]<<24)+(swp->c[1]<<16)+(swp->c[2]<<8)+swp->c[3];
        swp=(union Swp *)&mapdata[i].y;
        swp->i=(swp->c[0]<<24)+(swp->c[1]<<16)+(swp->c[2]<<8)+swp->c[3];
        }
      alat0=(double)mapdata[0].x;
      along0=(double)mapdata[0].y;
      }
    if(lat_cent>90.0)
      {
      lat_cent=alat0;
      lon_cent=along0;
      }
    }

  pixels_per_km=pdpi/2.54/(mapsteps[ppk_idx]*0.1);
  conv=0;
  width_map=width_dpy;
  if(map_vert)
    {
    height_map=height_dpy-2*MARGIN;
    if(width_map<height_map) length_map=width_map;
    else length_map=height_map;
    length_vert=(int)((double)(length_map-MARGIN)/(double)ratio_vert);
    length_horiz=length_map-MARGIN-length_vert;
    for(i=3;i<sizeof(depsteps)/sizeof(*depsteps)-2;i++)
      if(length_vert<(int)(pixels_per_km*depsteps[i]))
        {
        length_vert=(int)(pixels_per_km*depsteps[--i]);
        if(length_vert<WIDTH_TEXT*16)
          length_vert=(int)(pixels_per_km*depsteps[++i]);
        break;
        }
    sel.dep1=depsteps[sel.dep1_idx];
    if(sel.dep1_idx==0)
      {
      sel.dep2=depsteps[sel.deplen_idx=i];
      sel.dep2_idx=i;
      dep_base=0.0;
      }
    else
      {
      sel.dep2=sel.dep1+depsteps[sel.deplen_idx=i];
      for(sel.dep2_idx=i;sel.dep2_idx<sizeof(depsteps)/
          sizeof(*depsteps)-2;sel.dep2_idx++)
        if(depsteps[sel.dep2_idx]>=sel.dep2) break;
      dep_base=sel.dep1;
      }
    width_horiz=width_map-MARGIN-length_vert;
    height_horiz=height_map-MARGIN-length_vert;
    }
  else
    {
    height_map=height_dpy-MARGIN;
    width_horiz=width_map;
    height_horiz=height_map;
    if(width_horiz<height_horiz) length_horiz=width_horiz;
    else length_horiz=height_horiz;
    sel.dep2=depsteps[sel.dep2_idx];
    }

  arg=PI*(double)map_dir/180.0;
  cs=cos(arg);
  sn=sin(arg);
  if(map_dir) conv=1;

  xzero=width_horiz/2;
  yzero=height_horiz/2+MARGIN;
  pltxy(alat0,along0,&lat_cent,&lon_cent,&x_cent,&y_cent,0);

  if((idx && map_vert==0) || map_mode==MODE_FIND2)
    {
    x1=0;
    x2=width_horiz-1;
    y1=MARGIN;
    y2=MARGIN+height_horiz-1;
    goto only;
    }
  else map_line=0;  /* reset line no. for 'FIND' listing */
  /* clear screen */
  fflush(stderr);
  put_white(&dpy,0,0,width_dpy,height_dpy);

  /* draw coast lines and prefectural borders */
  i=(int)(120.0*pixels_per_km);
  if(i>1000) i=1000;
  x1=(-i);
  x2=width_horiz+i;
  y1=MARGIN-i;
  y2=height_horiz+i;
  lptn=LPTN_FF;
  i=1;
  if(mapdata) for(;;)  /* if map data exists */
    {
    farout=0;
    for(j=0;j<ft.sr_max;j++)
      {
      if(mapdata[i].x<10000.0)
        {
        if(km2pixel(conv,xzero,yzero,x1,y1,x2,y2,x_cent,y_cent,
            (double)mapdata[i].x,(double)mapdata[i].y,&xi,&yi,cs,sn))
          {
          i++;
          farout=1;
          break;
          }
        i++;
        points[j].x=xi;
        points[j].y=yi;
        }
      else
        {
        i++;
        break;
        }
      }
    if(mapdata[i-1].y>=10000.0) break;
    else if(!farout && mapdata[i-1].x<10000.0) i--; /* join */
    if(j>1) draw_line(points,j,lptn,BF_SDO,&dpy,0,MARGIN,width_dpy,height_dpy,0);
    if(mapdata[i-1].x>=1000000.0) lptn=LPTN_0F;
    else if(mapdata[i-1].x>=100000.0) lptn=LPTN_55;
    }

  /* obtain range of lat. and long. */
  if(conv)
    {
    xd=XINV(0,height_horiz)/pixels_per_km+x_cent;
    yd=(-YINV(0,height_horiz))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[0],&alo[0],&xd,&yd,1);
    xd=XINV(width_horiz,height_horiz)/pixels_per_km+x_cent;
    yd=(-YINV(width_horiz,height_horiz))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[1],&alo[1],&xd,&yd,1);
    xd=XINV(0,MARGIN)/pixels_per_km+x_cent;
    yd=(-YINV(0,MARGIN))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[2],&alo[2],&xd,&yd,1);
    xd=XINV(width_horiz,MARGIN)/pixels_per_km+x_cent;
    yd=(-YINV(width_horiz,MARGIN))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[3],&alo[3],&xd,&yd,1);
    j=4;
    }
  else
    {
    xd=((double)(-xzero))/pixels_per_km+x_cent;
    yd=(-(double)(height_horiz-yzero))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[0],&alo[0],&xd,&yd,1);
    xd=((double)(width_horiz-xzero))/pixels_per_km+x_cent;
    yd=(-(double)(height_horiz-yzero))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[1],&alo[1],&xd,&yd,1);
    xd=((double)(-xzero))/pixels_per_km+x_cent;
    yd=(-(double)(MARGIN-yzero))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[2],&alo[2],&xd,&yd,1);
    xd=((double)(width_horiz-xzero))/pixels_per_km+x_cent;
    yd=(-(double)(MARGIN-yzero))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[3],&alo[3],&xd,&yd,1);
    xd=x_cent;
    yd=(-(double)(height_horiz-yzero))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[4],&alo[4],&xd,&yd,1);
    xd=x_cent;
    yd=(-(double)(MARGIN-yzero))/pixels_per_km+y_cent;
    pltxy(alat0,along0,&ala[5],&alo[5],&xd,&yd,1);
    j=6;
    }
  for(i=0;i<j;i++)
    {
    if(i==0 || ala[i]<alat1) alat1=ala[i];
    if(i==0 || ala[i]>alat2) alat2=ala[i];
    if(i==0 || alo[i]<along1) along1=alo[i];
    if(i==0 || alo[i]>along2) along2=alo[i];
    }

  /* draw coordinate axes */
  if((double)length_horiz/pixels_per_km>111.0*1.6)
    {
    i=0;
    step=1.0;
    }
  else if((double)length_horiz/pixels_per_km>111.0*2.0/6.0)
    {
    i=1;
    step=15.0/60.0;
    }
  else
    {
    step=7.5/60.0;
    i=2;
    }
  jj=0;
  step0=5.0/60.0;
  for(alat=floor(alat1);alat<alat2+step0;alat+=step0)
    {
    if(jj%12==0)
      draw_coord(0,conv,alat,along1,along2,step,LPTN_33,
        xzero,yzero,x_cent,y_cent,cs,sn);
    else if(jj%2==0 && i>=1)
      draw_coord(0,conv,alat,along1,along2,step,LPTN_01,
        xzero,yzero,x_cent,y_cent,cs,sn);
    else if(i>=2)
      draw_coord(0,conv,alat,along1,along2,step,LPTN_0001,
        xzero,yzero,x_cent,y_cent,cs,sn);
    jj++;
    }
  if(i==0) step=1.0;
  else if(i==1) step=10.0/60.0;
  else step=5.0/60.0;
  jj=0;
  step0=7.5/60.0;
  for(along=floor(along1);along<along2+step0;along+=step0)
    {
    if(jj%8==0)
      draw_coord(1,conv,along,alat1,alat2,step,LPTN_33,
        xzero,yzero,x_cent,y_cent,cs,sn);
    else if(jj%2==0 && i>=1)
      draw_coord(1,conv,along,alat1,alat2,step,LPTN_01,
        xzero,yzero,x_cent,y_cent,cs,sn);
    else if(i>=2)
      draw_coord(1,conv,along,alat1,alat2,step,LPTN_0001,
        xzero,yzero,x_cent,y_cent,cs,sn);
    jj++;
    }

  if(map_vert)
    {
    put_white(&dpy,0,0,width_dpy,MARGIN);
    put_white(&dpy,0,MARGIN+height_horiz,width_map,length_vert+MARGIN*2);
    put_white(&dpy,width_horiz,MARGIN,length_vert+MARGIN,height_map);
    /* horiz */
    draw_rect(0,MARGIN,width_horiz-1,MARGIN+height_horiz-1,
      LPTN_FF,BF_SDO,&dpy);
    if(map_mode==MODE_TS2) goto no_vert;
    /* vert (bottom) */
    draw_rect(0,MARGIN*2+height_horiz,width_horiz-1,
      MARGIN*2+height_horiz+length_vert-1,LPTN_FF,BF_SDO,&dpy);
    if(sel.dep1_idx==0) i=0;
    else i=(int)sel.dep1;
    draw_ticks(1,0,MARGIN*2+height_horiz,length_vert,i,(int)sel.dep2, 1);
    draw_ticks(1,width_horiz-1,MARGIN*2+height_horiz,
      length_vert,i,(int)sel.dep2,-1);
    /* vert (right) */
    draw_rect(width_horiz+MARGIN,MARGIN,width_horiz+MARGIN+length_vert-1,
      MARGIN+height_horiz-1,LPTN_FF,BF_SDO,&dpy);
    draw_ticks(0,MARGIN,width_horiz+MARGIN,length_vert,i,
      (int)sel.dep2, 1);
    draw_ticks(0,MARGIN+height_horiz-1,width_horiz+MARGIN,
      length_vert,i,(int)sel.dep2,-1);

    sprintf(textbuf,"%-3d",i);
    put_text(&dpy,width_horiz+MARGIN-WIDTH_TEXT,
      height_horiz+MARGIN*2-HEIGHT_TEXT/2,textbuf,BF_SDO);
    sprintf(textbuf,"%-3d",(int)sel.dep2);
    put_text(&dpy,width_horiz+MARGIN-WIDTH_TEXT,
      height_dpy-MARGIN-HEIGHT_TEXT,textbuf,BF_SDO);
    sprintf(textbuf,"%3d",(int)sel.dep2);
    put_text(&dpy,width_dpy-WIDTH_TEXT*strlen(textbuf),
      MARGIN+height_horiz+MARGIN-HEIGHT_TEXT/2,textbuf,BF_SDO);
    }
no_vert:
  /* plot stations */
  x1=0;
  x2=width_horiz-1;
  y1=MARGIN;
  y2=MARGIN+height_horiz-1;

  if(map_name>=0)
    {
    for(i=0;i<ft.n_ch_ex;i++)
        if(ft.stn[i].north!=0.0 && ft.stn[i].east!=0.0)
      {
      if(km2pixel(conv,xzero,yzero,x1,y1,x2,y2,x_cent,y_cent,
          (double)ft.stn[i].x,(double)ft.stn[i].y,&xi,&yi,cs,sn))
        continue;
      if(i<ft.n_ch) jj=1;
      else jj=0;
      put_bitblt(&sym_stn,size_sym_stn[jj][0],size_sym_stn[jj][1],
        size_sym_stn[jj][2],size_sym_stn[jj][2],&dpy,
        xi-size_sym_stn[jj][3],yi-size_sym_stn[jj][3],BF_SDO);
      if(map_name) put_text(&dpy,xi+WIDTH_TEXT,yi-(HEIGHT_TEXT-1)/2,
              ft.stn[i].name,BF_SDO);
      if(map_vert && map_vstn && sel.dep1<=0.0)
        {
        zi_x=width_horiz+MARGIN+(int)(((double)ft.stn[i].z*
          (-0.001)-dep_base)*pixels_per_km);
        zi_y=height_horiz+MARGIN*2+(int)(((double)ft.stn[i].z*
          (-0.001)-dep_base)*pixels_per_km);
        if(zi_x<width_horiz) continue;
        put_bitblt(&sym_stn,size_sym_stn[jj][0],size_sym_stn[jj][1],
          size_sym_stn[jj][2],size_sym_stn[jj][2],&dpy,
          xi-size_sym_stn[jj][3],zi_y-size_sym_stn[jj][3],BF_SDO);
        put_bitblt(&sym_stn,size_sym_stn[jj][0],size_sym_stn[jj][1],
          size_sym_stn[jj][2],size_sym_stn[jj][2],&dpy,
          zi_x-size_sym_stn[jj][3],yi-size_sym_stn[jj][3],BF_SDO);
        }
      }
    for(i=0;i<ft.n_ch;i++)
      {
      if(ft.pos2idx[i]<0) continue;
      if(ft.pick[ft.pos2idx[i]][P].valid)
        {
        if(ft.stn[ft.pos2idx[i]].north!=0.0 &&
            ft.stn[ft.pos2idx[i]].east!=0.0)
          {
          if(km2pixel(conv,xzero,yzero,x1,y1,x2,y2,x_cent,y_cent,
            (double)ft.stn[ft.pos2idx[i]].x,
            (double)ft.stn[ft.pos2idx[i]].y,
            &xi,&yi,cs,sn)) continue;
          jj=3;
          put_bitblt(&sym_stn,size_sym_stn[jj][0],size_sym_stn[jj][1],
            size_sym_stn[jj][2],size_sym_stn[jj][2],&dpy,
            xi-size_sym_stn[jj][3],yi-size_sym_stn[jj][3],BF_SDO);
          if(map_vert && map_vstn && sel.dep1<=0.0)
            {
            zi_x=width_horiz+MARGIN+
              (int)((double)ft.stn[ft.pos2idx[j]].z*(-0.001)*pixels_per_km);
            zi_y=height_horiz+MARGIN*2+
              (int)((double)ft.stn[ft.pos2idx[j]].z*(-0.001)*pixels_per_km);
            if(zi_x<width_horiz) continue;
            put_bitblt(&sym_stn,size_sym_stn[jj][0],size_sym_stn[jj][1],
              size_sym_stn[jj][2],size_sym_stn[jj][2],&dpy,
              xi-size_sym_stn[jj][3],zi_y-size_sym_stn[jj][3],BF_SDO);
            put_bitblt(&sym_stn,size_sym_stn[jj][0],size_sym_stn[jj][1],
              size_sym_stn[jj][2],size_sym_stn[jj][2],&dpy,
              zi_x-size_sym_stn[jj][3],yi-size_sym_stn[jj][3],BF_SDO);
            }
          }
        }
      }
    }

only:
  /* plot hypocenter(s) */
  if(other_epis)
    {
    if(map_vert && map_mode!=MODE_TS2)
      {
  /* sample of symbols */
      xi=width_dpy-MARGIN;
      yi=MARGIN*2+height_horiz+length_vert/4+HEIGHT_TEXT;
      xi-=WIDTH_TEXT*2;
      mag=0.0;
      for(;;)
        {
        if(map_true) radi=(int)(pixels_per_km*MAG2RAD(mag));
        else if((radi=MAG2RAD2(mag))<=0) radi=1;
        if(xi-radi<width_horiz || yi-radi<MARGIN+height_horiz) break;
        if(radi>size_sym[N_SYM_USE-1][4])
          {
          zi_x=xi;
          if(radi*2>WIDTH_TEXT*3/2) xi-=radi*2;
          else xi-=WIDTH_TEXT*3/2;
          }
        else
          {
          for(i=1;i<N_SYM-2;i++) if(radi<=size_sym[i][4]) break;
          zi_x=xi;
          if(size_sym[i][3]*3>WIDTH_TEXT*3/2) xi-=size_sym[i][3]*3;
          else xi-=WIDTH_TEXT*3/2;
          }
        mag+=1.0;
        }
      if((jj=((zi_x-WIDTH_TEXT*3)-(width_horiz+WIDTH_TEXT/2))/((int)mag-1))<0)
        jj=0;

      xi=width_dpy-MARGIN;
    /* UD */
      i=0;
      put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
        size_sym[i][2],size_sym[i][2],&dpy,
        xi-size_sym[i][3],yi-size_sym[i][3],BF_SDO);
      put_text(&dpy,xi-WIDTH_TEXT+HW,yi+length_vert/4,"UD",BF_SDO);
      xi-=WIDTH_TEXT*2+jj;
    /* ordinary M */
      mag=0.0;
      for(;;)
        {
        if(map_true) radi=(int)(pixels_per_km*MAG2RAD(mag));
        else if((radi=MAG2RAD2(mag))<=0) radi=1;

        if(xi-radi<width_horiz || yi-radi<MARGIN+height_horiz) break;
        if(radi>size_sym[N_SYM_USE-1][4])
          {
          draw_circle(xi,yi,radi,LPTN_FF,BF_SDO,&dpy);
          zi_x=xi;
          if(radi*2>WIDTH_TEXT*3/2) xi-=radi*2+jj;
          else xi-=WIDTH_TEXT*3/2+jj;
          if(length_vert/4<radi) zi_y=yi+radi;
          else zi_y=yi+length_vert/4;
          }
        else
          {
          for(i=1;i<N_SYM-2;i++) if(radi<=size_sym[i][4]) break;
          put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
            size_sym[i][2],size_sym[i][2],&dpy,
            xi-size_sym[i][3],yi-size_sym[i][3],BF_SDO);
          zi_x=xi;
          if(size_sym[i][3]*3>WIDTH_TEXT*3/2) xi-=size_sym[i][3]*3+jj;
          else xi-=WIDTH_TEXT*3/2+jj;
          zi_y=yi+length_vert/4;
          }
        sprintf(textbuf,"%d",(int)mag);
        put_text(&dpy,zi_x-WIDTH_TEXT/2,zi_y,textbuf,BF_SDO);
        mag+=1.0;
        }
      if((zi_x=zi_x-WIDTH_TEXT*3)<width_horiz+WIDTH_TEXT/2)
        zi_x=width_horiz+WIDTH_TEXT/2;
      put_text(&dpy,zi_x,zi_y,"M",BF_SDO);
      }

    /* if no temporary hypocenters file exists, make it */ 
    if(read_hypo || (fp_othrs=fopen(ft.othrs_file,"r"))==NULL)
      {
      t1=t1a=time2long(124,12,31,23,59);
      t2=t2a=0;
      n_hypo=0;
      if(first_map_others)  /* i.e. for the first time */
        {
        if(map_vert==0)
          {
          sel.dep1=depsteps[sel.dep1_idx=0];
          sel.dep2=depsteps[sel.dep2_idx=
            sizeof(depsteps)/sizeof(*depsteps)-1];
          }
        sel.mag1=magsteps[sel.mag1_idx=0];
        sel.mag2=magsteps[sel.mag2_idx=
          sizeof(magsteps)/sizeof(*magsteps)-1];
        }
      if(*ft.final_opt)
        {
        strcpy(final_dir,ft.final_opt);
        i=1;
        }
      else i=read_parameter(PARAM_FINAL,final_dir);
      if(i==0 || (((dir_ptr=opendir(final_dir))==NULL) &&
		  ((fp=fopen(final_dir,"r"))==NULL)))
        {
        /* (1) read hypocenter data from 'PICK' directory */
        if((dir_ptr=opendir(ft.hypo_dir))==NULL)
          fprintf(stderr,"directory '%s' not open\007\007\n",ft.hypo_dir);
        else
          {
          fp_othrs=fopen(ft.othrs_file,"w+");
          while((dir_ent=readdir(dir_ptr))!=NULL)
            {
            if(*dir_ent->d_name=='.') continue;
            sprintf(filename,"%s/%s",ft.hypo_dir,dir_ent->d_name);
            if((fp=fopen(filename,"r"))!=NULL)
              {
              i=1;
              while(fgets(textbuf,LINELEN,fp)!=NULL)
                {
                if(i && strncmp(textbuf,"#p",2)==0)
                  {
                  *tbuf1=0;
                  sscanf(textbuf+3,"%*s%s",tbuf1);
                  for(ptr=tbuf1;*ptr;ptr++) *ptr=toupper(*ptr);
                  if(*tbuf1 && strcmp(tbuf1,"BLAST")==0) hypo.blast=1;
                  else hypo.blast=0;                  
                  i=0;
                  }
                if(strncmp(textbuf,"#f",2)==0)
                  {
                  fstat(fileno(fp),&st_buf);
                  ptr=getname(st_buf.st_uid);
                  if(ptr) strncpy(hypo.o,ptr,4);
                  else *hypo.o=0;
                  sscanf(textbuf+3,"%d%d%d%d%d%lf%lf%lf%f%f",
                    &ye,&mo,&da,&ho,&mi,&se,&alat,&along,&hypo.d,&mag);
                  pltxy(alat0,along0,&alat,&along,&xd,&yd,0);
                  hypo.lat=(float)alat;
                  hypo.lon=(float)along;
                  hypo.x=(float)xd;
                  hypo.y=(float)yd;
                  if(mag>=0.0) hypo.m=(int)(mag*10.0+0.5);
                  else hypo.m=(int)(mag*10.0-0.5);
                  hypo.t=time2long(ye,mo,da,ho,mi);
                  while(se<0.0) {hypo.t--;se+=60.0;}
                  hypo.s=(int)se;
                  hypo.ss=(int)(se*10.0+0.5)%10;
                  if(hypo.t<t1) put_time1(1440,hypo.t,
                    &t1,&t1a,ye,mo,da,ho,mi,textbuf);
                  if(hypo.t>t2) put_time2(1440,hypo.t,
                    &t2,&t2a,ye,mo,da,ho,mi,textbuf);
                  fwrite(&hypo,sizeof(hypo),1,fp_othrs);
                  n_hypo++;
                  break;
                  }
                }
              fclose(fp);
              }
            }
          fclose(fp_othrs);
          closedir(dir_ptr);
          bell();
          }
        }
      else
        {
        /* (2) read hypocenter data from 'FINAL' directory */
        fp_othrs=fopen(ft.othrs_file,"w+");
        if(dir_ptr==NULL) goto is_a_file;
        while((dir_ent=readdir(dir_ptr))!=NULL)
          {
          if(*dir_ent->d_name=='.') continue;
          sprintf(filename,"%s/%s",final_dir,dir_ent->d_name);
          if((fp=fopen(filename,"r"))!=NULL)
            {
is_a_file:  fread(textbuf,1,20,fp);
            for(i=0;i<20;i++) if(!isprint(textbuf[i])) break;
            fseek(fp,0,0);
            if(i==20) while(fgets(textbuf,LINELEN,fp)!=NULL)
              { /* ascii format file */
              *tbuf1=(*tbuf2)=0;
              sscanf(textbuf,"%d%d%d%d%d%lf%lf%lf%f%f%s%s",
                &ye,&mo,&da,&ho,&mi,&se,&alat,&along,&hypo.d,&mag,tbuf1,tbuf2);
              strncpy(hypo.o,tbuf1,4);
              for(ptr=tbuf2;*ptr;ptr++) *ptr=toupper(*ptr);
              if(*tbuf2 && strcmp(tbuf2,"BLAST")==0) hypo.blast=1;
              else hypo.blast=0;                  
              pltxy(alat0,along0,&alat,&along,&xd,&yd,0);
              hypo.lat=(float)alat;
              hypo.lon=(float)along;
              hypo.x=(float)xd;
              hypo.y=(float)yd;
              if(mag>=0.0) hypo.m=(int)(mag*10.0+0.5);
              else hypo.m=(int)(mag*10.0-0.5);
              hypo.t=time2long(ye,mo,da,ho,mi);
              while(se<0.0) {hypo.t--;se+=60.0;}
              hypo.s=(int)se;
              hypo.ss=(int)(se*10.0+0.5)%10;
              if(hypo.t<t1) put_time1(7200,hypo.t,
                &t1,&t1a,ye,mo,da,ho,mi,textbuf);
              if(hypo.t>t2) put_time2(7200,hypo.t,
                &t2,&t2a,ye,mo,da,ho,mi,textbuf);
              fwrite(&hypo,sizeof(hypo),1,fp_othrs);
              n_hypo++;
              }
            /* else while(fread(&hypob,sizeof(hypob),1,fp)>0) */
            else while(FinalB_read(&hypob,fp)==FinalB_SIZE)
              { /* binary format file */
              /* swp=(union Swp *)&hypob.alat; */
/*               swp->i=(swp->c[0]<<24)+(swp->c[1]<<16)+(swp->c[2]<<8)+swp->c[3]; */
/*               swp=(union Swp *)&hypob.along; */
/*               swp->i=(swp->c[0]<<24)+(swp->c[1]<<16)+(swp->c[2]<<8)+swp->c[3]; */
/*               swp=(union Swp *)&hypob.dep; */
/*               swp->i=(swp->c[0]<<24)+(swp->c[1]<<16)+(swp->c[2]<<8)+swp->c[3]; */
              strncpy(hypo.o,hypob.owner,4);
              for(ptr=hypob.diag;ptr<hypob.diag+4;ptr++) *ptr=toupper(*ptr);
              if(strncmp(hypob.diag,"BLAST",4)==0) hypo.blast=1;
              else hypo.blast=0;
              alat=(double)(hypo.lat=hypob.alat);
              along=(double)(hypo.lon=hypob.along);
              pltxy(alat0,along0,&alat,&along,&xd,&yd,0);
              hypo.x=(float)xd;
              hypo.y=(float)yd;
              hypo.d=hypob.dep;
              hypo.m=hypob.time[7];
              hypo.t=time2long(hypob.time[0],hypob.time[1],
                hypob.time[2],hypob.time[3],hypob.time[4]);
              se=(double)hypob.time[5]+0.1*(double)hypob.time[6];
              while(se<0.0) {hypo.t--;se+=60.0;}
              hypo.s=(int)se;
              hypo.ss=(int)(se*10.0+0.5)%10;
              if(hypo.t<t1) put_time1(7200,hypo.t,&t1,&t1a,
                hypob.time[0],hypob.time[1],hypob.time[2],
                hypob.time[3],hypob.time[4],textbuf);
              if(hypo.t>t2) put_time2(7200,hypo.t,&t2,&t2a,
                hypob.time[0],hypob.time[1],hypob.time[2],
                hypob.time[3],hypob.time[4],textbuf);
              fwrite(&hypo,sizeof(hypo),1,fp_othrs);
              n_hypo++;
              }
            fclose(fp);
            if(dir_ptr==NULL) break;
            }
          }
        fclose(fp_othrs);
        if(dir_ptr) closedir(dir_ptr);
        bell();
        }

      if(t1<t2) /* having read data */
        {
        long2time(&sel.time1_save,&t1);
        if(first_map_others) sel.time1=sel.time1_save;
        long2time(&sel.time2_save,&t2);
        if(first_map_others || map_update) sel.time2=sel.time2_save;
        }

      if(first_map_others || map_update)
        {
        get_time_win(&sel.time2,0);
        if(map_interval) map_period=map_period_save;
        if(map_period_unit=='h') i=map_period*3600;
        else i=map_period*(3600*24);
        if(map_period) get_time_win(&sel.time1,-i);
        }
      fp_othrs=open_file(ft.othrs_file,"hypocenters");
      read_hypo=0;
      }

    if(map_all)
      {
      sel.time1=sel.time1_save;
      sel.time2=sel.time2_save;
      map_all=0;
      }

    sel.t1=time2long(sel.time1.ye,sel.time1.mo,sel.time1.da,
      sel.time1.ho,sel.time1.mi);
    sel.t2=time2long(sel.time2.ye,sel.time2.mo,sel.time2.da,
      sel.time2.ho,sel.time2.mi);
    first_map_others=map_all=0;

    if(map_mode==MODE_TS2)  /* draw frames for TIME-SPACE */
      {
      put_white(&dpy,map_f2x,map_f1y-MARGIN*3/2,width_dpy-map_f2x-1,
        map_f2y-map_f1y+MARGIN*3);
      draw_rect(map_f1x,map_f1y,map_f2x,map_f2y,LPTN_FF,BF_S,&dpy);
      draw_rect(map_f1x-1,map_f1y-1,map_f2x+1,map_f2y+1,LPTN_FF,BF_S,&dpy);
      draw_rect(map_f2x+MARGIN*3/2,map_f1y,width_dpy-1,map_f2y,LPTN_FF,BF_S,&dpy);
      if(map_vert)
        {
        put_white(&dpy,map_f2x,MARGIN+height_horiz,
          width_dpy-map_f2x,length_vert+MARGIN);
        draw_rect(map_f1x,MARGIN*2+height_horiz,map_f2x,
          MARGIN*2+height_horiz+length_vert-1,LPTN_FF,BF_S,&dpy);
        draw_rect(map_f1x-1,MARGIN*2+height_horiz,map_f2x+1,
          MARGIN*2+height_horiz+length_vert-1,LPTN_FF,BF_S,&dpy);
        draw_rect(map_f2x+MARGIN*3/2,MARGIN*2+height_horiz,width_dpy-1,
          MARGIN*2+height_horiz+length_vert-1,LPTN_FF,BF_S,&dpy);
        if(sel.dep1_idx==0) i=0;
        else i=(int)sel.dep1;
        draw_ticks(1,map_f2x+MARGIN*3/2,MARGIN*2+height_horiz,
          length_vert,i,(int)sel.dep2, 1);
        draw_ticks(1,width_dpy-1,MARGIN*2+height_horiz,
          length_vert,i,(int)sel.dep2,-1);
        sprintf(textbuf,"%3d",i);
        put_text(&dpy,map_f2x+HW,MARGIN*2+height_horiz-HEIGHT_TEXT/2,
          textbuf,BF_SDO);
        sprintf(textbuf,"%3d",(int)sel.dep2);
        put_text(&dpy,map_f2x+HW,height_dpy-MARGIN-HEIGHT_TEXT,textbuf,BF_SDO);
        }
    /* draw ticks */
      i=sel.t2-sel.t1;
      j=width_dpy-map_f2x-MARGIN*3/2;
      it=1;
      long2time(&tim1,&sel.t1);
      if(j/i>TICKL)  /* 1m */
        {
        jj=sel.t1;
        if((it=draw_ticks_ts(j,jj,it,1,4))>2) goto tsjump;
        }
      if(j/(i/=10)>TICKL) /* 10m */
        {
        /* jj : int. must be check */
        jj=time2long(tim1.ye,tim1.mo,tim1.da,tim1.ho,tim1.mi/10*10);
        if((it=draw_ticks_ts(j,jj,it,10,4))>2) goto tsjump;
        }
      if(j/(i/=6)>TICKL) /* 1h */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,tim1.ho,0);
        if((it=draw_ticks_ts(j,jj,it,60,3))>2) goto tsjump;
        }
      if(j/(i/=6)>TICKL) /* 6h */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,tim1.ho/6*6,0);
        if((it=draw_ticks_ts(j,jj,it,60*6,3))>2) goto tsjump;
        }
      istep=60*24;
      if(j/(i/=4)>TICKL) /* 1d */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,0,0);
        if((it=draw_ticks_ts(j,jj,it,istep,2))>2) goto tsjump;
        }
      if(j/(i/=5)>TICKL) /* 5d */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,0,0);
        if(it==2)
          {
          sprintf(textbuf,"%d",t=tim1.mo);
          put_text(&dpy,map_f2x+MARGIN*3/2-strlen(textbuf)*WIDTH_TEXT/2,
            map_f1y-HEIGHT_TEXT-Y_LINE1,textbuf,BF_SDO);
          }
        if(jj<sel.t1) jj+=istep;
        while(jj<=sel.t2)
          {
          long2time((struct YMDhms *)tim,(long *)&jj);
          if(tim[2]%5==1) t=draw_ticks_ts2(j,jj,it,2,tim,t);
          jj+=istep;
          }
        if(++it>2) goto tsjump;
        }
      if(j/(i/=3)>TICKL) /* 1mon */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,0,0);
        if(it==2)
          {
          sprintf(textbuf,"%d",t=tim1.ye);
          put_text(&dpy,map_f2x+MARGIN*3/2-strlen(textbuf)*WIDTH_TEXT/2,
            map_f1y-HEIGHT_TEXT-Y_LINE1,textbuf,BF_SDO);
          }
        if(jj<sel.t1) jj+=istep;
        while(jj<=sel.t2)
          {
          long2time((struct YMDhms *)tim,(long *)&jj);
          if(tim[2]==1) t=draw_ticks_ts2(j,jj,it,1,tim,t);
          jj+=istep;
          }
        if(++it>2) goto tsjump;
        }
      if(j/(i/=3)>TICKL) /* 3mon */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,0,0);
        if(it==2)
          {
          sprintf(textbuf,"%d",t=tim1.ye);
          put_text(&dpy,map_f2x+MARGIN*3/2-strlen(textbuf)*WIDTH_TEXT/2,
            map_f1y-HEIGHT_TEXT-Y_LINE1,textbuf,BF_SDO);
          }
        if(jj<sel.t1) jj+=istep;
        while(jj<=sel.t2)
          {
          long2time((struct YMDhms *)tim,(long *)&jj);
          if(tim[2]==1 && tim[1]%3==1)
            t=draw_ticks_ts2(j,jj,it,1,tim,t);
          jj+=istep;
          }
        if(++it>2) goto tsjump;
        }
      if(j/(i/=4)>TICKL) /* 1y */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,0,0);
        if(jj<sel.t1) jj+=istep;
        while(jj<=sel.t2)
          {
          long2time((struct YMDhms *)tim,(long *)&jj);
          if(tim[1]==1 && tim[2]==1)
            t=draw_ticks_ts2(j,jj,it,0,tim,t);
          jj+=istep;
          }
        if(++it>2) goto tsjump;
        }
      else /* 5y */
        {
        jj=time2long(tim1.ye,tim1.mo,tim1.da,0,0);
        if(jj<sel.t1) jj+=istep;
        while(jj<=sel.t2)
          {
          long2time((struct YMDhms *)tim,(long *)&jj);
          if(tim[1]==1 && tim[2]==1 && tim[0]%5==0)
            t=draw_ticks_ts2(j,jj,it,0,tim,t);
          jj+=istep;
          }
        if(++it>2) goto tsjump;
        }
tsjump: map_mode=MODE_TS3;
      }
    else if(map_mode==MODE_TS3) map_mode=MODE_NORMAL;

    /* temporary hypo data file is already opened */
    jj=0; /* N of etq. */
    if(fp_othrs!=NULL)
      {
      for(j=0;j<n_hypo;j++)
        {
        if(fread(&hypo,sizeof(hypo),1,fp_othrs)==0)
          {
          fclose(fp_othrs);
          break;
          }
        mag=0.1*(float)hypo.m;
        if(km2pixel(conv,xzero,yzero,x1,y1,x2,y2,x_cent,y_cent,
          (double)hypo.x,(double)hypo.y,&xi,&yi,cs,sn)) continue;
       /* select */
        if(!(hypo.t>=sel.t1 && hypo.t<=sel.t2)) continue;
        if(!(hypo.d>=sel.dep1 && hypo.d<sel.dep2)) continue;
        if(!(mag>=sel.mag1 && mag<sel.mag2))
          if(!(sel.mag_ud && mag>9.8)) continue;
        if(*sel.o && *hypo.o)
          {
          if(*sel.o=='-')
            {
            if(strncmp(sel.o+1,hypo.o,4)==0) continue;
            }
          else if(strncmp(sel.o,hypo.o,4)) continue;
          }
        if(sel.no_blast && hypo.blast) continue;
        if(map_vert)
          {
          zi_x=width_horiz+MARGIN+(int)((hypo.d-dep_base)*pixels_per_km);
          zi_y=height_horiz+MARGIN*2+(int)((hypo.d-dep_base)*pixels_per_km);
          if(zi_x<width_horiz) continue;
          }
        if(map_mode==MODE_TS3)
          {
          if(map_f1x<xi && xi<map_f2x && map_f1y<yi && yi<map_f2y)
            {
            xt=map_f2x+MARGIN*3/2+(int)((double)
              (width_dpy-map_f2x-MARGIN*3/2)*(double)
              (hypo.t-sel.t1)/(double)(sel.t2-sel.t1));
            yt=1;
            }
          else yt=0;
          }
        else yt=0;
        if(mag>9.8)
          {
          i=0;
          x=xi-size_sym[i][3];
          y=yi-size_sym[i][3];
          if(map_mode==MODE_FIND2) phypo(xi,yi,&hypo);
          else if(yt)
            put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
              size_sym[i][2],size_sym[i][2],&dpy,xt-size_sym[i][3],y,BF_SDO);
          if(map_mode==MODE_NORMAL || yt)
            put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
              size_sym[i][2],size_sym[i][2],&dpy,x,y,BF_SDO);
          if(map_vert)
            {
            x=xi-size_sym[i][3];
            y=zi_y-size_sym[i][3];
            if(map_mode==MODE_FIND2) phypo(xi,zi_y,&hypo);
            else if(yt)
              put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                size_sym[i][2],size_sym[i][2],&dpy,xt-size_sym[i][3],y,BF_SDO);
            if(map_mode==MODE_NORMAL || yt)
              put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                size_sym[i][2],size_sym[i][2],&dpy,x,y,BF_SDO);
            x=zi_x-size_sym[i][3];
            y=yi-size_sym[i][3];
            if(map_mode==MODE_FIND2) phypo(zi_x,yi,&hypo);
            else if(map_mode==MODE_NORMAL)
              put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                size_sym[i][2],size_sym[i][2],&dpy,x,y,BF_SDO);
            }
          }
        else
          {
          if(map_true) radi=(int)(pixels_per_km*MAG2RAD(mag));
          else if((radi=MAG2RAD2(mag))<=0) radi=1;
/*        if(radi>size_sym[N_SYM-1][4])*/
          if(radi>size_sym[4][4])
            {
            if(map_mode==MODE_FIND2) phypo(xi,yi,&hypo);
            else if(yt) draw_circle(xt,yi,radi,LPTN_FF,BF_SDO,&dpy);
            if(map_mode==MODE_NORMAL || yt)
              draw_circle(xi,yi,radi,LPTN_FF,BF_SDO,&dpy);
            if(map_vert)
              {
              if(map_mode==MODE_FIND2) phypo(xi,zi_y,&hypo);
              else if(yt) draw_circle(xt,zi_y,radi,LPTN_FF,BF_SDO,&dpy);
              if(map_mode==MODE_NORMAL || yt)
                draw_circle(xi,zi_y,radi,LPTN_FF,BF_SDO,&dpy);
              if(map_mode==MODE_FIND2) phypo(zi_x,yi,&hypo);
              else if(map_mode==MODE_NORMAL)
              draw_circle(zi_x,yi,radi,LPTN_FF,BF_SDO,&dpy);
              }
            }
          else
            {
            for(i=1;i<N_SYM-2;i++) if(radi<=size_sym[i][4]) break;
            x=xi-size_sym[i][3];
            y=yi-size_sym[i][3];
            if(map_mode==MODE_FIND2) phypo(xi,yi,&hypo);
            else if(yt)
              put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                size_sym[i][2],size_sym[i][2],&dpy,xt-size_sym[i][3],y,BF_SDO);
            if(map_mode==MODE_NORMAL || yt)
              put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                size_sym[i][2],size_sym[i][2],&dpy,x,y,BF_SDO);
            if(map_vert)
              {
              x=xi-size_sym[i][3];
              y=zi_y-size_sym[i][3];
              if(map_mode==MODE_FIND2) phypo(xi,zi_y,&hypo);
              else if(yt)
                put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                  size_sym[i][2],size_sym[i][2],&dpy,xt-size_sym[i][3],y,BF_SDO);
              if(map_mode==MODE_NORMAL || yt)
                put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                  size_sym[i][2],size_sym[i][2],&dpy,x,y,BF_SDO);
              x=zi_x-size_sym[i][3];
              y=yi-size_sym[i][3];
              if(map_mode==MODE_FIND2) phypo(zi_x,yi,&hypo);
              else if(map_mode==MODE_NORMAL)
                     put_bitblt(&sym,size_sym[i][0],size_sym[i][1],
                       size_sym[i][2],size_sym[i][2],&dpy,x,y,BF_SDO);
              }
            }
          }
        if(map_mode!=MODE_TS3) jj++;
        else if(yt) jj++;
        }
      fclose(fp_othrs);
      }
    put_white(&dpy,0,height_dpy-MARGIN,width_dpy,MARGIN);

    /* 0-4 */
    if(*sel.o) sprintf(textbuf,"%-5.5s",sel.o);
    else sprintf(textbuf," ALL ");
    put_text(&dpy,0,height_dpy-MARGIN+Y_LINE1,textbuf,BF_SI);
    /* 6-36 */
    sprintf(textbuf,
      "%02d/%02d/%02d %02d:%02d - %02d/%02d/%02d %02d:%02d",
      sel.time1.ye,sel.time1.mo,sel.time1.da,sel.time1.ho,sel.time1.mi,
      sel.time2.ye,sel.time2.mo,sel.time2.da,sel.time2.ho,sel.time2.mi);
    put_text(&dpy,WIDTH_TEXT*6-HW,height_dpy-MARGIN+Y_LINE1," ",BF_SI);
    put_text(&dpy,WIDTH_TEXT*6,height_dpy-MARGIN+Y_LINE1,textbuf,BF_SI);

    /* 53-69 */
    if(sel.mag1_idx==0) sprintf(textbuf,"    < M <");
    else sprintf(textbuf,"%4.1f< M <",sel.mag1);
    if(sel.mag2_idx==sizeof(magsteps)/sizeof(*magsteps)-1)
      sprintf(textbuf+strlen(textbuf),"     ");
    else sprintf(textbuf+strlen(textbuf),"%4.1f ",sel.mag2);
    if(sel.mag_ud) sprintf(textbuf+strlen(textbuf),"+UD");
    else sprintf(textbuf+strlen(textbuf),"-UD");
    put_text(&dpy,WIDTH_TEXT*53-HW,height_dpy-MARGIN+Y_LINE1," ",BF_SI);
    put_text(&dpy,WIDTH_TEXT*53,height_dpy-MARGIN+Y_LINE1,textbuf,BF_SI);
    put_text(&dpy,WIDTH_TEXT*57,height_dpy-MARGIN+Y_LINE1,"_",BF_SIDA);

    /* 71-73 */
    if(sel.no_blast) sprintf(textbuf,"-BL");
    else sprintf(textbuf,"+BL");
    put_text(&dpy,WIDTH_TEXT*71-HW,height_dpy-MARGIN+Y_LINE1," ",BF_SI);
    put_text(&dpy,WIDTH_TEXT*71,height_dpy-MARGIN+Y_LINE1,textbuf,BF_SI);
    }

  if(other_epis || map_vert)
    {
    /* 38-51 */
    if(sel.dep1_idx==0) sprintf(textbuf,"   < H <");
    else sprintf(textbuf,"%3d< H <",(int)sel.dep1);
    if(sel.dep2==depsteps[sizeof(depsteps)/sizeof(*depsteps)-1])
      sprintf(textbuf+strlen(textbuf),"    km");
    else sprintf(textbuf+strlen(textbuf),"%3d km",(int)sel.dep2);
    put_text(&dpy,WIDTH_TEXT*38-HW,height_dpy-MARGIN+Y_LINE1," ",BF_SI);
    put_text(&dpy,WIDTH_TEXT*38,height_dpy-MARGIN+Y_LINE1,textbuf,BF_SI);
    put_text(&dpy,WIDTH_TEXT*41,height_dpy-MARGIN+Y_LINE1,"_",BF_SIDA);
    }

  put_white(&dpy,0,0,width_dpy,MARGIN);
  if(flag_hypo)
    {
    put_text(&dpy,0,Y_LINE1,ft.hypo.textbuf,BF_SI);
    pltxy(alat0,along0,&ft.hypo.alat,&ft.hypo.along,&xd,&yd,0);
    if(map_ellipse && ft.hypo.ellipse)
      {
      km2pixel(conv,xzero,yzero,x1,y1,x2,y2,x_cent,y_cent,xd,yd,&xi,&yi,cs,sn);
      /* rotate error matrix */
      get_mat(cs,-sn,mat_r);
      get_mat(cs,sn,mat_ri);
      mat_copy(mat_error,ft.hypo.c);      /* H */
      mat_mul(mat_error,mat_r,mat_error);   /* RH */
      mat_mul(mat_error,mat_error,mat_ri);  /* RHR-1 */
      MAT2ELLIPSE(0,1);   /* X-Y  ->  s1,s2,roh */
      draw_ellipse(xi,yi,s1,s2,roh,LPTN_FF,BF_SDO,&dpy,x1,x2,y1,y2);
      if(map_vert)
        {
        zi_x=width_horiz+MARGIN+(int)((ft.hypo.dep-dep_base)*pixels_per_km);
        zi_y=height_horiz+MARGIN*2+(int)((ft.hypo.dep-dep_base)*pixels_per_km);
        MAT2ELLIPSE(0,2); /* X-Z  ->  s1,s2,roh */
        draw_ellipse(xi,zi_y,s1,s2,roh,LPTN_FF,BF_SDO,&dpy,x1,x2,y2,height_dpy);
        MAT2ELLIPSE(2,1); /* Z-Y  ->  s1,s2,roh */
        draw_ellipse(zi_x,yi,s1,s2,roh,LPTN_FF,BF_SDO,&dpy,x2,width_dpy,y1,y2);
        }
      }
    else if(km2pixel(conv,xzero,yzero,x1,y1,x2,y2,x_cent,y_cent,
        xd,yd,&xi,&yi,cs,sn)==0)
      {
      if(map_ellipse) bell();
      put_bitblt(&epi_l,0,0,16,16,&dpy,xi-7,yi-7,BF_SDO);
      if(map_vert && !(ft.hypo.dep<sel.dep1 || ft.hypo.dep>sel.dep2))
        {
        zi_x=width_horiz+MARGIN+(int)((ft.hypo.dep-dep_base)*pixels_per_km);
        zi_y=height_horiz+MARGIN*2+(int)((ft.hypo.dep-dep_base)*pixels_per_km);
        put_bitblt(&epi_l,0,0,16,16,&dpy,xi-7,zi_y-7,BF_SDO);
        put_bitblt(&epi_l,0,0,16,16,&dpy,zi_x-7,yi-7,BF_SDO);
        }
      }
    }

  if(other_epis)
    {
    /* 75- */
    sprintf(mapbuf," N= %ld ",jj);
    put_text(&dpy,WIDTH_TEXT*75,height_dpy-MARGIN+Y_LINE1,mapbuf,BF_SI);
    }
  put_function_map();
  return (0);
  }

static void
init_map(int idx)
  {

  map_mode=MODE_NORMAL;
  if(!map_only) put_reverse(&dpy,x_func(MAP),0,WB,MARGIN);
  if(first_map)
    {
    if(flag_hypo)
      {
      lat_cent=ft.hypo.alat;
      lon_cent=ft.hypo.along;
      pixels_per_km=pdpi/2.54/(mapsteps[ppk_idx=PPK_HYPO]*0.1);
      }
    else pixels_per_km=pdpi/2.54/(mapsteps[ppk_idx=PPK_INIT]*0.1);
    }
  switch(idx)
    {
    case MSE_BUTNL: break;
    case MSE_BUTNM: map_vert=1;break;
    case MSE_BUTNR: map_vert=map_ellipse=1;break;
    }
  refresh(0);
  first_map=0;
  }

static int
check(int code, int *ptr, int low, int high)
  {
  int now;

  now=(*ptr);
  switch(code)
    {
    case MSE_BUTNL: if(now<high) now++;else now=low;break;
    case MSE_BUTNM: if(now==low) now=high;else now=low;break;
    case MSE_BUTNR: if(now>low) now--;else now=high;break;
    }
  if(now==(*ptr)) return (0);
  else {*ptr=now;return (1);} 
  }

static int
check_year(int code, int *ptr, int low, int high)
  {
  int now;

  now=(*ptr);
/* this code is vaild from 1925 to 2024 */
  if(now<25) now+=100;
  if(low<25) low+=100;
  if(high<25) high+=100;
  switch(code)
    {   
    case MSE_BUTNL: if(now<high) now++;else now=low;break;
    case MSE_BUTNM: if(now==low) now=high;else now=low;break;
    case MSE_BUTNR: if(now>low) now--;else now=high;break;
    }
  if(now>99) now-=100;
  if(now==(*ptr)) return (0);
  else {*ptr=now;return (1);}
  }

static void
proc_map(void)
  {
  double alat,along,xd,yd,x_cent,y_cent,cs,sn,arg,ala,alo;
  int i,j,x,y,xzero,yzero,plot,ring_bell;
  static char textbuf[LINELEN],textbuf1[LINELEN],textbuf2[LINELEN];
  char ulat,ulon;
  struct YMDhms tm;

  arg=cs=sn=0.0; /* for supress warning */
  if(map_dir)
    {
    arg=PI*(double)map_dir/180.0;
    cs=cos(arg);
    sn=sin(arg);
    }
  x=event.mse_data.md_x;
  y=event.mse_data.md_y;
  xzero=width_horiz/2;
  yzero=height_horiz/2+MARGIN;
  pltxy(alat0,along0,&lat_cent,&lon_cent,&x_cent,&y_cent,0);
  if(event.mse_trig==MSE_MOTION)
    {
    strcpy(textbuf,"                  ");
    if(map_mode==MODE_NORMAL) strcpy(textbuf1,mapbuf);
    strcpy(textbuf2,ft.hypo.textbuf);
    if(map_mode==MODE_FIND2 || map_mode==MODE_TS2)
      {
      /* echo rubber band */
      draw_rect(map_f1x,map_f1y,map_f2x,map_f2y,LPTN_FF,BF_SDX,&dpy);
      draw_rect(map_f1x,map_f1y,x,y,LPTN_FF,BF_SDX,&dpy);
      map_f2x=x;map_f2y=y;  /* reserve */
      }
    if(y>=MARGIN && y<=height_dpy-MARGIN)
      {
      if(map_mode==MODE_TS3 && x>=map_f2x+MARGIN*3/2 &&
          ((y>map_f1y && y<map_f2y) || y>MARGIN*2+height_horiz))
        {
        i=sel.t1+(int)((double)(x-(map_f2x+MARGIN*3/2))*(double)(sel.t2-sel.t1)
          /(double)(width_dpy-map_f2x-MARGIN*3/2));
        long2time(&tm,(long *)&i);
        sprintf(textbuf,"  %02d/%02d/%02d %02d:%02d  ",
          tm.ye,tm.mo,tm.da,tm.ho,tm.mi);
        }
      else if(x<width_horiz && y<MARGIN+height_horiz)
        {
        if(map_dir)
          {
          xd=XINV(x,y)/pixels_per_km+x_cent;
          yd=(-YINV(x,y))/pixels_per_km+y_cent;
          }
        else
          {
          xd=((double)(x-xzero))/pixels_per_km+x_cent;
          yd=(-(double)(y-yzero))/pixels_per_km+y_cent;
          }
        pltxy(alat0,along0,&alat,&along,&xd,&yd,1);
        ala=alat;
        alo=along;
        if(ala<0.0) {ulat='S';ala=(-ala);}
        else ulat='N';
        if(alo<0.0) {ulon='W';alo=(-alo);}
        else ulon='E';
        sprintf(textbuf,"%7.4f%c %8.4f%c",ala,ulat,alo,ulon);
        }
      else if(x>=width_horiz && y<MARGIN+height_horiz)
        {
        if(sel.dep1_idx==0)
          {
          xd=(double)(x-(width_horiz+MARGIN))/pixels_per_km;
          sprintf(textbuf,"    %7.2f km    ",xd);
          }
        else
          {
          xd=(double)(x-(width_horiz+MARGIN))/pixels_per_km+sel.dep1;
          if(xd>=sel.dep1) sprintf(textbuf,"    %7.2f km    ",xd);
          }
        }
      else if(x<width_horiz && y>=MARGIN+height_horiz)
        {
        if(sel.dep1_idx==0)
          {
          yd=(double)(y-(height_horiz+MARGIN*2))/pixels_per_km;
          sprintf(textbuf,"    %7.2f km    ",yd);
          }
        else
          {
          yd=(double)(y-(height_horiz+MARGIN*2))/pixels_per_km+sel.dep1;
          if(yd>=sel.dep1) sprintf(textbuf,"    %7.2f km    ",yd);
          }
        }
      }
    else  /* function area */
      {
      if(map_mode==MODE_NORMAL && other_epis &&
          y>height_dpy-MARGIN && WIDTH_TEXT*75<=x &&
          x<=WIDTH_TEXT*(75+(i=strlen(mapbuf))))
        { /* FIND */
        strcpy(textbuf1," FIND ");
        while(i>strlen(textbuf1)) strcat(textbuf1," ");
        }
      else if(map_mode==MODE_NORMAL && flag_hypo && y<MARGIN &&
           x<(i=strlen(ft.hypo.textbuf))*WIDTH_TEXT)
        { /* ellipse */
        strcpy(textbuf2," ERROR ELLIPSOID ");
        while(i>strlen(textbuf2)) strcat(textbuf2," ");
        }
      ala=lat_cent;
      alo=lon_cent;
      if(ala<0.0) {ulat='S';ala=(-ala);}
      else ulat='N';
      if(alo<0.0) {ulon='W';alo=(-alo);}
      else ulon='E';
      sprintf(textbuf,"%7.4f%c %8.4f%c",ala,ulat,alo,ulon);
      }
    if(flag_hypo)
      {
      put_text(&dpy,0,Y_LINE1,textbuf2,BF_SI);
      i=WIDTH_TEXT*strlen(textbuf2)+HW;
      }
    else i=0;
    put_text(&dpy,i,Y_LINE1,textbuf,BF_SI);
    if(other_epis)
      {
      if(map_mode==MODE_NORMAL)
	put_text(&dpy,WIDTH_TEXT*75,height_dpy-MARGIN+Y_LINE1,textbuf1,BF_SI);
      else if(map_mode==MODE_FIND1 || map_mode==MODE_FIND2)
        put_text(&dpy,WIDTH_TEXT*75,height_dpy-MARGIN+Y_LINE1,textbuf1,BF_S);
      }
    }
  else if(event.mse_trig==MSE_BUTTON && event.mse_dir==MSE_DOWN)
    {
    plot=ring_bell=0;
    if(map_mode==MODE_FIND1 || map_mode==MODE_TS1)
      {
      map_f1x=map_f2x=x;
      map_f1y=map_f2y=y;
      if(map_mode==MODE_FIND1 && y>MARGIN && y<height_dpy-MARGIN)
        map_mode=MODE_FIND2;
      else if(map_mode==MODE_TS1 && y>MARGIN && x<width_horiz &&
             y<MARGIN+height_horiz) map_mode=MODE_TS2;
      else
        {
        if(map_mode==MODE_TS1) put_func(func_map[TMSP],TMSP,0,0,0);
        ring_bell=1;
        map_mode=MODE_NORMAL;
        }
      }
    else if(y<MARGIN)
      {
      ring_bell=1;
      switch(get_func(x))
        {
        case RETN:
          if(map_only) end_process(0);
          loop=loop_stack[--loop_stack_ptr];
          if(!background) refresh(0);    
          return;
        case RFSH:
            switch(event.mse_code)
              {
              case MSE_BUTNR: if(other_epis) read_hypo=1;
                      break;
              case MSE_BUTNM: if(other_epis) map_all=1;
              case MSE_BUTNL: break;
              }
            plot=1;
            break;
        case VERT:
          if(map_vert) map_vert=0;
          else
            {
            map_vert=1;
            switch(event.mse_code)
              {
              case MSE_BUTNL: ratio_vert=2;break;
              case MSE_BUTNM: ratio_vert=3;break;
              case MSE_BUTNR: ratio_vert=5;break;
              }
            }
          plot=1;
          break;
        case STNS:
          switch(event.mse_code)
            {
            case MSE_BUTNL: i=1;break;
            case MSE_BUTNM: i=0;break;
            case MSE_BUTNR: i=(-1);break;
            }
          if(i!=map_name)
            {
            map_name=i;
            plot=1;
            }
          break;
        case COPY:
          put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
          switch(event.mse_code)
            {
            case MSE_BUTNL: hard_copy(3);break;
            case MSE_BUTNM: hard_copy(2);break;
            case MSE_BUTNR: hard_copy(1);break;
            }
          put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
          ring_bell=0;
          break;
        case TMSP:
          if(!map_only || sel.t1>=sel.t2) break;
          put_func(func_map[TMSP],TMSP,0,1,0);
          map_mode=MODE_TS1;
          ring_bell=0;
          break;
        }
      if(flag_hypo && x<strlen(ft.hypo.textbuf)*WIDTH_TEXT)
        {
        if(map_ellipse) map_ellipse=0;
        else map_ellipse=1;
        plot=1;
        }
      }
    else if(y>height_dpy-MARGIN)
      {
      ring_bell=1;
      if(other_epis && 0<=x && x<=WIDTH_TEXT*5) /* owner's name */
        {
        switch(event.mse_code)
          {
          case MSE_BUTNL:
            strcpy(sel.o,"-auto");
            sprintf(textbuf,"%-5.5s",sel.o);
            break;
          case MSE_BUTNM:
            *sel.o=0;
            sprintf(textbuf," ALL ");
            break;
          case MSE_BUTNR:
            strncpy(sel.o,getname(geteuid()),8);
            sprintf(textbuf,"%-5.5s",sel.o);
            break;
          }
        put_text(&dpy,0,height_dpy-MARGIN+Y_LINE1,textbuf,BF_S);
        ring_bell=0;
        }
      else if(other_epis && WIDTH_TEXT*6<=x && x<=WIDTH_TEXT*37)
        {     /* date & time */
        i=j=0;
        switch((x/WIDTH_TEXT-5)/3)
          {
          case 0: /* ye1 */
            i=check_year(event.mse_code,&sel.time1.ye,
              sel.time1_save.ye,sel.time2_save.ye);
            break;
          case 1: /* mo1 */
            i=check(event.mse_code,&sel.time1.mo,1,12);
            break;
          case 2: /* da1 */
            i=check(event.mse_code,&sel.time1.da,1,31);
            break;
          case 3: /* ho1 */
            i=check(event.mse_code,&sel.time1.ho,0,23);
            break;
          case 4: /* mi1 */
            i=check(event.mse_code,&sel.time1.mi,0,59);
            break;
          case 6: /* ye2 */
            i=j=check_year(event.mse_code,&sel.time2.ye,
              sel.time1_save.ye,sel.time2_save.ye);
            break;
          case 7: /* mo2 */
            i=j=check(event.mse_code,&sel.time2.mo,1,12);
            break;
          case 8: /* da2 */
            i=j=check(event.mse_code,&sel.time2.da,1,31);
            break;
          case 9: /* ho2 */
            i=j=check(event.mse_code,&sel.time2.ho,0,23);
            break;
          case 10: /* mi2 */
            i=j=check(event.mse_code,&sel.time2.mi,0,59);
            break;
          }
        if(i==1)
          {
          map_period=0;
          sprintf(textbuf,
            "%02d/%02d/%02d %02d:%02d - %02d/%02d/%02d %02d:%02d",
            sel.time1.ye,sel.time1.mo,sel.time1.da,sel.time1.ho,sel.time1.mi,
            sel.time2.ye,sel.time2.mo,sel.time2.da,sel.time2.ho,sel.time2.mi);
          put_text(&dpy,WIDTH_TEXT*6-HW,height_dpy-MARGIN+Y_LINE1," ",BF_S);
          put_text(&dpy,WIDTH_TEXT*6,height_dpy-MARGIN+Y_LINE1,textbuf,BF_S);
          ring_bell=0;
          }
        if(j==1) map_update=0;
        }
      else if(WIDTH_TEXT*38<=x && x<=WIDTH_TEXT*52) /* depth */
        {
        ring_bell=1;
        if(x<WIDTH_TEXT*44 || map_vert) /* dep1 */
          {
change_dep:
          i=sel.dep1_idx;
          switch(event.mse_code)
            {
            case MSE_BUTNL:
              if(i<sizeof(depsteps)/sizeof(*depsteps)-2) i++;
              break;
            case MSE_BUTNM:
              if(i==0) while(depsteps[i]!=0.0) i++;
              else i=0;
              break;
            case MSE_BUTNR: if(i>0) i--;break;
            }
          if(sel.dep1_idx!=i)
            {
            sel.dep1=depsteps[sel.dep1_idx=i];
            if(map_vert)
              {
              if(i==0) sel.dep2=depsteps[sel.deplen_idx];
              else sel.dep2=sel.dep1+depsteps[sel.deplen_idx];
              }
            else if(sel.dep1>sel.dep2)
              sel.dep2=depsteps[sel.dep2_idx=sel.dep1_idx];
            ring_bell=0;
            }
          }
        else
          {
          i=sel.dep2_idx;
          switch(event.mse_code)
            {
            case MSE_BUTNL:
              if(i<sizeof(depsteps)/sizeof(*depsteps)-1) i++;
              break;
            case MSE_BUTNM:
              i=sizeof(depsteps)/sizeof(*depsteps)-1;break;
            case MSE_BUTNR: if(i>1) i--;break;
            }
          if(sel.dep2_idx!=i)
            {
            sel.dep2=depsteps[sel.dep2_idx=i];
            if(sel.dep1>sel.dep2) sel.dep1=depsteps[sel.dep1_idx=sel.dep2_idx];
            ring_bell=0;
            }
          }
        if(ring_bell==0)
          {
          if(sel.dep1_idx==0) sprintf(textbuf,"   < H <");
          else sprintf(textbuf,"%3d< H <",(int)sel.dep1);
          if(sel.dep2==depsteps[sizeof(depsteps)/sizeof(*depsteps)-1])
            sprintf(textbuf+strlen(textbuf),"    km");
          else sprintf(textbuf+strlen(textbuf),"%3d km",(int)sel.dep2);
          put_text(&dpy,WIDTH_TEXT*38-HW,height_dpy-MARGIN+Y_LINE1," ",BF_S);
          put_text(&dpy,WIDTH_TEXT*38,height_dpy-MARGIN+Y_LINE1,textbuf,BF_S);
          put_text(&dpy,WIDTH_TEXT*41,height_dpy-MARGIN+Y_LINE1,"_",BF_SDO);

          if(sel.dep1_idx==0) sprintf(textbuf,"0  ");
          else sprintf(textbuf,"%-3d",(int)sel.dep1);
          put_text(&dpy,width_horiz+MARGIN-WIDTH_TEXT,
            MARGIN*2+height_horiz-HEIGHT_TEXT/2,textbuf,BF_SI);
          sprintf(textbuf,"%-3d",(int)sel.dep2);
          put_text(&dpy,width_horiz+MARGIN-WIDTH_TEXT,
            height_dpy-MARGIN-HEIGHT_TEXT,textbuf,BF_SI);
          sprintf(textbuf,"%3d",(int)sel.dep2);
          put_text(&dpy,width_dpy-WIDTH_TEXT*strlen(textbuf),
            MARGIN*2+height_horiz-HEIGHT_TEXT/2,textbuf,BF_SI);
          }
        }
      else if(other_epis && WIDTH_TEXT*53<=x && x<=WIDTH_TEXT*70) /* magnitude */
        {
        ring_bell=1;
        if(x<WIDTH_TEXT*60) /* mag1 */
          {
          i=sel.mag1_idx;
          switch(event.mse_code)
            {
            case MSE_BUTNL:
              if(i<sizeof(magsteps)/sizeof(*magsteps)-2) i++;
              break;
            case MSE_BUTNM: i=0;break;
            case MSE_BUTNR: if(i>0) i--;break;
            }
          if(sel.mag1_idx!=i)
            {
            sel.mag1=magsteps[sel.mag1_idx=i];
            if(sel.mag1_idx>sel.mag2_idx)
              sel.mag2=magsteps[sel.mag2_idx=sel.mag1_idx];
            ring_bell=0;
            }
          }
        else if(x<WIDTH_TEXT*67)
          {
          i=sel.mag2_idx;
          switch(event.mse_code)
            {
            case MSE_BUTNL:
              if(i<sizeof(magsteps)/sizeof(*magsteps)-1) i++;
              break;
            case MSE_BUTNM:
              i=sizeof(magsteps)/sizeof(*magsteps)-1;break;
            case MSE_BUTNR: if(i>1) i--;break;
            }
          if(sel.mag2_idx!=i)
            {
            sel.mag2=magsteps[sel.mag2_idx=i];
            if(sel.mag1_idx>sel.mag2_idx)
              sel.mag1=magsteps[sel.mag1_idx=sel.mag2_idx];
            ring_bell=0;
            }
          }
        else
          {
          if(sel.mag_ud) sel.mag_ud=0;
          else sel.mag_ud=1;
          ring_bell=0;
          }
        if(ring_bell==0)
          {
          if(sel.mag1_idx==0) sprintf(textbuf,"    < M <");
          else sprintf(textbuf,"%4.1f< M <",sel.mag1);
          if(sel.mag2_idx==sizeof(magsteps)/sizeof(*magsteps)-1)
            sprintf(textbuf+strlen(textbuf),"     ");
          else sprintf(textbuf+strlen(textbuf),"%4.1f ",sel.mag2);
          if(sel.mag_ud) sprintf(textbuf+strlen(textbuf),"+UD");
          else sprintf(textbuf+strlen(textbuf),"-UD");
          put_text(&dpy,WIDTH_TEXT*53-HW,height_dpy-MARGIN+Y_LINE1," ",BF_S);
          put_text(&dpy,WIDTH_TEXT*53,height_dpy-MARGIN+Y_LINE1,textbuf,BF_S);
          put_text(&dpy,WIDTH_TEXT*57,height_dpy-MARGIN+Y_LINE1,"_",BF_SDO);
          }
        }
      else if(other_epis && WIDTH_TEXT*71<=x && x<=WIDTH_TEXT*74) /* no blast */
        {
        if(sel.no_blast)
          {
          sel.no_blast=0;
          sprintf(textbuf,"+BL");
          }
        else
          {
          sel.no_blast=1;
          sprintf(textbuf,"-BL");
          }
        put_text(&dpy,WIDTH_TEXT*71-HW,height_dpy-MARGIN+Y_LINE1," ",BF_S);
        put_text(&dpy,WIDTH_TEXT*71,height_dpy-MARGIN+Y_LINE1,textbuf,BF_S);
        ring_bell=0;
        }
      else if(map_mode==MODE_NORMAL && other_epis && WIDTH_TEXT*75<=x &&
          x<=WIDTH_TEXT*(75+strlen(mapbuf)))  /* FIND */
        {
        /* enter FIND mode */
        map_mode=MODE_FIND1;
        put_text(&dpy,WIDTH_TEXT*75,height_dpy-MARGIN+Y_LINE1,textbuf1,BF_S);
        if(event.mse_code==MSE_BUTNR) list_on_map=0;
        else list_on_map=1;
        ring_bell=0;
        }
      else switch(get_func(x))
        {
        case QUIT:
          i=ppk_idx;
          switch(event.mse_code)
            {
            case MSE_BUTNL:
              if(i<sizeof(mapsteps)/sizeof(*mapsteps)-1) i++;
              break;
            case MSE_BUTNM: break;
            case MSE_BUTNR: if(i>0) i--;break;
            }
          if(ppk_idx!=i)
            {
            ppk_idx=i;
            if(mapsteps[ppk_idx]<10.0)
              sprintf(textbuf,"%3.1f",mapsteps[ppk_idx]);
            else sprintf(textbuf,"%3d",(int)mapsteps[ppk_idx]);
            put_func(textbuf,QUIT,height_dpy-MARGIN+Y_LINE1,1,1);
            ring_bell=0;
            }
          break;
        case RFSH:    /* OTHRS / UPDAT(map_only) */
          if(map_only)
            {
            read_hypo=1;
            plot=1;
            }
          else
            {
            put_reverse(&dpy,x_func(RFSH),height_dpy-MARGIN,
              WB,MARGIN);
            if(other_epis) other_epis=0;
            else other_epis=1;
            refresh(other_epis);
            ring_bell=0;
            }
          break;
        }
      }
    else if(x<width_horiz && y<height_horiz+MARGIN)
      {
      if(map_dir)
        {
        xd=XINV(x,y)/pixels_per_km+x_cent;
        yd=(-YINV(x,y))/pixels_per_km+y_cent;
        }
      else
        {
        xd=((double)(x-xzero))/pixels_per_km+x_cent;
        yd=(-(double)(y-yzero))/pixels_per_km+y_cent;
        }
      pltxy(alat0,along0,&alat,&along,&xd,&yd,1);
      if(event.mse_code==MSE_BUTNL)
        {
        if(ppk_idx<sizeof(mapsteps)/sizeof(*mapsteps)-1)
          {
          ppk_idx++;
          plot=1;
          }
        else ring_bell=1;
        }
      else if(event.mse_code==MSE_BUTNM) plot=1;
      else if(event.mse_code==MSE_BUTNR)
        {
        if(ppk_idx>0)
          {
          ppk_idx--;
          plot=1;
          }
        else ring_bell=1;
        }
      if(plot)
        {
        lat_cent=alat;
        lon_cent=along;
        }
      }
    else if(map_vert==0) ring_bell=1;

  /* map_vert==1 hereafter */
    else if(x<width_horiz || y<MARGIN+height_horiz)
      {
      if(map_vstn) map_vstn=0;
      else map_vstn=1;
      plot=1;
      }
    else if(y<MARGIN*2+height_horiz+HEIGHT_TEXT/2 ||
        x<width_horiz+MARGIN+2*WIDTH_TEXT)
      {
      ring_bell=1;
      goto change_dep;
      }
    else if(y<MARGIN*2+height_horiz+(width_dpy-width_horiz-
        MARGIN)/2+HEIGHT_TEXT*2)
      {
      if(other_epis)
        {
        if(map_true) map_true=0;
        else map_true=1;
        plot=1;
        }
      else ring_bell=1;
      }
    else if(y>height_dpy-MARGIN*2-HW && y<height_dpy-MARGIN-HW)
      {
      switch(get_func(x))
        {
        case QUIT:
          ring_bell=1;
          i=map_dir;
          switch(event.mse_code)
            {
            case MSE_BUTNL: if(i<90) i+=5;break;
            case MSE_BUTNM: i=0;break;
            case MSE_BUTNR: if(i>(-90)) i-=5;break;
            }
          if(map_dir!=i)
            {
            map_dir=i;
            if(map_dir>=0) sprintf(textbuf,"N%2dE",map_dir);
            else sprintf(textbuf,"N%2dW",(-map_dir));
            put_func(textbuf,QUIT,height_dpy-MARGIN*2-HW+Y_LINE1,1,1);
            ring_bell=0;
            }
          break;
        case RATIO:
          i=ratio_vert;
          switch(event.mse_code)
            {
            case MSE_BUTNL: i=2;break;
            case MSE_BUTNM: i=3;break;
            case MSE_BUTNR: i=5;break;
            }
          if(i!=ratio_vert)
            {
            ratio_vert=i;
            plot=1;
            }
          else ring_bell=1;
          break;
        }
      }
    if(plot) refresh(0);
    else if(ring_bell) bell();
    }
  else if((map_mode==MODE_FIND2 || map_mode==MODE_TS2) && 
      event.mse_trig==MSE_BUTTON && event.mse_dir==MSE_UP)
    {
    if(map_f2x<map_f1x)
      {
      i=map_f2x;
      map_f2x=map_f1x;
      map_f1x=i;
      }
    if(map_f2y<map_f1y)
      {
      i=map_f2y;
      map_f2y=map_f1y;
      map_f1y=i;
      }
    if(map_mode==MODE_FIND2)
      {
      draw_rect(map_f1x,map_f1y,map_f2x,map_f2y,LPTN_FF,BF_S,&dpy);
      draw_rect(map_f1x-1,map_f1y-1,map_f2x+1,map_f2y+1,LPTN_FF,BF_S,&dpy);
      map_n_find=0;
      if(event.mse_code==MSE_BUTNR) phypo_format=1;
      else phypo_format=0;
      refresh(1);
      map_mode=MODE_NORMAL;
      }
    else refresh(0);
    }
  else if(event.mse_trig==MSE_EXP) refresh(0);
  }

static int
open_sock(char *host, unsigned short port)
  {
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *h;

  memset((char *)&serv_addr,0,sizeof(serv_addr));
  serv_addr.sin_family=AF_INET;
  if(!(h=gethostbyname(host)))
    {
    fprintf(stderr,"can't find host '%s'\n",host);
    return (-1);
    }
  memcpy((caddr_t)&serv_addr.sin_addr,h->h_addr,h->h_length);
  serv_addr.sin_port=htons(port);

  if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
    fprintf(stderr,"can't open stream socket\n");
    return (-1);
    }
  if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
    fprintf(stderr,"can't connect to server\n");
    return (-1);
    }
  return (sockfd);
  }

static int
load_data(int btn) /* return=1 means success */
  /* int btn;    MSE_BUTNL, MSE_BUTNM or MSE_BUTNR */
  {
  FILE *fp,*fq;
  struct dirent *dir_ent;
  DIR *dir_ptr;
  int re,ii,i,j,k1,k2,k3,k4,k5,find_file,tm_begin[6],tm[6],sec_max,sockfd;
  float k6;
  time_t lsec_begin;
  char text_buf[LINELEN],*ptr,name1[NAMLEN],name2[NAMLEN],pickfile[NAMLEN],
    name_low[NAMLEN],name_high[NAMLEN],filename[NAMLEN],
    namebuf[NAMLEN],namebuf1[NAMLEN],diagbuf[50],userbuf[50];

  *name_low=(*name_high)=0;
  if(*ft.save_file) find_file=0;  /* pick file name already fixed */
  else find_file=1;               /* search file name here */
  if(btn==MSE_BUTNM) find_file=1; /* discard present pick file name */
                                  /* and search again */
  else if(btn==MSE_BUTNR && find_file==0) /* search the "next" pick file */
    {
    strcpy(name_low,ft.save_file);
    find_file=1;
    }

  if(find_file) /* search in a directory */
    {
    /* get time range */
    i=ft.len-1;
    sprintf(name1,"%02x%02x%02x%1c%02x%02x%02x",ft.ptr[0].time[0],
      ft.ptr[0].time[1],ft.ptr[0].time[2],dot,ft.ptr[0].time[3],
      ft.ptr[0].time[4],ft.ptr[0].time[5]);
    sprintf(name2,"%02x%02x%02x%1c%02x%02x%02x",ft.ptr[i].time[0],
      ft.ptr[i].time[1],ft.ptr[i].time[2],dot,ft.ptr[i].time[3],
      ft.ptr[i].time[4],ft.ptr[i].time[5]);

    re=0;
    if(*ft.pick_server) /* try pick file server */
      {
      if((ptr=strrchr(ft.data_file,'/'))==NULL) ptr=ft.data_file;
      else ptr++;
      if((sockfd=open_sock(ft.pick_server,ft.pick_server_port))!=-1)
        {
        fprintf(stderr,"connected to pick file server %s - ",ft.pick_server);
        fp=fdopen(sockfd,"r+");
        while(fgets(text_buf,LINELEN,fp)) if(strncmp(text_buf,"PICKS OK",8)==0)
          {
          re=1;
          rewind(fp);
          break;
          }
        if(re && (re=fprintf(fp,"%s %s %s %s\n",name1,name2,ptr,ft.hypo_dir))>0)
          {
          fflush(fp);
          rewind(fp);
          fprintf(stderr,"OK\n");
          while(fgets(text_buf,LINELEN,fp))
            {
            sscanf(text_buf,"%s",pickfile);
            /* find the earliest (but later than "name_low") pick file */
            if(*name_low && strncmp2(pickfile,name_low,17)<=0) continue;
            if(*name_high && strncmp2(pickfile,name_high,17)>=0) continue;
            strcpy(name_high,pickfile);
            }
          }
        close(sockfd);
        if(*name_high) strcpy(ft.save_file,name_high);
        else if(re) {fprintf(stderr,"NG\n");return (0);}
        }
      }
    if(re==0) /* search pick directory */
      { 
      if((dir_ptr=opendir(ft.hypo_dir))==NULL)
        {
        fprintf(stderr,"directory '%s' not open\007\007\n",ft.hypo_dir);
        return (0);
        }
      while((dir_ent=readdir(dir_ptr))!=NULL)
        {
        if(*dir_ent->d_name=='.') continue; /* skip "." & ".." */
      /* pick file name must be in the time range of data file */
        if(strncmp2(dir_ent->d_name,name1,13)<0 ||
          strncmp2(dir_ent->d_name,name2,13)>0) continue;
      /* read the first line */
        sprintf(filename,"%s/%s",ft.hypo_dir,dir_ent->d_name);
        if((fp=fopen(filename,"r"))==NULL) continue;
        *text_buf=0;
        fgets(text_buf,LINELEN,fp);
        fclose(fp);
      /* first line must be "#p [data file name] ..." */
        if((ptr=strrchr(ft.data_file,'/'))==NULL) ptr=ft.data_file;
        else ptr++;
        sscanf(text_buf,"%s%s",filename,namebuf);
        strcpy(namebuf1,namebuf);
        if(namebuf[6]=='.') namebuf1[6]='_';
        else if(namebuf[6]=='_') namebuf1[6]='.';
        if(strcmp(filename,"#p") ||
          (strcmp(namebuf,ptr) && strcmp(namebuf1,ptr))) continue;
      /* find the earliest (but later than "name_low") pick file */
        if(*name_low && strncmp2(dir_ent->d_name,name_low,17)<=0) continue;
        if(*name_high && strncmp2(dir_ent->d_name,name_high,17)>=0) continue;
        strcpy(name_high,dir_ent->d_name);
        }
      closedir(dir_ptr);
      if(*name_high) strcpy(ft.save_file,name_high);
      else return (0);
      }
    }
  /* file name fixed */
  sprintf(filename,"%s/%s",ft.hypo_dir,ft.save_file);
  if((fp=fopen(filename,"r"))==NULL) return (0);
  if(fgets(text_buf,LINELEN,fp)==NULL)
    {
    fclose(fp);
    return (0);
    }
  /* picks */
  *diagbuf=(*userbuf)=0;
  sscanf(text_buf+3,"%s %s %s",namebuf,diagbuf,userbuf);
  if(strcmp(diagbuf,".")==0) *diagbuf=0;
  if(just_hypo)
    {
    ft.pick=(struct Pick_Time (*)[4])
      win_xmalloc(sizeof(struct Pick_Time)*4*ft.n_ch);
    for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++) ft.pick[i][j].valid=0;
    }
  cancel_picks(NULL,-1);    /* cancel all picks */
  set_diagnos(diagbuf,userbuf);
  /* read picks */
  flag_hypo=flag_mech=sec_max=0;
  for(ii=0;;ii++)
    {
    *text_buf=0;
    if(fgets(text_buf,LINELEN,fp)==NULL || strncmp(text_buf,"#p",2)) break;
    if(ii==0)
      {
      if(strlen(text_buf)<25) /* for compatibility to old format */
        /* new format has time of beginning of data */
        {
        sscanf(text_buf+3,"%d%d%d%d%d%d",&tm_begin[0],&tm_begin[1],&tm_begin[2],
          &tm_begin[3],&tm_begin[4],&tm_begin[5]);
        if(!just_hypo)
          {
          bcd_dec(tm,ft.ptr[0].time);
          if(time_cmp_win(tm,tm_begin,6))
            {
            *ft.save_file=0;
            fclose(fp);
            return (0); /* incorrect time */
            }
          }
        continue;
        }
      else if(just_hypo)
        {
        fprintf(stderr,"time offset : 'FileName - %d sec'\n",just_hypo_offset);
        sscanf(ft.data_file+strlen(ft.data_file)-13,"%2d%2d%2d.%2d%2d%2d",
          &tm_begin[0],&tm_begin[1],&tm_begin[2],
          &tm_begin[3],&tm_begin[4],&tm_begin[5]);
        lsec2time(time2lsec(tm_begin)-just_hypo_offset,tm_begin);
        }
      }
    sscanf(text_buf+3,"%x%d%d%d%d%d%d%e",&i,&j,&k1,&k2,&k3,&k4,&k5,&k6);
    /* 2001.6.7. if data file exists, don't accept out-of-range picks */
    if(!just_hypo && (k1<0 || k1>=ft.len || k3<0 || k3>=ft.len))
      {
      fprintf(stderr,"out-of-range pick ignored - %s",text_buf);
      continue;
      }
    if(ft.ch2idx[i]<0) continue;
    if(j>MD) continue;
    ft.pick[ft.ch2idx[i]][j].valid=1;
    ft.pick[ft.ch2idx[i]][j].sec1 =k1;
    ft.pick[ft.ch2idx[i]][j].msec1=k2;
    ft.pick[ft.ch2idx[i]][j].sec2 =k3;
    ft.pick[ft.ch2idx[i]][j].msec2=k4;
    ft.pick[ft.ch2idx[i]][j].polarity=k5;
    if(j==MD) *(float *)&ft.pick[ft.ch2idx[i]][j].valid=k6;
    if(k3>sec_max) sec_max=k3;
    if(!just_hypo) put_mark(j,ft.idx2pos[ft.ch2idx[i]],1);
    }
  fprintf(stderr,"loaded from pick file '%s'\n",filename);
  if(just_hypo)
    {
    ft.len=sec_max+1;
    if((ft.ptr=(struct File_Ptr *)win_xmalloc(sizeof(*(ft.ptr))*ft.len))==NULL)
      emalloc("ft.ptr");
    lsec_begin=time2lsec(tm_begin);
    for(i=0;i<ft.len;i++)
      {
      dec_bcd(ft.ptr[i].time,tm_begin);
      lsec2time(++lsec_begin,tm_begin);
      }
    return (1);
    }
  /* read seis */
  if(strncmp(text_buf,"#s",2)==0)
    {
    fq=fopen(ft.seis_file,"w+");
    do
      {
      fprintf(fq,"%s",text_buf+3);
      *text_buf=0;
      if(fgets(text_buf,LINELEN,fp)==NULL) break;
      } while(strncmp(text_buf,"#s",2)==0);
    fclose(fq);
    }
  /* read final */
  if(strncmp(text_buf,"#f",2)==0)
    {
    fq=fopen(ft.finl_file,"w+");
    do
      {
      fprintf(fq,"%s",text_buf+3);
      *text_buf=0;
      if(fgets(text_buf,LINELEN,fp)==NULL) break;
      } while(strncmp(text_buf,"#f",2)==0);
    fclose(fq);
    read_final(ft.finl_file,&ft.hypo);
    flag_hypo=1;
    get_calc();
    }
  /* read mecha */
  if(strncmp(text_buf,"#m",2)==0)
    {
    fq=fopen(ft.mech_file,"w+");
    do
      {
      fprintf(fq,"%s",text_buf+3);
      *text_buf=0;
      if(fgets(text_buf,LINELEN,fp)==NULL) break;
      } while(strncmp(text_buf,"#m",2)==0);
    fclose(fq);
    flag_mech=1;
    }
  fclose(fp);
  fprintf(stderr,"loaded from pick file end '%s'\n",filename);
  flag_change=0;
  return (1);
  }

static void
init_mecha(void)
  {
  char textbuf[LINELEN];

  mecha_mode=MODE_DOWN;
  if(flag_hypo==0)
    {
    loop=loop_stack[--loop_stack_ptr];
    raise_ttysw(1);
    list_line();
    fprintf(stderr,"no hypocenter\007\n");
    return;
    }
  if(*mec_hemi==0)
    {
    read_parameter(PARAM_HEMI,textbuf);
    if(*textbuf=='l' || *textbuf=='L') strcpy(mec_hemi,"LOWER");
    else strcpy(mec_hemi,"UPPER");
    }
  refresh(999);
  }

static void
proc_mecha(void)
  {
  int ring_bell,x,y;
  double phi1,theta1,phi2,theta2;

  x=event.mse_data.md_x;
  y=event.mse_data.md_y;
  if(event.mse_trig==MSE_BUTTON)
    {
    ring_bell=1;
    if(event.mse_dir==MSE_DOWN)
      {
      if(y<MARGIN)
        {
        switch(get_func(x))
          {
          case RETN:
            loop=loop_stack[--loop_stack_ptr];
            if(!background) refresh(0);    
            return;
          case UPPER:
            if(*mec_hemi=='L') strcpy(mec_hemi,"UPPER");
            else strcpy(mec_hemi,"LOWER");
            refresh(999);
            ring_bell=0;
            break;
          case RFSH:
            refresh(999);
            ring_bell=0;
            break;
          case MAP:
            loop_stack[loop_stack_ptr++]=loop;
            loop=LOOP_MAP;
            init_map(event.mse_code);
            return;
          case COPY:
            put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
            switch(event.mse_code)
              {
              case MSE_BUTNL: hard_copy(3);break;
              case MSE_BUTNM: hard_copy(2);break;
              case MSE_BUTNR: hard_copy(1);break;
              }
            put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
            ring_bell=0;
            break;
          case STNS:
            if(mech_name) mech_name=0;
            else mech_name=1;
            refresh(999);
            ring_bell=0;
            break;
          }
        }
      else if(mecha_mode==MODE_DOWN)
        {
        if((x-mec_xzero)*(x-mec_xzero)+
            (y-mec_yzero)*(y-mec_yzero)<(int)mec_rc*(int)mec_rc)
          {
          xy_pt(&x,&y,&phi1,&theta1,1);
          mecha_mode=MODE_UP;
          ring_bell=0;
          }
        }
      }
    else if(event.mse_dir==MSE_UP)
      {
      ring_bell=0;
      if(mecha_mode==MODE_UP)
        {
        if((x-mec_xzero)*(x-mec_xzero)+(y-mec_yzero)*(y-mec_yzero)<
            (int)mec_rc*(int)mec_rc)
          {
          xy_pt(&x,&y,&phi2,&theta2,1);
/*          rotate(phi1,theta1,phi2,theta2);*/
          }
        else
          {
          ring_bell=1;
          mecha_mode=MODE_DOWN;
          }
        }
      }
    if(ring_bell) bell();
    }
  else if(event.mse_trig==MSE_EXP) refresh(999);
  }

static void
xy_pt(int *x, int *y, double *p, double *t, int idx)
  {
  double r,xx,yy,pp,tt;

  if(idx)   /* if idx=1, (x,y) -> (p,t) */
    {
    xx=(double)((*x)-mec_xzero);
    yy=(double)((*y)-mec_yzero);
    /* r=sqrt(xx*xx+yy*yy); */
    r = hypot(xx, yy);
    *p=atan2(yy,xx)+HP;
    if(*p<0.0) *p+=PI*2.0;
    *t=2.0*asin(r/(mec_rc*sqrt(2.0)));  /* 0 <= (*t) <= PI/2 */
    if(*mec_hemi=='U') *t=PI-*t;
    }
  else    /* if idx=0, (p,t) -> (x,y) */
    {
    pp=(*p);
    if(*mec_hemi=='U') tt=PI-*t;
    else tt=(*t);
    if(tt>PI/2.0)
      {
      pp+=PI;
      tt=PI-tt;
      }
    r=sqrt(2.0)*sin(tt*0.5)*mec_rc;
    *x=(int)( r*sin(pp))+mec_xzero;
    *y=(int)(-r*cos(pp))+mec_yzero;
    }
  }

static int
read_final(char *final_file, struct Hypo *hypo)
  {
  FILE *fp;
  int i;
  char textbuf[LINELEN],ulat,ulon;
  double lat,lon;

  hypo->valid=0;
  if((fp=fopen(final_file,"r"))==NULL) return (0);
  if(fgets(textbuf,LINELEN,fp)==NULL) return (0);
  if(sscanf(textbuf,"%d%d%d%d%d%lf%lf%lf%lf%lf",&hypo->tm[0],&hypo->tm[1],
    &hypo->tm[2],&hypo->tm[3],&hypo->tm[4],&hypo->se,&hypo->alat,
    &hypo->along,&hypo->dep,&hypo->mag)<9) return (0);
  hypo->valid=1;
  if(fgets(textbuf,LINELEN,fp)==NULL) return (1);
  sscanf(textbuf,"%s%*f%lf%lf%lf",hypo->diag,&hypo->ye,&hypo->xe,&hypo->ze);
  if(fgets(textbuf,LINELEN,fp)==NULL) return (1);
  if(strchr(textbuf,'*')==0)
    {
    sscanf(textbuf,"%lf%lf%lf%lf%lf%lf",&hypo->c[0][0],&hypo->c[0][1],
      &hypo->c[0][2],&hypo->c[1][1],&hypo->c[1][2],&hypo->c[2][2]);
    hypo->c[0][1]=(-hypo->c[0][1]); /* because Y axis is positive */
    hypo->c[1][2]=(-hypo->c[1][2]); /* downward on display */
    mat_sym(hypo->c);
    hypo->ellipse=1;
    }
  else hypo->ellipse=0;
  if(fgets(textbuf,LINELEN,fp)==NULL) return (1);
  sscanf(textbuf,"%lf%lf%lf%lf%lf%lf",&hypo->alat0,&hypo->ye0,
    &hypo->along0,&hypo->xe0,&hypo->dep0,&hypo->ze0);
  if(fgets(textbuf,LINELEN,fp)==NULL) return (1);
  sscanf(textbuf,"%d",&hypo->ndata);
  /* if(hypo->fnl==NULL) */
  /*   hypo->fnl=(struct Fnl *)win_xmalloc(sizeof(*hypo->fnl)*hypo->ndata); */
  /* else hypo->fnl=(struct Fnl *)win_xrealloc(hypo->fnl, */
  /*        sizeof(*hypo->fnl)*hypo->ndata); */
  hypo->fnl=(struct Fnl *)win_xrealloc(hypo->fnl,sizeof(*hypo->fnl)*hypo->ndata);
  if(hypo->fnl==NULL)
    emalloc("hypo->fnl");
  for(i=0;i<hypo->ndata;i++)
    {
    fgets(textbuf,LINELEN,fp);
    sscanf(textbuf,"%s%s",hypo->fnl[i].stn,hypo->fnl[i].pol);
    str2double(textbuf,13,8,&hypo->fnl[i].delta);
    str2double(textbuf,21,6,&hypo->fnl[i].azim);
    str2double(textbuf,27,6,&hypo->fnl[i].emerg);
    str2double(textbuf,33,6,&hypo->fnl[i].incid);
    str2double(textbuf,39,7,&hypo->fnl[i].pt);
    str2double(textbuf,46,6,&hypo->fnl[i].pe);
    str2double(textbuf,52,7,&hypo->fnl[i].pomc);
    str2double(textbuf,59,7,&hypo->fnl[i].st);
    str2double(textbuf,66,6,&hypo->fnl[i].se);
    str2double(textbuf,72,7,&hypo->fnl[i].somc);
    str2double(textbuf,79,10,&hypo->fnl[i].amp);
    str2double(textbuf,89,5,&hypo->fnl[i].mag);
    hypo->fnl[i].azim *=PI/180.0;
    hypo->fnl[i].emerg*=PI/180.0;
    hypo->fnl[i].incid*=PI/180.0;
    hypo->fnl[i].idx=i;
    }
  if(fgets(textbuf,LINELEN,fp)==NULL) return (1);
  sscanf(textbuf,"%lf%lf",&hypo->pomc_rms,&hypo->somc_rms);
  fclose(fp);
  adj_sec_win(hypo->tm,&hypo->se,hypo->tm_c,&hypo->se_c);
  if(hypo->alat<0.0) {lat=(-hypo->alat);ulat='S';}
  else {lat=hypo->alat;ulat='N';}
  if(hypo->along<0.0) {lon=(-hypo->along);ulon='W';}
  else {lon=hypo->along;ulon='E';}
  sprintf(hypo->textbuf,
    "%02d/%02d/%02d %02d:%02d:%02d.%d %7.4f%1c %8.4f%1c %.1fkm M%.1f",
    hypo->tm_c[0],hypo->tm_c[1],hypo->tm_c[2],hypo->tm_c[3],
    hypo->tm_c[4],hypo->tm_c[5],hypo->tm_c[6]/100,
    lat,ulat,lon,ulon,hypo->dep,hypo->mag);
  return (1);
  }

/* static void */
/* str2double(char *t, int n, int m, double *d) */
/*   { */
/*   char tb[20]; */

/*   strncpy(tb,t+n,m); */
/*   tb[m]=0; */
/*   if(tb[0]=='*') *d=100.0; */
/*   else *d=atof(tb); */
/*   } */

static int
put_mecha(void)
  {
  char textbuf[LINELEN],p;
  int i,x,y;

  fflush(stderr);
  put_white(&dpy,0,0,width_dpy,height_dpy); /* clear */
  put_function_mecha();

  if(flag_hypo==0) return (0);

  put_text(&dpy,(width_dpy-strlen(ft.hypo.textbuf)*WIDTH_TEXT)/2,
    HEIGHT_TEXT*2,ft.hypo.textbuf,BF_SDO);

  /* draw a circle */
  draw_circle(mec_xzero,mec_yzero,(int)mec_rc,LPTN_FF,BF_SDO,&dpy);
  /* tick marks */
  draw_seg(mec_xzero,mec_yzero-2,mec_xzero,mec_yzero+2,LPTN_FF,BF_SDO,&dpy);
  draw_seg(mec_xzero-2,mec_yzero,mec_xzero+2,mec_yzero,LPTN_FF,BF_SDO,&dpy);
  draw_seg(mec_xzero,mec_yzero-(int)mec_rc,mec_xzero,
    mec_yzero-(int)mec_rc-WIDTH_TEXT,LPTN_FF,BF_SDO,&dpy);
  put_text(&dpy,mec_xzero-(WIDTH_TEXT-1)/2,
    mec_yzero-(int)mec_rc-WIDTH_TEXT-HEIGHT_TEXT-4,"N",BF_SDO);
  draw_seg(mec_xzero,mec_yzero+(int)mec_rc,mec_xzero,
    mec_yzero+(int)mec_rc+WIDTH_TEXT,LPTN_FF,BF_SDO,&dpy);
  put_text(&dpy,mec_xzero-(WIDTH_TEXT-1)/2,
    mec_yzero+(int)mec_rc+WIDTH_TEXT+4,"S",BF_SDO);
  draw_seg(mec_xzero-(int)mec_rc,mec_yzero,mec_xzero-(int)mec_rc-WIDTH_TEXT,
    mec_yzero,LPTN_FF,BF_SDO,&dpy);
  put_text(&dpy,mec_xzero-(int)mec_rc-WIDTH_TEXT*2-4,
    mec_yzero-(HEIGHT_TEXT-1)/2,"W",BF_SDO);
  draw_seg(mec_xzero+(int)mec_rc,mec_yzero,mec_xzero+(int)mec_rc+WIDTH_TEXT,
    mec_yzero,LPTN_FF,BF_SDO,&dpy);
  put_text(&dpy,mec_xzero+(int)mec_rc+WIDTH_TEXT+4,
    mec_yzero-(HEIGHT_TEXT-1)/2,"E",BF_SDO);

  /* plot up/down */
  for(i=0;i<ft.hypo.ndata;i++)
    {
    xy_pt(&x,&y,&ft.hypo.fnl[i].azim,&ft.hypo.fnl[i].emerg,0);
    p=(*ft.hypo.fnl[i].pol);
    if(p=='+' || p=='U') strcpy(textbuf,"+");
    else if(p=='-' || p=='D') strcpy(textbuf,"-");
    else strcpy(textbuf,"?");
    put_text(&dpy,x-(WIDTH_TEXT-1)/2,y-(HEIGHT_TEXT-1)/2,textbuf,
      BF_SDO);
    if(mech_name) put_text(&dpy,x-(WIDTH_TEXT-1)/2-WIDTH_TEXT,
      y-(HEIGHT_TEXT-1)/2+HEIGHT_TEXT,ft.hypo.fnl[i].stn,BF_SDO);
    }
  return (1);
  }

static void
switch_psup(int idx, int sw)
  {

  switch(sw)
    {
    case 0: ft.stn[idx].psup=0;break;
    case 1: ft.stn[idx].psup=1;break; 
    default:
      if(ft.stn[idx].psup==0) ft.stn[idx].psup=1;
      else ft.stn[idx].psup=0;
      break;
    }
  put_reverse(&info,0,pixels_per_trace*ft.idx2pos[idx],WIDTH_TEXT*4,
    HEIGHT_TEXT);
  }

static void
init_psup(void)
  {

  if(flag_hypo==0)
    {
    loop=loop_stack[--loop_stack_ptr];
    raise_ttysw(1);
    list_line();
    fprintf(stderr,"no hypocenter\007\n");
    return;
    }
  if(refresh(999))
    {
    loop=loop_stack[--loop_stack_ptr];
    if(!background) refresh(0);    
    bell();
    raise_ttysw(1);
    return;
    }
  }

static void
bell(void)
  {

  if(background || auto_flag || auto_flag_hint) return;
  fprintf(stderr,"\007");
  fflush(stderr);
  }

#define t2x(t)  (pu.xx1+(int)(pu.pixels_per_sec*((double)t-pu.t1)))
#define x2y(x)  (pu.yy1+(int)(pu.pixels_per_km*((double)x-pu.x1)))
#define x2t(x)  (pu.t1+(double)(x-pu.xx1)/pu.pixels_per_sec)
#define y2x(y)  (pu.x1+(double)(y-pu.yy1)/pu.pixels_per_km)

static void
proc_psup(void)
  {
  int ring_bell,x,y,idx,i,j;
  char textbuf[30],textbuf1[30];
  double f,dist,time;

  x=event.mse_data.md_x;
  y=event.mse_data.md_y;
  ring_bell=0;
  if(event.mse_trig==MSE_MOTION)
    {
    if(x>pu.xx1 && x<pu.xx2 && y>pu.yy1 && y<pu.yy2)
      {
      dist=y2x(y);
      time=x2t(x);
      if(pu.vred) time+=dist/pu.vred;
      sprintf(textbuf,"%6.2f s  %6.1f km ",time,dist);
      }
    else strcpy(textbuf,"                    ");
    put_text(&dpy,x_func(MAP)-WIDTH_TEXT*strlen(textbuf)-HW,
      Y_LINE1,textbuf,BF_SI);
    }
  else if(event.mse_trig==MSE_BUTTON && event.mse_dir==MSE_DOWN)
    {
    ring_bell=1;
    if(y<MARGIN)
      {
      switch(get_func(x))
        {
        case RETN:
            loop=loop_stack[--loop_stack_ptr];
            if(!background) refresh(0);    
            return;
        case RFSH:
            if(refresh(999))
            {
            loop=loop_stack[--loop_stack_ptr];
            if(!background) refresh(0);    
            bell();
            return;
            }
          else ring_bell=0;
          break;
        case MAP:
            loop_stack[loop_stack_ptr++]=loop;
            loop=LOOP_MAP;
            init_map(event.mse_code);
            return;
        case COPY:
            put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
            switch(event.mse_code)
                {
                case MSE_BUTNL: hard_copy(3);break;
                case MSE_BUTNM: hard_copy(2);break;
                case MSE_BUTNR: hard_copy(1);break;
                }
            put_reverse(&dpy,x_func(COPY),0,WB,MARGIN);
            ring_bell=0;
          break;
        }
      }
    else    /* not in the function area */
      {
      if(x>pu.xx1+(pu.xx2-pu.xx1)/2-WIDTH_TEXT*12/2 && 
          x<pu.xx1+(pu.xx2-pu.xx1)/2+WIDTH_TEXT*12/2 &&
          y<pu.yy1)
      /* change vred */
        {
        f=pu_new.vred;
        switch(event.mse_code)
          {
          case MSE_BUTNL: if((f+=0.1)>9.9) f=0.0;break;
          case MSE_BUTNM:
            if(f==0.0) f=5.0;
            else f=0.0;
            break;
          case MSE_BUTNR: if((f-=0.1)<0.0) f=9.9;break;
          }
        if(fabs(f)<0.01) f=0.0;
        if(f!=pu_new.vred)
          {
          pu_new.vred=f;
          if(pu_new.vred) sprintf(textbuf,"T - D /%4.1f ",pu_new.vred);
          else sprintf(textbuf,"     T      ");
          put_text(&dpy,pu.xx1+(pu.xx2-pu.xx1)/2-WIDTH_TEXT*12/2,
            pu.yy1-MARGIN,textbuf,BF_SI);
          pu_new.valid=1;
          ring_bell=0;
          }
        }
      else if(x>pu.xx1+WIDTH_TEXT*10 && 
              x<pu.xx1+WIDTH_TEXT*(10+strlen(pu.f.tfilt)) && y<pu.yy1)
      /* change filter */
        {
        i=pu_new.filt;
        switch(event.mse_code)
          {
          case MSE_BUTNL: if(++i==ft.n_filt) i=0;break;
          case MSE_BUTNM: i=0;break;
          case MSE_BUTNR: if(--i<0) i=ft.n_filt-1;break;
          }
        if(i!=pu_new.filt)
          {
          pu_new.filt=i;
          get_filter(pu_new.filt,&pu_new.f,100,0); /* SR is dummy */
          put_text(&dpy,pu.xx1+WIDTH_TEXT*10,pu.yy1-3*MARGIN/2,
            pu_new.f.tfilt,BF_SI);
          pu_new.valid=1;
          ring_bell=0;
          }
        }
      else if(x>pu.xx2-WIDTH_TEXT*(10+12) && 
              x<pu.xx2-WIDTH_TEXT*10 && y<pu.yy1)
      /* change clip */
        {
        i=pu_new.clip;
        switch(event.mse_code)
          {
          case MSE_BUTNL: ++i;break;
          case MSE_BUTNM: i=0;break;
          case MSE_BUTNR: if(--i<0) i=0;break;
          }
        if(i!=pu_new.clip)
          {
          pu_new.clip=i;
          if(pu_new.clip) sprintf(textbuf,"CLIP = %2d km",pu_new.clip);
          else sprintf(textbuf,"  NO CLIP   ");
          put_text(&dpy,pu.xx2-WIDTH_TEXT*(strlen(textbuf)+10),
            pu.yy1-3*MARGIN/2,textbuf,BF_SI);
          pu_new.valid=1;
          ring_bell=0;
          }
        }
      else if(x<pu.xx1 && y<pu.yy1+HEIGHT_TEXT/2)
      /* change pu.x1 */
        {
        i=pu_new.x1;
        switch(event.mse_code)
          {
          case MSE_BUTNL:
            if(i<10 && i+5<pu_new.x2) i+=5;
            else if(i>=10 && i+10>pu_new.x2) i+=10;
            break;
          case MSE_BUTNM:
            if((i=pu_new.x1_init)>=pu_new.x2)
	      {
	      if(pu_new.x2==10) i=5;
	      else if((i=pu_new.x2-10)<0) i=0;
	      }
            break;
          case MSE_BUTNR:
            if(i>10) i-=10;
            else if(i==10) i=5;
            else i=0;
            break;
          }
        if(i!=pu_new.x1)
          {
          pu_new.x1=i;
          sprintf(textbuf,"%4d",pu_new.x1);
          put_text(&dpy,pu.xx1-WIDTH_TEXT*strlen(textbuf),
            pu.yy1-HEIGHT_TEXT/2,textbuf,BF_SI);
          pu_new.valid=1;
          ring_bell=0;
          }
        }
      else if(x<pu.xx1 && y>pu.yy2-HEIGHT_TEXT/2)
      /* change pu.x2 */
        {
        i=pu_new.x2;
        switch(event.mse_code)
          {
          case MSE_BUTNL:
            if(i==5) i=10;
            else i+=10;
            break;
          case MSE_BUTNM:
            if((i=pu_new.x2_init)<=pu_new.x1)
	      {
	      if(pu_new.x1<10) i=pu_new.x1+5;
	      else i=pu_new.x1+10;
	      }
            break;
          case MSE_BUTNR:
            if(i==10 && pu_new.x1<5) i=5;
            else if(i>10 && (i-=10)<=pu_new.x1) i=pu_new.x1+10;
            break;
          }
        if(i!=pu_new.x2)
          {
          pu_new.x2=i;
          sprintf(textbuf,"%4d",pu_new.x2);
          put_text(&dpy,pu.xx1-WIDTH_TEXT*strlen(textbuf),
            pu.yy2-HEIGHT_TEXT/2,textbuf,BF_SI);
          pu_new.valid=1;
          ring_bell=0;
          }
        }
      else if(x>pu.xx1 && x<pu.xx1+WIDTH_TEXT*4 && y<pu.yy1)
      /* change pu.t1 */
        {
        i=pu_new.t1;
        switch(event.mse_code)
          {
          case MSE_BUTNL:
            if((i+=1)>=pu_new.t2) i=pu_new.t2;break;
          case MSE_BUTNM: break;
          case MSE_BUTNR: i-=1;break;
          }
        if(i!=pu_new.t1)
          {
          pu_new.t1=i;
          sprintf(textbuf,"%-4d",pu_new.t1);
          put_text(&dpy,pu.xx1,pu.yy1-MARGIN,textbuf,BF_SI);
          pu_new.valid=1;
          ring_bell=0;
          }
        }
      else if(x<pu.xx2 && x>pu.xx2-WIDTH_TEXT*4 && y<pu.yy1)
      /* change pu.t2 */
        {
        i=pu_new.t2;
        switch(event.mse_code)
          {
          case MSE_BUTNL: i+=1;break;
          case MSE_BUTNM: break;
          case MSE_BUTNR: if((i-=1)<=pu_new.t1) i=pu_new.t1+1;break;
          }
        if(i!=pu_new.t2)
          {
          pu_new.t2=i;
          sprintf(textbuf,"%4d",pu_new.t2);
          put_text(&dpy,pu.xx2-WIDTH_TEXT*strlen(textbuf),
            pu.yy1-MARGIN,textbuf,BF_SI);
          pu_new.valid=1;
          ring_bell=0;
          }
        }
      else if(x<WIDTH_TEXT*7 && y>pu.yy1+HEIGHT_TEXT/2)
      /* change channel */
        {
        for(j=0;j<ft.n_ch;j++) if(ft.stn[ft.pos2idx[j]].psup)
          {
          idx=ft.pos2idx[j];
          if(y>x2y(ft.stn[idx].delta)-HEIGHT_TEXT/2 &&
              y<x2y(ft.stn[idx].delta)+HEIGHT_TEXT/2)
            {
            i=j;  /* position in mon */
            switch(event.mse_code)
              {
              case MSE_BUTNL:
                if(i<ft.n_ch)
                  {
                  if(strcmp(ft.stn[ft.pos2idx[i]].name,
                  ft.stn[ft.pos2idx[i+1]].name)==0)
                  i++;
                  }
                break;
              case MSE_BUTNM: switch_psup(idx,0);break;
              case MSE_BUTNR:
                if(i>0)
                  {
                  if(strcmp(ft.stn[ft.pos2idx[i]].name,
                  ft.stn[ft.pos2idx[i-1]].name)==0)
                  i--;
                  }
                break;
              }
            if(i!=j)
              {
              switch_psup(idx,0);
              switch_psup(ft.pos2idx[i],1);
              ft.stn[ft.pos2idx[i]].psup_scale=ft.stn[idx].psup_scale;
              ft.stn[ft.pos2idx[i]].delta=ft.stn[idx].delta;
              idx=ft.pos2idx[i];
              sprintf(textbuf1,"%s-%s",ft.stn[idx].name,ft.stn[idx].comp);
              sprintf(textbuf," %-7s%2d ",textbuf1,ft.stn[idx].psup_scale);
              put_text(&dpy,0,x2y(ft.stn[idx].delta)-
                HEIGHT_TEXT/2+1,textbuf,BF_SI);
              ring_bell=0;
              }
            if(ft.stn[idx].psup==0)
              {
              put_text(&dpy,0,x2y(ft.stn[idx].delta)-
                HEIGHT_TEXT/2+1,"           ",BF_SI);
              ring_bell=0;
              }
            break;
            }
          }
        }
      else if(x<pu.xx1 && y>pu.yy1+HEIGHT_TEXT/2)
      /* change psup_scale */
        {
        for(j=0;j<ft.n_ch;j++) if(ft.stn[ft.pos2idx[j]].psup)
          {
          idx=ft.pos2idx[j];
          if(y>x2y(ft.stn[idx].delta)-HEIGHT_TEXT/2 &&
              y<x2y(ft.stn[idx].delta)+HEIGHT_TEXT/2)
            {
            i=ft.stn[idx].psup_scale;
            switch(event.mse_code)
              {
              case MSE_BUTNL: if(i>0) i--;break;
              case MSE_BUTNM: switch_psup(idx,0);break;
              case MSE_BUTNR: if(i<SCALE_MAX) i++;break;
              }
            if(i!=ft.stn[idx].psup_scale)
              {
              ft.stn[idx].psup_scale=i;
              sprintf(textbuf1,"%s-%s",ft.stn[idx].name,ft.stn[idx].comp);
              sprintf(textbuf," %-7s%2d ",textbuf1,ft.stn[idx].psup_scale);
              put_text(&dpy,0,x2y(ft.stn[idx].delta)-
                HEIGHT_TEXT/2+1,textbuf,BF_SI);
              ring_bell=0;
              }
            if(ft.stn[idx].psup==0)
              {
              put_text(&dpy,0,x2y(ft.stn[idx].delta)-
                HEIGHT_TEXT/2+1,"           ",BF_SI);
              ring_bell=0;
              }
            break;
            }
          }
        }
      }
    }
  else if(event.mse_trig==MSE_EXP)
    {
    ring_bell=1;
    if(refresh(999))
      {
      loop=loop_stack[--loop_stack_ptr];
      if(!background) refresh(0);    
      bell();
      return;
      }
    else ring_bell=0;
    }
  if(ring_bell) bell();
  }

static int
put_psup(void)
  {
  int i,j,k,n=0;
  char textbuf[LINELEN];
  double xd,yd,alat1,along1;
  float delta_max=0.0,delta_min=0.0;

  fflush(stderr);
  put_white(&dpy,0,0,width_dpy,height_dpy); /* clear */
  put_function_psup();
  for(i=0;i<7;i++) pu.ot[i]=ft.hypo.tm_c[i];
  put_text(&dpy,0,Y_LINE1,ft.hypo.textbuf,BF_SI);
  /* calculate epicentral distances for marked channels */
  k=1;
  for(j=0;j<ft.n_ch;j++) if(ft.stn[ft.pos2idx[j]].psup)
    {
    if(ft.stn[ft.pos2idx[j]].north!=0.0 &&
        ft.stn[ft.pos2idx[j]].east!=0.0)
      {
      alat1=(double)ft.stn[ft.pos2idx[j]].north;
      along1=(double)ft.stn[ft.pos2idx[j]].east;
      }
    pltxy(ft.hypo.alat,ft.hypo.along,&alat1,&along1,&xd,&yd,0);
    /* ft.stn[ft.pos2idx[j]].delta=(float)sqrt(xd*xd+yd*yd); */
    ft.stn[ft.pos2idx[j]].delta=(float)hypot(xd, yd);
    if((i=450-(int)(180.0*atan2(yd,xd)/PI))>=360) i-=360;
    ft.stn[ft.pos2idx[j]].azimuth=i;
    ft.stn[ft.pos2idx[j]].psup_done=0;
    if(k)
      {
      delta_max=delta_min=ft.stn[ft.pos2idx[j]].delta;
      k=0;
      }
    else if(ft.stn[ft.pos2idx[j]].delta > delta_max)
      delta_max=ft.stn[ft.pos2idx[j]].delta;
    else if(ft.stn[ft.pos2idx[j]].delta < delta_min)
      delta_min=ft.stn[ft.pos2idx[j]].delta;
    if(delta_max-delta_min>=50.0)
      {
      pu.x1_init=((int)(delta_min-5.0)/10)*10;
      pu.x2_init=((int)(delta_max+15.0)/10)*10;
      }
    else
      {
      pu.x1_init=((int)delta_min/10)*10;
      pu.x2_init=((int)(delta_max+10.0)/10)*10;
      }
    }

  if(k)
    {
    fprintf(stderr,"no channels selected for paste-up\007\n");
    return(1);
    }

  if(pu.valid==0)
    {
    pu.x1=pu.x1_init;
    pu.x2=pu.x2_init;
    pu.xx1=PSUP_LMARGIN;
    pu.yy1=MARGIN+PSUP_TMARGIN;
    pu.clip=0;
    pu.filt=0;
    pu.valid=1;
    pu.t1=0;
    pu.t2=20;
    if(delta_max<50) pu.vred=6.0;
    else pu.vred=8.0;
    }
  if(pu_new.valid) pu=pu_new;
  pu.xx2=width_dpy-PSUP_RMARGIN;
  pu.yy2=height_dpy-PSUP_BMARGIN;

  pu.pixels_per_sec=(pu.xx2-pu.xx1)/(double)(pu.t2-pu.t1);
  pu.pixels_per_km=(pu.yy2-pu.yy1)/(double)(pu.x2-pu.x1);
  pu_new=pu;
  pu_new.valid=0;

  /* draw axes */
  draw_seg(t2x(pu.t1),x2y(pu.x1),t2x(pu.t1),x2y(pu.x2),LPTN_FF,BF_SDO,&dpy);
  draw_seg(t2x(pu.t1),x2y(pu.x2),t2x(pu.t2),x2y(pu.x2),LPTN_FF,BF_SDO,&dpy);
  draw_seg(t2x(pu.t2),x2y(pu.x2),t2x(pu.t2),x2y(pu.x1),LPTN_FF,BF_SDO,&dpy);
  draw_seg(t2x(pu.t2),x2y(pu.x1),t2x(pu.t1),x2y(pu.x1),LPTN_FF,BF_SDO,&dpy);
  /* draw ticks */
  draw_ticks(0,x2y(pu.x1),t2x(pu.t1),t2x(pu.t2)-t2x(pu.t1),pu.t1,pu.t2,1);
  draw_ticks(0,x2y(pu.x2),t2x(pu.t1),t2x(pu.t2)-t2x(pu.t1),pu.t1,pu.t2,-1);
  draw_ticks(1,t2x(pu.t1),x2y(pu.x1),x2y(pu.x2)-x2y(pu.x1),pu.x1,pu.x2,1);
  draw_ticks(1,t2x(pu.t2),x2y(pu.x1),x2y(pu.x2)-x2y(pu.x1),pu.x1,pu.x2,-1);
  if(pu.vred) sprintf(textbuf,"T - D /%4.1f ",pu.vred);
  else sprintf(textbuf,"     T      ");
  put_text(&dpy,pu.xx1+(pu.xx2-pu.xx1)/2-WIDTH_TEXT*12/2,
    pu.yy1-MARGIN,textbuf,BF_S);

  if(pu.clip) sprintf(textbuf,"CLIP = %2d km",pu.clip);
  else sprintf(textbuf,"  NO CLIP   ");
  put_text(&dpy,pu.xx2-WIDTH_TEXT*(strlen(textbuf)+10),
    pu.yy1-3*MARGIN/2,textbuf,BF_S);

  sprintf(textbuf,"%-4d",pu.t1);
  put_text(&dpy,pu.xx1,pu.yy1-MARGIN,textbuf,BF_S);
  sprintf(textbuf,"%4d",pu.t2);
  put_text(&dpy,pu.xx2-WIDTH_TEXT*strlen(textbuf),pu.yy1-MARGIN,textbuf,BF_S);
  sprintf(textbuf,"%4d",pu.x1);
  put_text(&dpy,pu.xx1-WIDTH_TEXT*strlen(textbuf),
    pu.yy1-HEIGHT_TEXT/2,textbuf,BF_S);
  sprintf(textbuf,"%4d",pu.x2);
  put_text(&dpy,pu.xx1-WIDTH_TEXT*strlen(textbuf),
    pu.yy2-HEIGHT_TEXT/2,textbuf,BF_S);
  put_text(&dpy,pu.xx2,pu.yy1-HEIGHT_TEXT/2,"   km  deg",BF_SDO);

  /* plot traces */
  for(;;)
    {
    k=1;
    for(j=0;j<ft.n_ch;j++)
       if(ft.stn[ft.pos2idx[j]].psup &&
          !ft.stn[ft.pos2idx[j]].psup_done)
        {
        if(k==1)  {n=j; k=0;}
        else if(ft.stn[ft.pos2idx[j]].delta <=
            ft.stn[ft.pos2idx[n]].delta) n=j;
        }
    if(k) break;
    plot_psup(ft.pos2idx[n]);
    ft.stn[ft.pos2idx[n]].psup_done=1;
    }
  put_text(&dpy,pu.xx1+WIDTH_TEXT*10,pu.yy1-3*MARGIN/2,pu.f.tfilt,BF_S);
  return(0);
  }

static void
plot_psup(int idx)
  {
  double x0,uv[MAX_FILT*4],tred;
  int yy0,ylim1,ylim2,i,j,start,join,np,np_last=0,sec,xzero,
    tm[6],tm1[7],tm2[7],y,
    tred_s,tred_ms,cp1,cp2;
  int32_w  zero=0;
  WIN_sr  sr,sr_start,sr_end;
  char textbuf[30],textbuf1[30];

  x0=ft.stn[idx].delta;
  if(x0<(double)pu.x1 || x0>(double)pu.x2) return;
  yy0=x2y(x0);      /* zero line */
  ylim1=x2y(x0-(double)pu.clip);  /* clip level (-) */
  ylim2=x2y(x0+(double)pu.clip);  /* clip level (+) */

  /* print info */
  sprintf(textbuf1,"%s-%s",ft.stn[idx].name,ft.stn[idx].comp);
  sprintf(textbuf," %-7s%2d ",textbuf1,ft.stn[idx].psup_scale);
  put_text(&dpy,0,yy0-HEIGHT_TEXT/2+1,textbuf,BF_SDO);
  sprintf(textbuf,"%6.1f %3d",x0,ft.stn[idx].azimuth);
  put_text(&dpy,pu.xx2,yy0-HEIGHT_TEXT/2+1,textbuf,BF_SDO);

  /* obtain time range */
  if(pu.vred) tred=x0/pu.vred;
  else tred=0.0;
  tred_s=(int)tred+pu.t1;
  tred_ms=(int)(tred*1000.0)%1000;
  for(i=0;i<7;i++) tm1[i]=pu.ot[i];

  lsec2time(time2lsec(tm1)+tred_s,tm1);
  if((tm1[6]+=tred_ms)>=1000) {tm1[6]-=1000;lsec2time(time2lsec(tm1)+1,tm1);}
  for(i=0;i<7;i++) tm2[i]=tm1[i];
  lsec2time(time2lsec(tm2)+(pu.t2-pu.t1),tm2);

  /* reduced time range : tm1 - tm2 */
  join=0;
  start=1;
  for(i=0;i<ft.len;i++)
    {
    bcd_dec(tm,ft.ptr[i].time);
    cp1=time_cmp_win(tm,tm1,6);
    cp2=time_cmp_win(tm,tm2,6);
    if(cp1<0) continue;
    if(cp2>0) break;
  /* obtain plot position (time) xzero */
    sec=(int)(time2lsec(tm)-time2lsec(tm1));
    xzero=pu.xx1+pu.pixels_per_sec*sec-pu.pixels_per_sec*tm1[6]/1000;

    sr=read_one_sec((int32_w)i,ft.idx2ch[idx],buf,NOT_KILL_SPIKE);
    if(sr>0)
      {
      if(cp1==0) {sr_start=sr*tm1[6]/1000;sr_end=sr;}
      else if(cp2==0) {sr_start=0;sr_end=sr*tm2[6]/1000;}
      else {sr_start=0;sr_end=sr;}
      if(start)
        {
        /* get filter coefs */
        get_filter(pu.filt,&pu.f,sr,0);
        if(pu.filt) for(j=0;j<pu.f.m_filt*4;j++) uv[j]=0.0;
        if(1) /* always remove offset */
          {
          zero=0;
          for(j=0;j<sr;j++) zero+=buf[j];
          zero/=sr;  /* if sr is unsinged and zero < 0 , must (int)sr. */
          }
        else zero=0;
        start=0;
        }
      if(join) points[0]=points[np_last-1];
      np=join;
      for(j=0;j<sr;j++) buf[j]-=zero;
      /* filtering */
      if(pu.filt)
        {
        for(j=0;j<sr;j++) dbuf[j]=(double)buf[j];
        tandem(dbuf,dbuf,(int)sr,pu.f.coef,pu.f.m_filt,1,uv);
        for(j=0;j<sr;j++) buf[j]=(int32_w)(dbuf[j]*pu.f.gn_filt);
        }
      /* plot */
      for(j=sr_start;j<sr_end;j++)
        {
        points[np].x=xzero+(pu.pixels_per_sec*j+(sr>>1))/sr;
        y=yy0-(buf[j]>>ft.stn[idx].psup_scale);
        if(pu.clip)
          {
          if(y<ylim1)   points[np++].y=ylim1;
          else if(y>ylim2) points[np++].y=ylim2;
          else points[np++].y=y;
          }
        else points[np++].y=y;
        }
      draw_line(points,np,LPTN_FF,BF_SDO,&dpy,0,0,width_dpy,height_dpy,0);
      join=1;
      np_last=np;
      }
    else join=0;
    }
  }

static int
save_data(int private)
  {
  FILE *fp,*fq;
  int i,j,k,year,month,day,hour,minute,tm[6];
  char textbuf[LINELEN],filename[NAMLEN],filename1[NAMLEN],
    *ptr,textbuf1[LINELEN];
  float sec;

  if(not_save) return (0);   /* Is it OK? */
  /* if already loaded, delete previous file */
  if(*ft.save_file)
    {
    sprintf(filename,"%s/%s",ft.hypo_dir,ft.save_file);
    sprintf(filename1,"%s/%s",ft.hypo_dir1,ft.save_file);
    if((fp=fopen(filename,"r+"))==0) /* check 'write' permission */
      {
      fprintf(stderr,"you can't delete PRIVATE pick file '%s'\n",filename);
      return (0);
      }
    else fclose(fp);
    if(!just_hypo)
      {
      if(*ft.hypo_dir1 && !unlink(filename1))
        fprintf(stderr,"deleted pick file '%s'\n",filename1);
      if(unlink(filename))
        {
        fprintf(stderr,"can't delete pick file '%s'\n",filename);
        return (0);
        }
      else
        {
        fprintf(stderr,"deleted pick file '%s'\n",filename);
        if(mailer_flag)   /* cancel (delete) */
          {
          sprintf(textbuf,"echo '- %s' | lpr -P%s",ft.save_file,ft.mailer);
          system(textbuf);
          fprintf(stderr,"canceled submission to '%s'\n",ft.mailer);
          }
        }
      }
    }

  /* search picks */
  k=0;
  for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++) if(ft.pick[i][j].valid) k=1;
  if(k==0 && diagnos[1]==' ')
    {
    *ft.save_file=0;
    return (0);
    }
  flag_change=0;

  /* make save file name */
  /*  if(*ft.save_file==0 && !just_hypo) */
  if(!just_hypo)
    {
    if(k) /* there is at least one pick */
      {
      /* get the earliest P time from the 'seis' file */
      fp=fopen(ft.seis_file,"w+");
      output_pick(fp);
      fseek(fp,0L,0);   /* rewind file */
      fgets(textbuf,LINELEN,fp);
      sscanf(textbuf,"%2d/%2d/%2d %2d:%2d",&year,&month,&day,&hour,&minute);
      fgets(textbuf,LINELEN,fp);
      sscanf(textbuf,"%*s%*s%f",&sec); /* read the earliest P time */
      fclose(fp);
      if(*ft.save_file==0)
        {
	if(sec==0.0) /* 2001.6.7 for picks only with max ampl. */
	  sprintf(ft.save_file,"%02X%02x%02x%1c%02X%02x%02x.000",
	    ft.ptr[0].time[0],ft.ptr[0].time[1],ft.ptr[0].time[2],dot,
	    ft.ptr[0].time[3],ft.ptr[0].time[4],ft.ptr[0].time[5]);
	else
	  sprintf(ft.save_file,"%02d%02d%02d%1c%02d%02d%02d.%03d",
	    year,month,day,dot,hour,minute,(int)sec,(int)(sec*1000.0)%1000);
	}
      }
    else /* there is no pick */
      {
      if(*ft.save_file==0)
        {
        sprintf(ft.save_file,"%02X%02x%02x%1c%02X%02x%02x.000",
          ft.ptr[0].time[0],ft.ptr[0].time[1],ft.ptr[0].time[2],dot,
          ft.ptr[0].time[3],ft.ptr[0].time[4],ft.ptr[0].time[5]);
	}
      }
    }

  /* open save(hypo) file */
  sprintf(filename,"%s/%s",ft.hypo_dir,ft.save_file);
  if((fp=fopen(filename,"w+"))==NULL)
    fprintf(stderr,"file '%s' not open to save\n",filename);
  else
    {
    /* pick */
    if((ptr=strrchr(ft.data_file,'/'))==NULL) ptr=ft.data_file;
    else ptr++;
    fprintf(fp,"#p %s",ptr);
    *textbuf=(*textbuf1)=0;
    sscanf(diagnos,"%s%s",textbuf,textbuf1);
    if(diagnos[1]==' ' && *textbuf)
      {
      strcpy(textbuf1,textbuf);
      *textbuf=0;
      }
    if(*textbuf==0) strcpy(textbuf,".");
    fprintf(fp," %s %s\n",textbuf,textbuf1);
    bcd_dec(tm,ft.ptr[0].time);
    fprintf(fp,"#p %02d %02d %02d %02d %02d %02d\n",
      tm[0],tm[1],tm[2],tm[3],tm[4],tm[5]);
    if(k)
      {
      for(i=0;i<ft.n_ch;i++) for(j=0;j<4;j++)
        {
        if(ft.pick[i][j].valid)
          {
          fprintf(fp,"#p %04X %d %02d %03d %02d %03d %+d",
            ft.idx2ch[i],j,ft.pick[i][j].sec1,ft.pick[i][j].msec1,
            ft.pick[i][j].sec2,ft.pick[i][j].msec2,ft.pick[i][j].polarity);
          if(j==MD) fprintf(fp," %.2e\n",*(float *)&ft.pick[i][j].valid);
          else fprintf(fp,"\n");
          }
        }
    /* seis */
      fq=fopen(ft.seis_file,"r");
      while(fgets(textbuf,LINELEN,fq)!=NULL) fprintf(fp,"#s %s",textbuf);
      fclose(fq);
      }

    /* final */
    if(flag_hypo)
      {
      fq=fopen(ft.finl_file,"r");
      while(fgets(textbuf,LINELEN,fq)!=NULL) fprintf(fp,"#f %s",textbuf);
      fclose(fq);
      }

    /* mecha */
    if(flag_mech)
      {
      fq=fopen(ft.mech_file,"r");
      while(fgets(textbuf,LINELEN,fq)!=NULL) fprintf(fp,"#m %s",textbuf);
      fclose(fq);
      }
    fclose(fp);
    if(*ft.hypo_dir1)   /* save into the second pick directory */
      {
      sprintf(filename1,"%s/%s",ft.hypo_dir1,ft.save_file);
      sprintf(textbuf,"cp %s %s",filename,filename1);
      if(system(textbuf)==0)
        fprintf(stderr,"saved to pick file '%s'\n",filename1);
      }
    if(private)
      {
      chmod(filename,0644);
      fprintf(stderr,"saved to PRIVATE pick file '%s'\n",filename);
      }
    else
      {
      if(!just_hypo) chmod(filename,0664);
      fprintf(stderr,"saved to pick file '%s'\n",filename);
      }
    if(mailer_flag)   /* submit */
      {
      sprintf(textbuf,"(echo '+ %s';cat %s) | lpr -P%s",
        ft.save_file,filename,ft.mailer);
      system(textbuf);
      fprintf(stderr,"submitted to '%s'\n",ft.mailer);
      }
    }
  return (1);
  }

static int
read_parameter(int n, char *tbuf)
  {
  FILE *fp;
  int i;
  char text_buf[LINELEN];

  if((fp=open_file(ft.param_file,"parameter"))==NULL) return (0);
  i=0;
  for(;;)
    {
    if(fgets(text_buf,LINELEN,fp)==NULL)
      {
      fprintf(stderr,"parameter #%d not found in param file '%s'\007\n",
        n,ft.param_file);
      return (0);
      }
    if(*text_buf=='#') continue;
    if(++i==n) break;
    }
  fclose(fp);
  *tbuf=0;
  if(sscanf(text_buf,"%s",tbuf)!=1) return (0);
  return (1);
  }

static int
read_filter_file(void)
  {
  FILE *fp,*fc;
  int i,j;
  char text_buf[LINELEN];

  if((fp=open_file(ft.filt_file,"filter"))==NULL) return (0);
  strcpy(ft.filt[0].kind,"APF");
  i=1;
  while(i<N_FILTERS)
    {
    if(fgets(text_buf,LINELEN,fp)==NULL) break;
    if(*text_buf=='#') continue;
    ft.filt[i].order=0;
    if(strchr(text_buf,'/'))
      {
      *strchr(text_buf,'\n')=0;
      if((fc=open_file(text_buf,"filter coefs"))==NULL) continue;
      strncpy(ft.filt[i].kind,strrchr(text_buf,'/')+1,11);
      j=0;
      while(fgets(text_buf,LINELEN,fc))
        {
        if(*text_buf=='#') continue;
        j++;
        }
      rewind(fc);
      if(j>MAX_FILT*4)
        {
        fprintf(stderr,"Too many filter coefs given (%d)\007\007\n",j);
        ft.filt[i].order=MAX_FILT*4;
        }
      else ft.filt[i].order=j;
      ft.filt[i].coef=(double *)win_xmalloc(sizeof(double)*j);
      j=0;
      while(fgets(text_buf,LINELEN,fc))
        {
        if(*text_buf=='#') continue;
        sscanf(text_buf,"%lf",&ft.filt[i].coef[j++]);
        }
      fclose(fc);
      i++;
      continue;
      }
    sscanf(text_buf,"%3s",ft.filt[i].kind);
    if(strcmp(ft.filt[i].kind,"LPF")==0 || strcmp(ft.filt[i].kind,"lpf")==0)
      strcpy(ft.filt[i].kind,"LPF");
    else if(strcmp(ft.filt[i].kind,"HPF")==0 || strcmp(ft.filt[i].kind,"hpf")==0)
      strcpy(ft.filt[i].kind,"HPF");
    else if(strcmp(ft.filt[i].kind,"BPF")==0 || strcmp(ft.filt[i].kind,"bpf")==0)
      strcpy(ft.filt[i].kind,"BPF");
    else
      {
      fprintf(stderr,"bad filter name '%s'\007\007\n",ft.filt[i].kind);
      continue;
      }
    for(j=0;j<strlen(text_buf)-3;j++)
      {
      if(strncmp(text_buf+j,"fl=",3)==0)
        sscanf(text_buf+j+3,"%lf",&ft.filt[i].fl);
      else if(strncmp(text_buf+j,"fh=",3)==0)
        sscanf(text_buf+j+3,"%lf",&ft.filt[i].fh);
      else if(strncmp(text_buf+j,"fp=",3)==0)
        sscanf(text_buf+j+3,"%lf",&ft.filt[i].fp);
      else if(strncmp(text_buf+j,"fs=",3)==0)
        sscanf(text_buf+j+3,"%lf",&ft.filt[i].fs);
      else if(strncmp(text_buf+j,"ap=",3)==0)
        sscanf(text_buf+j+3,"%lf",&ft.filt[i].ap);
      else if(strncmp(text_buf+j,"as=",3)==0)
        sscanf(text_buf+j+3,"%lf",&ft.filt[i].as);
      }
    if(!(strcmp(ft.filt[i].kind,"LPF")==0 &&
        ft.filt[i].fp<ft.filt[i].fs) &&
        !(strcmp(ft.filt[i].kind,"HPF")==0 &&
        ft.filt[i].fs<ft.filt[i].fp) &&
        !(strcmp(ft.filt[i].kind,"BPF")==0 &&
        ft.filt[i].fl<ft.filt[i].fh &&
        ft.filt[i].fh<ft.filt[i].fs))
      {
      fprintf(stderr,"%d:%s %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f",
        i,ft.filt[i].kind,ft.filt[i].fl,ft.filt[i].fh,
        ft.filt[i].fp,ft.filt[i].fs,ft.filt[i].ap,ft.filt[i].as);
      fprintf(stderr," : illegal filter\007\007\n");
      }
    i++;
    }
  fclose(fp);
  return (i);
  }

static int
read_label_file(void)
  {
  FILE *fp;
  int i;
  char text_buf[LINELEN];

  if((fp=open_file(ft.label_file,"label"))==NULL) return (0);
  *ft.label[0]=0;
  i=1;
  while(fgets(text_buf,LINELEN,fp))
    {
    if(*text_buf=='#') continue;
    *strchr(text_buf,'\n')=text_buf[19]=0;
    strcpy(ft.label[i],text_buf);
    if(++i==N_LABELS) break;
    }
  fclose(fp);
  return (i);
  }

static void
get_filter(int filt, struct Filt *f, WIN_sr sr, int iz)
  {
  double dt,*x,zero;
  struct Pick_Time pt;
  int n,idx,i;
  char tbuf1[3],tbuf2[3];

  dt=1.0/(double)sr;
  if(filt==0) strcpy(f->tfilt,"  NO FILTER  ");
  else if(filt<0)
    {
    idx=ft.ch2idx[zoom_win[iz].sys_ch];
    if(ft.pick[idx][X].valid && get_width(&ft.pick[idx][X])>0)
      pt=ft.pick[idx][X];
    else set_pick(&pt,zoom_win[iz].sec,500,500,500);
    n=getdata(idx,pt,&x,&i);
    getar(x,n,&f->gn_filt,&f->m_filt,f->coef,&zero,0);
    sprintf(f->tfilt," AR ORDER=%-2d ",f->m_filt);
    FREE(x);
    }
  else if(ft.filt[filt].order>0)
    {
    f->m_filt=ft.filt[filt].order;
    f->n_filt=0;  /* indicates that coefs given */
    for(i=0;i<ft.filt[filt].order;i++) f->coef[i]=ft.filt[filt].coef[i];
    sprintf(f->tfilt,"%2d %-10.10s",filt,ft.filt[filt].kind);
    }
  else if(strcmp(ft.filt[filt].kind,"LPF")==0)
    {
    form2(ft.filt[filt].fp,tbuf1);
    sprintf(f->tfilt,"%2d %.2s   -%sHz",filt,ft.filt[filt].kind,tbuf1);
    butlop(f->coef,&f->m_filt,&f->gn_filt,&f->n_filt,
      ft.filt[filt].fp*dt,ft.filt[filt].fs*dt,ft.filt[filt].ap,
      ft.filt[filt].as);
    }
  else if(strcmp(ft.filt[filt].kind,"HPF")==0)
    {
    form2(ft.filt[filt].fp,tbuf1);
    sprintf(f->tfilt,"%2d %.2s %s-  Hz",filt,ft.filt[filt].kind,tbuf1);
    buthip(f->coef,&f->m_filt,&f->gn_filt,&f->n_filt,
      ft.filt[filt].fp*dt,ft.filt[filt].fs*dt,ft.filt[filt].ap,
      ft.filt[filt].as);
    }
  else if(strcmp(ft.filt[filt].kind,"BPF")==0)
    {
    form2(ft.filt[filt].fl,tbuf1);
    form2(ft.filt[filt].fh,tbuf2);
    sprintf(f->tfilt,"%2d %.2s %s-%sHz",filt,ft.filt[filt].kind,tbuf1,tbuf2);
    butpas(f->coef,&f->m_filt,&f->gn_filt,&f->n_filt,
      ft.filt[filt].fl*dt,ft.filt[filt].fh*dt,
      ft.filt[filt].fs*dt,ft.filt[filt].ap,ft.filt[filt].as);
    }
  if((filt>0 && ft.filt[filt].order>0 && f->m_filt>MAX_FILT*4) ||
      (filt>0 && ft.filt[filt].order==0 && f->m_filt>MAX_FILT) ||
      (filt<0 && f->m_filt>MAX_FILT*4))
    {
    fprintf(stderr,"*** filter order exceeded limit ***\007\007\n");
    exit(1);
    }
  }

static int
form2(double s, char *d)
  {
  char tbuf[8];

  if(s>=100.0 || s<0.0) return (0);
  sprintf(tbuf,"%6.2f",s);
  if(s<1.0) sprintf(d,"%.2s",tbuf+3);
  else sprintf(d,"%.2s",tbuf+1);
  return (1);
  }

static int
getar(double *x, int n, double *sd, int *m, double *c, double *z, int lh)
  /* double *x;    input data */
  /* int n;        n of data */
  /* double *sd;   standard deviation */
  /* int *m;       order of filter */
  /* double *c;    filter coefficients */
  /* double *z;    mean level */
  /* int lh;       maximum lag */
  {
  double fpe,*cxx;
  int lagh;

  lagh=(int)(3.0*sqrt((double)n));
  if(lh>0 && lh<lagh) lagh=lh;
  if ((cxx=(double *)win_xmalloc(sizeof(double)*(lagh+1))) == NULL)
    emalloc("getar:cxx");
  autcor(x,n,lagh,cxx,z);
  if (fpeaut(lagh,n,lagh,cxx,&fpe,sd,m,c) == 1)
    emalloc("fpeaut : a");
  else if (fpeaut(lagh,n,lagh,cxx,&fpe,sd,m,c) == 2)
    emalloc("fpeaut : b");
  if(*sd<0.0) *sd=0.0;
  FREE(cxx);
  if(*m==lagh) return (-1);
  return (*m);
  }

static void
digfil(double *x, double *y, int n, double *c, int m, double *r, double *sd)
  /* double *x;  input data */
  /* double *y;  output data */
  /* int n;       n of data */
  /* double *c;   filter coefficients */
  /* int m;       order of filter */
  /* double *r;   last m data */
  /* double *sd;  sd (mean-square of residuals) */
  {
  int i,j,k;
  double t,s;

  for(i=0;i<n;i++)
    {
    t=0.0;
    if((k=m-i)<0) k=0;
    for(j=0;j<k;j++) t+=c[m-j-1]*r[j+i];
    for(j=k;j<m;j++) t+=c[m-j-1]*x[j+i-m];
    s=((y[i]=t)-x[i]);
    *sd+=s*s;
    }
  for(i=0;i<m;i++) r[i]=x[n-m+i];
  *sd/=(double)n;
  }

/**** library for handling matrices (URABE) ****/
static void
mat_sym(double (*mat)[3])
  {

  int i,j;
  for(i=1;i<3;i++) for(j=0;j<i;j++) mat[i][j]=mat[j][i];
  }

static void
mat_copy(double (*mat1)[3], double(*mat2)[3])
  {

  int i,j;
  for(i=0;i<3;i++) for(j=0;j<3;j++) mat1[i][j]=mat2[i][j];
  }

static void
mat_mul(double (*mat)[3], double (*mat1)[3], double (*mat2)[3])
  {
  int i,j,k;
  double mat_r[3][3];

  for(i=0;i<3;i++) for(j=0;j<3;j++)
    {
    mat_r[i][j]=0.0;
    for(k=0;k<3;k++) mat_r[i][j]+=mat1[i][k]*mat2[k][j];
    }
  mat_copy(mat,mat_r);
  }

/*
static void
mat_print(char *textbuf, double (*mat)[3])
  {
  int i;

  fprintf(stderr,"%s\n",textbuf);
  for(i=0;i<3;i++)
    fprintf(stderr,"   { %+9.2e  %+9.2e  %+9.2e }\n",
      mat[i][0],mat[i][1],mat[i][2]);
  }
*/

static void
get_mat(double cs, double sn, double (*mat)[3])
  {

  mat[0][0]=mat[1][1]=cs;
  mat[0][1]=(-sn);
  mat[1][0]=sn;
  mat[2][2]=1;
  mat[0][2]=mat[1][2]=mat[2][0]=mat[2][1]=0;
  }

static long
time2long(int ye, int mo, int da, int ho, int mi)
  {
  register int i,j;
  static int dm[13]={0,31,28,31,30,31,30,31,31,30,31,30,31};

  if(ye<25) ye+=100;
  j=0;
  for(i=1;i<mo;i++) j+=dm[i]; /* days till the previous month */
  if(!(ye&0x3) && mo>2) j++;  /* leap year */
  j+=da-1+ye*365+((ye+3)>>2); /* days */
  return (j*(24*60)+ho*60+mi);
  }

static void
long2time(struct YMDhms *tm, long *tl)
  {
  register int j,k,*d;
  long i;
  static int dm[13]={0,31,28,31,30,31,30,31,31,30,31,30,31},
    dml[13]={0,31,29,31,30,31,30,31,31,30,31,30,31},
    dy[4]={366,365,365,365};

  i=(*tl);
  tm->mi=i%60;
  tm->ho=(i%(60*24))/60;
  k=i/(60*24);
  j=k/(365*4+1);
  tm->ye=j*4;
  k-=j*(365*4+1);
  i=0;
  while(k>=dy[i])
    {
    k-=dy[i++];
    tm->ye++;
    }
  if(i==0) d=dml;
  else d=dm;
  i=1;
  while(k>=d[i]) k-=d[i++];
  tm->da=(++k);
  tm->mo=i;
  if(tm->ye>99) tm->ye-=100;
  }

static int
time_cmp_win(int *t1, int *t2, int i)
  {
  int cntr;

  cntr=0;
  if(t1[cntr]<25 && t2[cntr]>25) return (1);
  if(t1[cntr]>25 && t2[cntr]<25) return (-1);
  for(;cntr<i;cntr++)
    {
    if(t1[cntr]>t2[cntr]) return (1);
    if(t1[cntr]<t2[cntr]) return (-1);
    } 
  return (0);  
  }

/*
bcd_dec(dest,sour)
  unsigned char *sour;
  unsigned int *dest;
  {
  int cntr;
  for(cntr=0;cntr<6;cntr++)
    dest[cntr]=((sour[cntr]>>4)&0xf)*10+(sour[cntr]&0xf);
  return 0;
  }
*/

static void
fill(int *buffer, int count, int data)
  {

  while(count-->0) *buffer++=data;
  }

static void
emalloc(char *mes)
  {
  char tb[LINELEN];

  sprintf(tb,"malloc failed ! (%s)",mes);
  fprintf(stderr,"%s\007\n",tb);
  writelog(tb);
  end_process(1);
  }

static void
writelog(char *mes)
  {

  if(ft.fp_log==NULL)
    {
    ft.fp_log=fopen(ft.log_file,"a+");
    fprintf(ft.fp_log,"%s %s\n",get_time_win(0,0),mes);
    fclose(ft.fp_log);
    ft.fp_log=NULL;
    }
  else fprintf(ft.fp_log,"%s %s\n",get_time_win(0,0),mes);
  }
