#
# $Id: Makefile.am,v 1.39 2019/09/07 12:48:47 urabe Exp $
#
#  Makefile.am in man subdirectory.
#

mandir = @mandir@/ja

WIN_MAN = _bj.1 _update.1 automan.1 autopick.1 cormeisei.1 dat.1 dewin.1 \
	ecore.1 elist.1 events.1 extraw.1 find_picks.1 fromtape.1 hypomh.1 \
	latest.1 lpf_lbpvf.1 newym.1 order.1 pickfile.1 pmon.1 ras2lips.1 \
	ras2rpdl.1 raw2mon.1 raw_100.1 raw_ch.1 raw_mon.1 raw_raw.1 raw_ddp.1 \
	recvnmx.1 recvs.1 recvt.1 recvts.1 relay.1 rtape.1 send_raw.1 \
	send_raw_old.1 sends.1 shmdump.1 wadd.1 wch.1 wchch.1 wck.1 \
	wck_wdisk.1 wdisk.1 raw_shift.1 insert_trg.1 wdiskts.1 raw_snd.1 \
        insert_raw2.1 wadd2.1 recvstatus.1 recvstatus2.1 select_stations.1 \
	wed.1 wex.1 win.1 win2fix.1 winform.1 winformat.1 wtape.1 wtime.1 \
	win2raw.1 shmx.1 shmz.1 shmck.1 wintowin.1 wintosac.1 wchasn.1 clean_nmx.1 winadd.1 \
	pick2pick.1 raw_time.1 mkpicknoise.1 \
	ls8tel16_win.1 ls8tel16_raw.1 ls8tel_STM.1 ls8tel_STS.1 \
	ls7000_A8.1 ls7000_A9.1 split_win.1 chtbl_ck.1 ck_ls8800cst.1\
	intro_win.1 wshift.1 send_raw46.1 recvt46.1 lily.1 wxt.1 wxtrecv.1 wxtsend.1 \
	vanpro2.1 cmg.1 sts.1 mk_windata.1

if IPV6
IPV6_MAN = relaym.1 recvstatus3.1 recvt_LS7000.1 recvstatus4.1
else
IPV6_MAN =
endif

man_MANS = $(WIN_MAN) $(IPV6_MAN)

# EXTRA_DIST = $(WIN_MAN) makefile.1 win.1.original winformat.org
EXTRA_DIST = $(WIN_MAN) $(IPV6_MAN)

install-data-hook:
	(rm -f $(DESTDIR)$(man1dir)/eventsrtape.1; cd $(DESTDIR)$(man1dir) && @LN_S@ events.1 eventsrtape.1)

	(rm -f $(DESTDIR)$(man1dir)/send_mon.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw.1 send_mon.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_mon.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw.1 sendt_mon.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_raw.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw.1 sendt_raw.1)
	(rm -f $(DESTDIR)$(man1dir)/send_rawd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw.1 send_rawd.1)
	(rm -f $(DESTDIR)$(man1dir)/send_mond.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw.1 send_mond.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_mond.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw.1 sendt_mond.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_rawd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw.1 sendt_rawd.1)
	(rm -f $(DESTDIR)$(man1dir)/wdisk60.1; cd $(DESTDIR)$(man1dir) && @LN_S@ wdisk.1 wdisk60.1)
	(rm -f $(DESTDIR)$(man1dir)/wdiskd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ wdisk.1 wdiskd.1)
	(rm -f $(DESTDIR)$(man1dir)/wdisk60d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ wdisk.1 wdisk60d.1)
	(rm -f $(DESTDIR)$(man1dir)/orderd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ order.1 orderd.1)
	(rm -f $(DESTDIR)$(man1dir)/recvtd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ recvt.1 recvtd.1)
	(rm -f $(DESTDIR)$(man1dir)/relayd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ relay.1 relayd.1)
	(rm -f $(DESTDIR)$(man1dir)/relaymd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ relaym.1 relaymd.1)
	(rm -f $(DESTDIR)$(man1dir)/raw_mond.1; cd $(DESTDIR)$(man1dir) && @LN_S@ raw_mon.1 raw_mond.1)
	(rm -f $(DESTDIR)$(man1dir)/raw_rawd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ raw_raw.1 raw_rawd.1)
	(rm -f $(DESTDIR)$(man1dir)/raw_ddpd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ raw_ddp.1 raw_ddpd.1)
	(rm -f $(DESTDIR)$(man1dir)/wdisktsd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ wdiskts.1 wdisktsd.1)
	(rm -f $(DESTDIR)$(man1dir)/raw_timed.1; cd $(DESTDIR)$(man1dir) && @LN_S@ raw_time.1 raw_timed.1)
	(rm -f $(DESTDIR)$(man1dir)/recvstatus2d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ recvstatus2.1 recvstatus2d.1)
	(rm -f $(DESTDIR)$(man1dir)/recvstatus3d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ recvstatus3.1 recvstatus3d.1)
	(rm -f $(DESTDIR)$(man1dir)/recvstatus4d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ recvstatus4.1 recvstatus4d.1)
	(rm -f $(DESTDIR)$(man1dir)/ls8tel16_rawd.1; cd $(DESTDIR)$(man1dir) && @LN_S@ ls8tel16_raw.1 ls8tel16_rawd.1)
	(rm -f $(DESTDIR)$(man1dir)/recvt_LS7000d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ recvt_LS7000.1 recvt_LS7000d.1)
	(rm -f $(DESTDIR)$(man1dir)/send_mon46.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw46.1 send_mon46.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_mon46.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw46.1 sendt_mon46.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_raw46.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw46.1 sendt_raw46.1)
	(rm -f $(DESTDIR)$(man1dir)/send_raw46d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw46.1 send_raw46d.1)
	(rm -f $(DESTDIR)$(man1dir)/send_mon46d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw46.1 send_mon46d.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_mon46d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw46.1 sendt_mon46d.1)
	(rm -f $(DESTDIR)$(man1dir)/sendt_raw46d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ send_raw46.1 sendt_raw46d.1)
	(rm -f $(DESTDIR)$(man1dir)/recvt46d.1; cd $(DESTDIR)$(man1dir) && @LN_S@ recvt46.1 recvt46d.1)

