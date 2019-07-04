#include <stdlib.h>

#include "thread.h"
#include "parallel.h"
#include "gzip.h"
#include "buffer.h"

#define IN_BUF_SIZE 131072
#define OUT_BUF_SIZE 32768
#define DICTIONARY_SIZE 32768

buffer *
get_buffer (struct buffer_pool *pool)
{
  buffer *result;
  lock (&pool->lock);
  // wait until you can create a new buffer or grab an existing one
  while (pool->num_buffers == 0)
    {
      wait_lock (&pool->lock);
    }

  pool->num_buffers--;
  if (pool->head == NULL)
    {
      unlock (&pool->lock);
      // allocate a new buffer
      result = malloc (sizeof (buffer));
      if (result == NULL)
      	{
      	  return NULL;
      	}
      init_lock (&result->lock);
      result->data = malloc (pool->buffer_size);
      result->size = pool->buffer_size;
      result->len = 0;
      result->pool = pool;
    }
  else
    {
      result = pool->head;
      pool->head = pool->head->next;
      unlock (&pool->lock);
    }

  return result;
}

void
return_buffer (buffer *buffer)
{
  buffer_pool *pool = buffer->pool;
  buffer->len = 0;

  lock (&pool->lock);
  buffer->next = pool->head;
  pool->head = buffer;
  pool->num_buffers++;
  broadcast (&pool->lock);
  unlock (&pool->lock);
}

void
free_buffer (buffer *buffer)
{
  if (buffer != NULL)
    {
      if (buffer->data != NULL)
        {
          free (buffer->data);
          buffer->data = NULL;
        }
      // Invalidate next ptr
      buffer->next = NULL;
      // buffer *searched = in_pool->head;
      free (buffer);
    }
}

static inline size_t
grow (size_t size)
{
  size_t was = size;
  size_t top;
  int shift;

  size += size >> 2;
  top = size;
  for (shift = 0; top > 7; shift++)
    {
      top >>= 1;
    }
  if (top == 7)
    {
      size = (size_t) 1 << (shift + 3);
    }
  if (size < 16)
    {
      size = 16;
    }
  if (size <= was)
    {
      size = (size_t) 0 - 1;
    }
  return size;
}

void
grow_buffer (buffer *buffer)
{
  unsigned char *tmp;
  size_t bigger = grow (buffer->size);
  tmp = realloc (buffer->data, bigger);
  if (tmp != NULL)
    {
      buffer->size = bigger;
      buffer->data = tmp;
    }
}

void
init_pools (void)
{
  // input pool
  init_lock (&in_pool.lock);
  in_pool.head = NULL;
  in_pool.buffer_size = rsync ? CHUNK : IN_BUF_SIZE;
  in_pool.num_buffers = threads * 2 + 1;

  // output pool
  init_lock (&out_pool.lock);
  out_pool.head = NULL;
  out_pool.buffer_size = OUT_BUF_SIZE;
  out_pool.num_buffers = -1;

  // dictionary pool
  init_lock (&dict_pool.lock);
  dict_pool.head = NULL;
  dict_pool.buffer_size = DICTIONARY_SIZE;
  dict_pool.num_buffers = -1;
}

void
free_buffer_pool (buffer_pool *pool)
{
  if (pool != NULL)
  {
    buffer *current = pool->head;
    while (current != NULL)
      {
        buffer *next = current->next;
        free_buffer (current);
        current = next;
      }
  }
}
