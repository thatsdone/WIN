#
# $Id: Makefile.am,v 1.38.2.10.2.29 2010/12/09 02:42:25 uehira Exp $
#

# Version number of shared libraries.
LIB_WINSYSTEM_VERSION = -version-info 1:0:1
LIB_WINLOG_VERSION = -version-info 0:0:0

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
IPV6_PGM = relaym recvstatus3 recvstatus4 winrawreq rawsrv_info recvt_LS7000
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

if NO_FORTRAN
OFFLINE_F77=
else
OFFLINE_F77=hypomh
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
TAPE_USER_MATH=pmon elist events evdetect
ONLINE_MATH=ecore ecore2 raw_time winrandsimu6
OFFLINE_USER_MATH=wtime
#OFFLINE_MATH=dewin select_stations
OFFLINE_MATH=dewin select_stations hypomhc wintosac select_bylatlon

OFFLINE_USER=$(OFFLINE_X) $(OFFLINE_F77) wed wck wadd $(OFFLINE_MATH) wchch pick2tseis \
             wch pick2finalb pick2final finalb2final final2finalb finalb2seis \
             wform wintowin $(OFFLINE_USER_MATH) winadd wck_wdisk wadd2 \
             win2raw wchasn ls8tel16_win ls8tel_STM ls8tel_STS ls7000_A8 ls7000_A9 split_win
TAPE_USER=$(RTAPE) $(FROMTAPE) wdisk wdiskts ras2rpdl ras2lips file_older raw2mon \
          $(TAPE_USER_MATH)
ONLINE=order raw_mon $(WTAPE) setexb timeout raw_raw raw_100 cormeisei shmdump \
       raw_ch extraw insert_raw insert_trg find_picks raw_shift ls8tel16_raw\
	shmcounter\
       $(ONLINE_SO) $(ONLINE_MATH) $(ONL_FREEBSD)

ONL_OPT_=robot
OTHR=cpdat sepline joint order_raw
OTHR_=rund
SUB=winform win2fix

#WIN_UTIL = win_system.h win_system.c
WIN_UTIL =
WINSYSTEM_H = winsystem.h
SUBFUNC_H = subst_func.h
DAEMON_MODE = daemon_mode.h daemon_mode.c
WIN_LOG = win_log.h win_log.c win_externs.h winlib_log.c
UDP_UTILITY = udpu.h udp_accept.c udp_dest.c
TCP_UTILITY = tcpu.h tcp_connect.c

WIN_INCLUDABLE=wconst.h wmacros.h wproto.h wstruct.h  win_log.h filter.h pltxy.h ulaw.h daemon_mode.h udpu.h tcpu.h ls7000.h ls8tel.h winpickfile.h
WIN_LIBH=winlib.h $(WIN_INCLUDABLE) $(SUBFUNC_H)
WIN_LIBC=winlib.c filter.c pltxy.c ulaw.c winpickfile.c

#WIN_SYSTEM_LIB = libwinsystem.a
WIN_SYSTEM_LTLIB = libwinsystem.la

#WIN_LOG_LIB = libwinlog.a
WIN_LOG_LTLIB = libwinlog.la

#WIN_SYSTEM_LIB2 = libwinsystem2.a
WIN_SYSTEM_LIB2 = 

bin_PROGRAMS = $(OFFLINE_USER) $(TAPE_USER) $(ONLINE)
if IPV6
libexec_PROGRAMS = winrawsrvd
noinst_PROGRAMS = winrawcli_test
endif
#lib_LIBRARIES = $(WIN_SYSTEM_LIB) $(WIN_LOG_LIB)
lib_LTLIBRARIES = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB)
include_HEADERS = $(WINSYSTEM_H) $(WIN_INCLUDABLE) winraw_bp.h

DISTCLEANFILES = $(WINSYSTEM_H)

