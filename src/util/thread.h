#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

struct lock
{
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  long value;
};

void init_lock (struct lock *lock);
int lock (struct lock *lock);
int unlock (struct lock *lock);
int wait_lock (struct lock *lock);
int broadcast (struct lock *lock);

#endif
