/* unzip.c -- decompress files in gzip

   Copyright (C) 1997-1999, 2009-2019 Free Software Foundation, Inc.
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

/*
 * The code in this file is derived from the file funzip.c written
 * and put in the public domain by Mark Adler.
 */

/*
   This version can extract files in gzip or pkzip format.
   For the latter, only the first entry is extracted, and it has to be
   either deflated or stored.
 */

#include <config.h>
#include <assert.h>
#include "tailor.h"
#include "gzip.h"
#include "zlib.h"
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <string.h>
#define CHUNK 16384

/* PKZIP header definitions */
#define LOCSIG 0x04034b50L      /* four-byte lead-in (lsb first) */
#define LOCFLG 6                /* offset of bit flag */
#define  CRPFLG 1               /*  bit for encrypted entry */
#define  EXTFLG 8               /*  bit for extended local header */
#define LOCHOW 8                /* offset of compression method */
/* #define LOCTIM 10               UNUSED file mod time (for decryption) */
/* #define LOCCRC 14               UNUSED offset of crc */
/* #define LOCSIZ 18               UNUSED offset of compressed size */
/* #define LOCLEN 22               UNUSED offset of uncompressed length */
#define LOCFIL 26               /* offset of file name field length */
#define LOCEXT 28               /* offset of extra field length */
#define LOCHDR 30               /* size of local header, including sig */
/* #define EXTHDR 16               UNUSED size of extended local header,
                                   inc sig */
/* #define RAND_HEAD_LEN  12       UNUSED length of encryption random header */

/* Globals */

static int decrypt;        /* flag to turn on decryption */
static int ext_header = 0; /* set if extended local header */

/* ===========================================================================
 * Check zip file and advance inptr to the start of the compressed data.
 * Get ofname from the local header if necessary.
 */
int
check_zipfile (int in)
{
  uch *h = inbuf + inptr; /* first local header */

  ifd = in;

  /* Check validity of local header, and skip name and extra fields */
  inptr += LOCHDR + SH(h + LOCFIL) + SH(h + LOCEXT);

  if (inptr > insize || LG(h) != LOCSIG)
    {
      fprintf (stderr, "\n%s: %s: not a valid zip file\n",
               program_name, ifname);
      exit_code = ERROR;
      return ERROR;
    }
  method = h[LOCHOW];
  if (method != STORED && method != DEFLATED)
    {
      fprintf (stderr,
               "\n%s: %s: first entry not deflated or stored -- use unzip\n",
               program_name, ifname);
      exit_code = ERROR;
      return ERROR;
    }

  /* If entry encrypted, decrypt and validate encryption header */
  if ((decrypt = h[LOCFLG] & CRPFLG) != 0)
    {
      fprintf (stderr, "\n%s: %s: encrypted file -- use unzip\n",
               program_name, ifname);
      exit_code = ERROR;
      return ERROR;
    }

  /* Save flags for unzip() */
  ext_header = (h[LOCFLG] & EXTFLG) != 0;
  pkzip = 1;

  /* Get ofname and timestamp from local header (to be done) */
  return OK;
}

/* Inflate pkzip files using zlib
 *
 * This function assumes that check_zipfile has already been run, and that inptr
 * points to the start of the data.
 */
