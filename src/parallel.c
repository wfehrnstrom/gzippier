// TODO: error checking on all mallocs
// TODO: error checking on reads and writes
// TODO: add error check to zlib functions
// TODO: fix race condition

#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <config.h>
#include <unistd.h>
#include <stdlib.h>
#include "gzip.h"
#include <stdio.h>
#include "zlib.h"
#include <time.h>

#define IN_BUF_SIZE 131072
#define OUT_BUF_SIZE 32768
#define DICTIONARY_SIZE 32768
#define MAXP2 (UINT_MAX - (UINT_MAX >> 1))

static struct buffer_pool in_pool;
static struct buffer_pool out_pool;
static struct buffer_pool dict_pool;
static struct job_list compress_jobs;
static struct job_list write_jobs;
static struct job_list free_jobs;
static int pack_level;

struct gzip_header {
  uint8_t magic1;
  uint8_t magic2;
  uint8_t deflate;
  uint8_t flags1;
  uint32_t time;
  uint8_t flags2;
  uint8_t os;
};

static struct gzip_header create_header(char const *name, int level) {
  struct gzip_header header;
  header.magic1 = 31;
  header.magic2 = 139;
  header.deflate = 8;
  header.flags1 = (name != NULL) ? 8 : 0;
  header.time = (uint32_t)time_stamp.tv_sec;
  header.flags2 = (level >= 9 ? 2 : level == 1 ? 4: 0);
  header.os = 3;

  return header;
}

struct gzip_trailer {
  uint32_t check;
  uint32_t uncompressed_len;
};

static struct gzip_trailer create_trailer(unsigned long check, size_t ulen) {
  struct gzip_trailer trailer;
  trailer.check = check;
  trailer.uncompressed_len = ulen;
  return trailer;
}

static size_t readn(int fd, unsigned char *buf, size_t len) {
  ssize_t result;
  size_t amount = 0;

  while (len) {
    result = read(fd, buf, len);
    if (result == 0) {
      break;
    }
    buf += result;
    amount += (size_t)result;
    len -= (size_t)result;
  }
  return amount;
}

static void writen(int fd, unsigned char const *buf, size_t len) {
  size_t const max = SIZE_MAX >> 1;
  ssize_t result;
  
  while (len != 0) {
    result = write(fd, buf, len > max ? max : len);
    buf += result;
    len -= (size_t)result;
  }
}

// Locks and helpers

struct lock {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  long value;
};

static void init_lock(struct lock *lock) {
  pthread_mutex_init(&lock->mutex, NULL);
  pthread_cond_init(&lock->cond, NULL);
  lock->value = 0;
}

static int lock(struct lock *lock) {
  return pthread_mutex_lock(&lock->mutex);
}

static int unlock(struct lock *lock) {
  return pthread_mutex_unlock(&lock->mutex);
}

static int wait(struct lock *lock) {
  return pthread_cond_wait(&lock->cond, &lock->mutex);
}

static int broadcast(struct lock *lock) {
  return pthread_cond_broadcast(&lock->cond);
}

// Buffer pools and helpers

struct buffer_pool {
  struct lock lock;
  struct buffer *head;
  size_t buffer_size;
  int num_buffers;
};

struct buffer {
  struct lock lock;
  unsigned char *data;
  size_t size;
  size_t len;
  struct buffer_pool *pool;
  struct buffer *next;
};

static struct buffer *get_buffer(struct buffer_pool *pool) {
  struct buffer *result;
  lock(&pool->lock);
  // wait until you can create a new buffer or grab an existing one
  while (pool->num_buffers == 0) {
    wait(&pool->lock);
  }
  
  if (pool->head == NULL) {
    // allocate a new buffer
    result = malloc(sizeof(struct buffer));
    init_lock(&result->lock);
    result->data = malloc(pool->buffer_size);
    result->size = pool->buffer_size;
    result->len = 0;
    result->pool = pool;
  } else {
    result = pool->head;
    pool->head = pool->head->next;
  }

  pool->num_buffers--;
  unlock(&pool->lock);
  return result;
}

static void return_buffer(struct buffer *buffer) {
  struct buffer_pool *pool = buffer->pool;
  buffer->len = 0;

  lock(&pool->lock);
  buffer->next = pool->head;
  pool->head = buffer;
  pool->num_buffers++;
  broadcast(&pool->lock);
  unlock(&pool->lock);
}

