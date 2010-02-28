#
# $Id: Makefile.am,v 1.40 2010/02/28 08:58:45 uehira Exp $
#

if SUNOS4
RECVNMX =
else
RECVNMX = recvnmx
endif

if SOL2
ONL_SOL2 = recvs sends
else
ONL_SOL2 = 
endif

if FREEBSD
ONL_FREEBSD=recvts
else
ONL_FREEBSD=
endif

if NO_X_SRC
OFFLINE_X=
else
OFFLINE_X=win win32
endif

if IPV6
IPV6_PGM = relaym recvstatus3
else
IPV6_PGM =
endif

if MT_HEADER
FROMTAPE = fromtape
RTAPE = rtape
WTAPE = wtape
else
FROMTAPE =
RTAPE =
WTAPE =
endif

if GC_MEMORY_LEAK_TEST
GC_CHECK_INCLUDE = gc_leak_detector.h
GC_CHECK_LIB = -lgc
else
GC_CHECK_INCLUDE =
GC_CHECK_LIB =
endif

ONLINE_SO=recvt send_raw recvstatus recvstatus2 send_raw_old relay\
	recvnmx $(ONL_SOL2) $(IPV6_PGM)
TAPE_USER_MATH=pmon elist events
ONLINE_MATH=ecore ecore2 raw_time
OFFLINE_USER_MATH=wtime
#OFFLINE_MATH=dewin select_stations
OFFLINE_MATH=dewin select_stations hypomhc wintosac
OFFLINE_F77=hypomh

OFFLINE_USER=$(OFFLINE_X) $(OFFLINE_F77) wed wck wadd $(OFFLINE_MATH) wchch pick2tseis \
             wch pick2finalb pick2final finalb2final final2finalb finalb2seis \
             wform wintowin $(OFFLINE_USER_MATH) winadd wck_wdisk wadd2 \
             win2raw wchasn ls8tel16_win ls8tel_STM ls8tel_STS select_bylatlon
TAPE_USER=$(RTAPE) $(FROMTAPE) wdisk wdiskts ras2rpdl ras2lips file_older raw2mon \
          $(TAPE_USER_MATH)
ONLINE=order raw_mon $(WTAPE) setexb timeout raw_raw raw_100 cormeisei shmdump \
	raw_ch extraw insert_raw insert_trg find_picks raw_shift ls8tel16_raw\
	winrandsimu6\
	$(ONLINE_SO) $(ONLINE_MATH) $(ONL_FREEBSD)

ONL_OPT_=robot
OTHR=cpdat sepline joint order_raw
OTHR_=rund
SUB=winform win2fix

WIN_UTIL = win_system.h win_system.c
SUBFUNC_H = subst_func.h
DAEMON_MODE = daemon_mode.h daemon_mode.c
WIN_LOG = win_log.h win_log.c win_externs.h
UDP_UTILITY = udpu.h udp_accept.c udp_dest.c
TCP_UTILITY = tcpu.h tcp_connect.c

bin_PROGRAMS = $(OFFLINE_USER) $(TAPE_USER) $(ONLINE)
if IPV6
libexec_PROGRAMS = winrawsrvd
noinst_PROGRAMS = winrawcli_test
endif

