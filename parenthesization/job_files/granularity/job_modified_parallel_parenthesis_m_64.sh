#!/bin/bash
#SBATCH -J JOB_modified_parallel_parenthesis_m_64
#SBATCH -o JOB_modified_parallel_parenthesis_m_64.o%j
#SBATCH -n 16
#SBATCH -p development
#SBATCH -t 00:30:00

export CILK_NWORKERS=16
./modified_parallel_parenthesis_m_64 input_files/power_13.txt output_files/power_13_m_64.txt > timings_modified_parallel_parenthesis_m_64