install-exec-hook:
	(rm -f $(DESTDIR)@bindir@/send_mon; cd $(DESTDIR)@bindir@ && @LN_S@ send_raw send_mon)
	(rm -f $(DESTDIR)@bindir@/sendt_raw; cd $(DESTDIR)@bindir@ && @LN_S@ send_raw sendt_raw)
	(rm -f $(DESTDIR)@bindir@/sendt_mon; cd $(DESTDIR)@bindir@ && @LN_S@ send_raw sendt_mon)
	(rm -f $(DESTDIR)@bindir@/send_rawd; cd $(DESTDIR)@bindir@ && @LN_S@ send_raw send_rawd)
	(rm -f $(DESTDIR)@bindir@/send_mond; cd $(DESTDIR)@bindir@ && @LN_S@ send_raw send_mond)
	(rm -f $(DESTDIR)@bindir@/sendt_rawd; cd $(DESTDIR)@bindir@ && @LN_S@ send_raw sendt_rawd)
	(rm -f $(DESTDIR)@bindir@/sendt_mond; cd $(DESTDIR)@bindir@ && @LN_S@ send_raw sendt_mond)
	(rm -f $(DESTDIR)@bindir@/send_mon_old;\
			cd $(DESTDIR)@bindir@ && @LN_S@ send_raw_old send_mon_old)
	(rm -f $(DESTDIR)@bindir@/sendt_raw_old;\
			cd $(DESTDIR)@bindir@ && @LN_S@ send_raw_old sendt_raw_old)
	(rm -f $(DESTDIR)@bindir@/sendt_mon_old;\
			cd $(DESTDIR)@bindir@ && @LN_S@ send_raw_old sendt_mon_old)
	(rm -f $(DESTDIR)@bindir@/wdiskd; cd $(DESTDIR)@bindir@ && @LN_S@ wdisk wdiskd)
	(rm -f $(DESTDIR)@bindir@/wdisk60; cd $(DESTDIR)@bindir@ && @LN_S@ wdisk wdisk60)
	(rm -f $(DESTDIR)@bindir@/wdisk60d; cd $(DESTDIR)@bindir@ && @LN_S@ wdisk wdisk60d)
	(rm -f $(DESTDIR)@bindir@/orderd; cd $(DESTDIR)@bindir@ && @LN_S@ order orderd)
	(rm -f $(DESTDIR)@bindir@/recvtd; cd $(DESTDIR)@bindir@ && @LN_S@ recvt recvtd)
	(rm -f $(DESTDIR)@bindir@/relayd; cd $(DESTDIR)@bindir@ && @LN_S@ relay relayd)
	(rm -f $(DESTDIR)@bindir@/relaymd; cd $(DESTDIR)@bindir@ && @LN_S@ relaym relaymd)
	(rm -f $(DESTDIR)@bindir@/raw_mond; cd $(DESTDIR)@bindir@ && @LN_S@ raw_mon raw_mond)
	(rm -f $(DESTDIR)@bindir@/raw_timed; cd $(DESTDIR)@bindir@ && @LN_S@ raw_time raw_timed)
	(rm -f $(DESTDIR)@bindir@/raw_rawd; cd $(DESTDIR)@bindir@ && @LN_S@ raw_raw raw_rawd)
	(rm -f $(DESTDIR)@bindir@/wdisktsd; cd $(DESTDIR)@bindir@ && @LN_S@ wdiskts wdisktsd)
	(rm -f $(DESTDIR)@bindir@/recvstatus2d; cd $(DESTDIR)@bindir@ && @LN_S@ recvstatus2 recvstatus2d)
	(rm -f $(DESTDIR)@bindir@/recvstatus3d; cd $(DESTDIR)@bindir@ && @LN_S@ recvstatus3 recvstatus3d)
	(rm -f $(DESTDIR)@bindir@/recvstatus4d; cd $(DESTDIR)@bindir@ && @LN_S@ recvstatus4 recvstatus4d)
	(rm -f $(DESTDIR)@bindir@/ls8tel16_rawd; cd $(DESTDIR)@bindir@ && @LN_S@ ls8tel16_raw ls8tel16_rawd)
	(rm -f $(DESTDIR)@bindir@/recvt_LS7000d; cd $(DESTDIR)@bindir@ && @LN_S@ recvt_LS7000 recvt_LS7000d)

