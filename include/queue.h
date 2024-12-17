
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define MAX_QUEUE_SIZE 10

struct queue_t {
	struct pcb_t * proc[MAX_QUEUE_SIZE];
	int size;
	// each queue have only fixed slot to use the CPU and when it is
	// used up, the system must change the resource to the other process in the next queue and left the remaining
	// work for future slot eventhough it needs a completed round of ready queue.
	// An example in Linux MAX PRIO=140, prio=0..(MAX PRIO - 1)
	unsigned int slot;
};

void enqueue(struct queue_t * q, struct pcb_t * proc);

struct pcb_t * dequeue(struct queue_t * q);

int empty(struct queue_t * q);

#endif

