# SUBDIRS = src etc html man pick prog
SUBDIRS = src prog man etc pick

EXTRA_DIST = README.Cygwin README.MacOSX

md5: dist
	md5 $(distdir).tar.gz > $(distdir).tar.gz.md5