int 
inflatePKZIP (void)
{
    int ret;
    unsigned writtenOutBytes;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    int source = ifd; // set to global input fd
    int dest = ofd; // set to global output fd
    bool read_prev = false;
    int bytes_to_read = CHUNK;

    memzero(in, CHUNK);
    memzero(out, CHUNK);

    // assumes that check_zipfile incremented inptr to the first data value.
    if ((insize - inptr) > 0)
      {
          memmove ((char *) in, (char *)(inbuf + inptr), insize - inptr);
          bytes_to_read -= (insize - inptr);
          read_prev = true;
      }

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, -15); // -15 sets for raw deflate, no headers.
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        int read_in;
        if (read_prev) {
            read_in = read(source, in + (insize-inptr), bytes_to_read);
        } else {
            read_in = read(source, in, CHUNK);
        }
        if (read_in < 0)
          {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
          }
        else
          {
            if (read_prev) {
                strm.avail_in = read_in + (insize-inptr);
                read_prev = false;
            } else {
                strm.avail_in = read_in;
            }
          }
        if (strm.avail_in == 0) {
          break;
        }
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            /* We need this line for a nasty side effect:
             * inptr must be set to the end of the input buffer for
             * input_eof to recognize that
             * we've processed all of the gzipped input.
             * TODO: remove this side effect dependent code by removing
             * branching on inptr.
             */
            inptr = strm.total_in;
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
              case Z_NEED_DICT:
                  ret = Z_DATA_ERROR;     /* and fall through */
                  FALLTHROUGH;
              case Z_DATA_ERROR:
              case Z_MEM_ERROR:
                  (void)inflateEnd(&strm);
                  return ret;
            }
            writtenOutBytes = CHUNK - strm.avail_out;
            int bytes_written = write(dest, out, writtenOutBytes);
            if (bytes_written != writtenOutBytes) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        }
      while (strm.avail_out == 0);
      /* done when inflate() says it's done */
    }
  while (ret != Z_STREAM_END);
  /* clean up and return */
  int end_ret = inflateEnd (&strm);
  if (end_ret == Z_STREAM_ERROR) {
      return Z_STREAM_ERROR;
  }
  int result = ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
  return result;
}

/* Inflate gzip files using zlib
 */
int
inflateGZIP (void)
{
    int ret;
    unsigned writtenOutBytes;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    int source = ifd; // set to global input fd
    int dest = ofd; // set to global output fd
    bool read_prev = false;
    int bytes_to_read = CHUNK;

    memzero(in, CHUNK);
    memzero(out, CHUNK);

    if (insize > 0)
      {
          memmove ((char *) in, (char *)inbuf, insize);
          bytes_to_read -= insize;
          read_prev = true;
      }

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, MAX_WBITS + 16);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        int read_in;
        if (read_prev) {
            read_in = read(source, in + insize, bytes_to_read);
        } else {
            read_in = read(source, in, CHUNK);
        }
        if (read_in < 0)
          {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
          }
        else
          {
            if (read_prev) {
                strm.avail_in = read_in + insize;
                read_prev = false;
            } else {
                strm.avail_in = read_in;
            }
          }
        if (strm.avail_in == 0) {
          break;
        }
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            /* We need this line for a nasty side effect:
             * inptr must be set to the end of the input buffer for
             * input_eof to recognize that
             * we've processed all of the gzipped input.
             * TODO: remove this side effect dependent code by removing
             * branching on inptr.
             */
            inptr = strm.total_in;
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
              case Z_NEED_DICT:
                  ret = Z_DATA_ERROR;     /* and fall through */
                  FALLTHROUGH;
              case Z_DATA_ERROR:
              case Z_MEM_ERROR:
                  (void)inflateEnd(&strm);
                  return ret;
            }
            writtenOutBytes = CHUNK - strm.avail_out;
            int bytes_written = write(dest, out, writtenOutBytes);
            if (bytes_written != writtenOutBytes) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        }
      while (strm.avail_out == 0);
      /* done when inflate() says it's done */
    }
  while (ret != Z_STREAM_END);
  /* clean up and return */
  int end_ret = inflateEnd (&strm);
  if (end_ret == Z_STREAM_ERROR) {
      return Z_STREAM_ERROR;
  }
  int result = ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
  return result;
}

/* ===========================================================================
 * Unzip in to out.  This routine works on both gzip and pkzip files.
 *
 * IN assertions: the buffer inbuf contains already the beginning of
 *   the compressed data, from offsets inptr to insize-1 included.
 *   The magic header has already been checked. The output buffer is cleared.
 */
int
unzip (int in, int out)
{
  ifd = in;
  ofd = out;

  /* Decompress */
  if (method == DEFLATED)
    {
      int res;

      if (pkzip == 1) {
        res = inflatePKZIP (); 
        pkzip = 1 // set for next file
      } else {

#ifdef IBM_Z_DFLTCC
        res = dfltcc_inflate ();
#else
        res = inflateGZIP ();
#endif

      }

      if (res == 3)
        {
          xalloc_die ();
        }
      else if (res != 0)
        {
          gzip_error ("invalid compressed data--format violated");
        }
    }
  else 
    {
      gzip_error ("internal error, invalid method");
    }

  return OK;
}
