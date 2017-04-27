#!/bin/bash
#SBATCH -J JOB_gpu_info
#SBATCH -o JOB_gpu_info.o%j
#SBATCH -n 1
#SBATCH -p gpudev
#SBATCH -t 00:10:00

module load cuda
./gpu_info
