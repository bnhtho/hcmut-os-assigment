#include "timer.h"
#include <stdio.h>
#include <stdlib.h>

// Khai báo một biến toàn cục _timer kiểu pthread_t để quản lý luồng (thread) của bộ đếm thời gian.
static pthread_t _timer;

// Định nghĩa cấu trúc timer_id_container_t để chứa thông tin về các thiết bị và liên kết chúng thành danh sách liên kết.
struct timer_id_container_t {
	struct timer_id_t id;
	struct timer_id_container_t * next;
};

// Khai báo một con trỏ dev_list kiểu struct timer_id_container_t để quản lý danh sách các thiết bị.
static struct timer_id_container_t * dev_list = NULL;

// Khai báo biến toàn cục _time kiểu uint64_t để lưu trữ thời gian hiện tại.
static uint64_t _time;

// Khai báo các biến toàn cục để kiểm soát trạng thái của bộ đếm thời gian.
static int timer_started = 0;
static int timer_stop = 0;

// Định nghĩa hàm timer_routine để thực hiện công việc của bộ đếm thời gian trong một luồng riêng biệt.
static void * timer_routine(void * args) {
	while (!timer_stop) {
		printf("Time slot %3lu\n", current_time());
		int fsh = 0;
		int event = 0;
		/* Wait for all devices have done the job in current
		 * time slot */
		 /* Chờ tất cả các thiết bị hoàn thành công việc trong khoảng thời gian hiện tại */
		struct timer_id_container_t * temp;
		for (temp = dev_list; temp != NULL; temp = temp->next) {
			pthread_mutex_lock(&temp->id.event_lock);
			while (!temp->id.done && !temp->id.fsh) {
				pthread_cond_wait(
					&temp->id.event_cond,
					&temp->id.event_lock
				);
			}
			if (temp->id.fsh) {
				fsh++;
			}
			event++;
			pthread_mutex_unlock(&temp->id.event_lock);
		}

		/* Increase the time slot */
		 /* Tăng thời gian */
		_time++;
		
		/* Let devices continue their job */
		 /* Cho phép các thiết bị tiếp tục công việc */
		for (temp = dev_list; temp != NULL; temp = temp->next) {
			pthread_mutex_lock(&temp->id.timer_lock);
			temp->id.done = 0;
			pthread_cond_signal(&temp->id.timer_cond);
			pthread_mutex_unlock(&temp->id.timer_lock);
		}
		if (fsh == event) {
			break;
		}
	}
	pthread_exit(args);
}

// Định nghĩa hàm next_slot để thông báo rằng thiết bị đã hoàn thành công việc trong khoảng thời gian hiện tại.
void next_slot(struct timer_id_t * timer_id) {
	/* Tell to timer that we have done our job in current slot */
	 /* Thông báo cho bộ đếm thời gian rằng thiết bị đã hoàn thành công việc */
	pthread_mutex_lock(&timer_id->event_lock);
	timer_id->done = 1;
	pthread_cond_signal(&timer_id->event_cond);
	pthread_mutex_unlock(&timer_id->event_lock);

	/* Wait for going to next slot */
	 /* Chờ đến khoảng thời gian tiếp theo */
	pthread_mutex_lock(&timer_id->timer_lock);
	while (timer_id->done) {
		pthread_cond_wait(
			&timer_id->timer_cond,
			&timer_id->timer_lock
		);
	}
	pthread_mutex_unlock(&timer_id->timer_lock);
}

// Định nghĩa hàm current_time để trả về thời gian hiện tại.
uint64_t current_time() {
	return _time;
}

// Định nghĩa hàm start_timer để bắt đầu bộ đếm thời gian.
void start_timer() {
	timer_started = 1;
	pthread_create(&_timer, NULL, timer_routine, NULL);
}

// Định nghĩa hàm detach_event để tách một sự kiện ra khỏi bộ đếm thời gian.
void detach_event(struct timer_id_t * event) {
	pthread_mutex_lock(&event->event_lock);
	event->fsh = 1;
	pthread_cond_signal(&event->event_cond);
	pthread_mutex_unlock(&event->event_lock);
}

// Định nghĩa hàm attach_event để gắn một sự kiện vào bộ đếm thời gian.
struct timer_id_t * attach_event() {
	if (timer_started) {
		return NULL;
	}else{
		struct timer_id_container_t * container =
			(struct timer_id_container_t*)malloc(
				sizeof(struct timer_id_container_t)		
			);
		container->id.done = 0;
		container->id.fsh = 0;
		pthread_cond_init(&container->id.event_cond, NULL);
		pthread_mutex_init(&container->id.event_lock, NULL);
		pthread_cond_init(&container->id.timer_cond, NULL);
		pthread_mutex_init(&container->id.timer_lock, NULL);
		if (dev_list == NULL) {
			dev_list = container;
			dev_list->next = NULL;
		}else{
			container->next = dev_list;
			dev_list = container;
		}
		return &(container->id);
	}
}

// Định nghĩa hàm stop_timer để dừng bộ đếm thời gian và giải phóng tài nguyên.
void stop_timer() {
	timer_stop = 1;
	pthread_join(_timer, NULL);
	while (dev_list != NULL) {
		struct timer_id_container_t * temp = dev_list;
		dev_list = dev_list->next;
		pthread_cond_destroy(&temp->id.event_cond);
		pthread_mutex_destroy(&temp->id.event_lock);
		pthread_cond_destroy(&temp->id.timer_cond);
		pthread_mutex_destroy(&temp->id.timer_lock);
		free(temp);
	}
}




