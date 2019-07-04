#include <pthread.h>
#include "thread.h"

void
init_lock (struct lock *lock)
{
  pthread_mutex_init (&lock->mutex, NULL);
  pthread_cond_init (&lock->cond, NULL);
  lock->value = 0;
}

int
lock (struct lock *lock)
{
  return pthread_mutex_lock (&lock->mutex);
}

int
unlock (struct lock *lock)
{
  return pthread_mutex_unlock (&lock->mutex);
}

int
wait_lock (struct lock *lock)
{
  return pthread_cond_wait (&lock->cond, &lock->mutex);
}

int
broadcast (struct lock *lock)
{
  return pthread_cond_broadcast (&lock->cond);
}
