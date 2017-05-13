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


void C_par(int M, long (*X)[SIZE], int start_row_X, int start_col_X, int end_row_X, int end_col_X,
	   int start_row_U, int start_col_U, int end_row_U, int end_col_U,
	   int start_row_V, int start_col_V, int end_row_V, int end_col_V);

void B_par(int M, long (*X)[SIZE], int start_row_X, int start_col_X, int end_row_X, int end_col_X,
	   int start_row_U, int start_col_U, int end_row_U, int end_col_U,
	   int start_row_V, int start_col_V, int end_row_V, int end_col_V);

void A_par(int M, long (*X)[SIZE], int start_row, int start_col, int end_row, int end_col);



void C_par(int M, long (*X)[SIZE], int start_row_X, int start_col_X, int end_row_X, int end_col_X,
	   int start_row_U, int start_col_U, int end_row_U, int end_col_U,
	   int start_row_V, int start_col_V, int end_row_V, int end_col_V) {
	
    if (M == 1) {
        X[start_row_X][start_col_X] = min(X[start_row_X][start_col_X], X[start_row_U][start_col_U] + X[start_row_V][start_col_V]);
    
    } else {
		cilk_spawn C_par(M/2, X, start_row_X, start_col_X, (end_row_X + start_row_X)/2, (end_col_X + start_col_X)/2,            // X-11
			start_row_U, start_col_U, (end_row_U + start_row_U)/2, (end_col_U + start_col_U)/2,                     // U-11
			start_row_V, start_col_V, (end_row_V + start_row_V)/2, (end_col_V + start_col_V)/2);                    // V-11
			
		cilk_spawn C_par(M/2, X,  start_row_X, ((end_col_X + start_col_X)/2) + 1, ((end_row_X + start_row_X)/2), end_col_X,     // X-12
			start_row_U, start_col_U, (end_row_U + start_row_U)/2, (end_col_U + start_col_U)/2,                     // U-11
			start_row_V, ((end_col_V + start_col_V)/2) + 1, ((end_row_V + start_row_V)/2), end_col_V);              // V-12 
			
		cilk_spawn C_par(M/2, X, ((end_row_X + start_row_X)/2) + 1, start_col_X, end_row_X, (end_col_X + start_col_X)/2,        // X-21
			((end_row_U + start_row_U)/2) + 1, start_col_U, end_row_U, (end_col_U + start_col_U)/2,         	// U-21
			start_row_V, start_col_V, (end_row_V + start_row_V)/2, (end_col_V + start_col_V)/2);                    // V-11

		   C_par(M/2, X, ((end_row_X + start_row_X)/2) + 1, ((end_col_X + start_col_X)/2) + 1, end_row_X, end_col_X,    // X-22
			((end_row_U + start_row_U)/2) + 1, start_col_U, end_row_U, (end_col_U + start_col_U)/2,                 // U-21
			start_row_V, ((end_col_V + start_col_V)/2) + 1, ((end_row_V + start_row_V)/2), end_col_V);              // V-12
		cilk_sync;


		cilk_spawn C_par(M/2, X, start_row_X, start_col_X, (end_row_X + start_row_X)/2, (end_col_X + start_col_X)/2,            // X-11
			start_row_U, ((end_col_U + start_col_U)/2) + 1, ((end_row_U + start_row_U)/2), end_col_U,               // U-12
			((end_row_V + start_row_V)/2) + 1, start_col_V, end_row_V, (end_col_V + start_col_V)/2);                 // V-21

		cilk_spawn C_par(M/2, X,  start_row_X, ((end_col_X + start_col_X)/2) + 1, ((end_row_X + start_row_X)/2), end_col_X,     // X-12
			start_row_U, ((end_col_U + start_col_U)/2) + 1, ((end_row_U + start_row_U)/2), end_col_U,               // U-12
			((end_row_V + start_row_V)/2) + 1, ((end_col_V + start_col_V)/2) + 1, end_row_V, end_col_V);            // V-22
			
		cilk_spawn C_par(M/2, X, ((end_row_X + start_row_X)/2) + 1, start_col_X, end_row_X, (end_col_X + start_col_X)/2,        // X-21
			((end_row_U + start_row_U)/2) + 1, ((end_col_U + start_col_U)/2) + 1, end_row_U, end_col_U,             // U-22
			((end_row_V + start_row_V)/2) + 1, start_col_V, end_row_V, (end_col_V + start_col_V)/2);                 // V-21

		   C_par(M/2, X, ((end_row_X + start_row_X)/2) + 1, ((end_col_X + start_col_X)/2) + 1, end_row_X, end_col_X,    // X-22
			((end_row_U + start_row_U)/2) + 1, ((end_col_U + start_col_U)/2) + 1, end_row_U, end_col_U,             // U-22
			((end_row_V + start_row_V)/2) + 1, ((end_col_V + start_col_V)/2) + 1, end_row_V, end_col_V);            // V-22
		cilk_sync;
    }    	

    return;
}