uninstall-local:
	rm -f $(DESTDIR)@bindir@/send_mon
	rm -f $(DESTDIR)@bindir@/sendt_raw
	rm -f $(DESTDIR)@bindir@/sendt_mon
	rm -f $(DESTDIR)@bindir@/send_rawd
	rm -f $(DESTDIR)@bindir@/send_mond
	rm -f $(DESTDIR)@bindir@/sendt_rawd
	rm -f $(DESTDIR)@bindir@/sendt_mond
	rm -f $(DESTDIR)@bindir@/send_mon_old;
	rm -f $(DESTDIR)@bindir@/sendt_raw_old;
	rm -f $(DESTDIR)@bindir@/sendt_mon_old;
	rm -f $(DESTDIR)@bindir@/wdiskd
	rm -f $(DESTDIR)@bindir@/wdisk60
	rm -f $(DESTDIR)@bindir@/wdisk60d
	rm -f $(DESTDIR)@bindir@/orderd
	rm -f $(DESTDIR)@bindir@/recvtd
	rm -f $(DESTDIR)@bindir@/relayd
	rm -f $(DESTDIR)@bindir@/relaymd
	rm -f $(DESTDIR)@bindir@/raw_mond
	rm -f $(DESTDIR)@bindir@/raw_timed
	rm -f $(DESTDIR)@bindir@/raw_rawd
	rm -f $(DESTDIR)@bindir@/wdisktsd
	rm -f $(DESTDIR)@bindir@/recvstatus2d
	rm -f $(DESTDIR)@bindir@/recvstatus3d
	rm -f $(DESTDIR)@bindir@/recvstatus4d
	rm -f $(DESTDIR)@bindir@/ls8tel16_rawd
	rm -f $(DESTDIR)@bindir@/recvt_LS7000d

######----- Libraries -----
### static library
#libwinsystem_a_SOURCES = $(WIN_LIBH) $(WIN_LIBC) $(WIN_LOG)
#libwinsystem_a_CFLAGS = $(AM_CFLAGS)

#libwinlog_a_SOURCES =  $(WIN_LOG) $(DAEMON_MODE) $(UDP_UTILITY) $(TCP_UTILITY)
#libwinlog_a_CFLAGS = $(AM_CFLAGS)

#libwinsystem2_a_SOURCES = $(WIN_UTIL) $(DAEMON_MODE) $(WIN_LOG) $(UDP_UTILITY) $(TCP_UTILITY)

### shared library
libwinsystem_la_SOURCES = $(WIN_LIBH) $(WIN_LIBC)
libwinsystem_la_LDFLAGS = $(LIB_WINSYSTEM_VERSION)
libwinsystem_la_LIBADD = -lm  $(LTLIBOBJS)

libwinlog_la_SOURCES =  $(WIN_LOG) $(DAEMON_MODE) $(UDP_UTILITY) $(TCP_UTILITY)
libwinlog_la_LDFLAGS = $(LIB_WINLOG_VERSION)
libwinlog_la_LIBADD = @LIBNSL@ @LIBSOCKET@

#####----- Sources -----
win_SOURCES = win.c $(WIN_LIBH)
#win_CFLAGS = @X_CFLAGS@ $(AM_CFLAGS)
win_CFLAGS = @X_CFLAGS@
win_LDFLAGS = @X_LIBS@
win_LDADD = $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@ -lm -lX11

win32_SOURCES = win.c $(WIN_LIBH)
#win32_CFLAGS = @X_CFLAGS@ $(AM_CFLAGS) -DHINET_WIN32=1
win32_CFLAGS = @X_CFLAGS@ -DHINET_WIN32=1
win32_LDFLAGS = @X_LIBS@
win32_LDADD = $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@ -lm -lX11

