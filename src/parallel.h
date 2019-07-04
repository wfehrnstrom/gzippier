#ifndef PARALLEL_H
#define PARALLEL_H

#include "util/buffer.h"

extern struct job_list compress_jobs;
extern struct job_list write_jobs;
extern struct job_list free_jobs;

extern buffer_pool in_pool;
extern buffer_pool out_pool;
extern buffer_pool dict_pool;

#endif
