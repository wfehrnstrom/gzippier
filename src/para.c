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

void write_thread(void* nothing) {
  
}

void compress_thread(void* nothing) {

}

void parallel_zip(int in, int out) {
  init_pools();

  // launch compress threads

  // launch write thread

  // start reading

  // join threads - write will be last to finish
  
  
}

struct job_list {
  // lock
  struct job *head;
  struct job *tail;
}

struct job {
  long seq;
  struct buffer *in;
  struct buffer *out;
  unsigned long check;
  // lock check_done;
  struct job *next;
}

struct buffer_pool {
  // lock
  struct buffer *head;
  size_t buffer_size;
  int num_buffers;
  int max_buffers;
};

struct buffer {
  // lock
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
