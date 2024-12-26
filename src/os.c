#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// time_slot (time slice) là khoảng thời gian (tính bằng giây) mà một tiến trình được phép chạy
static int time_slot;
// là số lượng CPU có sẵn và M là số lượng tiến trình sẽ được chạy
static int num_cpus; 
static int done = 0;

#ifdef CPU_TLB
static int tlbsz;
#endif

#ifdef MM_PAGING
static int memramsz;
static int memswpsz[PAGING_MAX_MMSWP];

struct mmpaging_ld_args {
	/* A dispatched argument struct to compact many-fields passing to loader */
	/* Một cấu trúc đối số được phân phối để nén nhiều trường truyền vào loader */
	struct memphy_struct *tlb;
	struct memphy_struct *mram;
	struct memphy_struct **mswp;
	struct memphy_struct *active_mswp;
	struct timer_id_t  *timer_id;
};
#endif

static struct ld_args{
	char ** path;
	unsigned long * start_time;
#ifdef MLQ_SCHED
	unsigned long * prio;
#endif
} ld_processes;
int num_processes;

struct cpu_args {
	struct timer_id_t * timer_id;
	int id;
};

struct load_args {
	const char *path;
	struct pcb_t *proc;
};

// Hàm tải tiến trình trong luồng riêng biệt
void *load_proc(void *arg) {
	struct load_args *load_arg = (struct load_args*)arg;
	load_arg->proc = load(load_arg->path);
	pthread_exit(NULL);
}

