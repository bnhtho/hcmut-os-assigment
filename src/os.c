
#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int time_slot;
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


static void * cpu_routine(void * args) {
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */
	int time_left = 0;
	struct pcb_t * proc = NULL;
	while (1) {
		/* Check the status of current process */
		if (proc == NULL) {
			/* No process is running, the we load new process from
		 	* ready queue */
			proc = get_proc();
			if (proc == NULL) {
                           next_slot(timer_id);
                           continue; /* First load failed. skip dummy load */
                        }
		}else if (proc->pc == proc->code->size) {
			/* The porcess has finish it job */
			printf("\tCPU %d: Processed %2d has finished\n",
				id ,proc->pid);
			free(proc);
			proc = get_proc();
			time_left = 0;
		}else if (time_left == 0) {
			/* The process has done its job in current time slot */
			printf("\tCPU %d: Put process %2d to run queue\n",
				id, proc->pid);
			put_proc(proc);
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		if (proc == NULL && done) {
			/* No process to run, exit */
			printf("\tCPU %d stopped\n", id);
			break;
		}else if (proc == NULL) {
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			printf("\tCPU %d: Dispatched process %2d\n",
				id, proc->pid);
			time_left = time_slot;
		}
		
		/* Run current process */
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);
	pthread_exit(NULL);
}

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
	while (i < num_processes) {
		struct pcb_t * proc = load(ld_processes.path[i]);
#ifdef MLQ_SCHED
		proc->prio = ld_processes.prio[i];
#endif
		while (current_time() < ld_processes.start_time[i]) {
			next_slot(timer_id);
		}
#ifdef MM_PAGING
		proc->mm = malloc(sizeof(struct mm_struct));
		init_mm(proc->mm, proc);
		proc->mram = mram;
		proc->mswp = mswp;
		proc->active_mswp = active_mswp;
#endif
		printf("\tLoaded a process at %s, PID: %d PRIO: %ld\n",
			ld_processes.path[i], proc->pid, ld_processes.prio[i]);
		add_proc(proc);
		free(ld_processes.path[i]);
		i++;
		next_slot(timer_id);
	}
	free(ld_processes.path);
	free(ld_processes.start_time);
	done = 1;
	detach_event(timer_id);
	pthread_exit(NULL);
}

static void read_config(const char * path) {
    FILE *file;
    if ((file = fopen(path, "r")) == NULL) {
        fprintf(stderr, "Error: Cannot find configuration file at %s\n", path);
        exit(1);
    }

    // Đọc thông tin dòng đầu tiên
    if (fscanf(file, "%d %d %d\n", &time_slot, &num_cpus, &num_processes) != 3) {
        fprintf(stderr, "Error: Invalid configuration file format (expected 3 integers in first line).\n");
        fclose(file);
        exit(1);
    }

    // Đọc thông tin dòng thứ hai: RAM size và Swap sizes
#ifdef MM_PAGING
    if (fscanf(file, "%d", &memramsz) != 1) {
        fprintf(stderr, "Error: Failed to read RAM size in configuration file.\n");
        fclose(file);
        exit(1);
    }
    for (int i = 0; i < PAGING_MAX_MMSWP; i++) {
        if (fscanf(file, "%d", &memswpsz[i]) != 1) {
            memswpsz[i] = 0;  // Nếu không có giá trị, đặt thành 0 (không dùng swap)
        }
    }
    fscanf(file, "\n");  // Đọc ký tự xuống dòng
#endif

    // Khởi tạo mảng lưu thông tin quy trình
    ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
    ld_processes.start_time = (unsigned long*)malloc(sizeof(unsigned long) * num_processes);
    if (ld_processes.path == NULL || ld_processes.start_time == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        fclose(file);
        exit(1);
    }

#ifdef MLQ_SCHED
    ld_processes.prio = (unsigned long*)malloc(sizeof(unsigned long) * num_processes);
    if (ld_processes.prio == NULL) {
        fprintf(stderr, "Error: Memory allocation for priorities failed.\n");
        fclose(file);
        exit(1);
    }
#endif

    // Đọc thông tin các quy trình
    for (int i = 0; i < num_processes; i++) {
        ld_processes.path[i] = (char*)malloc(sizeof(char) * 100);
        if (ld_processes.path[i] == NULL) {
            fprintf(stderr, "Error: Memory allocation for process path failed.\n");
            fclose(file);
            exit(1);
        }
        ld_processes.path[i][0] = '\0';

        strcat(ld_processes.path[i], "input/proc/");
        char proc[100];
#ifdef MLQ_SCHED
        if (fscanf(file, "%lu %s %lu\n", &ld_processes.start_time[i], proc, &ld_processes.prio[i]) != 3) {
            fprintf(stderr, "Error: Invalid process entry format in configuration file.\n");
            fclose(file);
            exit(1);
        }
#else
        if (fscanf(file, "%lu %s\n", &ld_processes.start_time[i], proc) != 2) {
            fprintf(stderr, "Error: Invalid process entry format in configuration file.\n");
            fclose(file);
            exit(1);
        }
#endif
        strcat(ld_processes.path[i], proc);
    }

    fclose(file);
}


int main(int argc, char * argv[]) {
	/* Read config */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n");
		return 1;
	}
	char path[100];
	path[0] = '\0';
	strcat(path, "input/");
	strcat(path, argv[1]);
	read_config(path);

	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	struct cpu_args * args =
		(struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	pthread_t ld;
	
	/* Init timer */
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
	int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */

	struct memphy_struct mram;
	struct memphy_struct mswp[PAGING_MAX_MMSWP];


	/* Create MEM RAM */
	init_memphy(&mram, memramsz, rdmflag);

	/* Create all MEM SWAP */ 
	int sit;
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++)
	       init_memphy(&mswp[sit], memswpsz[sit], rdmflag);

	/* In Paging mode, it needs passing the system mem to each PCB through loader*/
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
	mm_ld_args->tlb = (struct memphy_struct *) &tlb;
#endif
#endif

	/* Init scheduler */
	init_scheduler();

	/* Run CPU and loader */
#ifdef MM_PAGING
	pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
#else
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL,
			cpu_routine, (void*)&args[i]);
	}

	/* Wait for CPU and loader finishing */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);
	}
	pthread_join(ld, NULL);

	/* Stop timer */
	stop_timer();

	return 0;

}