static inline size_t grow(size_t size) {
  size_t was = size;
  size_t top;
  int shift;

  size += size >> 2;
  top = size;
  for (shift = 0; top > 7; shift++) {
    top >>= 1;
  }
  if (top == 7) {
    size = (size_t)1 << (shift + 3);
  }
  if (size < 16) {
    size = 16;
  }
  if (size <= was) {
    size = (size_t)0 - 1;
  }
  return size;
}

static void grow_buffer(struct buffer *buffer) {
  unsigned char *tmp;
  size_t bigger = grow(buffer->size);
  tmp = realloc(buffer->data, buffer->size);
  if (tmp != NULL) {
    buffer->size = bigger;
    buffer->data = tmp;
  }
}

// Jobs and helpers

struct job_list {
  struct lock lock;
  struct job *head;
  struct job *tail;
};

struct job {
  long seq;
  struct buffer *in;
  struct buffer *out;
  struct buffer *dict;
  unsigned long check;
  struct lock check_done;
  struct job *next;
  int more;
};

static struct job *get_job(long seq) {
  struct job* result;
  lock(&free_jobs.lock);
  if (free_jobs.head == NULL) {
    unlock(&free_jobs.lock);
    // allocate a new job
    result = malloc(sizeof(struct job));
    init_lock(&result->check_done);
  } else {
    // grab one from the list
    result = free_jobs.head;
    free_jobs.head = free_jobs.head->next;
    unlock(&free_jobs.lock);
  }
  lock(&result->check_done);
  result->next = NULL;
  result->seq = seq;
  result->in = get_buffer(&in_pool);
  return result;
}

static void return_job(struct job *job) {
  lock(&free_jobs.lock);
  job->next = free_jobs.head;
  free_jobs.head = job;
  broadcast(&free_jobs.lock);
  unlock(&free_jobs.lock);
}

// init functions

static void init_pools(void) {
  // input pool
  init_lock(&in_pool.lock);
  in_pool.head = NULL;
  in_pool.buffer_size = IN_BUF_SIZE;
  in_pool.num_buffers = threads * 2 + 1;

  // output pool
  init_lock(&out_pool.lock);
  out_pool.head = NULL;
  out_pool.buffer_size = OUT_BUF_SIZE;
  out_pool.num_buffers = -1;

  // dictionary pool
  init_lock(&dict_pool.lock);
  dict_pool.head = NULL;
  dict_pool.buffer_size = DICTIONARY_SIZE;
  dict_pool.num_buffers = -1;
}

static void init_jobs(void) {
  // compress jobs
  init_lock(&compress_jobs.lock);
  compress_jobs.head = NULL;
  compress_jobs.tail = NULL;

  // write jobs
  init_lock(&write_jobs.lock);
  write_jobs.head = NULL;
  write_jobs.tail = NULL;

  // free jobs
  init_lock(&free_jobs.lock);
  free_jobs.head = NULL;
  free_jobs.tail = NULL;
}

// compress function
static void deflate_buffer(z_stream *stream, struct buffer *out, int flush) {
  size_t room;

  do {
    room = out->size - out->len;
    if (room == 0) {
      grow_buffer(out);
      room = out->size - out->len;
    }
    stream->next_out = out->data + out->len;
    stream->avail_out = room < UINT_MAX ? (unsigned)room : UINT_MAX;
    deflate(stream, flush);
    out->len = (size_t)(stream->next_out - out->data);
  } while (stream->avail_out == 0);
}

// crc math functions (thanks adler)

