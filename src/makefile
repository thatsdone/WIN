#########################
#                       #
# win system makefile   #
#    99.4.7   urabe     #
#                       #
#########################
#
##### COMMON MACROS #####
SRCDIR = /dat/bin/src
PRGDIR = /dat/bin/prog
COPY_SRC =cp -p $@.c $(SRCDIR)
COPY_SRCF=cp -p $@.f $(SRCDIR)
#COPY_SH=cp -p $(SITE) $(SH_ONL) $(SH_ONL_USR) $(SH_ONL_OPT) $(SH_UTIL) $(SH_OTHR) $(PRGDIR)
COPY_SH=echo $(SITE) $(SH_ONL) $(SH_ONL_USR) $(SH_ONL_OPT) $(SH_UTIL)|tr -cs .A-Za-z_0-9 '\012'|cpio -pvm $(PRGDIR)
SRC = .
#########################
##### SOL2 ##############
#########################
BINDIR = /dat/bin/SOL2
CF=-w -O
XF=-I/usr/openwin/include -L/usr/openwin/lib
SF=-lsocket -lnsl
#########################
##### SUN-OS4 ###########
#########################
#BINDIR = /net/ryu/dat/bin/SPARC
#SRC=/net/ryu/dat/bin/src
#CF=-O -Bstatic -Dconst= -DSUNOS4
#COPY_SRC=
#COPY_SRCF=
#COPY_SH=
#########################
##### X86_OWN ###########
#########################
#BINDIR = /dat/bin/X86
#CF=-w -O
#XF=-I/usr/X11R6/include -L/usr/X11R6/lib
#ONL_X86=recvts
#########################
##### X86 ###############
#########################
#BINDIR = /net/ryu/dat/bin/X86
#CF=-w -O
#XF=-I/usr/X11R6/include -L/usr/X11R6/lib
#ONL_X86=recvts
#SRC=/net/ryu/dat/bin/src
#COPY_SRC=:
#COPY_SRCF=
#COPY_SH=
#########################
##### RISCNEWS ##########
#########################
#BINDIR = /dat/bin/RISCNEWS
#SRC=$(SRCDIR)
#CF=-Dconst=
#COPY_SRC=
#COPY_SRCF=
#COPY_SH=
#########################
##### CISCNEWS ##########
#########################
#BINDIR = /dat/bin/CISCNEWS
#SRC=$(SRCDIR)
#CF=-Dconst=
#COPY_SRC=
#COPY_SRCF=
#COPY_SH=
#########################
#########################
#########################

ONL=recvt send_raw order raw_mon wdisk pmon win events wed file_older elist \
    pick2finalb finalb2seis wtape setexb timeout raw_raw hypomh $(ONL_X86)
ONL_OPT=recvstatus ecore relay pick2tseis send_raw_old raw_100 cormeisei \
        ras2rpdl ras2lips
ONL_OPT_=robot
UTIL=rtape pick2final wck wadd fromtape raw2mon dewin finalb2final \
     final2finalb shmdump wch wchch
OTHR=wform cpdat sepline joint order_raw
OTHR_=rund
SUB=winform win2fix

SITE=win.conf _win makefile pmon.prm events.prm win.prm winap.prm wtape.prm
SH_ONL=_update _stop _cont newym _update1 picks_settle autopick _bj _bj1 \
    automan newtape setRT monitor_procs renlog
SH_ONL_USR=ee latest picks tapelist printmon _monit monitor_procs1
SH_ONL_OPT=eee w2m w2h w2hh molist dist_tbl make_trg_pick make_day_LP \
        list_orphan_picks make_mon_data denoise wt
SH_UTIL=mktrg seis final trg el wex trgstat stc sumup
SH_OTHR=_kill mk_station _find ap bj mon _mon map _joint copy_trg_from_sso \
     keepcount tseis-eri stlist


all : $(ONL) $(ONL_OPT) $(UTIL)

clean :
	rm $(ONL) $(ONL_OPT) $(UTIL) $(OTHR)

copy :
	$(COPY_SH)

cpio :
	(echo $(ONL) $(ONL_OPT) $(UTIL)|tr -cs A-Za-z_0-9 '\012'|sed 's/$$/.c/g'|sed 's/hypomh.c/hypomh.f/';echo recvts.c;echo $(SH_ONL) $(SH_ONL_USR) $(SH_ONL_OPT) $(SH_UTIL)|tr -cs A-Za-z_0-9 '\012')|cpio -o -H odc >win.cpio