uninstall-local:
	rm -f $(DESTDIR)$(man1dir)/eventsrtape.1

	rm -f $(DESTDIR)$(man1dir)/send_mon.1
	rm -f $(DESTDIR)$(man1dir)/sendt_mon.1
	rm -f $(DESTDIR)$(man1dir)/sendt_raw.1
	rm -f $(DESTDIR)$(man1dir)/send_rawd.1
	rm -f $(DESTDIR)$(man1dir)/send_mond.1
	rm -f $(DESTDIR)$(man1dir)/sendt_mond.1
	rm -f $(DESTDIR)$(man1dir)/sendt_rawd.1
	rm -f $(DESTDIR)$(man1dir)/wdisk60.1
	rm -f $(DESTDIR)$(man1dir)/wdiskd.1
	rm -f $(DESTDIR)$(man1dir)/wdisk60d.1
	rm -f $(DESTDIR)$(man1dir)/orderd.1
	rm -f $(DESTDIR)$(man1dir)/recvtd.1
	rm -f $(DESTDIR)$(man1dir)/relayd.1
	rm -f $(DESTDIR)$(man1dir)/relaymd.1
	rm -f $(DESTDIR)$(man1dir)/raw_mond.1
	rm -f $(DESTDIR)$(man1dir)/raw_rawd.1
	rm -f $(DESTDIR)$(man1dir)/raw_ddpd.1
	rm -f $(DESTDIR)$(man1dir)/wdisktsd.1
	rm -f $(DESTDIR)$(man1dir)/raw_timed.1
	rm -f $(DESTDIR)$(man1dir)/recvstatus2d.1
	rm -f $(DESTDIR)$(man1dir)/recvstatus3d.1
	rm -f $(DESTDIR)$(man1dir)/recvstatus4d.1
	rm -f $(DESTDIR)$(man1dir)/ls8tel16_rawd.1
	rm -f $(DESTDIR)$(man1dir)/recvt_LS7000d.1
	rm -f $(DESTDIR)$(man1dir)/send_mon46.1
	rm -f $(DESTDIR)$(man1dir)/sendt_mon46.1
	rm -f $(DESTDIR)$(man1dir)/sendt_raw46.1
	rm -f $(DESTDIR)$(man1dir)/send_raw46d.1
	rm -f $(DESTDIR)$(man1dir)/send_mon46d.1
	rm -f $(DESTDIR)$(man1dir)/sendt_mon46d.1
	rm -f $(DESTDIR)$(man1dir)/sendt_raw46d.1
	rm -f $(DESTDIR)$(man1dir)/recvt46d.1
