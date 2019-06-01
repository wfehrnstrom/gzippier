// TODO: error checking on all mallocs
// TODO: checksumming in write thread
// TODO: error checking on reads and writes
// TODO: remove lock from buffer struct

#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <config.h>
#include <unistd.h>
#include <stdlib.h>
#include "gzip.h"
#include <stdio.h>

#define IN_BUF_SIZE 131072
#define OUT_BUF_SIZE 131072 //????

static struct buffer_pool in_pool;
static struct buffer_pool out_pool;
static struct job_list compress_jobs;
static struct job_list write_jobs;
static struct job_list free_jobs;


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
    if (result == 0) {
      break;
    }
    buf += result;
    amount += (size_t)result;
    len -= (size_t)result;
  }
  return amount;
}

static void writen(int fd, unsigned char *buf, size_t len) {
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
  while (pool->head != NULL && pool->num_buffers != 0) {
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
    lock(&result->check_done);
  } else {
    // grab one from the list
    result = free_jobs.head;
    free_jobs.head = free_jobs.head->next;
    unlock(&free_jobs.lock);
  }
  result->next = NULL;
  result->seq = seq;
  result->in = get_buffer(&in_pool);
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

static noreturn void *write_thread(void* nothing) {
  // write the header

  long seq = 0;
  struct job *job;
  do {
    // wait for the next job in sequence
    lock(&write_jobs.lock);
    while (write_jobs.head == NULL || seq != write_jobs.head->seq) {
      db("write thread bout to wait for a job");
      wait(&write_jobs.lock);
    }
    
    // get job
    job = write_jobs.head;
    write_jobs.head = job->next;
    if (job->next == NULL) {
      write_jobs.tail = NULL;
    }
    unlock(&write_jobs.lock);
    db("write thread got a job");
    
    // write data and return out buffer
    writen(ofd, job->out->data, job->out->len);
    db("1");
    // return the buffer
    return_buffer(job->out);
    db("2");
    // wait for checksum
    lock(&job->check_done);
    // assemble the checksum
    db("3");
    // return the job
    return_job(job);
    db("write thread finished a job");
    
    seq++;
  } while (job->more);

  // write the trailer
  db("write thread's work is done");
  pthread_exit(NULL);
}

static noreturn void *compress_thread(void* nothing) {
  struct job *job;

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

    // get a compress job
    job = compress_jobs.head;
    if (compress_jobs.head == compress_jobs.tail) {
      compress_jobs.tail = NULL;
    }
    compress_jobs.head = compress_jobs.head->next;
    unlock(&compress_jobs.lock);
    tb("compress thread got a job");
    
    // get an out buffer
    job->out = get_buffer(&out_pool);
    
    // check if this is the last (empty) block
    if (job->more) {  
      // compress

      // PLACEHOLDER COPY
      memcpy(job->out->data, job->in->data, job->in->len);
      job->out->len = job->in->len;
      
      
    }
    
    // return in buffer
    return_buffer(job->in);
    tb("compress thread finished job");
    
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
    fprintf(stderr, "seq: %ld\n", job->seq);
    // calculate check
    
    // unlock check
    unlock(&job->check_done);
  }
}

void parallel_zip(int pack_level) {
  pack_level = pack_level; // dummy
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
  while (true) {
    // get a job
    struct job *job = get_job(seq);

    // read into it
    job->in->len = readn(ifd, job->in->data, IN_BUF_SIZE);
    job->more = job->in->len;
    
    // put it on the back of the compress list
    lock(&compress_jobs.lock);
    if (compress_jobs.head == NULL) {
      compress_jobs.head = job;
      compress_jobs.tail = job;
    } else {
      compress_jobs.tail->next = job;
      compress_jobs.tail = job;
    }
    unlock(&compress_jobs.lock);
    db("read thread put a job on the compress list");

    // launch a compress thread if possible
    if (threads_compressing < threads) {
      pthread_create(compression_threads_t + threads_compressing, NULL, compress_thread, NULL);
      threads_compressing++;
    }

    fprintf(stderr, "read thread just read %d bytes\n", (int)job->in->len);
    if (job->in->len == 0) {
      db("read thread's work is done");
      break;
    }

    seq++;
  }

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
