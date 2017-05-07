#!/bin/bash
#SBATCH -J JOB_modified_parallel_parenthesis_m_16
#SBATCH -o JOB_modified_parallel_parenthesis_m_16.o%j
#SBATCH -n 16
#SBATCH -p development
#SBATCH -t 00:30:00

export CILK_NWORKERS=16
./modified_parallel_parenthesis_m_16 input_files/power_13.txt output_files/power_13_m_16.txt > timings_modified_parallel_parenthesis_m_16

