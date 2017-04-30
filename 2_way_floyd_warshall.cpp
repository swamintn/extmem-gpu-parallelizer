#include <algorithm>
#include <stxxl/vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

using namespace std;

#define TWO_POWER 11
#define SIZE (1 << TWO_POWER)
#define SIZE_OF_LONG 8
#define MEMORY (1 << 25)

int tilesize = MEMORY;	//	2 MB RAM to be fixed
//int size = pow(2,11);		// 1 MB RAM will be occupied by a 2^9 * 2^9 matrix,
							// with the given size, there will be 16 such matrices
int counter = 0;

void A_loop( int xrow, int xcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd);

void B_loop( int xrow, int xcol, int urow, int ucol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd);

void C_loop( int xrow, int xcol, int vrow, int vcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd);

void D_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd);

void loop_fw(int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd);





//   Taken from https://fgiesen.wordpress.com/2009/ 12/13/decoding-morton-codes/

uint32_t encode2D_to_morton_32bit(uint32_t x, uint32_t y)
{
    x &= 0x0000ffff;                  // x = ---- ---- ---- ---- fedc ba98 7654 3210
    x = (x ^ (x <<  8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x <<  4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x <<  2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x <<  1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0

    y &= 0x0000ffff;                  // x = ---- ---- ---- ---- fedc ba98 7654 3210
    y = (y ^ (y <<  8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    y = (y ^ (y <<  4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    y = (y ^ (y <<  2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    y = (y ^ (y <<  1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    
    // This will return row-major order z ordering. If we switch x and y, it
    // will be column-major 
    return (x << 1) | y;
}





void loop_fw(int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd) {
	int Xi = 0, Xj = 0, Ui = 0, Vj = 0, K = 0, cur = 0, first = 0, second = 0;
	for (int k = 0; k < n; k++) {
		cilk_for (int i = 0; i < n; i++) {
			cilk_for (int j = 0; j < n; j++) {
				Xi = xrow + i;
				Xj = xcol + j;
				Ui = urow + i;
				K =  ucol + k;
				Vj = vcol + j;
				cur = encode2D_to_morton_32bit(Xi, Xj);
				first = encode2D_to_morton_32bit(Ui, k);
				second = encode2D_to_morton_32bit(K, Vj);
				//floyd[Xi][Xj] = min(floyd[Xi][Xj], floyd[Ui][K] + floyd[K][Vj]);
				floyd[cur] = min(floyd[cur], floyd[first] + floyd[second]);
			}
		}
	}	
	return;
}


void A_loop( int xrow, int xcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd) {

	/*
	 * Every n x n size matrix is split into r x r submatrices of 
	 * size n/r x n/r each
	 */
	int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (r <= tilesize)
		loop_fw(xrow, xcol, xrow, xcol, xrow, xcol, n, floyd);
	else {
		A_loop(xrow, xcol, (n/2), floyd);
		cilk_spawn B_loop(xrow, xcol + (n/2), xrow, xcol, (n/2), floyd);
					C_loop(xrow + (n/2), xcol, xrow, xcol, (n/2), floyd);
		cilk_sync;

		D_loop(xrow + (n/2), xcol + (n/2), xrow + (n/2), xcol, xrow, xcol + (n/2), (n/2), floyd);
		
		
		A_loop(xrow + (n/2), xcol + (n/2), (n/2), floyd);
		cilk_spawn B_loop(xrow + (n/2), xcol, xrow + (n/2), xcol + (n/2), (n/2), floyd);
					C_loop(xrow, xcol + (n/2), xrow + (n/2), xcol + (n/2), (n/2), floyd);
		cilk_sync;

		D_loop(xrow, xcol, xrow, xcol + (n/2), xrow + (n/2), xcol, (n/2), floyd);

	}
}


void B_loop( int xrow, int xcol, int urow, int ucol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd) {

	int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (r <= tilesize)
		loop_fw(xrow, xcol, urow, ucol, xrow, xcol, n, floyd);
	else {
		cilk_spawn B_loop(xrow, xcol, urow, ucol, (n/2), floyd);
					B_loop(xrow, xcol + (n/2), urow, ucol, (n/2), floyd);
		cilk_sync;
	
		cilk_spawn D_loop(xrow + (n/2), xcol, urow + (n/2), ucol, xrow, xcol, (n/2), floyd);
					D_loop(xrow + (n/2), xcol + (n/2), urow + (n/2), ucol, xrow, xcol + (n/2), (n/2), floyd);
		cilk_sync;

		cilk_spawn B_loop(xrow + (n/2), xcol, urow + (n/2), ucol + (n/2), (n/2), floyd);
					B_loop(xrow + (n/2), xcol + (n/2), urow + (n/2), ucol + (n/2), (n/2), floyd);
		cilk_sync;
	
		cilk_spawn D_loop(xrow, xcol, urow, ucol + (n/2), xrow + (n/2), xcol, (n/2), floyd);
					D_loop(xrow, xcol + (n/2), urow, ucol + (n/2), xrow + (n/2), xcol + (n/2), (n/2), floyd);
		cilk_sync;
		
	}
}

void C_loop( int xrow, int xcol, int vrow, int vcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd) {

	int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (r <= tilesize)
		loop_fw(xrow, xcol, xrow, xcol, vrow, vcol, n, floyd);
	else {
		cilk_spawn C_loop(xrow, xcol, vrow, vcol, (n/2), floyd);
					C_loop(xrow + (n/2), xcol, vrow, vcol, (n/2), floyd);
		cilk_sync;

		cilk_spawn D_loop(xrow, xcol + (n/2), xrow, xcol, vrow, vcol + (n/2), (n/2), floyd);
					D_loop(xrow + (n/2), xcol + (n/2), xrow + (n/2), xcol, vrow, vcol + (n/2), (n/2), floyd);
		cilk_sync;

		cilk_spawn C_loop(xrow, xcol + (n/2), vrow + (n/2), vcol + (n/2), (n/2), floyd);
					C_loop(xrow + (n/2), xcol + (n/2), vrow + (n/2), vcol + (n/2), (n/2), floyd);
		cilk_sync;

		cilk_spawn D_loop(xrow, xcol, xrow, xcol + (n/2), vrow + (n/2), vcol, (n/2), floyd);
					D_loop(xrow + (n/2), xcol, xrow + (n/2), xcol + (n/2), vrow + (n/2), vcol, (n/2), floyd);
		cilk_sync;
		
	}
}


void D_loop( int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n,
stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC,
stxxl::lru>::result& floyd) {

	int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (r <= tilesize)
		loop_fw(xrow, xcol, urow, ucol, vrow, vcol, n, floyd);
	else {
		cilk_spawn D_loop(xrow, xcol, urow, ucol, vrow, vcol, (n/2), floyd);
					D_loop(xrow, xcol + (n/2), urow, ucol, vrow, vcol + (n/2), (n/2), floyd);
					D_loop(xrow + (n/2), xcol, urow + (n/2), ucol, vrow, vcol, (n/2), floyd);
					D_loop(xrow + (n/2), xcol + (n/2), urow + (n/2), ucol, vrow, vcol + (n/2), (n/2), floyd);
		cilk_sync;

		cilk_spawn D_loop(xrow, xcol, urow, ucol + (n/2), vrow + (n/2), vcol, (n/2), floyd);
					D_loop(xrow, xcol + (n/2), urow, ucol + (n/2), vrow + (n/2), vcol + (n/2), (n/2), floyd);
					D_loop(xrow + (n/2), xcol, urow + (n/2), ucol + (n/2), vrow + (n/2), vcol, (n/2), floyd);
					D_loop(xrow + (n/2), xcol + (n/2), urow + (n/2), ucol + (n/2), vrow + (n/2), vcol + (n/2), (n/2), floyd);
		cilk_sync;
	}
}

int main(int argc, char *argv[]) {

	if (argc != 3) {
		cout << "Inadequate parameters, provide input in the format "
				"./a.out <input_file> <output_file>" << endl;
		exit(1);
	}

	typedef stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, MEMORY, stxxl::RC, stxxl::lru>::result vector;
	vector floyd;
	int row = 0, col = 0, index = 0;
	long result = 0;
	
	string ipfile_name(argv[1]);
	string opfile_name(argv[2]);

	ifstream ipfile(ipfile_name);
	ofstream opfile(opfile_name);

	string line;

	cout << "START INPUT IN Z-MORTON LAYOUT" << endl;
	while (getline(ipfile,line)) {
		stringstream ss(line);
		string buf;
		while (ss >> buf && index < (SIZE * SIZE)) {
			result = stol(buf);
			floyd.push_back(result);
			index++;
		}	
	}
	cout << "FINISHED INPUT IN Z-MORTON LAYOUT : " << index << endl;
	// Start function
	A_loop(0,0, SIZE, floyd);
	cout << "FLOYD done\n";	

	for (int i = 0; i < (SIZE); i++) {
		for (int j = 0; j < (SIZE); j++) {
			if (j != 0) {
				opfile << " ";
			}
			opfile << floyd[encode2D_to_morton_32bit(i,j)];
		}
		opfile << endl;
	}

	ipfile.close();
	opfile.close();
	return 0;
}
