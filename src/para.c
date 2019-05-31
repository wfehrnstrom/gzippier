// TODO: error checking on all mallocs
// TODO: checksumming in write thread
// TODO: error checking on reads and writes

#DEFINE IN_BUF_SIZE = 131072
#DEFINE OUT_BUF_SIZE = 64000 //????

static struct buffer_pool in_pool;
static struct buffer_pool out_pool;
static struct job_list compress_jobs;
static struct job_list write_jobs;
static struct job_list free_jobs;
static struct lock done;
static int ifd;
static int ofd;

void init_pools() {
  // input pool
  lock_init(&in_pool.lock);
  in_pool.head = NULL;
  in_pool.buffer_size = IN_BUF_SIZE;
  in_pool.num_buffers = threads * 2;

  // output pool
  lock_init(&out_pool.lock);
  out_pool.head = NULL;
  out_pool.buffer_size = OUT_BUF_SIZE;
  out_pool.num_buffers = -1;
}

void init_jobs() {
  // compress jobs
  lock_init(&compress_jobs.lock);
  compress_jobs.head = NULL;
  compress_jobs.tail = NULL;

  // write jobs
  lock_init(&write_jobs.lock);
  write_jobs.head = NULL;
  write_jobs.tail = NULL;
}

void write_thread(void* nothing) {
  // write the header

  long seq = 0;
  struct job *job;
  do {
    // wait for the next job in sequence
    struct lock *write_lock = &write_jobs.lock;
    lock(&write_jobs.lock);
    while (write_jobs.head == NULL || seq != write_jobs.head->seq) {
      wait(&write_jobs.lock);
    }
    
    // get job
    job = write_jobs.head;
    write_jobs.head = job->next;
    if (job->next == NULL) {
      write_jobs.tail = NULL;
    }
    unlock(&write_jobs.lock);
    
    // write data and return out buffer
    writen(ofd, job->out->data, job->out->size);
    // return the buffer
    return_buffer(job->out);
    
    // wait for checksum
    wait(&job->check_done);
    // assemble the checksum
    
    // free the job
    free_job(job);
    
    seq++;
  } while (job->more)

  // write the trailer
}

void compress_thread(void* nothing) {
  struct job *job;
  
  // get a job from the compress list
  lock(&compress_jobs.lock);
  while (compress_jobs.head == NULL) {
    wait(&compress_jobs.lock);
  }
  job = compress_jobs.head;
  if (compress_jobs.head == compress_jobs.tail) {
    compress_jobs.tail = NULL;
  }
  compress_jobs.head = compress_jobs.head.next;
  unlock(&compress_jobs.lock);

  // check if this is the last (empty) block
  job->more = job->in->size;
  if (job->more) {
  
    // compress
    job->out = get_pool(&out_pool);

  }
  
  // return in buffer
  return_buffer(job->in);
  
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

  // calculate check
  // unlock check
  unlock(&job->check_done);
}

void parallel_zip(int in, int out) {
  ifd = in;
  ofd = out;
  init_pools();
  init_jobs();
  // init lock to tell the write thread the last job
  init_lock(&done);
  lock(&done);

  // launch write thread
  pthread_t write_thread;
  pthread_create(&write_thread, NULL, write_thread, NULL);
  
  // launch compress threads

  // start reading
  size_t read;
  long seq = 0;
  while (true) {
    // get a job
    struct job *job = get_job(seq);

    // read into it
    job->in.size = readn(ifd, job->in->data, IN_BUF_SIZE);
    if (job->in.size == 0) {
      return_buffer(job->in);
      return_job(job);
      break;
    }

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
    
    if (job->in.size == 0) {
      break;
    }
  }

  // join threads - write will be last to finish
  
  
}

size_t readn(int fd, unsigned char *buf, size_t len) {
  ssize_t result;
  size_t amount;

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

void writen(int fd, unsigned char *buf, size_t len) {
  size_t const max = SIZE_MAX >> 1;
  ssize_t result;
  
  while (len != 0) {
    result = write(fd, buf, len > max ? max : len);
    buf += result;
    len -= (size_t)result;
  }
}

struct lock {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  long value;
};

void init_lock(struct lock *lock) {
  pthread_mutex_init(&lock->mutex, NULL);
  pthread_cond_init(&lock->cond, NULL);
}

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
  int more;
};

struct job *get_job(long seq) {
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
  result->next = NULL;
  result->seq = seq;
  lock(&result->check_done);
  job->in = get_buffer(&in_pool);
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
    init_lock(&result->lock);
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

  broadcast(&pool->lock);
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
