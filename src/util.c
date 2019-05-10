/* util.c -- utility functions for gzip support

   Copyright (C) 1997-1999, 2001-2002, 2006, 2009-2019 Free Software
   Foundation, Inc.
   Copyright (C) 1992-1993 Jean-loup Gailly

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

#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "tailor.h"
#include "gzip.h"
#include <dirname.h>
#include <xalloc.h>

#ifndef CHAR_BIT
#  define CHAR_BIT 8
#endif

static int write_buffer (int, voidp, unsigned int);

/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by makecrc.c)
 */
static const ulg crc_32_tab[] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

/* Shift register contents.  */
static ulg crc = 0xffffffffL;

/* ===========================================================================
 * Copy input to output unchanged: zcat == cat with --force.
 * IN assertion: insize bytes have already been read in inbuf and inptr bytes
 * already processed or copied.
 */
int copy(in, out)
    int in, out;   /* input and output file descriptors */
{
    int got;

    errno = 0;
    while (insize > inptr) {
        write_buf(out, (char*)inbuf + inptr, insize - inptr);
        bytes_out += insize - inptr;
        got = read_buffer (in, (char *) inbuf, INBUFSIZ);
        if (got == -1)
            read_error();
        bytes_in += got;
        insize = (unsigned)got;
        inptr = 0;
    }
    return OK;
}

/* ===========================================================================
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
ulg updcrc(s, n)
    const uch *s;           /* pointer to bytes to pump through */
    unsigned n;             /* number of bytes in s[] */
{
    register ulg c;         /* temporary variable */

    if (s == NULL) {
        c = 0xffffffffL;
    } else {
        c = crc;
        if (n) do {
            c = crc_32_tab[((int)c ^ (*s++)) & 0xff] ^ (c >> 8);
        } while (--n);
    }
    crc = c;
    return c ^ 0xffffffffL;       /* (instead of ~c for 64-bit machines) */
}

/* Return a current CRC value.  */
ulg
getcrc (void)
{
  return crc ^ 0xffffffffL;
}

/* Set a new CRC value.  */
void
setcrc (ulg c)
{
  crc = c ^ 0xffffffffL;
}

/* ===========================================================================
 * Clear input and output buffers
 */
void clear_bufs()
{
    outcnt = 0;
    insize = inptr = 0;
    bytes_in = bytes_out = 0L;
}

/* ===========================================================================
 * Fill the input buffer. This is called only when the buffer is empty.
 */
int fill_inbuf(eof_ok)
    int eof_ok;          /* set if EOF acceptable as a result */
{
    int len;

    /* Read as much as possible */
    insize = 0;
    do {
        len = read_buffer (ifd, (char *) inbuf + insize, INBUFSIZ - insize);
        if (len == 0) break;
        if (len == -1) {
          read_error();
          break;
        }
        insize += len;
    } while (insize < INBUFSIZ);

    if (insize == 0) {
        if (eof_ok) return EOF;
        flush_window();
        errno = 0;
        read_error();
    }
    bytes_in += (off_t)insize;
    inptr = 1;
    return inbuf[0];
}

/* Like the standard read function, except do not attempt to read more
   than INT_MAX bytes at a time.  */
int
read_buffer (fd, buf, cnt)
     int fd;
     voidp buf;
     unsigned int cnt;
{
  int len;
  if (INT_MAX < cnt)
    cnt = INT_MAX;
  len = read (fd, buf, cnt);

#if defined F_SETFL && O_NONBLOCK && defined EAGAIN
  /* Input files are opened O_NONBLOCK for security reasons.  On some
     file systems this can cause read to fail with errno == EAGAIN.  */
  if (len < 0 && errno == EAGAIN)
    {
      int flags = fcntl (fd, F_GETFL);
      if (0 <= flags)
        {
          if (! (flags & O_NONBLOCK))
            errno = EAGAIN;
          else if (fcntl (fd, F_SETFL, flags & ~O_NONBLOCK) != -1)
            len = read (fd, buf, cnt);
        }
    }
#endif

  return len;
}

/* Likewise for 'write'.  */
static int
write_buffer (fd, buf, cnt)
     int fd;
     voidp buf;
     unsigned int cnt;
{
  if (INT_MAX < cnt)
    cnt = INT_MAX;
  return write (fd, buf, cnt);
}

/* ===========================================================================
 * Write the output buffer outbuf[0..outcnt-1] and update bytes_out.
 * (used for the compressed data only)
 */
void flush_outbuf()
{
    if (outcnt == 0) return;

    if (!test)
      write_buf (ofd, outbuf, outcnt);
    bytes_out += (off_t)outcnt;
    outcnt = 0;
}

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
void flush_window()
{
    if (outcnt == 0) return;
    updcrc(window, outcnt);

    if (!test) {
        write_buf(ofd, (char *)window, outcnt);
    }
    bytes_out += (off_t)outcnt;
    outcnt = 0;
}

/* ===========================================================================
 * Does the same as write(), but also handles partial pipe writes and checks
 * for error return.
 */
void write_buf(fd, buf, cnt)
    int       fd;
    voidp     buf;
    unsigned  cnt;
{
    unsigned  n;

    while ((n = write_buffer (fd, buf, cnt)) != cnt) {
        if (n == (unsigned)(-1)) {
            write_error();
        }
        cnt -= n;
        buf = (voidp)((char*)buf+n);
    }
}

/* ========================================================================
 * Put string s in lower case, return s.
 */
