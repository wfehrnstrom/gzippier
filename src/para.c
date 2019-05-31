#DEFINE IN_BUF_SIZE = 131072
#DEFINE OUT_BUF_SIZE = 64000 //????

static struct buffer_pool in_pool;
static struct buffer_pool out_pool;
static struct job_list compress_jobs;
static struct job_list write_jobs;

void init_pools() {
  // input pool
  // in_pool.lock = ;
  in_pool.head = NULL;
  in_pool.buffer_size = IN_BUF_SIZE;
  in_pool.num_buffers = 0;
  in_pool.max_buffers = threads * 2;

  // output pool
  // out_pool.lock = ;
  out_pool.head = NULL;
  out_pool.buffer_size = OUT_BUF_SIZE;
  out_pool.num_buffers = 0;
  out_pool.max_buffers = -1;
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
  
}

void compress_thread(void* nothing) {
  
}

void parallel_zip(int in, int out) {
  init_pools();
  init_jobs();

  // launch write thread
  pthread_t write_thread;
  pthread_create(&write_thread, NULL, write_thread, NULL);
  
  // launch compress threads

  // start reading

  // join threads - write will be last to finish
  
  
}

struct lock {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  long value;
};

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

struct buffer_pool {
  struct lock lock;
  struct buffer *head;
  size_t buffer_size;
  int num_buffers;
  int max_buffers;
};

struct buffer {
  struct lock lock;
  unsigned char *data;
  size_t size;
  struct buffer_pool *pool;
  struct buffer *next;
};

buffer *

// 2xthreads read buffers, threads write buffers


// get the buffers

// launch the read thread

// put compress jobs on the compress job list

// launch the compress threads

// put write jobs on the write job list

// calculate the crc

// launch the write thread

// write everything to out
