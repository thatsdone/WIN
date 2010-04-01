#
# $Id: Makefile.am,v 1.5.4.1.4.1 2010/04/01 14:03:31 uehira Exp $
#

# SUBDIRS = src etc html man pick prog
SUBDIRS = src prog man man.en etc pick tools

EXTRA_DIST = README.Cygwin README.MacOSX

# ACLOCAL_AMFLAGS= -I m4

md5: dist
	md5 $(distdir).tar.gz > $(distdir).tar.gz.md5
