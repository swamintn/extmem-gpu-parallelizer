#!/usr/bin/env python
import random
import sys

def gen_matrix_as_array(full_size, min_val, max_val):
    return [random.randint(min_val, max_val) for i in range(full_size)]

def main():
    power_of_2, min_val, max_val = map(int, sys.argv[1:])
    n = 2**power_of_2
    mat = gen_matrix_as_array(n*n, min_val, max_val)
    print " ".join(map(str, mat))

if __name__ == "__main__":
    main()
