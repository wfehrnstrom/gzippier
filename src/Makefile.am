# Copyright (C) 1999, 2001-2002, 2006-2007, 2009-2019 Free Software Foundation,
# Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

CLEANFILES =
BUILT_SOURCES =
DEBUG_FLAGS = -g -O0
AM_CPPFLAGS = -I$(top_srcdir)/lib/zlib
AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS) $(DEBUG_FLAGS)

# Tell the linker to omit references to unused shared libraries.
AM_LDFLAGS = $(IGNORE_USED_LIBRARIES_CFLAGS) -pthread

noinst_LIBRARIES = libver.a
nodist_libver_a_SOURCES = version.c version.h

gzip_LDADD = libver.a ../lib/zlib/libz.a ../lib/libgzip.a
gzip_LDADD += $(LIB_CLOCK_GETTIME)

bin_PROGRAMS = gzip
gzip_SOURCES = \
  bits.c gzip.c trees.c unpack.c unzip.c util.c zip.c parallel.c

if IBM_Z_DFLTCC
gzip_SOURCES += dfltcc.c
endif


BUILT_SOURCES += version.c
version.c: Makefile
	$(AM_V_GEN)rm -f $@
	$(AM_V_at)printf '#include <config.h>\n' > $@t
	$(AM_V_at)printf 'char const *Version = "$(PACKAGE_VERSION)";\n' >> $@t
	$(AM_V_at)chmod a-w $@t
	$(AM_V_at)mv $@t $@

BUILT_SOURCES += version.h
version.h: Makefile
	$(AM_V_GEN)rm -f $@
	$(AM_V_at)printf 'extern char const *Version;\n' > $@t
	$(AM_V_at)chmod a-w $@t
	$(AM_V_at)mv $@t $@

BUILT_SOURCES += $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version
