#ifndef JOB_H
#define JOB_H

struct job_list
{
  struct lock lock;
  struct job *head;
  struct job *tail;
};

struct job
{
  long seq;
  struct buffer *in;
  struct buffer *out;
  struct buffer *dict;
  unsigned long check;
  struct lock check_done;
  struct job *next;
  int more;
};

void insert_job_back_of_list (struct job * job, struct job_list * list);
struct job* get_job (long seq);
ssize_t read_into_job (struct job *job);
void get_job_dict_from_last_job(struct job *job, struct job *last_job);
int init_job (struct job *job, long *seq);
void init_jobs (void);
void return_job (struct job *job);
void free_job (struct job *j);
void free_job_list (struct job_list *jobs);

#endif