wed_SOURCES = wed.c $(WIN_LIBH)
wed_LDADD = $(WIN_SYSTEM_LTLIB)

wck_SOURCES = wck.c $(WIN_LIBH)
wck_LDADD = $(WIN_SYSTEM_LTLIB)

wadd_SOURCES = wadd.c $(WIN_LIBH)
wadd_LDADD = $(WIN_SYSTEM_LTLIB)

wadd2_SOURCES = wadd2.c $(WIN_LIBH)
wadd2_LDADD = $(WIN_SYSTEM_LTLIB)

dewin_SOURCES = dewin.c $(WIN_LIBH)
dewin_LDADD = $(WIN_SYSTEM_LTLIB) -lm

wintosac_SOURCES = wintosac.c
wintosac_LDADD = -lm

select_stations_SOURCES = select_stations.c pltxy.h $(SUBFUNC_H)
select_stations_LDADD = $(WIN_SYSTEM_LTLIB) -lm
#select_stations_CFLAGS = $(AM_CFLAGS)

wchch_SOURCES = wchch.c $(WIN_LIBH)
wchch_LDADD = $(WIN_SYSTEM_LTLIB)

wchasn_SOURCES = wchasn.c $(WIN_LIBH) $(GC_CHECK_INCLUDE)
wchasn_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)

pick2tseis_SOURCES = pick2tseis.c $(SUBFUNC_H)

wch_SOURCES = wch.c $(WIN_LIBH)
wch_LDADD = $(WIN_SYSTEM_LTLIB)

pick2finalb_SOURCES = pick2finalb.c $(WIN_LIBH)
pick2finalb_LDADD = $(WIN_SYSTEM_LTLIB)

pick2final_SOURCES = pick2final.c $(SUBFUNC_H)

finalb2final_SOURCES = finalb2final.c $(WIN_LIBH)

final2finalb_SOURCES = final2finalb.c $(WIN_LIBH)
final2finalb_LDADD = $(WIN_SYSTEM_LTLIB)

finalb2seis_SOURCES = finalb2seis.c $(WIN_LIBH)
finalb2seis_LDADD = $(WIN_SYSTEM_LTLIB)

wform_SOURCES = wform.c $(WIN_LIBH)
wform_LDADD = $(WIN_SYSTEM_LTLIB)

wintowin_SOURCES = wintowin.c $(WIN_LIBH)
wintowin_LDADD = $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

wtime_SOURCES = wtime.c $(WIN_LIBH)
wtime_LDADD = $(WIN_SYSTEM_LTLIB) -lm

winadd_SOURCES = winadd.c $(WIN_LIBH) $(GC_CHECK_INCLUDE)
winadd_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)

wck_wdisk_SOURCES = wck_wdisk.c $(WIN_LIBH) $(WIN_UTIL) $(GC_CHECK_INCLUDE)
wck_wdisk_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)

rtape_SOURCES = rtape.c $(WIN_LIBH)
rtape_LDADD = $(WIN_SYSTEM_LTLIB)

fromtape_SOURCES = fromtape.c $(WIN_LIBH)
fromtape_LDADD = $(WIN_SYSTEM_LTLIB)

wdisk_SOURCES = wdisk.c $(WIN_LIBH)
wdisk_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

wdiskts_SOURCES = wdiskts.c $(WIN_LIBH) $(GC_CHECK_INCLUDE)
wdiskts_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@ $(GC_CHECK_LIB)

ras2rpdl_SOURCES = ras2util.c $(WIN_LIBH)
ras2rpdl_CFLAGS = -DRPDL
ras2rpdl_LDADD = $(LTLIBOBJS)

ras2lips_SOURCES = ras2util.c $(WIN_LIBH)
ras2lips_CFLAGS = -DLIPS
ras2lips_LDADD = $(LTLIBOBJS)

file_older_SOURCES = file_older.c $(SUBFUNC_H)

