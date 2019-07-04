#ifndef BUFFER_H
#define BUFFER_H

#ifndef IN_BUF_SIZE
#define IN_BUF_SIZE 131072
#endif

#ifndef OUT_BUF_SIZE
#define OUT_BUF_SIZE 32768
#endif

#ifndef DICTIONARY_SIZE
#define DICTIONARY_SIZE 32768
#endif

typedef struct buffer_pool
{
  struct lock lock;
  struct buffer *head;
  size_t buffer_size;
  int num_buffers;
} buffer_pool;

typedef struct buffer
{
  struct lock lock;
  unsigned char *data;
  size_t size;
  size_t len;
  struct buffer_pool *pool;
  struct buffer *next;
} buffer;

buffer* get_buffer (struct buffer_pool *pool);
void return_buffer (buffer *buffer);
void free_buffer (buffer *buffer);
void grow_buffer (buffer *buffer);
void init_pools (void);
void free_buffer_pool (buffer_pool *pool);

#endif
