#
# $Id: Makefile.am,v 1.5.2.1 2008/04/07 05:51:23 uehira Exp $
#

# SUBDIRS = src etc html man pick prog
SUBDIRS = src prog man man.en etc pick tools

EXTRA_DIST = README.Cygwin README.MacOSX

md5: dist
	md5 $(distdir).tar.gz > $(distdir).tar.gz.md5
