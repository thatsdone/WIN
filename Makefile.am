#
# $Id: Makefile.am,v 1.6 2008/04/07 02:57:53 uehira Exp $
#

# SUBDIRS = src etc html man pick prog
SUBDIRS = src prog man man.en etc pick tools

EXTRA_DIST = README.Cygwin README.MacOSX

md5: dist
	md5 $(distdir).tar.gz > $(distdir).tar.gz.md5
