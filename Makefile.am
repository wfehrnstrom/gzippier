# Make gzip (GNU zip).

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

SRC = src

ALL_RECURSIVE_TARGETS = $(SRC)
BUILT_SOURCES =

if ZLIB_MAKEFILE
  ZLIB_SRC = lib/zlib
endif

SUBDIRS = $(ZLIB_SRC) lib doc $(SRC) . tests benchmarks
AM_CPPFLAGS = -I$(top_srcdir)/lib
AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)

DISTCLEANFILES = $(SRC)/version.c $(SRC)/version.h

man_MANS = gunzip.1 gzexe.1 gzip.1 \
  zcat.1 zcmp.1 zdiff.1 zforce.1 zgrep.1 zless.1 zmore.1 znew.1

EXTRA_DIST = $(ACINCLUDE_INPUTS) $(man_MANS) \
  ChangeLog-2007 \
  cfg.mk	\
  dist-check.mk	\
  algorithm.doc \
  gunzip.in gzexe.in gzip.doc \
  revision.h sample/makecrc.c \
  sample/ztouch sample/add.c sample/sub.c sample/zread.c sample/zfile \
  $(SRC)/tailor.h \
  zcat.in zcmp.in zdiff.in \
  zegrep.in zfgrep.in zforce.in zgrep.in zless.in zmore.in znew.in
noinst_HEADERS = $(SRC)/gzip.h

bin_SCRIPTS = gunzip gzexe zcat zcmp zdiff \
  zegrep zfgrep zforce zgrep zless zmore znew

gzip.doc: gzip.1
	$(AM_V_GEN)groff -man -Tascii $(srcdir)/gzip.1 | col -b | uniq > $@-t \
	  && mv $@-t $@

gzip.doc.gz: gzip.doc $(bin_PROGRAMS)
	$(AM_V_GEN)$(top_srcdir)/$(SRC)/gzip < $(srcdir)/gzip.doc >$@-t && mv $@-t $@

SUFFIXES = .in
.in:
	$(AM_V_GEN)rm -f $@-t $@ \
          && sed \
		-e 's|/bin/sh|$(SHELL)|g' \
		-e 's|[@]GREP@|$(GREP)|g' \
		-e 's|[@]VERSION@|$(VERSION)|g' \
		$(srcdir)/$@.in >$@-t \
	  && chmod a=rx $@-t \
	  && mv $@-t $@

# Arrange so that .tarball-version appears only in the distribution
# tarball, and never in a checked-out repository.
dist-hook: gen-ChangeLog
	$(AM_V_GEN)echo $(VERSION) > $(distdir)/.tarball-version

gen_start_date = 2008-01-01
.PHONY: gen-ChangeLog
gen-ChangeLog:
	$(AM_V_GEN)if test -d .git; then				\
	  $(top_srcdir)/build-aux/gitlog-to-changelog			\
	    --since=$(gen_start_date) > $(distdir)/cl-t &&		\
	    { rm -f $(distdir)/ChangeLog &&				\
	      mv $(distdir)/cl-t $(distdir)/ChangeLog; }		\
	fi

# Prepend "." and "./lib" to $PATH:
new_path = PATH=.$(PATH_SEPARATOR)./lib$(PATH_SEPARATOR)$$PATH

# A simple test, just of gzip -- more of a sanity check than anything else.
# AM_V_GEN signals that the command should be printed out in silent mode.
# AM_V_at signals that it should not be.

FILES_TO_CHECK = $(bin_SCRIPTS) \
  $(top_srcdir)/ChangeLog $(top_srcdir)/configure $(top_srcdir)/$(SRC)/gzip.c
check-local: $(FILES_TO_CHECK) $(bin_PROGRAMS) gzip.doc.gz
	$(AM_V_GEN)$(new_path); { test '$(srcdir)' != . \
				    || zdiff gzip.doc.gz; }
	$(AM_V_GEN)$(new_path); cp $(top_srcdir)/$(SRC)/gzip gzip
	$(AM_V_at)$(new_path); zdiff $(top_srcdir)/gzip.doc $(top_srcdir)/gzip.doc
	$(AM_V_at)$(new_path); zdiff $(top_srcdir)/gzip.doc gzip.doc.gz
	$(AM_V_at)$(new_path); zdiff - $(top_srcdir)/gzip.doc <gzip.doc.gz
	$(AM_V_at)$(new_path); zdiff gzip.doc.gz gzip.doc.gz
	$(AM_V_at)$(new_path); zgrep -iV >/dev/null
	$(AM_V_at)$(new_path);				\
	for opt in --rsyncable '' -1 -9; do		\
						for file in $(FILES_TO_CHECK); do		\
			        gzip $$opt -c -- "$$file"			\
			          | gzip -d | cmp - "$$file" || exit 1;	\
			      done;						\
	done
	$(AM_V_GEN)$(new_path)

install-exec-hook: remove-installed-links
install-exec-hook remove-installed-links:
	@for prog_ext in $(bin_PROGRAMS) $(bin_SCRIPTS); do \
	  prog=`echo "$$prog_ext"|sed 's/$(EXEEXT)$$//'`; \
	  case $$prog in \
	  gunzip) aliases='uncompress';; \
	  *) continue;; \
	  esac; \
	  transform='$(transform)'; \
	  test "X$$prog" = "X$$prog_ext" || \
	    transform="$$transform"';s/$$/$(EXEEXT)/'; \
	  destbindir=$(DESTDIR)$(bindir); \
	  source=`echo "$$prog"|sed "$$transform"`; \
	  for alias in $$aliases; do \
	    dest=`echo "$$alias"|sed "$$transform"`; \
	    (set -x; \
	     cd "$$destbindir" && \
	     rm -f "$$dest" && \
	     case $@ in \
	     install-exec-hook) \
	       ln "$$source" "$$dest" || $(LN_S) "$$source" "$$dest";; \
	     esac \
	    ) || exit; \
	  done; \
	done

uninstall-local: remove-installed-links

ALL_RECURSIVE_TARGETS += distcheck-hook
distcheck-hook:
	$(MAKE) my-distcheck

MAINTAINERCLEANFILES = gzip.doc

MOSTLYCLEANFILES = _match.i match_.s _match.S gzip.doc.gz \
  gunzip gzexe zcat zcmp zdiff zegrep zfgrep zforce zgrep zless zmore znew
