#!/bin/bash
#SBATCH -J JOB_parallel_parenthesis_2_power_13_cores_8
#SBATCH -o JOB_parallel_parenthesis_2_power_13_cores_8.o%j
#SBATCH -n 8
#SBATCH -p serial
#SBATCH -t 12:00:00

export CILK_NWORKERS=8
./parallel_parenthesis_2_power_13_maxcores input_files/power_13.txt output_files/power_13_cores_8.txt > timings_parallel_parenthesis_2_power_13_cores_8

