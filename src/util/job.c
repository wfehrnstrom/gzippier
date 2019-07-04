/*
   Copyright (C) 1999, 2001-2002, 2006-2007, 2009-2019 Free Software
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

#include <pthread.h>
#include <sys/types.h>
#include <config.h>
#include <unistd.h>
#include "zlib.h"
#include "gzip.h"
#include "thread.h"
#include "buffer.h"
#include "job.h"
#include "parallel.h"

/* Job helpers */

/* I/O helper */

static ssize_t
readn (int fd, unsigned char *buf, size_t len)
{
  ssize_t result;
  ssize_t amount = 0;

  while (len)
    {
      result = read (fd, buf, len);
      if (result < 0)
      	{
      	  return result;
      	}
      if (result == 0)
      	{
      	  break;
      	}
      buf += result;
      amount += result;
      len -= (size_t) result;
    }
  return amount;
}

/*
 * inserts job at the end of the job list, and broadcasts that the new job
 * is inserted. If a null ptr is passed for either job or list, then this
 * is a no-op.
 */
void
insert_job_back_of_list (struct job * job,
  struct job_list * list)
{
  if (list && job)
  {
    lock (&(list->lock));
    if (list->head == NULL)
      {
        list->head = job;
        list->tail = job;
      }
    else
      {
        list->tail->next = job;
        list->tail = job;
      }
    broadcast (&(list->lock));
    unlock (&(list->lock));
  }
}

struct job *
get_job (long seq)
{
  struct job *result;
  lock (&free_jobs.lock);
  if (free_jobs.head == NULL)
    {
      unlock (&free_jobs.lock);
      // allocate a new job
      result = malloc (sizeof (struct job));
      if (result == NULL)
      	{
      	  return NULL;
      	}
      result->in = NULL;
      result->out = NULL;
      result->dict = NULL;
      init_lock (&result->check_done);
    }
  else
    {
      // grab one from the list
      result = free_jobs.head;
      free_jobs.head = free_jobs.head->next;
      unlock (&free_jobs.lock);
    }
  lock (&result->check_done);
  result->next = NULL;
  result->seq = seq;
  return result;
}

/*
 * Precondition: job is properly initialized and has an in buffer of at least
 * size.
 * Postcondition: job may
 */
ssize_t
read_into_job (struct job *job)
{
  int read = readn (ifd, job->in->data, job->in->size);
  if (read < 0)
    {
      return Z_ERRNO;
    }
  job->in->len = read;
  return read;
}

void
get_job_dict_from_last_job(struct job *job, struct job *last_job)
{
  if (!rsync && last_job->in->len >= DICTIONARY_SIZE)
    {
      // job->dict = get_buffer (&dict_pool);
      // memcpy (job->dict->data,
      //   last_job->in->data + last_job->in->len - DICTIONARY_SIZE,
      //   DICTIONARY_SIZE);
      // job->dict->len = DICTIONARY_SIZE;
      job->dict = NULL;
    }
  else
    {
      job->dict = NULL;
    }
}

int
init_job (struct job *job, long *seq)
{
  if (job == NULL)
    {
      return Z_ERRNO;
    }
  job->in = get_buffer (&in_pool);
  if (job->in == NULL)
    {
      return Z_ERRNO;
    }
  return Z_OK;
}

void
init_jobs (void)
{
  // compress jobs
  init_lock (&compress_jobs.lock);
  compress_jobs.head = NULL;
  compress_jobs.tail = NULL;

  // write jobs
  init_lock (&write_jobs.lock);
  write_jobs.head = NULL;
  write_jobs.tail = NULL;

  // free jobs
  init_lock (&free_jobs.lock);
  free_jobs.head = NULL;
  free_jobs.tail = NULL;
}

void
return_job (struct job *job)
{
  lock (&free_jobs.lock);
  job->next = free_jobs.head;
  free_jobs.head = job;
  broadcast (&free_jobs.lock);
  unlock (&free_jobs.lock);
}

void
free_job (struct job *j)
{
  if (j != NULL)
    {
      j->dict = NULL;
      // invalidate next ptr
      j->next = NULL;
      // free space allocated to job struct
      free (j);
    }
}

void
free_job_list (struct job_list *jobs)
{
  if (jobs != NULL)
  {
    struct job *current = jobs->head;
    jobs->head = NULL;
    while (current != NULL)
      {
        struct job *next = current->next;
        free_job (current);
        current = next;
      }
    jobs->tail = NULL;
  }
}