install-exec-hook:
	(rm -f @bindir@/send_mon; cd @bindir@ && @LN_S@ send_raw send_mon)
	(rm -f @bindir@/sendt_raw; cd @bindir@ && @LN_S@ send_raw sendt_raw)
	(rm -f @bindir@/sendt_mon; cd @bindir@ && @LN_S@ send_raw sendt_mon)
	(rm -f @bindir@/send_rawd; cd @bindir@ && @LN_S@ send_raw send_rawd)
	(rm -f @bindir@/send_mond; cd @bindir@ && @LN_S@ send_raw send_mond)
	(rm -f @bindir@/sendt_rawd; cd @bindir@ && @LN_S@ send_raw sendt_rawd)
	(rm -f @bindir@/sendt_mond; cd @bindir@ && @LN_S@ send_raw sendt_mond)
	(rm -f @bindir@/send_mon_old;\
			cd @bindir@ && @LN_S@ send_raw_old send_mon_old)
	(rm -f @bindir@/sendt_raw_old;\
			cd @bindir@ && @LN_S@ send_raw_old sendt_raw_old)
	(rm -f @bindir@/sendt_mon_old;\
			cd @bindir@ && @LN_S@ send_raw_old sendt_mon_old)
	(rm -f @bindir@/wdiskd; cd @bindir@ && @LN_S@ wdisk wdiskd)
	(rm -f @bindir@/wdisk60; cd @bindir@ && @LN_S@ wdisk wdisk60)
	(rm -f @bindir@/wdisk60d; cd @bindir@ && @LN_S@ wdisk wdisk60d)
	(rm -f @bindir@/orderd; cd @bindir@ && @LN_S@ order orderd)
	(rm -f @bindir@/recvtd; cd @bindir@ && @LN_S@ recvt recvtd)
	(rm -f @bindir@/relayd; cd @bindir@ && @LN_S@ relay relayd)
	(rm -f @bindir@/relaymd; cd @bindir@ && @LN_S@ relaym relaymd)
	(rm -f @bindir@/raw_mond; cd @bindir@ && @LN_S@ raw_mon raw_mond)
	(rm -f @bindir@/raw_timed; cd @bindir@ && @LN_S@ raw_time raw_timed)
	(rm -f @bindir@/raw_rawd; cd @bindir@ && @LN_S@ raw_raw raw_rawd)
	(rm -f @bindir@/wdisktsd; cd @bindir@ && @LN_S@ wdiskts wdisktsd)
	(rm -f @bindir@/recvstatus3d; cd @bindir@ && @LN_S@ recvstatus3 recvstatus3d)
	(rm -f @bindir@/ls8tel16_rawd; cd @bindir@ && @LN_S@ ls8tel16_raw ls8tel16_rawd)

win_SOURCES = win.c $(SUBFUNC_H)
win_CFLAGS = @X_CFLAGS@ $(AM_CFLAGS)
win_LDFLAGS = @X_LIBS@
win_LDADD = @LIBNSL@ @LIBSOCKET@ -lm -lX11

win32_SOURCES = win.c $(SUBFUNC_H)
win32_CFLAGS = @X_CFLAGS@ $(AM_CFLAGS) -DHINET_WIN32=1
win32_LDFLAGS = @X_LIBS@
win32_LDADD = @LIBNSL@ @LIBSOCKET@ -lm -lX11

wed_SOURCES = wed.c $(SUBFUNC_H)

wck_SOURCES = wck.c $(SUBFUNC_H)

wadd_SOURCES = wadd.c $(SUBFUNC_H)

wadd2_SOURCES = wadd2.c $(SUBFUNC_H)

dewin_SOURCES = dewin.c $(SUBFUNC_H)
dewin_LDADD = -lm

wintosac_SOURCES = wintosac.c
wintosac_LDADD = -lm

select_stations_SOURCES = select_stations.c $(SUBFUNC_H)
select_stations_LDADD = -lm

wchch_SOURCES = wchch.c $(SUBFUNC_H)

wchasn_SOURCES = wchasn.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE)
wchasn_LDADD = $(GC_CHECK_LIB)

pick2tseis_SOURCES = pick2tseis.c $(SUBFUNC_H)

wch_SOURCES = wch.c $(SUBFUNC_H)

pick2finalb_SOURCES = pick2finalb.c $(SUBFUNC_H)

pick2final_SOURCES = pick2final.c $(SUBFUNC_H)

finalb2final_SOURCES = finalb2final.c $(SUBFUNC_H)

final2finalb_SOURCES = final2finalb.c $(SUBFUNC_H)

finalb2seis_SOURCES = finalb2seis.c $(SUBFUNC_H)

