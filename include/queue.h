
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define MAX_QUEUE_SIZE 10

struct queue_t {
	struct pcb_t * proc[MAX_QUEUE_SIZE]; // Mảng chứa các tiến trình
	int size; // Kích thước hàng đợi

	// each queue have only fixed slot to use the CPU and when it is
	// used up, the system must change the resource to the other process in the next queue and left the remaining
	// work for future slot eventhough it needs a completed round of ready queue.
	// An example in Linux MAX PRIO=140, prio=0..(MAX PRIO - 1)
	int slot; // Slot cố định cho mỗi hàng đợi để sử dụng CPU
};

void enqueue(struct queue_t * q, struct pcb_t * proc);

struct pcb_t * dequeue(struct queue_t * q);

int empty(struct queue_t * q);

#endif

