#
# $Id: Makefile.am,v 1.6 2002/05/07 09:50:22 urabe Exp $
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

if X86
ONL_X86=recvts
else
ONL_X86=
endif

ONLINE_SO=recvt send_raw recvstatus send_raw_old relay recvnmx $(ONL_SOL2)
TAPE_USER_MATH=pmon elist events
ONLINE_MATH=ecore
OFFLINE_USER_MATH=wtime
#OFFLINE_MATH=dewin select_stations
OFFLINE_MATH=dewin select_stations hypomhc
OFFLINE_F77=hypomh

OFFLINE_USER=win $(OFFLINE_F77) wed wck wadd $(OFFLINE_MATH) wchch pick2tseis \
             wch pick2finalb pick2final finalb2final final2finalb finalb2seis \
             wform $(OFFLINE_USER_MATH) winadd wck_wdisk wadd2
TAPE_USER=rtape fromtape wdisk wdiskts ras2rpdl ras2lips file_older raw2mon \
          $(TAPE_USER_MATH)
ONLINE=order raw_mon wtape setexb timeout raw_raw raw_100 cormeisei shmdump \
       raw_ch extraw order2 insert_raw insert_trg find_picks raw_shift \
       $(ONLINE_SO) $(ONLINE_MATH) $(ONL_X86)

ONL_OPT_=robot
OTHR=cpdat sepline joint order_raw
OTHR_=rund
SUB=winform win2fix

WIN_UTIL_H = win_system.h
WIN_UTIL_C = win_system.c
SUBFUNC_H = subst_func.h

bin_PROGRAMS = $(OFFLINE_USER) $(TAPE_USER) $(ONLINE)

install-exec-hook:
	(rm -f @bindir@/send_mon; cd @bindir@ && @LN_S@ send_raw send_mon)
	(rm -f @bindir@/sendt_raw; cd @bindir@ && @LN_S@ send_raw sendt_raw)
	(rm -f @bindir@/sendt_mon; cd @bindir@ && @LN_S@ send_raw sendt_mon)
	(rm -f @bindir@/send_mon_old;\
			cd @bindir@ && @LN_S@ send_raw_old send_mon_old)
	(rm -f @bindir@/sendt_raw_old;\
			cd @bindir@ && @LN_S@ send_raw_old sendt_raw_old)
	(rm -f @bindir@/sendt_mon_old;\
			cd @bindir@ && @LN_S@ send_raw_old sendt_mon_old)
	(rm -f @bindir@/wdisk60; cd @bindir@ && @LN_S@ wdisk wdisk60)

win_SOURCES = win.c $(SUBFUNC_H)
win_CFLAGS = @X_CFLAGS@ $(AM_CFLAGS)
win_LDFLAGS = @X_LIBS@
win_LDADD = @LIBNSL@ @LIBSOCKET@ -lm -lX11

wed_SOURCES = wed.c $(SUBFUNC_H)

wck_SOURCES = wck.c $(SUBFUNC_H)

wadd_SOURCES = wadd.c $(SUBFUNC_H)

wadd2_SOURCES = wadd2.c $(SUBFUNC_H)

dewin_SOURCES = dewin.c $(SUBFUNC_H)
dewin_LDADD = -lm

select_stations_SOURCES = select_stations.c $(SUBFUNC_H)
select_stations_LDADD = -lm

wchch_SOURCES = wchch.c $(SUBFUNC_H)

pick2tseis_SOURCES = pick2tseis.c $(SUBFUNC_H)

wch_SOURCES = wch.c $(SUBFUNC_H)

pick2finalb_SOURCES = pick2finalb.c $(SUBFUNC_H)

pick2final_SOURCES = pick2final.c $(SUBFUNC_H)

finalb2final_SOURCES = finalb2final.c $(SUBFUNC_H)

final2finalb_SOURCES = final2finalb.c $(SUBFUNC_H)

finalb2seis_SOURCES = finalb2seis.c $(SUBFUNC_H)

wform_SOURCES = wform.c $(SUBFUNC_H)

wtime_SOURCES = wtime.c $(SUBFUNC_H)
wtime_LDADD = -lm

