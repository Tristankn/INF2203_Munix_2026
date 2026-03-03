#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

extern struct list_head ready_queue;

void sched_add(struct thread *t);
void sched_remove(struct thread *t);
void schedule(void);

#endif /* SCHEDULER_H */
