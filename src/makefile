# $Id: makefile,v 1.11 2001/03/09 07:50:27 urabe Exp $
#########################
#                       #
# win system makefile   #
#  2000.4.24  urabe     #
#                       #
#########################
#
##### COMMON MACROS #####
BINDIR = ../bin
#########################

#### <Solaris2.X>
#CFLAGS=-w -O
#XF=-I/usr/openwin/include -L/usr/openwin/lib
#SF=-lsocket -lnsl
#ONL_SOL2=recvs sends

#### <SUN-OS4.X>
#CFLAGS=-O -Bstatic -Dconst= -DSUNOS4
#XF=-I/usr/openwin/include -L/usr/openwin/lib

#### <FreeBSD & Linux>
CFLAGS=-w -O
XF=-I/usr/X11R6/include -L/usr/X11R6/lib
ONL_X86=recvts

#### <SGI IRIX>
#CFLAGS=-D__SVR4 -n32 -signed -w -O

#### <NEWS-OS 4.X>
#CFLAGS=-Dconst=


ONLINE_SO=recvt send_raw recvstatus send_raw_old relay $(ONL_SOL2)
TAPE_USER_MATH=pmon elist events
ONLINE_MATH=ecore
OFFLINE_USER_MATH=wtime
OFFLINE_MATH=dewin select_stations
#OFFLINE_MATH=dewin select_stations hypomhc
OFFLINE_F77=hypomh

OFFLINE_USER=win $(OFFLINE_F77) wed wck wadd $(OFFLINE_MATH) wchch pick2tseis \
             wch pick2finalb pick2final finalb2final final2finalb finalb2seis \
             wform $(OFFLINE_USER_MATH)
TAPE_USER=rtape fromtape wdisk wdiskts ras2rpdl ras2lips file_older raw2mon \
          $(TAPE_USER_MATH)
ONLINE=order raw_mon wtape setexb timeout raw_raw raw_100 cormeisei shmdump \
       raw_ch extraw order2 insert_raw insert_trg find_picks \
       $(ONLINE_SO) $(ONLINE_MATH) $(ONL_X86)

ONL_OPT_=robot
OTHR=cpdat sepline joint order_raw
OTHR_=rund
SUB=winform win2fix

WIN_INC = win_system.h
WIN_OBJ = win_system.o

all : $(OFFLINE_USER) $(TAPE_USER) $(ONLINE)
	@cp -p $(OFFLINE_USER) $(TAPE_USER) $(ONLINE) $(BINDIR)
	@cd $(BINDIR);rm -f sendt_raw_old;ln -s send_raw_old sendt_raw_old
	@cd $(BINDIR);rm -f sendt_mon_old;ln -s send_raw_old sendt_mon_old
	@cd $(BINDIR);rm -f send_mon_old;ln -s send_raw_old send_mon_old
	@cd $(BINDIR);rm -f sendt_raw;ln -s send_raw sendt_raw
	@cd $(BINDIR);rm -f sendt_mon;ln -s send_raw sendt_mon
	@cd $(BINDIR);rm -f send_mon;ln -s send_raw send_mon
	@cd $(BINDIR);rm -f wdisk60;ln -s wdisk wdisk60

tape_user : $(OFFLINE_USER) $(TAPE_USER)
	@cp -p $(OFFLINE_USER) $(TAPE_USER) $(BINDIR)
	@cd $(BINDIR);rm -f wdisk60;ln -s wdisk wdisk60

offline : $(OFFLINE_USER)
	@cp -p $(OFFLINE_USER) $(BINDIR)

clean :
	@rm -f  $(OFFLINE_USER) $(TAPE_USER) $(ONLINE) *.o

cleanbin :
	@cd $(BINDIR);rm -f  $(OFFLINE_USER) $(TAPE_USER) $(ONLINE) \
          send_* sendt_* wdisk60

$(ONLINE_SO) :
	cc $@.c $(CFLAGS) $(SF) -o $@

$(TAPE_USER_MATH) $(ONLINE_MATH) $(OFFLINE_MATH) $(OFFLINE_USER_MATH):
	cc $@.c $(CFLAGS) -lm -o $@

win :
	cc $@.c $(CFLAGS) $(XF) $(SF) -lm -lX11 -o $@

hypomh : hypomh.f
	f77 $@.f -o $@

insert_raw : insert_raw.o $(WIN_OBJ) $(WIN_INC)
	$(CC) $(CFLAGS) $@.o $(WIN_OBJ) -o $@

insert_trg : insert_trg.o $(WIN_OBJ) $(WIN_INC)
	$(CC) $(CFLAGS) $@.o $(WIN_OBJ) -o $@


insert_raw.o : $(WIN_INC)
insert_trg.o : $(WIN_INC)
$(WIN_OBJ) : $(WIN_INC)
