#
# $Id: Makefile.am,v 1.7 2011/06/01 11:09:19 uehira Exp $
#

# SUBDIRS = src etc html man pick prog
SUBDIRS = src prog man man.en etc pick tools

EXTRA_DIST = README.Cygwin README.MacOSX

# ACLOCAL_AMFLAGS= -I m4

md5: dist
	md5 $(distdir).tar.gz > $(distdir).tar.gz.md5
