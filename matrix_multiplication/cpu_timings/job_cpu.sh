#!/bin/bash
#SBATCH -J JOB_cpu_info
#SBATCH -o final_power_14
#SBATCH -n 1
#SBATCH -p development
#SBATCH -t 02:00:00

date
DATA_DIR="/work/04758/tg840577/project/extmem-gpu-parallelizer/matrix_multiplication/"
SIZE="14"
export CILK_NWORKERS=16
export STXXLCFG="/work/04758/tg840577/project/extmem-gpu-parallelizer/matrix_multiplication/cpu_testing/stxxl_config_file"
../mm_cpu ${DATA_DIR}/input_files/U/zmorton_2_${SIZE}.txt ${DATA_DIR}/input_files/V/zmorton_2_${SIZE}.txt $DATA_DIR/output_files/cpu_zmorton_out_2_${SIZE}.txt
date
