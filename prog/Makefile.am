#
# $Id: Makefile.am,v 1.3 2002/05/03 10:49:48 uehira Exp $
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
	stlist sumup tape tapelist trg trgstat w2h w2hh w2m wex \
	wt wtB ck_wdisk.sh

# Tcl/Tk script
WIN_WISH_PROG = win_m.in
WIN_WISH_BIN = win_m

# perl script
WIN_PERL_PROG = ch2mf.in ee_manager.in final2mf.in wincd.in
WIN_PERL_BIN = ch2mf ee_manager final2mf wincd

# destination files
EXTRA_DIST = $(WIN_SH_PROG) $(WIN_WISH_PROG) $(WIN_PERL_PROG)

# excutable files
bin_SCRIPTS = $(WIN_SH_PROG) $(WIN_WISH_BIN) $(WIN_PERL_BIN)