wform_SOURCES = wform.c $(SUBFUNC_H)

wintowin_SOURCES = wintowin.c $(SUBFUNC_H)
wintowin_LDADD = @LIBSHMGET@

wtime_SOURCES = wtime.c $(SUBFUNC_H)
wtime_LDADD = -lm

winadd_SOURCES = winadd.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE)
winadd_LDADD = $(GC_CHECK_LIB)

wck_wdisk_SOURCES = wck_wdisk.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE)
wck_wdisk_LDADD = $(GC_CHECK_LIB)

rtape_SOURCES = rtape.c $(SUBFUNC_H)

fromtape_SOURCES = fromtape.c $(SUBFUNC_H)

wdisk_SOURCES = wdisk.c $(SUBFUNC_H) $(DAEMON_MODE)
wdisk_LDADD = @LIBSHMGET@

wdiskts_SOURCES = wdiskts.c $(SUBFUNC_H) $(DAEMON_MODE) $(GC_CHECK_INCLUDE)
wdiskts_LDADD = @LIBSHMGET@ $(GC_CHECK_LIB)

ras2rpdl_SOURCES = ras2rpdl.c $(SUBFUNC_H)

ras2lips_SOURCES = ras2lips.c $(SUBFUNC_H)

file_older_SOURCES = file_older.c $(SUBFUNC_H)

raw2mon_SOURCES = raw2mon.c $(SUBFUNC_H)

win2raw_SOURCES = win2raw.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE)
win2raw_LDADD = $(GC_CHECK_LIB)

pmon_SOURCES = pmon.c $(SUBFUNC_H)
pmon_LDADD = -lm

elist_SOURCES = elist.c $(SUBFUNC_H)
elist_LDADD = -lm

events_SOURCES = events.c $(SUBFUNC_H)
events_LDADD = -lm

order_SOURCES = order.c $(SUBFUNC_H) $(DAEMON_MODE)
order_LDADD = @LIBSHMGET@

raw_mon_SOURCES = raw_mon.c $(SUBFUNC_H) $(DAEMON_MODE)
raw_mon_LDADD = @LIBSHMGET@

wtape_SOURCES = wtape.c $(SUBFUNC_H)

setexb_SOURCES = setexb.c $(SUBFUNC_H)

timeout_SOURCES = timeout.c $(SUBFUNC_H)

raw_raw_SOURCES = raw_raw.c $(SUBFUNC_H) $(DAEMON_MODE)
raw_raw_LDADD = @LIBSHMGET@

raw_shift_SOURCES = raw_shift.c $(SUBFUNC_H)
raw_shift_LDADD = @LIBSHMGET@

raw_100_SOURCES = raw_100.c $(SUBFUNC_H)
raw_100_LDADD = @LIBSHMGET@

cormeisei_SOURCES = cormeisei.c $(SUBFUNC_H)
cormeisei_LDADD = @LIBSHMGET@

shmdump_SOURCES = shmdump.c $(SUBFUNC_H)
shmdump_LDADD = @LIBSHMGET@ -lm

raw_ch_SOURCES = raw_ch.c $(SUBFUNC_H)
raw_ch_LDADD = @LIBSHMGET@

extraw_SOURCES = extraw.c $(SUBFUNC_H)
extraw_LDADD = @LIBSHMGET@

insert_raw_SOURCES = insert_raw.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE)
insert_raw_LDADD = $(GC_CHECK_LIB)

insert_trg_SOURCES = insert_trg.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE)
insert_trg_LDADD = $(GC_CHECK_LIB)

find_picks_SOURCES = find_picks.c $(SUBFUNC_H)

recvt_SOURCES = recvt.c $(SUBFUNC_H) $(DAEMON_MODE)
recvt_LDADD = @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

send_raw_SOURCES = send_raw.c $(SUBFUNC_H) $(DAEMON_MODE)
send_raw_LDADD = @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

recvstatus_SOURCES = recvstatus.c $(SUBFUNC_H)
recvstatus_LDADD = @LIBNSL@ @LIBSOCKET@