install :
	cpio -im < win.cpio

install_man :
	tar xvf win-man.tar -C /dat man/euc

man :
	tar cvf win-man.tar -C /dat man

recvts : $(SRC)/recvts.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

robot : $(SRC)/robot.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

pick2tseis : $(SRC)/pick2tseis.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

ras2rpdl : $(SRC)/ras2rpdl.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

ras2lips : $(SRC)/ras2lips.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wchch : $(SRC)/wchch.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wch : $(SRC)/wch.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

shmdump : $(SRC)/shmdump.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

recvstatus : $(SRC)/recvstatus.c
	cc $(SRC)/$@.c $(CF) $(SF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wform : $(SRC)/wform.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

joint : $(SRC)/joint.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

setexb : $(SRC)/setexb.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

relay : $(SRC)/relay.c
	cc $(SRC)/$@.c $(CF) $(SF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

recvt : $(SRC)/recvt.c
	cc $(SRC)/$@.c $(CF) $(SF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

send_raw_old : $(SRC)/send_raw_old.c
	cc $(SRC)/$@.c $(CF) $(SF) -o $@
	cp -p $@ $(BINDIR)
	rm -f $(BINDIR)/sendt_raw_old;ln -s $(BINDIR)/$@ $(BINDIR)/sendt_raw_old
	rm -f $(BINDIR)/sendt_mon_old;ln -s $(BINDIR)/$@ $(BINDIR)/sendt_mon_old
	rm -f $(BINDIR)/send_mon_old;ln -s $(BINDIR)/$@ $(BINDIR)/send_mon_old
	$(COPY_SRC)

send_raw : $(SRC)/send_raw.c
	cc $(SRC)/$@.c $(CF) $(SF) -o $@
	cp -p $@ $(BINDIR)
	rm -f $(BINDIR)/sendt_raw;ln -s $(BINDIR)/$@ $(BINDIR)/sendt_raw
	rm -f $(BINDIR)/sendt_mon;ln -s $(BINDIR)/$@ $(BINDIR)/sendt_mon
	rm -f $(BINDIR)/send_mon;ln -s $(BINDIR)/$@ $(BINDIR)/send_mon
	$(COPY_SRC)

order_raw : $(SRC)/order_raw.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

order : $(SRC)/order.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

raw2mon : $(SRC)/raw2mon.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

cormeisei : $(SRC)/cormeisei.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

raw_100 : $(SRC)/raw_100.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

raw_raw : $(SRC)/raw_raw.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

raw_mon : $(SRC)/raw_mon.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

rund : $(SRC)/rund.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

ecore : $(SRC)/ecore.c
	cc $(SRC)/$@.c $(CF) -lm -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

fromtape : $(SRC)/fromtape.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

sepline : $(SRC)/sepline.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

cpdat : $(SRC)/cpdat.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

dewin : $(SRC)/dewin.c
	cc $(SRC)/$@.c $(CF) -lm -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wck : $(SRC)/wck.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wadd : $(SRC)/wadd.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	chmod g+s $@ $(BINDIR)/$@
	$(COPY_SRC)

pick2final : $(SRC)/pick2final.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

pick2finalb : $(SRC)/pick2finalb.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

final2finalb : $(SRC)/final2finalb.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

finalb2final : $(SRC)/finalb2final.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

finalb2seis : $(SRC)/finalb2seis.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

file_older : $(SRC)/file_older.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

elist : $(SRC)/elist.c
	cc $(SRC)/$@.c $(CF) -lm -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wdisk : $(SRC)/wdisk.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

win : $(SRC)/win.c
	cc $(SRC)/$@.c $(CF) $(XF) -lm -lX11 -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

pmon : $(SRC)/pmon.c
	cc $(SRC)/$@.c $(CF) -lm -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

events : $(SRC)/events.c
	cc $(SRC)/$@.c $(CF) -lm -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wed : $(SRC)/wed.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

wtape : $(SRC)/wtape.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

rtape : $(SRC)/rtape.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

timeout : $(SRC)/timeout.c
	cc $(SRC)/$@.c $(CF) -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRC)

hypomh : $(SRC)/hypomh.f
	f77 $(SRC)/$@.f -o $@
	cp -p $@ $(BINDIR)
	$(COPY_SRCF)
