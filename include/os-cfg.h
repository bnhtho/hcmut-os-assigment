#ifndef OSCFG_H
#define OSCFG_H

#define MLQ_SCHED 1
#define MAX_PRIO 140

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

#pragma region Orginal
// #define CPU_TLB
// #define CPUTLB_FIXED_TLBSZ
// #define MM_PAGING
// //#define MM_FIXED_MEMSZ
// //#define VMDBG 1
// //#define MMDBG 1
// #define IODUMP 1
// #define PAGETBL_DUMP 1
#pragma endregion

#pragma region Sched
#pragma endregion

#pragma region os_0_mlq_paging / os_1_mlq_paging / os_1_mlq_paging_small_4K
// #define CPU_TLB
// #define CPUTLB_FIXED_TLBSZ
#define MM_PAGING
// #define MM_FIXED_MEMSZ
#define VMDBG 1
#define MMDBG 1
#define IODUMP 1
// #define PAGETBL_DUMP 1
#define DUMP_TO_FILE
#pragma endregion

#pragma region os_1_mlq_paging_small_1K
// #define CPU_TLB
// // #define CPUTLB_FIXED_TLBSZ
// #define MM_PAGING
// // #define MM_FIXED_MEMSZ
// #define VMDBG 1
// #define MMDBG 1
// #define IODUMP 1
// // #define PAGETBL_DUMP 1
#pragma endregion

#pragma region os_1_singleCPU_mlq
// #define CPU_TLB
// #define CPUTLB_FIXED_TLBSZ
// #define MM_PAGING
// #define MM_FIXED_MEMSZ
// #define VMDBG 1
// #define MMDBG 1
// #define IODUMP 1
// // #define PAGETBL_DUMP 1
#pragma endregion

#pragma region os_1_singleCPU_mlq_paging
// #define CPU_TLB
// // #define CPUTLB_FIXED_TLBSZ
// #define MM_PAGING
// // #define MM_FIXED_MEMSZ
// #define VMDBG 1
// #define MMDBG 1
// #define IODUMP 1
// #define PAGETBL_DUMP 1
#pragma endregion

#endif
