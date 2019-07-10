/* zip.c -- compress files to the gzip or pkzip format

   Copyright (C) 1997-1999, 2006-2007, 2009-2019 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <config.h>
#include <ctype.h>
#include <assert.h>
#include "gzip.h"
#include "zlib.h"
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/errno.h>
#define CHUNK 16384

off_t header_bytes;   /* number of bytes in gzip header */

/* Speed options for the general purpose bit flag.  */
enum { SLOW = 2, FAST = 4 };

/* Deflate using zlib
 */
off_t
deflateGZIP (int pack_level)
{
  if (threads > 0) {
    return parallel_zip(pack_level);
  }

    // source is input file descriptor, dest is input file descriptor
    int ret, flush;
    unsigned writtenOutBytes;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    int source = ifd;
    int dest = ofd;

    memzero(in, CHUNK);
    memzero(out, CHUNK);

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit2(
            &strm,
            pack_level,
            Z_DEFLATED,         // set this for deflation to work
            MAX_WBITS + 16,     // max window bits + 16 for gzip encoding
            8,                  // memlevel default
            Z_DEFAULT_STRATEGY  // strategy
    );
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        int bytes_in = read(source, in, CHUNK);
        if(bytes_in < 0)
          {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
          }
        else
          {
            strm.avail_in = bytes_in;
          }
        if (rsync == true)
          {
            // Very unsure about effectiveness of Z_FULL_FLUSH for rsyncable
            flush = (strm.avail_in == 0) ? Z_FINISH : Z_FULL_FLUSH;
          }
        else
          {
            flush = (strm.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH;
          }
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            writtenOutBytes = CHUNK - strm.avail_out;
            if (write(dest, out, writtenOutBytes) != writtenOutBytes) {
                fprintf(stderr, "%s\n", strerror(errno));
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        }
      while (strm.avail_out == 0);
      assert (strm.avail_in == 0);     /* all input will be used */

      /* done when last data in file processed */
    }
  while (flush != Z_FINISH);
  assert (ret == Z_STREAM_END);        /* stream will be complete */

  /* clean up and return */
  int end_ret = deflateEnd (&strm);
  if (end_ret == Z_STREAM_ERROR) {
      return Z_STREAM_ERROR;
  }
  return Z_OK;
}

/* ===========================================================================
 * Deflate in to out.
 * IN assertions: the input and output buffers are cleared.
 *   The variables time_stamp and save_orig_name are initialized.
 */
int
zip (int in, int out)
{

  ifd = in;
  ofd = out;

  method = DEFLATED;

  if(!(0 < time_stamp.tv_sec && time_stamp.tv_sec <= 0xffffffff) && !no_name
    && time_stamp.tv_nsec >= 0)
    {
      /* It's intended that timestamp 0 generates this warning,
         since gzip format reserves 0 for something else.  */
      warning ("file timestamp out of range for gzip format");
    }

#ifdef IBM_Z_DFLTCC
  dfltcc_deflate (level);
#else
  deflateGZIP (level);
#endif

  return OK;
}


/* ===========================================================================
 * Read a new buffer from the current input file, perform end-of-line
 * translation, and update the crc and input file size.
 * IN assertion: size >= 2 (for end-of-line translation)
 */
int
file_read (char *buf, unsigned size)
{
  unsigned len;

  Assert (insize == 0, "inbuf not empty");

  len = read_buffer (ifd, buf, size);
  if (len == 0)
    return (int)len;
  if (len == (unsigned)-1)
    read_error();

  updcrc ((uch *) buf, len);
  bytes_in += (off_t)len;
  return (int)len;
}
