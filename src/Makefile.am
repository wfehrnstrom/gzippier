bin_PROGRAMS = gzip
gzip_SOURCES = \
  bits.c deflate.c gzip.c inflate.c lzw.c \
  trees.c unlzh.c unlzw.c unpack.c unzip.c util.c zip.c
gzip_LDADD = ../libver.a ../lib/libgzip.a
gzip_LDADD += $(LIB_CLOCK_GETTIME)
if IBM_Z_DFLTCC
gzip_SOURCES += dfltcc.c
endif
