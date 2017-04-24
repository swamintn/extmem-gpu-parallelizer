/***************************************************************************
 *  examples/containers/vector1.cpp
 *
 *  Part of the STXXL. See http://stxxl.sourceforge.net
 *
 *  Copyright (C) 2013 Daniel Feist <daniel.feist@student.kit.edu>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#include <stxxl/vector>
#include <iostream>
using namespace std;

int size = 128;

void zmorton(int n, int row, int col, int matrix[][size],
stxxl::VECTOR_GENERATOR<unsigned int, 1, 1, 1*128*128, stxxl::RC, stxxl::lru>::result& my_vector) {

	if (n == 1) {
		my_vector.push_back(matrix[row][col]);
		return;
	}	
	zmorton(n/2, row, col, matrix, my_vector);
	zmorton(n/2, row, col + (n/2), matrix, my_vector);
	zmorton(n/2, row + (n/2), col, matrix, my_vector);
	zmorton(n/2, row + (n/2), col + (n/2), matrix, my_vector);
	return;
}


int main()
{
    typedef stxxl::VECTOR_GENERATOR<unsigned int, 1, 1, 1*128*128, stxxl::RC, stxxl::lru>::result vector;
    vector my_vector;
	
	int matrix[size][size];
	
	int count = 0, result = 0;
	int row = 0, col = 0;
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			matrix[i][j] = ++count;
		}
	}

	zmorton(size, row, col, matrix, my_vector);	

    while (!my_vector.empty())
    {
		result = my_vector[my_vector.size() - 1];
		cout << result << endl;
		my_vector.pop_back();
    }

    return 0;
}
