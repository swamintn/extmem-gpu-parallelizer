/* 
  Matrix Parenthesization
  Timing stats are logged to logfile
 */

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <time.h>
#include "helper.h"
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

using namespace std;


int main(int argc, char **argv)
{
    if (argc != 3) {
        cout << "'Specify <input_matrix_file> "  <<
                "<output_matrix_file> as arguments'" <<
                endl;
        exit(1);
    }
    
    string input_matrix(argv[1]);
    string output_matrix(argv[2]);

    long (*INPUT)[SIZE] = new long[SIZE][SIZE]();

    read_matrix(INPUT, input_matrix);
    
    int n = SIZE;
    struct timespec start, finish;
    double time_taken;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int steps = 2; steps <= n-1; steps++) {
	for (int i = 0; i <= n - steps - 1; i++) {
	    int j = steps + i;
	    for (int k = i+1; k <= j; k++) {
	        INPUT[i][j] = min(INPUT[i][j], INPUT[i][k] + INPUT[k][j]);
	    }
	}	
    }  
    clock_gettime(CLOCK_MONOTONIC, &finish);
    time_taken  =  finish.tv_sec - start.tv_sec;
    time_taken += (finish.tv_nsec - start.tv_nsec) / 1e9;

    write_matrix(INPUT, output_matrix);

    cout << "Algorithm: Matrix Parenthesization" << endl;
    cout << "Time taken: " << time_taken << endl;
    cout << "INPUT: " << input_matrix << ", OUTPUT: " << output_matrix << endl;
    
    delete[] INPUT;

    return 0;
}