// Định nghĩa hàm cpu_routine để thực hiện công việc của CPU trong một luồng riêng biệt.
static void * cpu_routine(void * args) {
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */
	/* Kiểm tra tiến trình mới trong hàng đợi sẵn sàng */
	int time_left = 0;
	struct pcb_t * proc = NULL;
	while (1) {
		/* Check the status of current process */
		/* Kiểm tra trạng thái của tiến trình hiện tại */
		if (proc == NULL) {
			/* No process is running, the we load new process from
		 	* ready queue */
			/* Không có tiến trình nào đang chạy, tải tiến trình mới từ hàng đợi sẵn sàng */
			proc = get_proc();
			if (proc == NULL) {
                           next_slot(timer_id);
                           continue; /* Tải lần đầu thất bại. bỏ qua tải giả */
                        }
		}else if (proc->pc == proc->code->size) {
			/* The porcess has finish it job */
			/* Tiến trình đã hoàn thành công việc */
			printf("\tCPU %d: ProcessID %2d %sđã hoàn thành tất cả công việc%s\n",
				id ,proc->pid,RED, RESET);
			free(proc);
			proc = get_proc();
			time_left = 0;
		}else if (time_left == 0) {
			/* The process has done its job in current time slot */
			/* Tiến trình đã hoàn thành công việc trong khoảng thời gian hiện tại */
			printf("\tCPU %d: ProcessID %2d %sđã hết thời gian chạy cho phép đưa trở lại hàng chờ%s\n",
				id, proc->pid,YELLOW, RESET);
			put_proc(proc);
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		/* Kiểm tra lại trạng thái tiến trình sau khi tải tiến trình mới */
		if (proc == NULL && done) {
			/* No process to run, exit */
			/* Không có tiến trình nào để chạy, thoát */
			printf("\tCPU %d : stopped\n", id);
			break;
		}else if (proc == NULL) {
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			/* Có thể có tiến trình mới để chạy trong các khoảng thời gian tiếp theo, chỉ bỏ qua khoảng thời gian hiện tại */
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			printf("\t%sCPU %d: Chạy ProcessID %2d%s\n",
				GREEN ,id, proc->pid,RESET);
			time_left = time_slot;
		}

		/* Run current process */
		/* Chạy tiến trình hiện tại */
		// printf("\t%sCPU %d: Chạy ProcessID %2d -----%s\n",
		// 		GREEN ,id, proc->pid,RESET);
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);
	pthread_exit(NULL);
}
/** Loader routine (thread) */
static void * ld_routine(void * args) {
#ifdef MM_PAGING
	struct memphy_struct* mram = ((struct mmpaging_ld_args *)args)->mram;
	struct memphy_struct** mswp = ((struct mmpaging_ld_args *)args)->mswp;
	struct memphy_struct* active_mswp = ((struct mmpaging_ld_args *)args)->active_mswp;
	struct timer_id_t * timer_id = ((struct mmpaging_ld_args *)args)->timer_id;
#else
	struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif
	int i = 0;
	printf("ld_routine\n");

	// Tạo một mảng các luồng để tải các tiến trình cùng lúc
	pthread_t *load_threads = (pthread_t*)malloc(num_processes * sizeof(pthread_t));

	// Cấu trúc để truyền đối số cho các luồng tải
	struct load_args *load_args_array = (struct load_args*)malloc(num_processes * sizeof(struct load_args));



	// Khởi tạo các luồng để tải các tiến trình
	for (i = 0; i < num_processes; i++) {
		load_args_array[i].path = ld_processes.path[i];
		pthread_create(&load_threads[i], NULL, load_proc, (void*)&load_args_array[i]);
	}

	// Chờ tất cả các luồng tải hoàn thành
	for (i = 0; i < num_processes; i++) {
		pthread_join(load_threads[i], NULL);
	}

	// Tiếp tục xử lý các tiến trình đã tải
	for (i = 0; i < num_processes; i++) {
		struct pcb_t *proc = load_args_array[i].proc;
#ifdef MLQ_SCHED
		proc->prio = ld_processes.prio[i];
#endif
		while (current_time() < ld_processes.start_time[i]) {
			// printf("\t%sChờ đến thời gian %lu%s\n", YELLOW, ld_processes.start_time[i], RESET);
			next_slot(timer_id);
		}
#ifdef MM_PAGING
		proc->mm = malloc(sizeof(struct mm_struct));
		init_mm(proc->mm, proc);
		proc->mram = mram;
		proc->mswp = mswp;
		proc->active_mswp = active_mswp;
#endif
		printf("\t%sNạp process ở file %s, ProcessID: %d ,PRIO: %ld%s\n",
			GREEN, ld_processes.path[i], proc->pid, ld_processes.prio[i],RESET);
		add_proc(proc);
#ifdef MLQ_SCHED
		// print_mlq_ready_queue();
#endif
		free(ld_processes.path[i]);
		next_slot(timer_id);
	}

	free(load_threads);
	free(load_args_array);
	free(ld_processes.path);
	free(ld_processes.start_time);
	done = 1;
	detach_event(timer_id);
	pthread_exit(NULL);
}

// Định nghĩa hàm read_config để đọc cấu hình từ file.
static void read_config(const char * path) {
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		printf("Cannot find configure file at %s\n", path);
		exit(1);
	}
	// time_slot , num_cpus, num_processes
	if(fscanf(file, "%d %d %d\n", &time_slot, &num_cpus, &num_processes) != 3) {
        fprintf(stderr, "Error reading the first line\n");
        fclose(file);
        exit(1);
    }
	printf("%sThời gian chạy : %d , CPUS : %d , Số tiến trình : %d%s\n",
		YELLOW, time_slot, num_cpus, num_processes,RESET);

	ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
	ld_processes.start_time = (unsigned long*)
		malloc(sizeof(unsigned long) * num_processes);

#ifdef CPU_TLB
#ifdef CPUTLB_FIXED_TLBSZ
	/* We provide here a back compatible with legacy OS simulatiom config file
	 * In which, it have no addition config line for CPU_TLB
	 */
	/* Chúng tôi cung cấp ở đây một tương thích ngược với tệp cấu hình mô phỏng OS cũ
	 * Trong đó, nó không có dòng cấu hình bổ sung cho CPU_TLB
	 */
	tlbsz = 0x10000;
#else
	/* Read input config of TLB size:
	 * Format:
	 *        CPU_TLBSZ
	*/
	/* Đọc cấu hình đầu vào của kích thước TLB:
	 * Định dạng:
	 *        CPU_TLBSZ
	*/
	fscanf(file, "%d\n", &tlbsz);
#endif
#endif

#ifdef MM_PAGING
	int sit;
#ifdef MM_FIXED_MEMSZ
	/* We provide here a back compatible with legacy OS simulatiom config file
	 * In which, it have no addition config line for Mema, keep only one line
	 * for legacy info 
	 *  [time slice] [N = Number of CPU] [M = Number of Processes to be run]
	 */
	/* Chúng tôi cung cấp ở đây một tương thích ngược với tệp cấu hình mô phỏng OS cũ
	 * Trong đó, nó không có dòng cấu hình bổ sung cho Mema, chỉ giữ một dòng
	 * cho thông tin cũ 
	 *  [time slice] [N = Number of CPU] [M = Number of Processes to be run]
	 */
	memramsz    =  0x100000;
	memswpsz[0] = 0x1000000;
	for(sit = 1; sit < PAGING_MAX_MMSWP; sit++)
		memswpsz[sit] = 0;
#else
	/* Read input config of memory size: MEMRAM and upto 4 MEMSWP (mem swap)
	 * Format: (size=0 result non-used memswap, must have RAM and at least 1 SWAP)
	 *        MEM_RAM_SZ MEM_SWP0_SZ MEM_SWP1_SZ MEM_SWP2_SZ MEM_SWP3_SZ
	*/
	/* Đọc cấu hình đầu vào của kích thước bộ nhớ: MEMRAM và tối đa 4 MEMSWP (mem swap)
	 * Định dạng: (size=0 result non-used memswap, must have RAM and at least 1 SWAP)
	 *        MEM_RAM_SZ MEM_SWP0_SZ MEM_SWP1_SZ MEM_SWP2_SZ MEM_SWP3_SZ
	*/
#ifdef MM_PAGING_DEBUG
	printf("Đọc cấu hình bộ nhớ MEMRAM và %d MEMSWP là %d số tiếp theo\n",PAGING_MAX_MMSWP,PAGING_MAX_MMSWP);
#endif
	fscanf(file, "%d\n", &memramsz);
#ifdef MM_PAGING_DEBUG
	printf("MEMRAM có size là %d byte => %d KB\n", memramsz, memramsz/1024);
#endif
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++){
		fscanf(file, "%d", &(memswpsz[sit])); 
		// printf("memswpsz %d : %d\n", sit, memswpsz[sit]);
	}
	fscanf(file, "\n"); /* Ký tự cuối cùng */