__attribute__((pure)) static unsigned long gf2_matrix_times(unsigned long *mat, unsigned long vec) {
    unsigned long sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

static void gf2_matrix_square(unsigned long *square, unsigned long *mat) {
    int n;

    for (n = 0; n < 32; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

__attribute__((const)) static unsigned long crc32_comb(unsigned long crc1, unsigned long crc2,
				size_t len2) {
    int n;
    unsigned long row;
    unsigned long even[32];     // even-power-of-two zeros operator
    unsigned long odd[32];      // odd-power-of-two zeros operator

    // degenerate case
    if (len2 == 0)
        return crc1;

    // put operator for one zero bit in odd
    odd[0] = 0xedb88320UL;          // CRC-32 polynomial
    row = 1;
    for (n = 1; n < 32; n++) {
        odd[n] = row;
        row <<= 1;
    }

    // put operator for two zero bits in even
    gf2_matrix_square(even, odd);

    // put operator for four zero bits in odd
    gf2_matrix_square(odd, even);

    // apply len2 zeros to crc1 (first square will put the operator for one
    // zero byte, eight zero bits, in even)
    do {
        // apply zeros operator for this bit of len2
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        // if no more bits set, then done
        if (len2 == 0)
            break;

        // another iteration of the loop with odd and even swapped
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        // if no more bits set, then done
    } while (len2 != 0);

    // return combined crc
    crc1 ^= crc2;
    return crc1;
}

static unsigned long crc32z(unsigned long crc,
                           unsigned char const *buf, size_t len) {
    while (len > UINT_MAX && buf != NULL) {
        crc = crc32(crc, buf, UINT_MAX);
        buf += UINT_MAX;
        len -= UINT_MAX;
    }
    return crc32(crc, buf, (unsigned)len);
}

// thread functions

static noreturn void *write_thread(void* nothing) {
  unsigned long check = crc32z(0L, Z_NULL, 0);

  // write the header
  struct gzip_header header;
  if (ifd == STDIN_FILENO) {
    header = create_header(NULL, pack_level);
    writen(ofd, (unsigned char *)&header, sizeof(header) - 2);
  } else {
    header = create_header(ifname, pack_level);
    writen(ofd, (unsigned char *)&header, sizeof(header) - 2);
    writen(ofd, (unsigned char *)ifname, strlen(ifname) + 1);
  }

  
  size_t ulen = 0;
  
  long seq = 0;
  struct job *job;
  do {
    // wait for the next job in sequence
    lock(&write_jobs.lock);
    while ((write_jobs.head == NULL || seq != write_jobs.head->seq) && !write_jobs.lock.value) {
      wait(&write_jobs.lock);
    }
    if (write_jobs.lock.value) {
      break;
    }
    
    // get job
    job = write_jobs.head;
    write_jobs.head = job->next;
    if (job->next == NULL) {
      write_jobs.tail = NULL;
    }
    unlock(&write_jobs.lock);

    // write data and return out buffer
    writen(ofd, job->out->data, job->out->len);
    // return the buffer
    return_buffer(job->out);
    // wait for checksum
    lock(&job->check_done);
    // assemble the checksum
    check = crc32_comb(check, job->check, job->check_done.value);
    
    ulen += job->check_done.value;
    
    unlock(&job->check_done);
    // return the job
    return_job(job);
    
    seq++;
  } while (job->more);

  // write the trailer
  struct gzip_trailer trailer = create_trailer(check, ulen);
  writen(ofd, (unsigned char *)&trailer, sizeof(trailer));
  
  pthread_exit(NULL);
}

static noreturn void *compress_thread(void* nothing) {
  struct job *job;
  size_t len;

  // init the deflate stream for this thread
  z_stream stream;
  stream.zfree = Z_NULL;
  stream.zalloc = Z_NULL;
  stream.opaque = Z_NULL;
  deflateInit2(&stream, pack_level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
  for (;;) {
    // get a job from the compress list
    lock(&compress_jobs.lock);
    while (compress_jobs.head == NULL) {
      if (compress_jobs.lock.value != 0) {
	unlock(&compress_jobs.lock);
	pthread_exit(NULL);
      }
      wait(&compress_jobs.lock);
    }
    // get a compress job
    job = compress_jobs.head;
    if (compress_jobs.head == compress_jobs.tail) {
      compress_jobs.tail = NULL;
    }
    compress_jobs.head = compress_jobs.head->next;
    unlock(&compress_jobs.lock);
    len = job->in->len;
    // reset stream
    deflateReset(&stream);
    // set the dictionary
    if (job->dict != NULL) {
      deflateSetDictionary(&stream, job->dict->data, DICTIONARY_SIZE);
      // return the dictionary buffer
      return_buffer(job->dict);
    }
    // get an out buffer
    job->out = get_buffer(&out_pool);
    // set up stream struct
    stream.next_in = job->in->data;
    stream.next_out = job->out->data;
    // check if this is the last (empty) block
    
    size_t left = job->in->len;
    while (left > MAXP2) {
      stream.avail_in = MAXP2;
      deflate_buffer(&stream, job->out, Z_NO_FLUSH);
      left -= MAXP2;
    }
    stream.avail_in = (unsigned)left;

    if (job->more) {  
      // compress normally
      deflate_buffer(&stream, job->out, Z_BLOCK); 
      int bits;
      deflatePending(&stream, Z_NULL, &bits);
      if (bits & 1) {
	deflate_buffer(&stream, job->out, Z_SYNC_FLUSH);
      } else if (bits & 7) {
	do {
	  bits = deflatePrime(&stream, 10, 2);
	  deflatePending(&stream, Z_NULL, &bits);
	} while (bits & 7);
	deflate_buffer(&stream, job->out, Z_BLOCK);
      }
    } else {
      // finish compression
      deflate_buffer(&stream, job->out, Z_FINISH);
    }
    // put job on the write list
    lock(&write_jobs.lock);
    struct job *prev = NULL;
    struct job *cur = write_jobs.head;
    while (cur != NULL && cur->seq < job->seq) {
      prev = cur;
      cur = cur->next;
    }
    if (write_jobs.head == NULL) {
      job->next = NULL;
      write_jobs.head = job;
    } else if (cur == write_jobs.head) {
      job->next = write_jobs.head;
      write_jobs.head = job;
    } else {
      job->next = cur;
      prev->next = job;
    }
    broadcast(&write_jobs.lock);
    unlock(&write_jobs.lock);
    // calculate check
    len = job->in->len;
    unsigned char *next = job->in->data;
    unsigned long check = crc32z(0L, Z_NULL, 0);
    while (len > MAXP2) {
      check = crc32z(check, next, len);
      len -= MAXP2;
      next += MAXP2;
    }
    check = crc32z(check, next, len);
    job->check_done.value = job->in->len;
    job->check = check;
    // unlock check
    unlock(&job->check_done);
    // return in buffer
    return_buffer(job->in);
  }

  deflateEnd(&stream);
}

void parallel_zip(int pack_lev) {
  pack_level = pack_lev;
  init_pools();
  init_jobs();

  // init compression threads array
  pthread_t *compression_threads_t = malloc(sizeof(pthread_t) * threads);

  // launch write thread
  pthread_t write_thread_t;
  pthread_create(&write_thread_t, NULL, write_thread, NULL);
  
  // start reading
  int threads_compressing = 0;
  long seq = 0;
  struct job *job;
  size_t last_read;

  // initial job
  struct job *last_job = get_job(seq);
  seq++;
  //last_job->in->lock.value = 1;
  last_read = readn(ifd, last_job->in->data, IN_BUF_SIZE);
  last_job->in->len = last_read;
  last_job->dict = NULL;

  // file wasn't empty
  while(last_read != 0) {
    // get a job
    job = get_job(seq);
    
    // read into it
    last_read = readn(ifd, job->in->data, IN_BUF_SIZE);
    job->in->len = last_read;
    last_job->more = last_read;

    // set the dict and prepare the dict for the next one
    if (last_job->in->len >= DICTIONARY_SIZE) {
      job->dict = get_buffer(&dict_pool);
      memcpy(job->dict->data, last_job->in->data + last_job->in->len - DICTIONARY_SIZE, DICTIONARY_SIZE);
      job->dict->len = DICTIONARY_SIZE;
    } else {
      job->dict = NULL;
    }
    
    // put the previous job on the back of the compress list
    lock(&compress_jobs.lock);
    if (compress_jobs.head == NULL) {
      compress_jobs.head = last_job;
      compress_jobs.tail = last_job;
    } else {
      compress_jobs.tail->next = last_job;
      compress_jobs.tail = last_job;
    }
    broadcast(&compress_jobs.lock);
    unlock(&compress_jobs.lock);
    
    // launch a compress thread if possible
    if (threads_compressing < threads) {
      pthread_create(compression_threads_t + threads_compressing, NULL, compress_thread, NULL);
      threads_compressing++;
    }
    
    last_job = job;
    seq++;
  }
  
  // call the threads home
  pthread_join(write_thread_t, NULL);
  lock(&compress_jobs.lock);
  compress_jobs.lock.value = 1;
  broadcast(&compress_jobs.lock);
  unlock(&compress_jobs.lock);
  for (int i = 0; i < threads_compressing; i++) {
    pthread_join(compression_threads_t[i], NULL);
  }
  return_buffer(last_job->in);
  return_job(last_job);
  if (last_job->dict != NULL) {
    return_buffer(last_job->dict);
  }
}
