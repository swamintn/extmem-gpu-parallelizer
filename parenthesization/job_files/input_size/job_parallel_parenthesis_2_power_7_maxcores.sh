#!/bin/bash
#SBATCH -J JOB_parallel_parenthesis_2_power_7_maxcores
#SBATCH -o JOB_parallel_parenthesis_2_power_7_maxcores.o%j
#SBATCH -n 16
#SBATCH -p development
#SBATCH -t 00:30:00

export CILK_NWORKERS=16
./serial_parenthesis_2_power_7_maxcores input_files/power_7.txt output_files/serial_power_7.txt > serial_timings_parallel_parenthesis_2_power_7_maxcores
./parallel_parenthesis_2_power_7_maxcores input_files/power_7.txt output_files/power_7.txt > timings_parallel_parenthesis_2_power_7_maxcores

