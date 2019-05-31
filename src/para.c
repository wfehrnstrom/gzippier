// TODO: error checking on all mallocs

#DEFINE IN_BUF_SIZE = 131072
#DEFINE OUT_BUF_SIZE = 64000 //????

static struct buffer_pool in_pool;
static struct buffer_pool out_pool;
static struct job_list compress_jobs;
static struct job_list write_jobs;
static struct job_list free_jobs;
static int ifd;
static int ofd;

void init_pools() {
  // input pool
  // in_pool.lock = ;
  in_pool.head = NULL;
  in_pool.buffer_size = IN_BUF_SIZE;
  in_pool.num_buffers = threads * 2;

  // output pool
  // out_pool.lock = ;
  out_pool.head = NULL;
  out_pool.buffer_size = OUT_BUF_SIZE;
  out_pool.num_buffers = -1;
}

void init_jobs() {
  // compress jobs
  // compress_jobs.lock = ;
  compress_jobs.head = NULL;
  compress_jobs.tail = NULL;

  // write jobs
  // write_jobs.lock = ;
  write_jobs.head = NULL;
  write_jobs.tail = NULL;
}

void write_thread(void* nothing) {
  // write the header

  long seq = 0;
  struct job *job;
  while (/* theres more to do */) {
    // wait for the next job in sequence
    struct lock *write_lock = &write_jobs.lock;
    lock(&write_jobs.lock);
    while (seq != write_jobs.lock.value) {
      wait(&write_jobs.lock);
    }
    
    // get job
    job = write_jobs.head;
    write_jobs.head = job.next;
    
    // write data and return out buffer
    writen(ofd, job.out.data, job.out.size);
    // return the buffer
    return_buffer(&job.out);
    
    // wait for checksum
    wait(&job.check_done);
    // assemble the checksum
    
    // free the job
    free_job(job);
    
    seq++;
  }

  // write the trailer
}

void compress_thread(void* nothing) {
  
}

void parallel_zip(int in, int out) {
  ifd = in;
  ofd = out;
  init_pools();
  init_jobs();

  // launch write thread
  pthread_t write_thread;
  pthread_create(&write_thread, NULL, write_thread, NULL);
  
  // launch compress threads

  // start reading

  // join threads - write will be last to finish
  
  
}

size_t writen(int fd, void const *buf, size_t len) {
  char const *next = buf;
  size_t const max = SIZE_MAX >> 1;
    
  while (len != 0) {
    ssize_t result = write(fd, next, len > max ? max : len);
    next += result;
    len -= result;
  }
}

struct lock {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  long value;
};

int lock(struct lock *lock) {
  return pthread_mutex_lock(&lock->mutex);
}

int unlock(struct lock *lock) {
  return pthread_mutex_unlock(&lock->mutex);
}

int wait(struct lock *lock) {
  return pthread_cond_wait(&lock->cond, &lock->mutex);
}

int broadcast(struct lock *lock) {
  return pthread_cond_broadcast(&lock->cond)
}


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
};

struct job *get_job() {
  struct job* result;
  lock(&free_jobs.lock);

  if (free_jobs.head == NULL) {
    unlock(&free_jobs.lock);
    // allocate a new job
    result = malloc(sizeof(struct job));
    result->seq = 0;
    lock(&result->check_done);
    result->next = NULL;
  } else {
    result = free_jobs.head;
    free_jobs.head = free_jobs.head->next;
    unlock(&free_jobs.lock);
  }
  
  return result;
}

void return_job(struct job *job) {
  lock(&free_jobs.lock);
  job->next = free_jobs.head;
  free_jobs.head = job;
  unlock(&free_jobs.lock);
}

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
  struct buffer_pool *pool;
  struct buffer *next;
};

struct buffer *get_buffer(struct buffer_pool *pool) {
  struct buffer *result;
  lock(&pool->lock);

  // wait until you can create a new buffer or grab an existing one
  while (pool->head == NULL && pool->num_buffers != 0) {
    wait(&pool->lock);
  }
  
  if (pool->head == NULL) {
    // allocate a new buffer
    result = malloc(sizeof(struct buffer));
    //result lock
    result->data = malloc(pool->buffer_size);
    result->size = pool->buffer_size;
    result->pool = pool;
  } else {
    result = pool->head;
    pool->head = pool->head->next;
  }

  pool->num_buffers--;
  unlock(&pool->lock);
  return result;
}

void return_buffer(struct buffer *buffer) {
  struct buffer_pool *pool = buffer->pool;
  lock(&pool->lock);

  buffer->next = pool->head;
  pool->head = buffer;
  pool->num_buffers++;
  
  unlock(&pool->lock);
}

void write_buffer(struct buffer *buffer, char const *input, size_t len) {
  // check if the buffer is big enough
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
