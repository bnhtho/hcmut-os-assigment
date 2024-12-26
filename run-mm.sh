clear
cp ./configs/os-cfg-mm.h ./include/os-cfg.h
make clean
make all
./os my_mm_001
# clear && ./os os_0_mlq_paging
# clear && ./os os_1_mlq_paging
# clear && ./os os_1_mlq_paging_small_1K
# clear && ./os os_1_mlq_paging_small_4K
# clear && ./os os_1_singleCPU_mlq_paging 