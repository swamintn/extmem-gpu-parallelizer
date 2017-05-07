#!/usr/bin/env python
"""
A simple matrix generator script

Run using floyd_matrix.py <power_of_two> <min_val> <max_val> <infinity_percentage>

Matrix is outputted to stdout.
"""
from __future__ import print_function
import math
import random
import sys

def generate_matrix(p, min_val, max_val, infinity_percent):
	n = 2**p
	matrix = [[(2**32)-1 for j in xrange(n)] for i in xrange(n)]
	for i in xrange(n):
		for j in xrange(n):
			if (i != j):
				# Give chance for infinity values
				actual_max = math.ceil(max_val + ((max_val-min_val)*infinity_percent))
				choice = random.randint(min_val, actual_max)
				if choice > max_val:
					choice = "inf"
				matrix[i][j] = choice
			else:
				matrix[i][j] = 0
	return matrix

def main():
    try:
        p, min_val, max_val = map(int, sys.argv[1:4])
        inf_percent = float(sys.argv[4])
    except:
        print("Run using floyd_matrix.py <power_of_two> <min_val> <max_val> <infinity_percentage>")
        sys.exit(1)
    matrix = generate_matrix(p, min_val, max_val, inf_percent)
    for row in matrix:
        print(" ".join(map(str, row)))

if __name__ == "__main__":
    main()