char *strlwr(s)
    char *s;
{
    char *t;
    for (t = s; *t; t++)
      *t = tolow ((unsigned char) *t);
    return s;
}

/* ========================================================================
 * Return the base name of a file (remove any directory prefix and
 * any version suffix). For systems with file names that are not
 * case sensitive, force the base name to lower case.
 */
char *
gzip_base_name (filename)
    char *filename;
{
    filename = last_component (filename);
    if (casemap('A') == 'a') strlwr(filename);
    return filename;
}

/* ========================================================================
 * Unlink a file, working around the unlink readonly bug (if present).
 */
int xunlink (filename)
     char *filename;
{
  int r = unlink (filename);

#ifdef UNLINK_READONLY_BUG
  if (r != 0)
    {
      int e = errno;
      if (chmod (filename, S_IWUSR) != 0)
        {
          errno = e;
          return -1;
        }

      r = unlink (filename);
    }
#endif

  return r;
}

/* ========================================================================
 * Make a file name legal for file systems not allowing file names with
 * multiple dots or starting with a dot (such as MSDOS), by changing
 * all dots except the last one into underlines.  A target dependent
 * function can be used instead of this simple function by defining the macro
 * MAKE_LEGAL_NAME in tailor.h and providing the function in a target
 * dependent module.
 */
void make_simple_name(name)
    char *name;
{
    char *p = strrchr(name, '.');
    if (p == NULL) return;
    if (p == name) p++;
    do {
        if (*--p == '.') *p = '_';
    } while (p != name);
}

/* Convert the value of the environment variable ENVVAR_NAME
   to a newly allocated argument vector, and set *ARGCP and *ARGVP
   to the number of arguments and to the vector, respectively.
   Make the new vector's zeroth element equal to the old **ARGVP.
   Return a pointer to the newly allocated string storage.

   If the vector would be empty, do not allocate storage,
   do not set *ARGCP and *ARGVP, and return NULL.  */

#define SEPARATOR	" \t"	/* separators in env variable */

char *add_envopt(
    int *argcp,          /* pointer to argc */
    char ***argvp,       /* pointer to argv */
    char const *envvar_name) /* name of environment variable */
{
    char *p;             /* running pointer through env variable */
    char **oargv;        /* runs through old argv array */
    char **nargv;        /* runs through new argv array */
    int  nargc = 0;      /* number of arguments in env variable */
    char *env_val;

    env_val = getenv(envvar_name);
    if (env_val == NULL) return NULL;

    env_val = xstrdup (env_val);

    for (p = env_val; *p; nargc++ ) {        /* move through env_val */
        p += strspn(p, SEPARATOR);	     /* skip leading separators */
        if (*p == '\0') break;

        p += strcspn(p, SEPARATOR);	     /* find end of word */
        if (*p) *p++ = '\0';		     /* mark it */
    }
    if (nargc == 0) {
        free(env_val);
        return NULL;
    }
    *argcp = nargc + 1;
    /* Allocate the new argv array, with an extra element just in case
     * the original arg list did not end with a NULL.
     */
    nargv = xcalloc (*argcp + 1, sizeof (char *));
    oargv  = *argvp;
    *argvp = nargv;

    /* Copy the program name first */
    *nargv++ = *oargv;

    /* Then copy the environment args */
    for (p = env_val; nargc > 0; nargc--) {
        p += strspn(p, SEPARATOR);	     /* skip separators */
        *(nargv++) = p;			     /* store start */
        while (*p++) ;			     /* skip over word */
    }

    *nargv = NULL;
    return env_val;
}

/* ========================================================================
 * Error handlers.
 */
void
gzip_error (char const *m)
{
    fprintf (stderr, "\n%s: %s: %s\n", program_name, ifname, m);
    abort_gzip();
}

void
xalloc_die ()
{
  fprintf (stderr, "\n%s: memory_exhausted\n", program_name);
  abort_gzip ();
}

void warning (char const *m)
{
    WARN ((stderr, "%s: %s: warning: %s\n", program_name, ifname, m));
}

void read_error()
{
    int e = errno;
    fprintf (stderr, "\n%s: ", program_name);
    if (e != 0) {
        errno = e;
        perror(ifname);
    } else {
        fprintf(stderr, "%s: unexpected end of file\n", ifname);
    }
    abort_gzip();
}

void write_error()
{
    int e = errno;
    fprintf (stderr, "\n%s: ", program_name);
    errno = e;
    perror(ofname);
    abort_gzip();
}

/* ========================================================================
 * Display compression ratio on the given stream on 6 characters.
 */
void display_ratio(num, den, file)
    off_t num;
    off_t den;
    FILE *file;
{
    fprintf(file, "%5.1f%%", den == 0 ? 0 : 100.0 * num / den);
}

/* ========================================================================
 * Print an off_t.  There's no completely portable way to use printf,
 * so we do it ourselves.
 */
void fprint_off(file, offset, width)
    FILE *file;
    off_t offset;
    int width;
{
    char buf[CHAR_BIT * sizeof (off_t)];
    char *p = buf + sizeof buf;

    /* Don't negate offset here; it might overflow.  */
    if (offset < 0) {
        do
          *--p = '0' - offset % 10;
        while ((offset /= 10) != 0);

        *--p = '-';
    } else {
        do
          *--p = '0' + offset % 10;
        while ((offset /= 10) != 0);
    }

    width -= buf + sizeof buf - p;
    while (0 < width--) {
        putc (' ', file);
    }
    for (;  p < buf + sizeof buf;  p++)
        putc (*p, file);
}
