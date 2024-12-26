#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

// Kiểm tra xem hàng đợi có rỗng không
int empty(struct queue_t * q) {
        if (q == NULL) return 1; // Nếu hàng đợi là NULL, trả về 1 (rỗng)
	return (q->size == 0);  // Nếu kích thước hàng đợi bằng 0, trả về 1 (rỗng)
}

// Thêm một tiến trình vào hàng đợi
void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
    if (q->size < MAX_QUEUE_SIZE) { // Kiểm tra xem hàng đợi có đầy không
        q->proc[q->size++] = proc; // Thêm tiến trình vào hàng đợi và tăng kích thước hàng đợi
    }
}

// Lấy một tiến trình ra khỏi hàng đợi
struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
    if (empty(q)) return NULL; // Nếu hàng đợi rỗng thì trả về NULL

    int highest_prio_index = 0;
    for (int i = 1; i < q->size; i++) {
#ifdef MLQ_SCHED
        if (q->proc[i]->prio < q->proc[highest_prio_index]->prio) { // Nếu MLQ_SCHED được định nghĩa, so sánh dựa trên trường prio
#else
        if (q->proc[i]->priority < q->proc[highest_prio_index]->priority) { // Nếu không, so sánh dựa trên trường priority
#endif
            highest_prio_index = i; // Tìm tiến trình có độ ưu tiên cao nhất (giá trị nhỏ nhất)
        }
    }

    struct pcb_t * highest_prio_proc = q->proc[highest_prio_index]; // Lưu tiến trình có độ ưu tiên cao nhất
    for (int i = highest_prio_index; i < q->size - 1; i++) {
        q->proc[i] = q->proc[i + 1]; // Dịch chuyển các tiến trình còn lại
    }
    q->size--; // Giảm kích thước hàng đợi

    return highest_prio_proc; // Trả về tiến trình có độ ưu tiên cao nhất
}

