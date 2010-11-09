#
# $Id: Makefile.am,v 1.16.2.2.4.1 2010/11/09 08:02:27 uehira Exp $
#
#  Makefile.am in prog subdirectory.
#

WIN_SH_PROG =  _bj _bj1 _bj2 _cont _find _joint _kill _mon _monit _stop \
	_update _update1 _update2 _win ap automan autopick bj \
	check_status_sh copy_trg_from_sso denoise dist_tbl ea ee \
	eee el final keepcount latest list_orphan_picks make_day_LP \
	make_mon_data make_pmon_index manplain map mkCDimage mk_station \
	mktrg molist mon monitor_procs monitor_procs1 newtape newym picks \
	picks_settle pp ppj printmon ras2gif ras2lips2a ras2lips2b ras2png \
	ras2rpdl2a ras2rpdl2b recvstatus_filter renlog rt seis setRT stc \
	stlist sumup tape tapelist trg trgstat w2h w2hh wex \
	wt wtB ck_wdisk.sh insert_raw2 insert_raw2p update_ch clean_nmx wintosac.sh \
	mkpicknoise chtbl2kml

# other shell script
WIN_SHELL_PROG = w2m.in
WIN_SHELL_BIN = w2m

# Tcl/Tk script
WIN_WISH_PROG = win_m.in shmx.in shmz.in winsimu.in shmck.in
WIN_WISH_BIN = win_m shmx shmz winsimu shmck

# perl script
WIN_PERL_PROG = ch2mf.in ee_manager.in final2mf.in wincd.in pick2pick.in datej.pl.in winmap2pickfile.in
WIN_PERL_BIN = ch2mf ee_manager final2mf wincd pick2pick datej.pl winmap2pickfile

# destination files
EXTRA_DIST = $(WIN_SH_PROG) $(WIN_SHELL_PROG) $(WIN_WISH_PROG) $(WIN_PERL_PROG)

# excutable files
bin_SCRIPTS = $(WIN_SH_PROG) $(WIN_SHELL_BIN) $(WIN_WISH_BIN) $(WIN_PERL_BIN)
