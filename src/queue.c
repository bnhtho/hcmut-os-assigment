#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	if (q == NULL)
		return 1;
	return (q->size == 0) ? 1 : 0;
}

void enqueue(struct queue_t *q, struct pcb_t *proc) {
    /* TODO: put a new process to queue [q] */
	if (q->size >= MAX_QUEUE_SIZE)
		return; // Hàng đợi đầy
	int i = q->size - 1;
	#ifdef MLQ_SCHED
	while (i >= 0 && q->proc[i]->prio > proc->prio)
	#else
	while (i >= 0 && q->proc[i]->priority > proc->priority)
	#endif
	{
		q->proc[i + 1] = q->proc[i]; // Dịch process có độ ưu tiên thấp hơn
		i--;
	}
	q->proc[i + 1] = proc; // Thêm process vào đúng vị trí
	q->size++;
}

struct pcb_t *dequeue(struct queue_t *q) {
    /* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	if (q->size == 0)
		return NULL;				 // Hàng đợi rỗng
	struct pcb_t *proc = q->proc[0]; // Lấy process đầu tiên (ưu tiên cao nhất)
	for (int i = 0; i < q->size - 1; i++)
	{
		q->proc[i] = q->proc[i + 1]; // Dịch các process còn lại lên
	}
	q->size--;
	return proc;
}

