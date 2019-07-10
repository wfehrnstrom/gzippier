#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <config.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "gzip.h"
#include "zlib.h"
#include "util/thread.h"
#include "util/buffer.h"
#include "util/job.h"
#include "parallel.h"

// initial buffer sizes
#define IN_BUF_SIZE 131072
#define OUT_BUF_SIZE 32768
#define DICTIONARY_SIZE 32768
#define MAXP2 (UINT_MAX - (UINT_MAX >> 1))

#define GZIP_MAGIC_BYTE_1 31
#define GZIP_MAGIC_BYTE_2 139

#define FNAME_BIT 3

#define DEFLATE 8

// compression level possibilities
#define BEST_COMPRESSION 9
#define FAST_COMPRESSION 1

#define NO_MORE_COMPRESSION 1

// compression level
static int pack_level;

// headers and trailers

struct gzip_header
{
  uint8_t magic1;
  uint8_t magic2;
  uint8_t method;
  uint8_t flags;
  uint32_t time;
  uint8_t xflags;
  uint8_t os;
};

static uint8_t
set_xflags (uint8_t const level)
{
  if (level == BEST_COMPRESSION)
    {
      return 2;
    }
  else if (level == FAST_COMPRESSION)
    {
      return 4;
    }
  return 0;
}

static struct gzip_header
create_header (char const *name, uint8_t const level)
{
  struct gzip_header header;
  header.magic1 = GZIP_MAGIC_BYTE_1;
  header.magic2 = GZIP_MAGIC_BYTE_2;
  header.method = DEFLATE;
  header.flags = (name != NULL) ? (1 << FNAME_BIT) : 0;
  /* TODO: time_stamp is a timespec struct currently set for compression
   * by the method get_input_size_and_time in gzip.c. time_stamp MAY also
   * be set by read_time_stamp_from_file which deals with decompression.
   * refactor these into a common function.
   */
  header.time = (uint32_t) time_stamp.tv_sec;
  header.xflags = set_xflags (level);
  header.os = 3;

  return header;
}

struct gzip_trailer
{
  uint32_t check;
  uint32_t uncompressed_len;
};

static struct gzip_trailer
create_trailer (unsigned long check, size_t ulen)
{
  struct gzip_trailer trailer;
  trailer.check = check;
  trailer.uncompressed_len = ulen;
  return trailer;
}

// i/o helpers

static void
writen (int fd, unsigned char const *buf, size_t len)
{
  size_t const max = SIZE_MAX >> 1;
  ssize_t result;

  while (len != 0)
    {
      result = write (fd, buf, len > max ? max : len);
      buf += result;
      len -= (size_t) result;
    }
}

struct job_list compress_jobs;
struct job_list write_jobs;
struct job_list free_jobs;

buffer_pool in_pool;
buffer_pool out_pool;
buffer_pool dict_pool;

// compress function

static void
deflate_buffer (z_stream * stream, buffer *out, int flush)
{
  size_t room;

  do
    {
      room = out->size - out->len;
      if (room <= 0)
      	{
      	  grow_buffer (out);
      	  room = out->size - out->len;
      	}
      stream->next_out = out->data + out->len;
      stream->avail_out = room < UINT_MAX ? (unsigned) room : UINT_MAX;
      deflate (stream, flush);
      out->len = (size_t) (stream->next_out - out->data);
    }
  while (stream->avail_out == 0);
}

// thread functions

static noreturn void *
write_thread (void *nothing)
{
  unsigned long check = crc32_z (0L, Z_NULL, 0);

  // write the header
  struct gzip_header header;
  if (ifd == STDIN_FILENO)
    {
      header = create_header (NULL, pack_level);
      writen (ofd, (unsigned char *) &header, sizeof (header) - 2);
    }
  else
    {
      header = create_header (ifname, pack_level);
      writen (ofd, (unsigned char *) &header, sizeof (header) - 2);
      writen (ofd, (unsigned char *) ifname, strlen (ifname) + 1);
    }


  size_t ulen = 0;

  long seq = 0;
  struct job *job;
  do
    {
      // wait for the next job in sequence
      lock (&write_jobs.lock);
      while ((write_jobs.head == NULL || seq != write_jobs.head->seq)
	     && !write_jobs.lock.value)
      	{
      	  wait_lock (&write_jobs.lock);
      	}
      if (write_jobs.lock.value)
      	{
      	  break;
      	}

      // get job
      job = write_jobs.head;
      write_jobs.head = job->next;
      if (job->next == NULL)
      	{
      	  write_jobs.tail = NULL;
      	}
      unlock (&write_jobs.lock);

      // write data and return out buffer
      writen (ofd, job->out->data, job->out->len);
      // return the buffer
      return_buffer (job->out);
      // wait for checksum
      lock (&job->check_done);
      // assemble the checksum
      check = crc32_combine (check, job->check, job->check_done.value);
      ulen += job->check_done.value;
      unlock (&job->check_done);
      // return the job
      return_job (job);

      seq++;
    }
  while (job->more);

  // write the trailer
  struct gzip_trailer trailer = create_trailer (check, ulen);
  writen (ofd, (unsigned char *) &trailer, sizeof (trailer));
  pthread_exit (NULL);
}

