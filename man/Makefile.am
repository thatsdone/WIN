#
# $Id: Makefile.am,v 1.1 2002/01/17 11:26:14 uehira Exp $
#
#  Makefile.am in man subdirectory.
#

WIN_MAN = _bj.1 _update.1 automan.1 autopick.1 cormeisei.1 dat.1 dewin.1 \
	ecore.1 elist.1 events.1 extraw.1 find_picks.1 fromtape.1 hypomh.1 \
	latest.1 lpf_lbpvf.1 newym.1 order.1 pickfile.1 pmon.1 ras2lips.1 \
	ras2rpdl.1 raw2mon.1 raw_100.1 raw_ch.1 raw_mon.1 raw_raw.1 \
	recvnmx.1 recvs.1 recvt.1 recvts.1 relay.1 rtape.1 send_raw.1 \
	send_raw_old.1 sends.1 shmdump.1 wadd.1 wch.1 wchch.1 wck.1 wdisk.1 \
	wed.1 wex.1 win.1 win2fix.1 winform.1 winformat.1 wtape.1 wtime.1

man_MANS = $(WIN_MAN)
	
# EXTRA_DIST = $(WIN_MAN) makefile.1 win.1.original winformat.org
EXTRA_DIST = $(WIN_MAN)

install-data-hook:
	(rm -f $(man1dir)/eventsrtape.1; cd $(man1dir) && @LN_S@ events.1 eventsrtape.1)
	(rm -f $(man1dir)/send_mon.1; cd $(man1dir) && @LN_S@ send_raw.1 send_mon.1)
	(rm -f $(man1dir)/sendt_mon.1; cd $(man1dir) && @LN_S@ send_raw.1 sendt_mon.1)
	(rm -f $(man1dir)/sendt_raw.1; cd $(man1dir) && @LN_S@ send_raw.1 sendt_raw.1)
