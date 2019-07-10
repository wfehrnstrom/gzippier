/* gzip.h -- common declarations for all gzip modules

   Copyright (C) 1997-1999, 2001, 2006-2007, 2009-2019 Free Software
   Foundation, Inc.

   Copyright (C) 1992-1993 Jean-loup Gailly.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef GZIP_H
#define GZIP_H

#ifdef __STDC__
  typedef void *voidp;
#else
  typedef char *voidp;
#endif

#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8) || __STRICT_ANSI__
#  define __attribute__(x)
# endif
#endif

/* I don't like nested includes, but the following headers are used
 * too often
 */
#include <stdio.h>
#include <sys/types.h> /* for off_t */
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdnoreturn.h>
#include <stdbool.h>
#include <config.h>

#define memzero(s, n) memset ((voidp)(s), 0, (n))

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

typedef struct magic_header {
  uch flags;
  uch magic[10];
  int imagic0;
  int imagic1;
  ulg stamp;
} magic_header;

/* Return codes from gzip */
#define OK      0
#define ERROR   1
#define WARNING 2

/* Compression methods (see algorithm.doc) */
#define STORED      0
#define COMPRESSED  1
#define PACKED      2
#define LZHED       3
/* methods 4 to 7 reserved */
#define DEFLATED    8
#define MAX_METHODS 9
extern int method;         /* compression method */

/* To save memory for 16 bit systems, some arrays are overlaid between
 * the various modules:
 * deflate:  prev+head   window      d_buf  l_buf  outbuf
 * unlzw:    tab_prefix  tab_suffix  stack  inbuf  outbuf
 * inflate:              window             inbuf
 * unpack:               window             inbuf  prefix_len
 * unlzh:    left+right  window      c_table inbuf c_len
 * For compression, input is done in window[]. For decompression, output
 * is done in window except for unlzw.
 */


// Rip out SMALL_MEM. Redefine it to be something greater for current embedded system.
// Good chance we want to align buffers on page size boundaries.
// Look into aligning buffers on page size boundaries
// if we don't already do this in gzip.


#ifndef	INBUFSIZE
#  define INBUFSIZE 0x40000 /* input buffer size */
#endif
#define INBUF_EXTRA  64     /* required by unlzw() */

#ifndef	OUTBUFSIZE
#  define OUTBUFSIZE   8192  /* output buffer size */
#endif
#define OUTBUF_EXTRA 2048   /* required by unlzw() */

#ifndef DIST_BUFSIZE
#  define DIST_BUFSIZE 0x8000 /* buffer for distances, see trees.c */
#endif

#ifdef DYN_ALLOC
#  define EXTERN(type, array)  extern type * array
#  define DECLARE(type, array, size)  type * array
#  define ALLOC(type, array, size) { \
      array = (type*)fcalloc((size_t)(((size)+1L)/2), 2*sizeof(type)); \
      if (!array) xalloc_die (); \
   }
#  define FREE(array) {if (array != NULL) fcfree(array), array=NULL;}
#else
#  define EXTERN(type, array)  extern type array[]
#  define DECLARE(type, array, size)  type array[size]
#  define ALLOC(type, array, size)
#  define FREE(array)
#endif

EXTERN(uch, inbuf);          /* input buffer */
EXTERN(uch, outbuf);         /* output buffer */
EXTERN(ush, d_buf);          /* buffer for distances, see trees.c */
EXTERN(uch, window);         /* Sliding window and suffix table (unlzw) */
#define tab_suffix window
#ifndef MAXSEG_64K
#  define tab_prefix prev    /* hash link (see deflate.c) */
#ifndef head
#  define headGZIP (prev+WSIZE)  /* hash head (see deflate.c) */
#endif
   EXTERN(ush, tab_prefix);  /* prefix code (see unlzw.c) */
#else
#  define tab_prefix0 prev
#  define head tab_prefix1
   EXTERN(ush, tab_prefix0); /* prefix for even codes */
   EXTERN(ush, tab_prefix1); /* prefix for odd  codes */
#endif

extern unsigned insize; /* valid bytes in inbuf */
extern unsigned inptr;  /* index of next byte to be processed in inbuf */
extern unsigned outcnt; /* bytes in output buffer */
extern int rsync;  /* deflate into rsyncable chunks */