recvstatus2_SOURCES = recvstatus2.c $(SUBFUNC_H)
recvstatus2_LDADD = @LIBNSL@ @LIBSOCKET@

send_raw_old_SOURCES = send_raw_old.c $(SUBFUNC_H)
send_raw_old_LDADD = @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

relay_SOURCES = relay.c $(SUBFUNC_H) $(DAEMON_MODE)
relay_LDADD = @LIBNSL@ @LIBSOCKET@

relaym_SOURCES = relaym.c $(SUBFUNC_H) $(DAEMON_MODE) $(WIN_LOG) $(UDP_UTILITY)
relaym_LDADD = @LIBNSL@ @LIBSOCKET@

recvnmx_SOURCES = recvnmx.c $(SUBFUNC_H)
recvnmx_LDADD = @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

ecore_SOURCES = ecore.c $(SUBFUNC_H)
ecore_LDADD = -lm

ecore2_SOURCES = ecore2.c $(SUBFUNC_H)
ecore2_LDADD = -lm @LIBSHMGET@

recvts_SOURCES = recvts.c $(SUBFUNC_H)

hypomh_SOURCES = hypomh.f

hypomhc_SOURCES = hypomhc.c $(SUBFUNC_H)
hypomhc_LDADD = -lm

sends_SOURCES = sends.c $(SUBFUNC_H)
sends_LDADD = @LIBNSL@ @LIBSOCKET@

recvs_SOURCES = recvs.c $(SUBFUNC_H)
recvs_LDADD = @LIBNSL@ @LIBSOCKET@

raw_time_SOURCES = raw_time.c $(SUBFUNC_H) $(DAEMON_MODE) $(WIN_LOG) $(WIN_UTIL) $(GC_CHECK_INCLUDE)
raw_time_LDADD = -lm @LIBSHMGET@ $(GC_CHECK_LIB)

recvstatus3_SOURCES = recvstatus3.c $(SUBFUNC_H) $(DAEMON_MODE) $(WIN_LOG) udpu.h udp_accept.c ls8tel.h
recvstatus3_LDADD = @LIBNSL@ @LIBSOCKET@

ls8tel16_win_SOURCES = ls8tel16_win.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE) ls8tel.h ls8tel.c
ls8tel16_win_LDADD = $(GC_CHECK_LIB)

ls8tel16_raw_SOURCES = ls8tel16_raw.c ls8tel.h ls8tel.c $(SUBFUNC_H) $(DAEMON_MODE) $(WIN_LOG) $(WIN_UTIL) $(GC_CHECK_INCLUDE)
ls8tel16_raw_LDADD = @LIBSHMGET@ $(GC_CHECK_LIB)

ls8tel_STM_SOURCES = ls8tel_STM.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE) ls8tel.h ls8tel.c
ls8tel_STM_LDADD = $(GC_CHECK_LIB)

ls8tel_STS_SOURCES = ls8tel_STS.c $(WIN_UTIL) $(SUBFUNC_H) $(GC_CHECK_INCLUDE) ls8tel.h ls8tel.c
ls8tel_STS_LDADD = $(GC_CHECK_LIB)

winrawsrvd_SOURCES = winrawsrv.c $(DAEMON_MODE) $(WIN_LOG) winraw_bp.h
winrawsrvd_LDADD = @LIBNSL@ @LIBSOCKET@

winrawcli_test_SOURCES = winrawcli_test.c $(DAEMON_MODE) $(WIN_LOG) $(TCP_UTILITY) winraw_bp.h
winrawcli_test_LDADD = @LIBNSL@ @LIBSOCKET@

winrandsimu6_SOURCES = winrandsimu6.c $(SUBFUNC_H)
winrandsimu6_LDADD = @LIBSHMGET@ -lm

select_bylatlon_SOURCES = select_bylatlon.c $(SUBFUNC_H)
select_bylatlon_LDADD = -lm

