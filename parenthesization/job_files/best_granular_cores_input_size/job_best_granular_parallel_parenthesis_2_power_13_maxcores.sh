#!/bin/bash
#SBATCH -J JOB_best_granular_parallel_parenthesis_2_power_13_maxcores
#SBATCH -o JOB_best_granular_parallel_parenthesis_2_power_13_maxcores.o%j
#SBATCH -n 16
#SBATCH -p development
#SBATCH -t 00:30:00

export CILK_NWORKERS=16
./best_granular_parallel_parenthesis_2_power_13_maxcores input_files/power_13.txt output_files/best_granular_parallel_parenthesis_2_power_13_maxcores.txt > timings_best_granular_parallel_parenthesis_2_power_13_maxcores
