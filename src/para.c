
#DEFINE 

void init_pools() {
  // input pool
  // output pool
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
  char *data;
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
