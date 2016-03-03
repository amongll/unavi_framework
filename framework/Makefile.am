SUBDIRS = src include
ACLOCAL_AMFLAGS = -I m4

if BUILD_TEST
  SUBDIRS += test
endif

if BUILD_DOC
  SUBDIRS += doc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libunavi.pc