winadd_SOURCES = winadd.c $(WIN_UTIL_C) $(WIN_UTIL_H) $(SUBFUNC_H)

wck_wdisk_SOURCES = wck_wdisk.c $(WIN_UTIL_C) $(WIN_UTIL_H) $(SUBFUNC_H)

rtape_SOURCES = rtape.c $(SUBFUNC_H)

fromtape_SOURCES = fromtape.c $(SUBFUNC_H)

wdisk_SOURCES = wdisk.c $(SUBFUNC_H)

wdiskts_SOURCES = wdiskts.c $(SUBFUNC_H)

ras2rpdl_SOURCES = ras2rpdl.c $(SUBFUNC_H)

ras2lips_SOURCES = ras2lips.c $(SUBFUNC_H)

file_older_SOURCES = file_older.c $(SUBFUNC_H)

raw2mon_SOURCES = raw2mon.c $(SUBFUNC_H)

pmon_SOURCES = pmon.c $(SUBFUNC_H)
pmon_LDADD = -lm

elist_SOURCES = elist.c $(SUBFUNC_H)
elist_LDADD = -lm

events_SOURCES = events.c $(SUBFUNC_H)
events_LDADD = -lm

order_SOURCES = order.c $(SUBFUNC_H)

raw_mon_SOURCES = raw_mon.c $(SUBFUNC_H)

wtape_SOURCES = wtape.c $(SUBFUNC_H)

setexb_SOURCES = setexb.c $(SUBFUNC_H)

timeout_SOURCES = timeout.c $(SUBFUNC_H)

raw_raw_SOURCES = raw_raw.c $(SUBFUNC_H)

raw_shift_SOURCES = raw_shift.c $(SUBFUNC_H)

raw_100_SOURCES = raw_100.c $(SUBFUNC_H)

cormeisei_SOURCES = cormeisei.c $(SUBFUNC_H)

shmdump_SOURCES = shmdump.c $(SUBFUNC_H)

raw_ch_SOURCES = raw_ch.c $(SUBFUNC_H)

extraw_SOURCES = extraw.c $(SUBFUNC_H)

order2_SOURCES = order2.c $(SUBFUNC_H)

insert_raw_SOURCES = insert_raw.c $(WIN_UTIL_C) $(WIN_UTIL_H) $(SUBFUNC_H)

insert_trg_SOURCES = insert_trg.c $(WIN_UTIL_C) $(WIN_UTIL_H) $(SUBFUNC_H)

find_picks_SOURCES = find_picks.c $(SUBFUNC_H)

recvt_SOURCES = recvt.c $(SUBFUNC_H)
recvt_LDADD = @LIBNSL@ @LIBSOCKET@

send_raw_SOURCES = send_raw.c $(SUBFUNC_H)
send_raw_LDADD = @LIBNSL@ @LIBSOCKET@

recvstatus_SOURCES = recvstatus.c $(SUBFUNC_H)
recvstatus_LDADD = @LIBNSL@ @LIBSOCKET@

send_raw_old_SOURCES = send_raw_old.c $(SUBFUNC_H)
send_raw_old_LDADD = @LIBNSL@ @LIBSOCKET@

relay_SOURCES = relay.c $(SUBFUNC_H)
relay_LDADD = @LIBNSL@ @LIBSOCKET@

recvnmx_SOURCES = recvnmx.c $(SUBFUNC_H)
recvnmx_LDADD = @LIBNSL@ @LIBSOCKET@

ecore_SOURCES = ecore.c $(SUBFUNC_H)
ecore_LDADD = -lm

recvts_SOURCES = recvts.c $(SUBFUNC_H)

hypomh_SOURCES = hypomh.f

hypomhc_SOURCES = hypomhc.c $(SUBFUNC_H)
hypomhc_LDADD = -lm

sends_SOURCES = sends.c $(SUBFUNC_H)
sends_LDADD = @LIBNSL@ @LIBSOCKET@

recvs_SOURCES = recvs.c $(SUBFUNC_H)
recvs_LDADD = @LIBNSL@ @LIBSOCKET@