#endif
#endif

#ifdef MLQ_SCHED
	ld_processes.prio = (unsigned long*)
		malloc(sizeof(unsigned long) * num_processes);
#endif
	int i;
	for (i = 0; i < num_processes; i++) {
		ld_processes.path[i] = (char*)malloc(sizeof(char) * 100);
		ld_processes.path[i][0] = '\0';
		strcat(ld_processes.path[i], "input/proc/");
		char proc[100];
#ifdef MLQ_SCHED
		fscanf(file, "%lu %s %lu\n", &ld_processes.start_time[i], proc, &ld_processes.prio[i]);
#else
		fscanf(file, "%lu %s\n", &ld_processes.start_time[i], proc);
#endif
		strcat(ld_processes.path[i], proc);
		// In tất cả các tiến trình
		printf("%sTiến trình : %s, Thời gian vào: %lu, Độ ưu tiên: %lu%s\n",GREEN, ld_processes.path[i], ld_processes.start_time[i], ld_processes.prio[i],RESET);
	}
}

// Định nghĩa hàm main để khởi động hệ điều hành mô phỏng.
int main(int argc, char * argv[]) {
	/* Đọc cấu hình */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n");
		return 1;
	}
	char path[100];
	path[0] = '\0'; // Chuỗi rỗng
	strcat(path, "input/");
	strcat(path, argv[1]);
	printf("%sĐọc dữ liệu từ file : %s%s\n",GREEN, path, RESET);
	read_config(path);

	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	struct cpu_args * args =
		(struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	pthread_t ld;
	
	/* Khởi tạo bộ đếm thời gian */
	int i;
	for (i = 0; i < num_cpus; i++) {
		args[i].timer_id = attach_event();
		args[i].id = i;
	}
	struct timer_id_t * ld_event = attach_event();
	start_timer();
#ifdef CPU_TLB
	struct memphy_struct tlb;

	init_tlbmemphy(&tlb, tlbsz);
#endif

#ifdef MM_PAGING
	/* Init all MEMPHY include 1 MEMRAM and n of MEMSWP */
	/* Khởi tạo tất cả MEMPHY bao gồm 1 MEMRAM và n của MEMSWP */
	/* Theo mặc định memphy là BỘ NHỚ TRUY CẬP NGẪU NHIÊN */
	int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */

	struct memphy_struct mram;
	struct memphy_struct mswp[PAGING_MAX_MMSWP];


	/* Create MEM RAM */
	/* Tạo MEM RAM */
#ifdef MM_PAGING_DEBUG
	printf("Khởi tạo bộ nhớ vật lý mram cho chương trình main có size là %d byte => %d KB\n",memramsz,memramsz/1024);
#endif
	init_memphy(&mram, memramsz, rdmflag);

	/* Create all MEM SWAP */ 
	/* Tạo tất cả MEM SWAP */ 
	int sit;
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++){
	       init_memphy(&mswp[sit], memswpsz[sit], rdmflag);
#ifdef MM_PAGING_DEBUG
			printf("Khởi tạo bộ nhớ vật lý mswp thứ %d có size là %d byte => %d KB\n",sit,memswpsz[sit],memswpsz[sit]/1024);
#endif
	}

	/* In Paging mode, it needs passing the system mem to each PCB through loader*/
	/* Trong chế độ phân trang, cần truyền bộ nhớ hệ thống cho mỗi PCB thông qua loader */
	struct mmpaging_ld_args *mm_ld_args = malloc(sizeof(struct mmpaging_ld_args));

	mm_ld_args->timer_id = ld_event;
	mm_ld_args->mram = (struct memphy_struct *) &mram;
	mm_ld_args->mswp = (struct memphy_struct**) &mswp;
	mm_ld_args->active_mswp = (struct memphy_struct *) &mswp[0];
#endif

#ifdef CPU_TLB
#ifdef MM_PAGING
	/* In MM_PAGING employ CPU_TLB mode, it needs passing
	 * the system tlb to each PCB through loader
	*/
	/* Trong chế độ phân trang sử dụng CPU_TLB, cần truyền
	 * bộ nhớ hệ thống tlb cho mỗi PCB thông qua loader
	*/
	mm_ld_args->tlb = (struct memphy_struct *) &tlb;
#endif
#endif

	/* Khởi tạo bộ lập lịch */
	init_scheduler();

	/* Chạy CPU và loader */
#ifdef MM_PAGING
	pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
#else
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL,
			cpu_routine, (void*)&args[i]);
	}

	/* Chờ CPU và loader hoàn thành */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);
	}
	pthread_join(ld, NULL);

	/* Dừng bộ đếm thời gian */
	stop_timer();

	return 0;

}



