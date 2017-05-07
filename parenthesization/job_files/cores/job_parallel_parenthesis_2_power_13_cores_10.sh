#!/bin/bash
#SBATCH -J JOB_parallel_parenthesis_2_power_13_cores_10
#SBATCH -o JOB_parallel_parenthesis_2_power_13_cores_10.o%j
#SBATCH -n 10
#SBATCH -p serial
#SBATCH -t 12:00:00

export CILK_NWORKERS=10
./parallel_parenthesis_2_power_13_maxcores input_files/power_13.txt output_files/power_13_cores_10.txt > timings_parallel_parenthesis_2_power_13_cores_10

