#
# $Id: Makefile.am,v 1.5 2005/02/02 15:36:35 uehira Exp $
#

# SUBDIRS = src etc html man pick prog
SUBDIRS = src prog man etc pick tools

EXTRA_DIST = README.Cygwin README.MacOSX

md5: dist
	md5 $(distdir).tar.gz > $(distdir).tar.gz.md5
