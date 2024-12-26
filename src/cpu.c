
#include "cpu.h"
#include "mem.h"
#include "mm.h"
#include <stdio.h>

/**
 * Hàm calc này luôn trả về 0 bất kể giá trị của proc là gì. Nó không thực hiện bất kỳ tính toán thực sự nào và có thể chỉ là một hàm giả lập hoặc hàm giữ chỗ (placeholder) trong mã nguồn.
 */
int calc(struct pcb_t * proc) {
	return ((unsigned long)proc & 0UL);
}

int alloc(struct pcb_t * proc, uint32_t size, uint32_t reg_index) {
	addr_t addr = alloc_mem(size, proc);
	if (addr == 0) {
		return 1;
	}else{
		proc->regs[reg_index] = addr;
		return 0;
	}
}

int free_data(struct pcb_t * proc, uint32_t reg_index) {
	return free_mem(proc->regs[reg_index], proc);
}

int read(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) { // Index of destination register
	
	BYTE data;
	if (read_mem(proc->regs[source] + offset, proc,	&data)) {
		proc->regs[destination] = data;
		return 0;		
	}else{
		return 1;
	}
}

int write(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset) { 	// Destination address =
					// [destination] + [offset]
	return write_mem(proc->regs[destination] + offset, proc, data);
} 

int run(struct pcb_t * proc) {
	/* Check if Program Counter point to the proper instruction */
	if (proc->pc >= proc->code->size) {
		MEMPHY_dump(proc->mram);
		return 1;
	}
	
	struct inst_t ins = proc->code->text[proc->pc];
	proc->pc++;
	int stat = 1;
	switch (ins.opcode) {
	case CALC:
		stat = calc(proc);
		break;
	case ALLOC:
#ifdef CPU_TLB 
		stat = tlballoc(proc, ins.arg_0, ins.arg_1);
#elif defined(MM_PAGING)
		printf("\tProcessID : %d ",proc->pid);
		printf("%syêu cầu cấp phát bộ nhớ %s",GREEN, RESET);
		printf("với dung lượng %d BYTE ở reg_index : %d\n",ins.arg_0,ins.arg_1);
		stat = pgalloc(proc, ins.arg_0, ins.arg_1);
		print_pgtbl(proc,0,-1);
#else
		stat = alloc(proc, ins.arg_0, ins.arg_1);
#endif
		break;
	case FREE:
#ifdef CPU_TLB
		stat = tlbfree_data(proc, ins.arg_0);
#elif defined(MM_PAGING)
		printf("\tProcessID : %d ",proc->pid);
		printf("%syêu cầu giải phóng vùng nhớ%s",RED,RESET);
		printf(" ở reg_index : %d\n",ins.arg_0);
		// print_pgtbl(proc,0,-1);
		struct vm_rg_struct *temp = get_symrg_byid(proc->mm,ins.arg_0);
		// print_pgtbl(proc,temp->rg_start,temp->rg_end);
		stat = pgfree_data(proc, ins.arg_0);
		print_pgtbl(proc,temp->rg_start,temp->rg_end);
#else
		stat = free_data(proc, ins.arg_0);
#endif
		break;
	case READ:
#ifdef CPU_TLB
		stat = tlbread(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#elif defined(MM_PAGING)
		printf("\tProcessID : %d ",proc->pid);
		printf("%syêu cầu đọc%s",YELLOW,RESET);
		// printf(" ở reg_index : %d , offset : %d , destination : %d\n",ins.arg_0,ins.arg_1,ins.arg_2);
		printf(" ở reg_index : %d , offset : %d\n",ins.arg_0,ins.arg_1);
		stat = pgread(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#else
		stat = read(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	case WRITE:
#ifdef CPU_TLB
		stat = tlbwrite(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#elif defined(MM_PAGING)
		printf("\tProcessID : %d ",proc->pid);
		printf("%syêu cầu ghi%s",GREEN,RESET);
		// printf(" giá trị : %d ,  ở reg_index : %d , offset : %d \n",ins.arg_0, ins.arg_1, ins.arg_2);
		printf(" giá trị : %d ,  ở reg_index : %d , offset : %d \n",ins.arg_0, ins.arg_1, ins.arg_2);
		stat = pgwrite(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#else
		stat = write(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	default:
		stat = 1;
	}
	return stat;

}


