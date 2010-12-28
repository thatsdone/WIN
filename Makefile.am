#
# $Id: Makefile.am,v 1.5.2.2 2010/12/28 12:55:37 uehira Exp $
#

# SUBDIRS = src etc html man pick prog
SUBDIRS = src prog man man.en etc pick tools

EXTRA_DIST = README.Cygwin README.MacOSX

# ACLOCAL_AMFLAGS= -I m4

md5: dist
	md5 $(distdir).tar.gz > $(distdir).tar.gz.md5
