#!/bin/bash
#SBATCH -J JOB_best_granular_parallel_parenthesis_2_power_13_cores_1
#SBATCH -o JOB_best_granular_parallel_parenthesis_2_power_13_cores_1.o%j
#SBATCH -n 1
#SBATCH -p serial
#SBATCH -t 12:00:00

export CILK_NWORKERS=1
./best_granular_parallel_parenthesis_2_power_13_maxcores input_files/power_13.txt output_files/best_granular_parallel_parenthesis_2_power_13_cores_1.txt > timings_best_granular_parallel_parenthesis_2_power_13_cores_1