raw2mon_SOURCES = raw2mon.c $(WIN_LIBH)
raw2mon_LDADD = $(WIN_SYSTEM_LTLIB)

#win2raw_SOURCES = win2raw.c $(WIN_LIBH) $(WIN_UTIL) $(GC_CHECK_INCLUDE)
win2raw_SOURCES = win2raw.c $(WIN_LIBH) $(GC_CHECK_INCLUDE)
win2raw_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)

pmon_SOURCES = pmon.c $(WIN_LIBH)
pmon_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) -lm

elist_SOURCES = elist.c $(WIN_LIBH)
elist_LDADD = $(WIN_SYSTEM_LTLIB) -lm

events_SOURCES = events.c $(WIN_LIBH)
events_LDADD = $(WIN_SYSTEM_LTLIB) -lm

order_SOURCES = order.c $(WIN_LIBH)
order_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

raw_mon_SOURCES = raw_mon.c $(WIN_LIBH)
raw_mon_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

wtape_SOURCES = wtape.c $(WIN_LIBH)
wtape_LDADD = $(WIN_SYSTEM_LTLIB)

setexb_SOURCES = setexb.c $(SUBFUNC_H)
setexb_LDADD = $(WIN_SYSTEM_LTLIB)

timeout_SOURCES = timeout.c $(SUBFUNC_H)

raw_raw_SOURCES = raw_raw.c $(WIN_LIBH)
raw_raw_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

raw_shift_SOURCES = raw_shift.c $(WIN_LIBH)
raw_shift_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

raw_100_SOURCES = raw_100.c $(WIN_LIBH)
raw_100_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

cormeisei_SOURCES = cormeisei.c $(WIN_LIBH)
cormeisei_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

shmdump_SOURCES = shmdump.c $(WIN_LIBH)
shmdump_LDADD = $(WIN_SYSTEM_LTLIB) @LIBSHMGET@ -lm

shmcounter_SOURCES = shmcounter.c $(WIN_LIBH)
shmcounter_LDADD = $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

raw_ch_SOURCES = raw_ch.c $(WIN_LIBH)
raw_ch_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

extraw_SOURCES = extraw.c $(WIN_LIBH)
extraw_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@

insert_raw_SOURCES = insert_raw.c $(WIN_LIBH) $(WIN_UTIL) $(GC_CHECK_INCLUDE)
insert_raw_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)

insert_trg_SOURCES = insert_trg.c $(WIN_LIBH) $(WIN_UTIL) $(GC_CHECK_INCLUDE)
insert_trg_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)

find_picks_SOURCES = find_picks.c $(WIN_LIBH)
find_picks_LDADD = $(WIN_SYSTEM_LTLIB)

recvt_SOURCES = recvt.c $(WIN_LIBH)
recvt_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

send_raw_SOURCES = send_raw.c $(WIN_LIBH)
send_raw_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

send_raw_old_SOURCES = send_raw_old.c $(WIN_LIBH)
send_raw_old_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

recvstatus_SOURCES = recvstatus.c $(WIN_LIBH)
recvstatus_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

recvstatus2_SOURCES = recvstatus2.c $(WIN_LIBH)
recvstatus2_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

relay_SOURCES = relay.c $(WIN_LIBH)
relay_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

relaym_SOURCES = relaym.c $(WIN_LIBH)
relaym_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

recvnmx_SOURCES = recvnmx.c $(WIN_LIBH)
recvnmx_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

ecore_SOURCES = ecore.c $(WIN_LIBH)
ecore_LDADD = $(WIN_SYSTEM_LTLIB) -lm

ecore2_SOURCES = ecore2.c $(WIN_LIBH)
ecore2_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) -lm @LIBSHMGET@

recvts_SOURCES = recvts.c $(WIN_LIBH)
recvts_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB)

hypomh_SOURCES = hypomh.f

hypomhc_SOURCES = hypomhc.c pltxy.h $(SUBFUNC_H)
hypomhc_LDADD = $(WIN_SYSTEM_LTLIB) -lm
#hypomhc_CFLAGS = $(AM_CFLAGS)