void B_par(int M, long (*X)[SIZE], int start_row_X, int start_col_X, int end_row_X, int end_col_X,
	   int start_row_U, int start_col_U, int end_row_U, int end_col_U,
	   int start_row_V, int start_col_V, int end_row_V, int end_col_V) {

    if (M == 1) {
		X[start_row_X][start_col_X] = min(X[start_row_X][start_col_X], X[start_row_U][start_col_U] + X[start_row_V][start_col_V]);

    } else {

    	B_par(M/2, X, ((end_row_X + start_row_X)/2) + 1, start_col_X, end_row_X, (end_col_X + start_col_X)/2,			// X-21
		((end_row_U + start_row_U)/2) + 1, ((end_col_U + start_col_U)/2) + 1, end_row_U, end_col_U,			// U-22
		start_row_V, start_col_V, (end_row_V + start_row_V)/2, (end_col_V + start_col_V)/2);				// V-11

		cilk_spawn C_par(M/2, X, start_row_X, start_col_X, (end_row_X + start_row_X)/2, (end_col_X + start_col_X)/2, 		// X-11
			start_row_U, ((end_col_U + start_col_U)/2) + 1, ((end_row_U + start_row_U)/2), end_col_U,		// U-12
			((end_row_X + start_row_X)/2) + 1, start_col_X, end_row_X, (end_col_X + start_col_X)/2);		// X-21
				
		C_par(M/2, X, ((end_row_X + start_row_X)/2) + 1, ((end_col_X + start_col_X)/2) + 1, end_row_X, end_col_X, 	// X-22
			((end_row_X + start_row_X)/2) + 1, start_col_X, end_row_X, (end_col_X + start_col_X)/2,         	// X-21
			start_row_V, ((end_col_V + start_col_V)/2) + 1, ((end_row_V + start_row_V)/2), end_col_V);      	// V-12	
		cilk_sync;

		cilk_spawn B_par(M/2, X, start_row_X, start_col_X, (end_row_X + start_row_X)/2, (end_col_X + start_col_X)/2,    	// X-11
			start_row_U, start_col_U, (end_row_U + start_row_U)/2, (end_col_U + start_col_U)/2,    			// U-11
			start_row_V, start_col_V, (end_row_V + start_row_V)/2, (end_col_V + start_col_V)/2);    		// V-11		
	
		B_par(M/2, X, ((end_row_X + start_row_X)/2) + 1, ((end_col_X + start_col_X)/2) + 1, end_row_X, end_col_X, 	// X-22
			((end_row_U + start_row_U)/2) + 1, ((end_col_U + start_col_U)/2) + 1, end_row_U, end_col_U,       	// U-22
			((end_row_V + start_row_V)/2) + 1, ((end_col_V + start_col_V)/2) + 1, end_row_V, end_col_V);            // V-22	
		cilk_sync;

		C_par(M/2, X,  start_row_X, ((end_col_X + start_col_X)/2) + 1, ((end_row_X + start_row_X)/2), end_col_X,        // X-12
			 start_row_U, ((end_col_U + start_col_U)/2) + 1, ((end_row_U + start_row_U)/2), end_col_U,              // U-12
			((end_row_X + start_row_X)/2) + 1, ((end_col_X + start_col_X)/2) + 1, end_row_X, end_col_X);       	// X-22

		C_par(M/2, X,  start_row_X, ((end_col_X + start_col_X)/2) + 1, ((end_row_X + start_row_X)/2), end_col_X,        // X-12
			start_row_X, start_col_X, (end_row_X + start_row_X)/2, (end_col_X + start_col_X)/2,           	 	// X-11
			start_row_V, ((end_col_V + start_col_V)/2) + 1, ((end_row_V + start_row_V)/2), end_col_V);              // V-12 

		B_par(M/2, X, start_row_X, ((end_col_X + start_col_X)/2) + 1, ((end_row_X + start_row_X)/2), end_col_X,         // X-12
			start_row_U, start_col_U, (end_row_U + start_row_U)/2, (end_col_U + start_col_U)/2,                     // U-11
			((end_row_V + start_row_V)/2) + 1, ((end_col_V + start_col_V)/2) + 1, end_row_V, end_col_V);            // V-22 			
    }

    return;	
}



void A_par(int M, long (*X)[SIZE], int start_row, int start_col, int end_row, int end_col) {

    if (M != 1) {

		cilk_spawn A_par(M/2, X, start_row, start_col, (end_row + start_row)/2, (end_col + start_col)/2);			// X-11

		A_par(M/2, X, ((end_row + start_row)/2) + 1, ((end_col + start_col)/2) + 1, end_row, end_col);  		// X-22

		cilk_sync;

		B_par(M/2, X, start_row, ((end_col + start_col)/2) + 1, ((end_row + start_row)/2), end_col,				// X-12
		start_row, start_col, ((end_row + start_row)/2), ((end_col + start_col)/2),					// X-11
		((end_row + start_row)/2) + 1, ((end_col + start_col)/2) + 1, end_row, end_col); 				// X-22
    }

    return;
}



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
    A_par(n, INPUT, 0, 0, n-1, n-1);
    
    clock_gettime(CLOCK_MONOTONIC, &finish);
    time_taken  =  finish.tv_sec - start.tv_sec;
    time_taken += (finish.tv_nsec - start.tv_nsec) / 1e9;

    write_matrix(INPUT, output_matrix);

    cout << "Algorithm: Matrix Parenthesization" << endl;
    cout << "Time taken: " << time_taken << endl;
    cout << "INPUT: " << input_matrix << ", OUTPUT: " << output_matrix << endl;
    cout << "ANSWER : " << INPUT[0][n-1]  << endl;
    
    delete[] INPUT;

    return 0;
}
