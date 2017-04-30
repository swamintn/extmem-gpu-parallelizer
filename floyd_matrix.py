#!/usr/bin/env python
"""
A simple matrix generator script

Run using generate_matrix.py <power_of_two> <min_val> <max_val>

Matrix is outputted to stdout.
"""
from __future__ import print_function
import random
import sys

def generate_matrix(p, min_val, max_val):
	n = 2**p
	matrix = [[(2**32)-1 for j in xrange(n)] for i in xrange(n)]
	for i in xrange(n):
		for j in xrange(n):
			if (i != j):
				matrix[i][j] = random.randint(min_val, max_val)
			else:
				matrix[i][j] = 0
	return matrix

def main():
    try:
        p, min_val, max_val = map(int, sys.argv[1:])
    except:
        print("Run using generate_matrix.py <power_of_two> <min_val> <max_val>")
        sys.exit(1)
    matrix = generate_matrix(p, min_val, max_val)
    for row in matrix:
        print(" ".join(map(str, row)))

if __name__ == "__main__":
    main()
