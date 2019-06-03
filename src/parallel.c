// TODO: error checking on all mallocs
// TODO: checksumming in write thread
// TODO: error checking on reads and writes
// TODO: remove lock from buffer struct
// TODO: add error check to zlib functions
// TODO: think about buffer growth algorithm
// TODO: remove dict pool

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
#define OUT_BUF_SIZE 32768 //????
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

static struct gzip_header create_header(char const *name, time_t time, int level) {
  struct gzip_header header;
  header.magic1 = 31;
  header.magic2 = 139;
  header.deflate = 8;
  header.flags1 = (name != NULL) ? 8 : 0;
  header.time = (uint32_t)time;
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

static void db(char const *str) {
  fprintf(stderr, "%s\n", str);
}

static void tb(char const *str) {
  fprintf(stderr, "%u: %s\n", (unsigned)pthread_self(), str);
}

static size_t readn(int fd, unsigned char *buf, size_t len) {
  ssize_t result;
  size_t amount = 0;

  while (len) {
    result = read(fd, buf, len);
    fprintf(stderr, "READ: %ld bytes\n", result); 
    if (result == 0) {
      break;
    }
    buf += result;
    amount += (size_t)result;
    len -= (size_t)result;
  }
  fprintf(stderr, "READ TOTAL: %lu bytes\n", (unsigned long)amount);
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
  struct lock lock; // probably unnecessary
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
  fprintf(stderr, "This pool has %d buffers and its head is %p\n", pool->num_buffers, pool->head);
  while (pool->num_buffers == 0) {
    wait(&pool->lock);
  }
  db("there was a buffer available");
  
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
  fprintf(stdout, "i'm returning a buffer with data: %p", buffer->data);
  buffer->next = pool->head;
  pool->head = buffer;
  pool->num_buffers++;

  broadcast(&pool->lock);
  unlock(&pool->lock);
}

static void grow_buffer(struct buffer *buffer) {
  buffer->data = realloc(buffer->data, buffer->size * 2);
  buffer->size = buffer->size * 2;
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

static void pj(char const *str, struct job *job) {
  fprintf(stderr, "%s, seq: %li, in: %p, out: %p, dict: %p\n\n", str, job->seq, job->in, job->out, job->dict);
}

static struct job *get_job(long seq) {
  struct job* result;
  db("waiting for job list lock");
  lock(&free_jobs.lock);
  db("got job list lock");
  if (free_jobs.head == NULL) {
    db("allocating new job");
    unlock(&free_jobs.lock);
    // allocate a new job
    result = malloc(sizeof(struct job));
    init_lock(&result->check_done);
    lock(&result->check_done);
  } else {
    db("recycling old job");
    // grab one from the list
    result = free_jobs.head;
    free_jobs.head = free_jobs.head->next;
    unlock(&free_jobs.lock);
  }
  result->next = NULL;
  result->seq = seq;
  db("getting in buffer for new job");
  result->in = get_buffer(&in_pool);
  db("got in buffer for new job");
  return result;
}

static void return_job(struct job *job) {
  lock(&free_jobs.lock);
  job->next = free_jobs.head;
  free_jobs.head = job;
  unlock(&free_jobs.lock);
}

// init functions

static void init_pools(void) {
  // input pool
  init_lock(&in_pool.lock);
  in_pool.head = NULL;
  in_pool.buffer_size = IN_BUF_SIZE;
  in_pool.num_buffers = threads * 2;

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
  tb("adsfasdf");
  fprintf(stdout, "GOT CHUNK SIZE: %u\n", stream->avail_in);
  fprintf(stderr, "ZLIB ERROR: %s, %p, %u\n", stream->msg, out, stream->avail_in);
  do {
    room = out->size - out->len;
    if (room == 0) {
      grow_buffer(out);
      room = out->size - out->len;
    }
    fprintf(stderr, "out->data: %p, out->len:%lu\n", out->data, out->len);
    stream->next_out = out->data + out->len;
    stream->avail_out = room < UINT_MAX ? (unsigned)room : UINT_MAX;
    //db("about to deflate buffer");
    fprintf(stderr, "ROOM BEFORE: %lu\n", room);
    fprintf(stderr, "ZLIB ERROR: %s, %p, %u, %p, %p\n", stream->msg, out, stream->avail_in, stream->next_in, stream->next_out);
    fprintf(stderr, "RETURN VALUE OF DEFALTE: %d\n",deflate(stream, flush));
    fprintf(stderr, "ROOM AFTER: %lu\n", room);
    fprintf(stderr, "AVAIL OUT: %u\n", stream->avail_out);
    //db("deflated buffer");
    out->len = (size_t)(stream->next_out - out->data);
  } while (stream->avail_out == 0);
  tb("a");
  fprintf(stdout, "COMPRESSED TO SIZE: %lu\n", out->len);
  fprintf(stderr, "TOTAL READ: %lu, ADLER: %lu\n", stream->total_in, stream->adler); 
  db("                                                 WORKED ONCE\n");
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
  struct gzip_header header = create_header("zero", 0, pack_level);
  fprintf(stderr, "HEADER SIZE: %lu, %lu", sizeof(header), sizeof(header)); 
  writen(ofd, (unsigned char *)&header, sizeof(header) - 2);
  writen(ofd, (unsigned char const *)"zero", strlen("zero") + 1);
  size_t ulen = 0;
  
  long seq = 0;
  struct job *job;
  do {
    // wait for the next job in sequence
    lock(&write_jobs.lock);
    while ((write_jobs.head == NULL || seq != write_jobs.head->seq) && !write_jobs.lock.value) {
      db("write thread bout to wait for a job");
      wait(&write_jobs.lock);
    }
    if (write_jobs.lock.value) {
      db("im breaking");
      break;
    }
    
    // get job
    job = write_jobs.head;
    write_jobs.head = job->next;
    if (job->next == NULL) {
      write_jobs.tail = NULL;
    }
    unlock(&write_jobs.lock);
    db("write thread got a job");

    pj("Writing job", job);
    fprintf(stderr, "out len: %lu\n", job->out->len);
    // write data and return out buffer
    writen(ofd, job->out->data, job->out->len);
    db("1");
    // return the buffer
    return_buffer(job->out);
    db("2");
    // wait for checksum
    lock(&job->check_done);
    // assemble the checksum
    fprintf(stdout, "first check: %lu, second check: %lu\n", check, job->check);
    check = crc32_comb(check, job->check, job->check_done.value);
    
    ulen += job->check_done.value;
    
    unlock(&job->check_done);
    db("3");
    // return the job
    return_job(job);
    db("write thread finished a job");
    
    seq++;
  } while (job->more);

  // write the trailer
  db("writing trailer");
  fprintf(stderr, "Total Length: %lu, CHECK: %lu\n", ulen, check);
  struct gzip_trailer trailer = create_trailer(check, ulen);
  fprintf(stderr, "TRAILER: %lu\n", sizeof(trailer));
  writen(ofd, (unsigned char *)&trailer, sizeof(trailer));
  
  db("write thread's work is done");
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
    while (compress_jobs.head == NULL && compress_jobs.lock.value == 0) {
      wait(&compress_jobs.lock);
    }
    // check if the work is over
    if (compress_jobs.lock.value != 0) {
      tb("compress thread's work is done");
      unlock(&compress_jobs.lock);
      pthread_exit(NULL);
    }
    broadcast(&compress_jobs.lock);
    
    // get a compress job
    job = compress_jobs.head;
    if (compress_jobs.head == compress_jobs.tail) {
      compress_jobs.tail = NULL;
    }
    compress_jobs.head = compress_jobs.head->next;
    unlock(&compress_jobs.lock);
    tb("compress thread got a job");

    len = job->in->len;
    if (job->dict) {
      fprintf(stdout, "MY in value is: %ld, my dict value is %ld\n", job->in->lock.value, job->dict->lock.value);
    } else {
      fprintf(stdout, "MY in value is: %ld\n", job->in->lock.value);
    }
    // reset stream
    deflateReset(&stream);
    
    // set the dictionary
    if (job->dict != NULL) {
      deflateSetDictionary(&stream, job->dict->data + (job->dict->len - DICTIONARY_SIZE), DICTIONARY_SIZE);
      
      // return the dictionary buffer if possible
      lock(&job->dict->lock);
      fprintf(stdout, "DICT SETUP: %li\n\n\n\n", job->dict->lock.value);
      if (job->dict->lock.value == 0) {
	db("return buffer");
	fprintf(stdout, "IVE RETURNED IN BUF: %p", job->dict);
	return_buffer(job->dict);
      } else {
	db("decrement");
	job->dict->lock.value--;
      }
      unlock(&job->dict->lock);
    }
    tb("compress thread set up dictionary");
    
    // get an out buffer
    job->out = get_buffer(&out_pool);
    tb("compress thread got buffer");
    
    // set up stream struct
    stream.next_in = job->in->data;
    stream.avail_in = job->in->len;

    fprintf(stderr, "IMPORT: %lu\n\n", job->in->len);
    
    pj("Compressing job", job);
    // check if this is the last (empty) block
    
    size_t left = job->in->len;
    while (left > MAXP2) {
      stream.avail_in = MAXP2;
      tb("initial deflate");
      fprintf(stderr, "lfet: %lu\n", left);
      deflate_buffer(&stream, job->out, Z_NO_FLUSH);
      left -= MAXP2;
    }
    
    stream.avail_in = (unsigned)left;

    if (job->more) {  
      // compress normally
      db("normal compress");
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

      // PLACEHOLDER COPY
      // memcpy(job->out->data, job->in->data, job->in->len);
      // job->out->len = job->in->len;
    } else {
      // finish compression
      db("compress finish");
      deflate_buffer(&stream, job->out, Z_FINISH);
    }
    tb("compress thread finished compressing");
    fprintf(stderr, "out len: %lu\n", job->out->len);
   
    // put job on the write list
    lock(&write_jobs.lock);
    struct job *prev;
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
    tb("compress thread put a job on the write list");
    // calculate check
    len = job->in->len;
    unsigned char *next = job->in->data;
    unsigned long check = crc32z(0L, Z_NULL, 0);
    while (len > MAXP2) {
      check = crc32z(check, next, len);
      len -= MAXP2;
      next += MAXP2;
    }
    fprintf(stdout, "IVE GOT THE IN BUFFER: %p\n", job->in);
    fprintf(stdout, "next: %p, len: %lu\n", next, len);
    check = crc32z(check, next, len);
    job->check_done.value = job->in->len;
    job->check = check;
    // unlock check
    fprintf(stdout, " THREAD CHECK: %lu,\n", check);
    unlock(&job->check_done);

    // return in buffer if you can
    lock(&job->in->lock);
    fprintf(stdout, "DONE COMPRESSING: %li\n\n\n\n", job->in->lock.value);
    if (job->in->lock.value == 0) {
      db("return buffer");
      return_buffer(job->in);
    } else {
      db("decrement");
      job->in->lock.value--;
    }
    unlock(&job->in->lock);
    tb("compress thread finished job");
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
  struct buffer *dict = NULL;
  size_t last_read;

  // initial job
  struct job *last_job = get_job(seq);
  seq++;
  last_job->in->lock.value = 1;
  last_read = readn(ifd, last_job->in->data, IN_BUF_SIZE);
  if (last_read == 0) {
    db("YO\n\n\n\n\n\n");
  }
  last_job->in->len = last_read;
  last_job->dict = dict;
  if (last_job->in->len >= DICTIONARY_SIZE) {
    dict = last_job->in;
  }
  fprintf(stdout, "FIRST IN VALUE: %ld\n", last_job->in->lock.value); 
  // file wasn't empty
  while(last_read != 0) {
    // get a job
    job = get_job(seq);
    job->in->lock.value = 1;
    
    // read into it
    last_read = readn(ifd, job->in->data, IN_BUF_SIZE);
    if (last_read == 0) {
      db("YO\n\n\n\n\n\n");
    }
    job->in->len = last_read;
    last_job->more = last_read;
    fprintf(stderr, "read thread just read %d bytes\n", (int)job->in->len);
    
    // set the dict and prepare the dict for the next one
    job->dict = dict;
    if (job->in->len >= DICTIONARY_SIZE) {
      dict = job->in;
    } else {
      dict = NULL;
    }
    
    // put the previous job on the back of the compress list
    pj("Adding job", last_job);
    if (last_job->more) {
      
      //last_job->in->lock.value--;
    }
    if (last_job != NULL) {
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
      db("read thread put a job on the compress list");
      
      // launch a compress thread if possible
      if (threads_compressing < threads) {
	pthread_create(compression_threads_t + threads_compressing, NULL, compress_thread, NULL);
	threads_compressing++;
      }
    }

    last_job = job;
    seq++;
  }

  db("AT end of read loop");
  // call the threads home
  pthread_join(write_thread_t, NULL);
  db("write thread joined the read thread");
  lock(&compress_jobs.lock);
  db("read thread got the lock for the compress list");
  compress_jobs.lock.value = 1;
  broadcast(&compress_jobs.lock);
  unlock(&compress_jobs.lock);
  db("read thread released the lock for the compress list");
  for (int i = 0; i < threads_compressing; i++) {
    pthread_join(compression_threads_t[i], NULL);
    db("a compress thread just joined the read thread");
  }
}

// 2xthreads read buffers, threads write buffers


// get the buffers

// launch the read thread

// put compress jobs on the compress job list

// launch the compress threads

// put write jobs on the write job list

// calculate the crc

// launch the write thread

// write everything to out
