#!/bin/bash
#SBATCH -J JOB_parallel_parenthesis_2_power_13_cores_2
#SBATCH -o JOB_parallel_parenthesis_2_power_13_cores_2.o%j
#SBATCH -n 2
#SBATCH -p serial
#SBATCH -t 12:00:00

export CILK_NWORKERS=2
./parallel_parenthesis_2_power_13_maxcores input_files/power_13.txt output_files/power_13_cores_2.txt > timings_parallel_parenthesis_2_power_13_cores_2