extern off_t bytes_in;   /* number of input bytes */
extern off_t bytes_out;  /* number of output bytes */
extern off_t header_bytes;/* number of bytes in gzip header */

extern int  ifd;        /* input file descriptor */
extern int  ofd;        /* output file descriptor */
extern int  threads;    /* number of compress threads */
extern char ifname[];   /* input file name or "stdin" */
extern char ofname[];   /* output file name or "stdout" */
extern char *program_name;  /* program name */

extern struct timespec time_stamp; /* original timestamp (modification time) */
extern off_t ifile_size; /* input file size, -1 for devices (debug only) */

typedef int file_t;     /* Do not use stdio */
#define NO_FILE  (-1)   /* in memory compression */


#define	PACK_MAGIC     "\037\036" /* Magic header for packed files */
#define	GZIP_MAGIC     "\037\213" /* Magic header for gzip files, 1F 8B */
#define	OLD_GZIP_MAGIC "\037\236" /* Magic header for gzip 0.5 = freeze 1.x */
#define	LZH_MAGIC      "\037\240" /* Magic header for SCO LZH Compress files*/
#define PKZIP_MAGIC    "\120\113\003\004" /* Magic header for pkzip files */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEADER_CRC   0x02 /* bit 1 set: CRC16 for the gzip header */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

/* internal file attribute */
#define UNKNOWN 0xffff
#define BINARY  0
#define ASCII   1

#ifndef WSIZE
#  define WSIZE 0x8000     /* window size--must be a power of two, and */
#endif                     /*  at least 32K for zip's deflate method */

#define MIN_MATCH  3
#define MAX_MATCH  258
/* The minimum and maximum match lengths */

#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */

#define MAX_DIST  (WSIZE-MIN_LOOKAHEAD)
/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */

#define CHUNK 16384
/* zlib deflate and inflate chunk size. See zlib docs for more details */

/* Common defaults */
#ifndef OS_CODE
#  define OS_CODE  0x03  /* assume Unix */
#endif

#ifndef SIGPIPE
# define SIGPIPE 0
#endif

#ifndef casemap
#  define casemap(c) (c)
#endif

#ifndef OPTIONS_VAR
#  define OPTIONS_VAR "GZIP"
#endif

#ifndef Z_SUFFIX
#  define Z_SUFFIX ".gz"
#endif

#ifdef MAX_EXT_CHARS
#  define MAX_SUFFIX  MAX_EXT_CHARS
#else
#  define MAX_SUFFIX  30
#endif

#ifndef MAKE_LEGAL_NAME
#  ifdef NO_MULTIPLE_DOTS
#    define MAKE_LEGAL_NAME(name)   make_simple_name(name)
#  else
#    define MAKE_LEGAL_NAME(name)
#  endif
#endif

#ifndef MIN_PART
#  define MIN_PART 3
   /* keep at least MIN_PART chars between dots in a file name. */
#endif

#ifndef EXPAND
#  define EXPAND(argc,argv)
#endif

#ifndef SET_BINARY_MODE
#  define SET_BINARY_MODE(fd)
#endif

// try to get rid of it.

#ifndef FALLTHROUGH
# if __GNUC__ < 7
#  define FALLTHROUGH ((void) 0)
# else
#  define FALLTHROUGH __attribute__ ((__fallthrough__))
# endif
#endif

extern int exit_code;      /* program exit code */
extern int verbose;        /* be verbose (-v) */
extern int quiet;          /* be quiet (-q) */
extern uint8_t level;          /* compression level */
extern int test;           /* check .z file integrity */
extern int to_stdout;      /* output to stdout (-c) */
extern int save_orig_name; /* set if original name must be saved */
extern int no_name;        /* original name should not be restored */
extern int pkzip;          /* global variable for pkzip */

#define get_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf(false, CHUNK))
#define try_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf(true, CHUNK))

/* put_byte is used for the compressed output, put_ubyte for the
 * uncompressed output. However unlzw() uses window for its
 * suffix table instead of its output buffer, so it does not use put_ubyte
 * (to be cleaned up).
 */
#define put_byte(c) {outbuf[outcnt++]=(uch)(c); if (outcnt==OUTBUFSIZE)\
   flush_outbuf();}
#define put_ubyte(c) {window[outcnt++]=(uch)(c); if (outcnt==WSIZE)\
   flush_window();}