static noreturn void *
compress_thread (void *nothing)
{
  struct job *job;
  size_t len;

  // init the deflate stream for this thread
  z_stream stream;
  stream.zfree = Z_NULL;
  stream.zalloc = Z_NULL;
  stream.opaque = Z_NULL;
  stream.next_in = 0;
  deflateInit2 (&stream, pack_level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
  for (;;)
    {
      // get a job from the compress list
      lock (&compress_jobs.lock);
      while (compress_jobs.head == NULL)
      	{
          /* main thread may broadcast at any time that all write
           * work is done by setting the lock value on compress jobs
           * to 1, so if this is the case, do no work.
           */
      	  if (compress_jobs.lock.value == NO_MORE_COMPRESSION)
      	    {
              int ret = deflateEnd (&stream);
              if (ret != Z_OK)
                {
                  fprintf(stderr, "zlib error on end of parallel deflate: %s\n",
                    stream.msg);
                }
      	      unlock (&compress_jobs.lock);
      	      pthread_exit (NULL);
      	    }
      	  wait_lock (&compress_jobs.lock);
      	}
      // pop compress job from head of linked list
      job = compress_jobs.head;
      if (compress_jobs.head == compress_jobs.tail)
      	{
      	  compress_jobs.tail = NULL;
      	}
      compress_jobs.head = compress_jobs.head->next;

      unlock (&compress_jobs.lock);
      len = job->in->len;
      // reset the stream
      deflateReset (&stream);
      deflateParams (&stream, pack_level, Z_DEFAULT_STRATEGY);
      // set the dictionary
      if (job->dict != NULL)
      	{
      	  deflateSetDictionary (&stream, job->dict->data, DICTIONARY_SIZE);
      	  // return the dictionary buffer
      	  return_buffer (job->dict);
      	}
      // get an out buffer
      do
      	{
      	  job->out = get_buffer (&out_pool);
      	}
      while (job->out == NULL);
      // set up stream struct
      stream.next_in = job->in->data;
      stream.next_out = job->out->data;
      // check if this is the last (empty) block

      size_t left = job->in->len;
      while (left > MAXP2)
      	{
      	  stream.avail_in = MAXP2;
      	  deflate_buffer (&stream, job->out, Z_NO_FLUSH);
      	  left -= MAXP2;
      	}
      stream.avail_in = (unsigned) left;

      if (job->more)
      	{
      	  // compress normally
      	  deflate_buffer (&stream, job->out, Z_BLOCK);
      	  int bits;
      	  deflatePending (&stream, Z_NULL, &bits);
      	  if (bits & 1)
      	    {
      	      deflate_buffer (&stream, job->out, Z_SYNC_FLUSH);
      	    }
      	  else if (bits & 7)
      	    {
      	      do
            		{
            		  // bits = deflatePrime (&stream, 10, 2);
                  deflatePrime(&stream, 10, 2);
            		  deflatePending (&stream, Z_NULL, &bits);
            		}
      	      while (bits & 7);
      	      deflate_buffer (&stream, job->out, Z_BLOCK);
      	    }
      	}
      else
      	{
      	  // finish compression
      	  deflate_buffer (&stream, job->out, Z_FINISH);
      	}
      // put job on the write list
      lock (&write_jobs.lock);
      struct job *prev = NULL;
      struct job *cur = write_jobs.head;
      while (cur != NULL && cur->seq < job->seq)
      	{
      	  prev = cur;
      	  cur = cur->next;
      	}
      if (write_jobs.head == NULL)
      	{
      	  job->next = NULL;
      	  write_jobs.head = job;
      	}
      else if (cur == write_jobs.head)
      	{
      	  job->next = write_jobs.head;
      	  write_jobs.head = job;
      	}
      else
      	{
      	  job->next = cur;
      	  prev->next = job;
      	}
      broadcast (&write_jobs.lock);
      unlock (&write_jobs.lock);
      // calculate check
      len = job->in->len;
      unsigned char *next = job->in->data;
      unsigned long check = crc32_z (0L, Z_NULL, 0);
      while (len > MAXP2)
      	{
      	  check = crc32_z (check, next, len);
      	  len -= MAXP2;
      	  next += MAXP2;
      	}
      check = crc32_z (check, next, len);
      job->check_done.value = job->in->len;
      job->check = check;
      // return in buffer
      return_buffer (job->in);
      // unlock check
      unlock (&job->check_done);
    }

  deflateEnd (&stream);
}

static void
free_resources (pthread_t *compression_threads)
{
  if (compression_threads != NULL)
    {
        free (compression_threads);
    }
  free_job_list (&compress_jobs);
  free_job_list (&write_jobs);
  free_job_list (&free_jobs);
  free_buffer_pool (&in_pool);
  free_buffer_pool (&out_pool);
  free_buffer_pool (&dict_pool);
}

static bool
should_launch_compress_thread (int threads_compressing, int num_threads)
{
  return threads_compressing < num_threads;
}

/*
 * compression_threads is an array that keeps track of all the compression
 * threads, threads_compressing is the number of threads spawned to compress
 */
static int
launch_compress_thread (pthread_t *compression_threads, int threads_compressing)
{
  int err =
    pthread_create (compression_threads + threads_compressing, NULL,
        compress_thread, NULL);
  if (err != 0 && threads_compressing == 0)
    {
      return Z_ERRNO;
    }
  return ++threads_compressing;
}

off_t
parallel_zip (int pack_lev)
{
  pack_level = pack_lev;
  init_pools ();
  init_jobs ();

  // init compression threads array
  pthread_t *compression_threads_t = malloc (sizeof (pthread_t) * threads);

  // launch write thread
  pthread_t write_thread_t;
  if (pthread_create (&write_thread_t, NULL, write_thread, NULL) != 0)
    {
      free_resources (compression_threads_t);
      return Z_ERRNO;
    }

  int threads_compressing = 0;
  long seq = 0;

  // initial job
  struct job *last_job = get_job (seq);
  if (init_job (last_job, &seq) == Z_ERRNO)
    {
      free_resources (compression_threads_t);
      return Z_ERRNO;
    }
  seq++;

  ssize_t last_read = read_into_job (last_job);
  if (last_read == Z_ERRNO)
    {
      free_resources (compression_threads_t);
      return Z_ERRNO;
    }

  // file wasn't empty
  while (last_read != 0)
    {
      // get a job
      struct job *job = get_job (seq);
      if (init_job (job, &seq) == Z_ERRNO)
        {
          free_resources (compression_threads_t);
          return Z_ERRNO;
        }

      // read into it
      last_read = read_into_job (job);
      if (last_read == Z_ERRNO)
        {
          free_resources (compression_threads_t);
          return Z_ERRNO;
        }
      last_job->more = (last_read != 0);

      get_job_dict_from_last_job (job, last_job);

      insert_job_back_of_list (last_job, &compress_jobs);

      // launch a compress thread if possible
      if (should_launch_compress_thread (threads_compressing, threads))
      	{
      	  threads_compressing = launch_compress_thread (compression_threads_t,
            threads_compressing);
          if (threads_compressing == Z_ERRNO)
            {
              free_resources (compression_threads_t);
              return Z_ERRNO;
            }
      	}
      last_job = job;
      seq++;
    }

  // call the threads home
  pthread_join (write_thread_t, NULL);

  lock (&compress_jobs.lock);
  // cease all compression jobs across all threads
  compress_jobs.lock.value = NO_MORE_COMPRESSION;
  broadcast (&compress_jobs.lock);
  unlock (&compress_jobs.lock);

  for (int i = 0; i < threads_compressing; i++)
    {
      pthread_join (compression_threads_t[i], NULL);
    }

  // return remaining resources
  return_buffer (last_job->in);
  return_job (last_job);


  free_resources (compression_threads_t);
  compression_threads_t = NULL;

  return Z_OK;
}
