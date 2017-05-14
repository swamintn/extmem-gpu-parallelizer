#!/bin/bash
#SBATCH -J JOB_gpu_info
#SBATCH -o final_power_14
#SBATCH -n 1
#SBATCH -p gpudev
#SBATCH -t 0:30:00

module load cuda
date
DATA_DIR="/work/04758/tg840577/project/extmem-gpu-parallelizer/matrix_multiplication/"
SIZE="14"
../mm_gpu ${DATA_DIR}/input_files/U/zmorton_2_${SIZE}.txt ${DATA_DIR}/input_files/V/zmorton_2_${SIZE}.txt $DATA_DIR/output_files/zmorton_out_2_${SIZE}.txt
date
