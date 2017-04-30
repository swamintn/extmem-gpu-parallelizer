#include <algorithm>
#include <stxxl/vector>
#include <iostream>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

using namespace std;

int tilesize = pow(2,30);	//	1 GB RAM to be fixed
int size = pow(2,16);		// 1 GB RAM will be occupied by 2^14 * 2^14 matrix,
						// with the given size, there will be 16 such matrices
int floyd[65536][65536];

void A_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n);
void B_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n);
void C_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n);
void D_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n);
void loop_fw(int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n);



void zmorton(int n, int row, int col, int floyd[][size],
stxxl::VECTOR_GENERATOR<unsigned int, 1, 1, 1024*1024*1024, stxxl::RC, stxxl::lru>::result& my_vector) {

	if (n == 1) {
		my_vector.push_back(floyd[row][col]);
		cout << floyd[row][col] << endl;
		return;
	}	
	zmorton(n/2, row, col, floyd, my_vector);
	zmorton(n/2, row, col + (n/2), floyd, my_vector);
	zmorton(n/2, row + (n/2), col, floyd, my_vector);
	zmorton(n/2, row + (n/2), col + (n/2), floyd, my_vector);
	return;
}


void loop_fw(int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n) {
	int Xi = 0, Xj = 0, Ui = 0, Vj = 0, K = 0;
	for (int k = 0; k < n; k++) {
		for (int i = 0; i < n; i++) {
			for (int j = 0; j < n; j++) {
				Xi = xrow + i;
				Xj = xcol + j;
				Ui = urow + i;
				K =  ucol + k;
				Vj = vcol + j;
				floyd[Xi][Xj] = min(floyd[Xi][Xj], floyd[Ui][K] + floyd[K][Vj]);
			}
		}
	}	
	return;
}


void A_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n) {

	/*
	 * Every n x n size matrix is split into r x r submatrices of 
	 * size n/r x n/r each
	 */
	int r = (4 * (n * n)/ tilesize);	//	Multiplied by 4 to account for int
	r = sqrt(r);

	if (r <= 1)
		loop_fw(xrow, xcol, urow, ucol, vrow, vcol, n);
	else {
		for (int k = 0; k < r; k++) {
			A_loop(xrow + k, xcol + k, urow + k, ucol + k, vrow + k, vcol + k, n/r);
			cilk_for (int i = 0; i < r; i++) {
				if (i != k) {
					cilk_spawn B_loop(xrow + k, xcol + i, urow + k, ucol + k, vrow + k, vcol + i, n/r);
								C_loop(xrow + i, xcol + k, urow + i, ucol + k, vrow + k, vcol + k, n/r);
					cilk_sync;
				}
			}
		
			cilk_for(int i = 0; i < r; i++) {
				cilk_for(int j = 0; j < r; j++) {
					if ((i != k) && (j != k)) {
						D_loop(xrow + i, xcol + j, urow + i, ucol + k, vrow + k, vcol + j, n/r);
					}
				}
			}
		}
	}
}


void B_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n) {

	int r = (4 * (n * n)/ tilesize);	//	Multiplied by 4 to account for int
	r = sqrt(r);

	if (r <= 1)
		loop_fw(xrow, xcol, urow, ucol, vrow, vcol, n);
	else {
		for (int k = 0; k < r; k++) {
			cilk_for (int j = 0; j < r; j++)
				B_loop(xrow + k, xcol + j, urow + k, ucol + k, vrow + k, vcol + j, n/r);
				
			cilk_for(int i = 0; i < r; i++) {
				cilk_for(int j = 0; j < r; j++) {
					if (i != k) {
						D_loop(xrow + i, xcol + j, urow + i, ucol + k, vrow + k, vcol + j, n/r);
					}
				}
			}
		}
	}
}

void C_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n) {

	int r = (4 * (n * n)/ tilesize);	//	Multiplied by 4 to account for int
	r = sqrt(r);

	if (r <= 1)
		loop_fw(xrow, xcol, urow, ucol, vrow, vcol, n);
	else {
		for (int k = 0; k < r; k++) {
			cilk_for (int i = 0; i < r; i++)
				C_loop(xrow + i, xcol + k, urow + i, ucol + k, vrow + k, vcol + k, n/r);
				
			cilk_for(int i = 0; i < r; i++) {
				cilk_for(int j = 0; j < r; j++) {
					if (j != k) {
						D_loop(xrow + i, xcol + j, urow + i, ucol + k, vrow + k, vcol + j, n/r);
					}
				}
			}
		}
	}
}


void D_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n) {

	int r = (4 * (n * n)/ tilesize);	//	Multiplied by 4 to account for int
	r = sqrt(r);

	if (r <= 1)
		loop_fw(xrow, xcol, urow, ucol, vrow, vcol, n);
	else {
		for (int k = 0; k < r; k++) {
			cilk_for (int i = 0; i < r; i++)
				cilk_for (int j = 0; j < r; j++)
					D_loop(xrow + i, xcol + j, urow + i, ucol + k, vrow + k, vcol + j, n/r);
		}
	}
}

int main() {

	typedef stxxl::VECTOR_GENERATOR<unsigned int, 1, 1, 1024*1024*1024, stxxl::RC, stxxl::lru>::result vector;
	vector my_vector;
	int row = 0, col = 0, result = 0;

	for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            floyd[i][j] = 1;
        }   
    }   

	cout << "Hello World\n";	
	zmorton(size, row, col, floyd, my_vector);
	cout << "ZMorton done\n";	
	// Start function
	A_loop(0,0, 0,0, 0,0, size);
	cout << "FLOYD done\n";	
	return 0;
}