/* Output a 16 bit value, lsb first */
#define put_short(w) \
{ if (outcnt < OUTBUFSIZE-2) { \
    outbuf[outcnt++] = (uch) ((w) & 0xff); \
    outbuf[outcnt++] = (uch) ((ush)(w) >> 8); \
  } else { \
    put_byte((uch)((w) & 0xff)); \
    put_byte((uch)((ush)(w) >> 8)); \
  } \
}

/* Output a 32 bit value to the bit stream, lsb first */
#define put_long(n) { \
    put_short((n) & 0xffff); \
    put_short(((ulg)(n)) >> 16); \
}

#define seekable()    0  /* force sequential output */
#define translate_eol 0  /* no option -a yet */

#define tolow(c)  (isupper (c) ? tolower (c) : (c))  /* force to lower case */

/* Macros for getting two-byte and four-byte header values */
#define SH(p) ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8))
#define LG(p) ((ulg)(SH(p)) | ((ulg)(SH((p)+2)) << 16))

/* Diagnostic functions */
#ifdef DEBUG
#  define Assert(cond,msg) {if (!(cond)) gzip_error (msg);}
#  define Trace(x) fprintf x
#  define Tracev(x) {if (verbose) fprintf x ;}
#  define Tracevv(x) {if (verbose>1) fprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (verbose>1 && (c)) fprintf x ;}
#else
#  define Assert(cond,msg)
#  define Trace(x)
#  define Tracev(x)
#  define Tracevv(x)
#  define Tracec(c,x)
#  define Tracecv(c,x)
#endif

#define WARN(msg) {if (!quiet) fprintf msg ; \
                   if (exit_code == OK) exit_code = WARNING;}

        /* in zip.c: */
extern int zip        (int in, int out);
extern int file_read  (char *buf,  unsigned size);

        /* in unzip.c */
extern int unzip      (int in, int out);
extern int check_zipfile (int in);

        /* in unpack.c */
extern int unpack     (int in, int out);

        /* in gzip.c */
extern noreturn void abort_gzip (void);

        /* in deflate.c */
extern void lm_init (int pack_level);
extern off_t deflateGZIP (int pack_level);

        /* in trees.c */
extern void ct_init     (ush *attr, int *method);
extern int  ct_tally    (int dist, int lc);
extern off_t flush_block (char *buf, ulg stored_len, int pad, int eof);

        /* in bits.c */
extern unsigned short bi_buf;
extern int bi_valid;
extern void     bi_init    (file_t zipfile);
extern void     send_bits  (int value, int length);
extern unsigned bi_reverse (unsigned value, int length) _GL_ATTRIBUTE_CONST;
extern void     bi_windup  (void);
extern void     copy_block (char *buf, unsigned len, int header);
extern int     (*read_buf) (char *buf, unsigned size);

        /* in util.c: */
extern int copy           (int in, int out);
extern ulg  updcrc        (const uch *s, unsigned n);
extern ulg  getcrc        (void) _GL_ATTRIBUTE_PURE;
extern void setcrc        (ulg c);
extern void clear_bufs    (void);
extern int  fill_inbuf    (int eof_ok, int max_fill);
extern void flush_outbuf  (void);
extern void flush_window  (void);
extern void write_buf     (int fd, voidp buf, unsigned cnt);
extern int read_buffer    (int fd, voidp buf, unsigned int cnt);
extern char *strlwr       (char *s);
extern char *gzip_base_name (char *fname) _GL_ATTRIBUTE_PURE;
extern int xunlink        (char *fname);
extern void make_simple_name (char *name);
extern char *add_envopt   (int *argcp, char ***argvp, char const *env);
extern noreturn void gzip_error    (char const *m);
extern noreturn void xalloc_die    (void);
extern void warning       (char const *m);
extern noreturn void read_error    (void);
extern noreturn void write_error   (void);
extern void display_ratio (off_t num, off_t den, FILE *file);
extern void fprint_off    (FILE *, off_t, int);

        /* in unzip.c */
extern int inflateGZIP (void);
extern int inflatePKZIP (void);

        /* in dfltcc.c */
#ifdef IBM_Z_DFLTCC
extern int dfltcc_deflate (int pack_level);
extern int dfltcc_inflate (void);
#endif

        /* in parallel.c */
extern off_t parallel_zip (int pack_level);

#endif