sends_SOURCES = sends.c $(WIN_LIBH)
sends_LDADD = $(WIN_SYSTEM_LTLIB) $(WIN_LOG_LTLIB) @LIBNSL@ @LIBSOCKET@

recvs_SOURCES = recvs.c $(WIN_LIBH)
recvs_LDADD = $(WIN_SYSTEM_LTLIB) $(WIN_LOG_LTLIB) @LIBNSL@ @LIBSOCKET@

raw_time_SOURCES = raw_time.c $(WIN_LIBH) $(GC_CHECK_INCLUDE)
raw_time_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) -lm @LIBSHMGET@ $(GC_CHECK_LIB)

recvstatus3_SOURCES = recvstatus3.c $(WIN_LIBH) ls8tel.h
recvstatus3_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

recvstatus4_SOURCES = recvstatus4.c $(WIN_LIBH) ls7000.h
recvstatus4_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

ls8tel16_win_SOURCES = ls8tel16_win.c $(WIN_LIBH) $(GC_CHECK_INCLUDE) ls8tel.h ls8tel.c
ls8tel16_win_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)

ls8tel16_raw_SOURCES = ls8tel16_raw.c ls8tel.h ls8tel.c $(WIN_LIBH) $(GC_CHECK_INCLUDE)
ls8tel16_raw_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBSHMGET@ $(GC_CHECK_LIB)

ls8tel_STM_SOURCES = ls8tel_STM.c $(SUBFUNC_H) $(GC_CHECK_INCLUDE) ls8tel.h
ls8tel_STM_LDADD = $(GC_CHECK_LIB)

ls8tel_STS_SOURCES = ls8tel_STS.c $(SUBFUNC_H) $(GC_CHECK_INCLUDE) ls8tel.h
ls8tel_STS_LDADD = $(GC_CHECK_LIB)

winrawsrvd_SOURCES = winrawsrv.c $(WIN_LIBH) winraw_bp.h
winrawsrvd_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

winrawcli_test_SOURCES = winrawcli_test.c $(WIN_LIBH) winraw_bp.h
winrawcli_test_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

winrawreq_SOURCES = winrawreq.c $(WIN_LIBH) winraw_bp.h
winrawreq_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

rawsrv_info_SOURCES = rawsrv_info.c $(WIN_LIBH) winraw_bp.h
rawsrv_info_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@

recvt_LS7000_SOURCES = recvt_LS7000.c $(WIN_LIBH) ls7000.h
recvt_LS7000_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) @LIBNSL@ @LIBSOCKET@ @LIBSHMGET@

ls7000_A8_SOURCES = ls7000_A8.c $(WIN_LIBH) ls7000.h
ls7000_A8_LDADD = $(WIN_SYSTEM_LTLIB) 

ls7000_A9_SOURCES = ls7000_A9.c $(WIN_LIBH) ls7000.h
ls7000_A9_LDADD = $(WIN_SYSTEM_LTLIB) 

evdetect_SOURCES = evdetect.c $(WIN_LIBH)
evdetect_LDADD = $(WIN_LOG_LTLIB) $(WIN_SYSTEM_LTLIB) -lm

winrandsimu6_SOURCES = winrandsimu6.c $(WIN_LIBH)
winrandsimu6_LDADD = @LIBSHMGET@ $(WIN_SYSTEM_LTLIB)  -lm

select_bylatlon_SOURCES = select_bylatlon.c pltxy.h $(SUBFUNC_H)
select_bylatlon_LDADD = $(WIN_SYSTEM_LTLIB) -lm
#select_bylatlon_CFLAGS = $(AM_CFLAGS)

split_win_SOURCES = split_win.c $(WIN_LIBH) $(GC_CHECK_INCLUDE)
split_win_LDADD = $(WIN_SYSTEM_LTLIB) $(GC_CHECK_LIB)
