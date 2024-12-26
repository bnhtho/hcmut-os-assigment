#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
	int i ;

	for (i = 0; i < MAX_PRIO; i ++)
		{
			mlq_ready_queue[i].size = 0;
			mlq_ready_queue[i].slot = MAX_PRIO - i;
		}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

/* Function to print the contents of the MLQ ready queue */
void print_mlq_ready_queue(void)
{
#ifdef MLQ_SCHED
	pthread_mutex_lock(&queue_lock);

	printf("|=====================================|\n");
	printf("Multilevel Queue Ready Queue:\n");
	for (int prio = 0; prio < MAX_PRIO; prio++)
	{
		if (mlq_ready_queue[prio].size > 0)
		{
			printf("Queue Priority %d (Slot : %d ,size : %d): ", prio, mlq_ready_queue[prio].slot, mlq_ready_queue[prio].size);
			for (int i = 0; i < mlq_ready_queue[prio].size; i++)
			{
				struct pcb_t *proc = mlq_ready_queue[prio].proc[i];
				printf("[process (PID): %u, Priority: %u, Prio: %u] ", proc->pid, proc->priority, proc->prio);
			}
			printf("\n");
		}
	}
	printf("|=====================================|\n");

	pthread_mutex_unlock(&queue_lock);
#endif
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 */
	pthread_mutex_lock(&queue_lock); // Khóa hàng đợi để bảo vệ truy cập đồng thời
	for (int prio = 0; prio < MAX_PRIO; prio++) {
		if (mlq_ready_queue[prio].slot > 0 && !empty(&mlq_ready_queue[prio])) {
			// Nếu slot còn và hàng đợi không rỗng, lấy tiến trình ra
			proc = dequeue(&mlq_ready_queue[prio]);
			mlq_ready_queue[prio].slot--; // Giảm slot
			break;
		}
	}
	
	if (proc == NULL) {
		// Nếu không tìm thấy tiến trình nào, reset lại slot cho tất cả các hàng đợi
		for (int prio = 0; prio < MAX_PRIO; prio++) {
			mlq_ready_queue[prio].slot = MAX_PRIO - prio;
		}
		// Thử lấy tiến trình lại sau khi reset slot
		for (int prio = 0; prio < MAX_PRIO; prio++) {
			if (mlq_ready_queue[prio].slot > 0 && !empty(&mlq_ready_queue[prio])) {
				proc = dequeue(&mlq_ready_queue[prio]);
				mlq_ready_queue[prio].slot--; // Giảm slot
				break;
			}
		}
	}
	pthread_mutex_unlock(&queue_lock); // Mở khóa hàng đợi
	return proc; // Trả về tiến trình lấy được
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);
	// Lấy tiến trình từ ready_queue nếu không rỗng
	if (ready_queue.size > 0)
	{
		proc = dequeue(&ready_queue);
	}
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


