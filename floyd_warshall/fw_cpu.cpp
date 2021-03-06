#include <limits>
#include <ctime>
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

/**
 * Different memory hierarchy configurations
 */
#define ALLOWED_SIZE_RAM (1 << 11)
#define INFINITY_LENGTH (1 << 20)
#define PARALLEL_ITERATIVE_THRESHOLD 32

/**
 * STXXL Configurations
 */
#define BLOCKS_PER_PAGE     1
#define PAGES_IN_CACHE      3
#define BLOCK_SIZE_IN_BYTES sizeof(unsigned long) * ALLOWED_SIZE_RAM * ALLOWED_SIZE_RAM

typedef stxxl::VECTOR_GENERATOR<unsigned long, BLOCKS_PER_PAGE, PAGES_IN_CACHE,
                                BLOCK_SIZE_IN_BYTES,
                                stxxl::RC, stxxl::lru>::result fw_vector_type;

void host_RAM_A_fw( unsigned long *X, uint64_t xrow, uint64_t xcol, uint64_t n);

void host_RAM_B_fw( unsigned long *X, unsigned long *U, uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n);

void host_RAM_C_fw( unsigned long *X, unsigned long *V, uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n);

void host_RAM_D_fw( unsigned long *X, unsigned long *U, unsigned long *V, uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n);
/** 
 * Encoding and decoding Morton codes
 * (Taken from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/)
 */
uint64_t encode2D_to_morton_64bit(uint64_t x, uint64_t y)
{
    x &= 0x00000000ffffffff;
    x = (x ^ (x <<  16)) & 0x0000ffff0000ffff;
    x = (x ^ (x <<  8))  & 0x00ff00ff00ff00ff;
    x = (x ^ (x <<  4))  & 0x0f0f0f0f0f0f0f0f;
    x = (x ^ (x <<  2))  & 0x3333333333333333;
    x = (x ^ (x <<  1))  & 0x5555555555555555;

    y &= 0x00000000ffffffff;
    y = (y ^ (y <<  16)) & 0x0000ffff0000ffff;
    y = (y ^ (y <<  8))  & 0x00ff00ff00ff00ff;
    y = (y ^ (y <<  4))  & 0x0f0f0f0f0f0f0f0f;
    y = (y ^ (y <<  2))  & 0x3333333333333333;
    y = (y ^ (y <<  1))  & 0x5555555555555555;

    // This will return row-major order z ordering. If we switch x and y, it will be column-major
    return (x << 1) | y;
}

/**
 * Serial Floyd-Warshall's algorithm
 */
void serial_fw(unsigned long *X, unsigned long *U, unsigned long *V,
               uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol,
               uint64_t n)
{
    for (uint64_t k = 0; k < n; k++) {
        cilk_for (uint64_t i = 0; i < n; i++) {
            cilk_for (uint64_t j = 0; j < n; j++) {
                uint64_t Xi = xrow + i; uint64_t Xj = xcol + j;
                uint64_t Ui = urow + i; uint64_t Vj = vcol + j;
                uint64_t K  = ucol + k;
                uint64_t cur = encode2D_to_morton_64bit(Xi, Xj);
                uint64_t first = encode2D_to_morton_64bit(Ui, K);
                uint64_t second = encode2D_to_morton_64bit(K, Vj);
                X[cur] = min(X[cur], (U[first] + V[second]));
            }
        }
    }
}

/**
 * RAM code
 */
void host_RAM_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n)
{
	/*
	 * Every n x n size matrix is split into r x r submatrices of 
	 * size n/r x n/r each
	 */
	//int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (n <= PARALLEL_ITERATIVE_THRESHOLD)
		serial_fw(X, X, X, xrow, xcol, xrow, xcol, xrow, xcol, n);
	else {
		host_RAM_A_fw(X, xrow, xcol, (n/2));
		cilk_spawn host_RAM_B_fw(X, X, xrow, xcol + (n/2), xrow, xcol, (n/2));
			   host_RAM_C_fw(X, X, xrow + (n/2), xcol, xrow, xcol, (n/2));
		cilk_sync;

		host_RAM_D_fw(X, X, X, xrow + (n/2), xcol + (n/2), xrow + (n/2), xcol, xrow, xcol + (n/2), (n/2));
		
		
		host_RAM_A_fw(X, xrow + (n/2), xcol + (n/2), (n/2));
		cilk_spawn host_RAM_B_fw(X, X, xrow + (n/2), xcol, xrow + (n/2), xcol + (n/2), (n/2));
			   host_RAM_C_fw(X, X, xrow, xcol + (n/2), xrow + (n/2), xcol + (n/2), (n/2));
		cilk_sync;

		host_RAM_D_fw(X, X, X, xrow, xcol, xrow, xcol + (n/2), xrow + (n/2), xcol, (n/2));

	}
}

void host_RAM_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n)
{
	//int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (n <= PARALLEL_ITERATIVE_THRESHOLD)
		serial_fw(X, U, X, xrow, xcol, urow, ucol, xrow, xcol, n);
	else {
		cilk_spawn host_RAM_B_fw(X, U, xrow, xcol, urow, ucol, (n/2));
			   host_RAM_B_fw(X, U, xrow, xcol + (n/2), urow, ucol, (n/2));
		cilk_sync;
	
		cilk_spawn host_RAM_D_fw(X, U, X, xrow + (n/2), xcol, urow + (n/2), ucol, xrow, xcol, (n/2));
			   host_RAM_D_fw(X, U, X, xrow + (n/2), xcol + (n/2), urow + (n/2), ucol, xrow, xcol + (n/2), (n/2));
		cilk_sync;

		cilk_spawn host_RAM_B_fw(X, U, xrow + (n/2), xcol, urow + (n/2), ucol + (n/2), (n/2));
			   host_RAM_B_fw(X, U, xrow + (n/2), xcol + (n/2), urow + (n/2), ucol + (n/2), (n/2));
		cilk_sync;
	
		cilk_spawn host_RAM_D_fw(X, U, X, xrow, xcol, urow, ucol + (n/2), xrow + (n/2), xcol, (n/2));
			   host_RAM_D_fw(X, U, X, xrow, xcol + (n/2), urow, ucol + (n/2), xrow + (n/2), xcol + (n/2), (n/2));
		cilk_sync;
		
	}
}

void host_RAM_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
	//int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (n <= PARALLEL_ITERATIVE_THRESHOLD)
		serial_fw(X, X, V, xrow, xcol, xrow, xcol, vrow, vcol, n);
	else {
		cilk_spawn host_RAM_C_fw(X, V, xrow, xcol, vrow, vcol, (n/2));
			   host_RAM_C_fw(X, V, xrow + (n/2), xcol, vrow, vcol, (n/2));
		cilk_sync;

		cilk_spawn host_RAM_D_fw(X, X, V, xrow, xcol + (n/2), xrow, xcol, vrow, vcol + (n/2), (n/2));
			   host_RAM_D_fw(X, X, V, xrow + (n/2), xcol + (n/2), xrow + (n/2), xcol, vrow, vcol + (n/2), (n/2));
		cilk_sync;

		cilk_spawn host_RAM_C_fw(X, V, xrow, xcol + (n/2), vrow + (n/2), vcol + (n/2), (n/2));
			   host_RAM_C_fw(X, V, xrow + (n/2), xcol + (n/2), vrow + (n/2), vcol + (n/2), (n/2));
		cilk_sync;

		cilk_spawn host_RAM_D_fw(X, X, V, xrow, xcol, xrow, xcol + (n/2), vrow + (n/2), vcol, (n/2));
			   host_RAM_D_fw(X, X, V, xrow + (n/2), xcol, xrow + (n/2), xcol + (n/2), vrow + (n/2), vcol, (n/2));
		cilk_sync;
		
	}
}

void host_RAM_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
	//int r = SIZE_OF_LONG * (n * n);	//	Multiplied by 4 to account for int

	if (n <= PARALLEL_ITERATIVE_THRESHOLD)
		serial_fw(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
	else {
		cilk_spawn host_RAM_D_fw(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, (n/2));
		cilk_spawn host_RAM_D_fw(X, U, V, xrow, xcol + (n/2), urow, ucol, vrow, vcol + (n/2), (n/2));
		cilk_spawn host_RAM_D_fw(X, U, V, xrow + (n/2), xcol, urow + (n/2), ucol, vrow, vcol, (n/2));
			   host_RAM_D_fw(X, U, V, xrow + (n/2), xcol + (n/2), urow + (n/2), ucol, vrow, vcol + (n/2), (n/2));
		cilk_sync;

		cilk_spawn host_RAM_D_fw(X, U, V, xrow, xcol, urow, ucol + (n/2), vrow + (n/2), vcol, (n/2));
		cilk_spawn host_RAM_D_fw(X, U, V, xrow, xcol + (n/2), urow, ucol + (n/2), vrow + (n/2), vcol + (n/2), (n/2));
		cilk_spawn host_RAM_D_fw(X, U, V, xrow + (n/2), xcol, urow + (n/2), ucol + (n/2), vrow + (n/2), vcol, (n/2));
			   host_RAM_D_fw(X, U, V, xrow + (n/2), xcol + (n/2), urow + (n/2), ucol + (n/2), vrow + (n/2), vcol + (n/2), (n/2));
		cilk_sync;
	}
}


/**
 * Read from stxxl vector to RAM vector
 */
void read_to_RAM(fw_vector_type& zfloyd, unsigned long *X, uint64_t row, uint64_t col, uint64_t n)
{
    //cout << "Reading from row " << row << " and col " << col << " of size " << n << " from disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = encode2D_to_morton_64bit(row, col);
    uint64_t end_pos = begin_pos + (n*n) - 1;
    for (fw_vector_type::const_iterator it = zfloyd.begin() + begin_pos; it != zfloyd.begin() + end_pos + 1; ++it, ++i) {
        X[i] = *it;
    }
}

/**
 * Write RAM data to stxxl vector
 */
void write_to_disk(fw_vector_type& zfloyd, unsigned long *X, uint64_t row, uint64_t col, uint64_t n)
{   
    //cout << "Writing from row " << row << " and col " << col << " of size " << n << " to disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = encode2D_to_morton_64bit(row, col);
    uint64_t end_pos = begin_pos + (n*n) - 1;
    for (fw_vector_type::iterator it = zfloyd.begin() + begin_pos; it != zfloyd.begin() + end_pos + 1; ++it, ++i) {
        *it = X[i];
    }
}

/*
 * Disk code
 */
void host_disk_A_fw(fw_vector_type& zfloyd,
                    uint64_t xrow, uint64_t xcol, uint64_t n)
{
    // Base case - If possible, read the entire array into RAM
    if (n <= ALLOWED_SIZE_RAM) {
        unsigned long *X = new unsigned long[n*n];
        read_to_RAM(zfloyd, X, xrow, xcol, n);
        host_RAM_A_fw(X, xrow, xcol, n);
        write_to_disk(zfloyd, X, xrow, xcol, n);
        delete [] X;
    }
    // If not, split into r chunks
    else {
        uint64_t r = n / ALLOWED_SIZE_RAM;
        uint64_t m = n / r;
        //cout << "Splitting disk matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        // Our RAM size is chosen such that it holds upto 3 times the allowed size
        unsigned long *W  = new unsigned long[m*m];
        unsigned long *R1 = new unsigned long[m*m];
        unsigned long *R2 = new unsigned long[m*m];

        for (uint64_t k = 0; k < r; k++) {
            //cout << "k " << k << endl;

            // Step 1: A_step - A(X_kk, U_kk, V_kk), X,U,V are the same
            //cout << "A\n";
            read_to_RAM(zfloyd, W, xrow + (m*k), xcol + m*k, m);
            host_RAM_A_fw(W, 0, 0, m);

		// Avoiding the 2 unnecessary "write to disk and read again without any intermediate operations"
            //write_to_disk(zfloyd, W, xrow + (m*k), xcol + m*k, m);

            // Step 2: B_C_step - B(X_kj, U_kk, V_kj), C(X_ik, U_ik, V_kk)
            // Note that B's U_kk and C's V_kk are the same
            // For B, X and V are the same, for C, X and U are the same
            //cout << "B\n";
            //read_to_RAM(zfloyd, R1, xrow + (m*k), xcol + m*k, m);

	    // We have to simulate reading from R1, 
            unsigned long *temp = W;
	    W = R1;
	    R1 = temp;
            for (uint64_t j = 0; j < r; j++) {
                if (j != k) {
                    // DEBUG: cout << "Bj " << j << endl;
                    read_to_RAM(zfloyd, W, xrow + (m*k), xcol + m*j, m);
                    host_RAM_B_fw(W, R1, 0, 0, 0, 0, m);
                    write_to_disk(zfloyd, W, xrow + (m*k), xcol + m*j, m);
                }
            }
            //cout << "C\n";
            for (uint64_t i = 0; i < r; i++) {
                if (i != k) {
                    // DEBUG: cout << "Ci " << i << endl;
                    read_to_RAM(zfloyd, W, xrow + (m*i), xcol + m*k, m);
                    host_RAM_C_fw(W, R1, 0, 0, 0, 0, m);
                    write_to_disk(zfloyd, W, xrow + (m*i), xcol + m*k, m);
                }
            }
		// Since, W had been swapped with R1 and we actually have to write to W
            write_to_disk(zfloyd, R1, xrow + (m*k), xcol + m*k, m);
            // Step 3: D_step - D(X_ij, U_ik, V_kj)
            //cout << "D\n";
            for (uint64_t i = 0; i < r; i++) {
                if (i != k) {
                    // U_ik is same for all j
                    read_to_RAM(zfloyd, R1, xrow + (m*i), xcol + m*k, m);
                    for (uint64_t j = 0; j < r; j++) {
                        if (j != k) {
                            // DEBUG: cout << "Di " << i << ", Dj " << j << endl; 
                            read_to_RAM(zfloyd, R2, xrow + (m*k), xcol + m*j, m);
                            read_to_RAM(zfloyd, W, xrow + (m*i), xcol + m*j, m);
                            host_RAM_D_fw(W, R1, R2, 0, 0, 0, 0, 0, 0, m);
                            write_to_disk(zfloyd, W, xrow + (m*i), xcol + m*j, m);
                        }
                    }
                }
            }

        }

        delete [] W;
        delete [] R1;
        delete [] R2;
    }
}


int main(int argc, char *argv[])
{
    if (argc != 3) {
        cout << "Inadequate parameters, provide input in the format "
                "./fw_gpu <z_morton_input_file> <z_morton_output_file>" << endl;
        exit(1);
    }
    
    fw_vector_type zfloyd;
    string inp_filename(argv[1]);
    string op_filename(argv[2]);
    ifstream inp_file(inp_filename);
    ofstream op_file(op_filename);
     
    string line;
    uint64_t full_size = 0;
    unsigned long value = 0;
    op_file << "Reading input file, " << inp_filename << endl;
    while (getline(inp_file, line)) {
        stringstream ss(line);
        string buf;
        while (ss >> buf) {
            if (buf == "inf")
                value = INFINITY_LENGTH;
            else
                value = stol(buf);
            zfloyd.push_back(value);
            full_size++;
        }
    }
    op_file << "Finished reading input file, full_size=" << full_size << endl;
    inp_file.close();
    
    /*
    op_file << "Array before execution in Z-morton vector format: " << endl;
    for (fw_vector_type::const_iterator it = zfloyd.begin(); it != zfloyd.end(); ++it)
        op_file << *it << " ";
    op_file << endl;
    */

    uint64_t n = sqrt(full_size);

    struct timespec start, finish;
    double time_taken;
    clock_gettime(CLOCK_MONOTONIC, &start);

    host_disk_A_fw(zfloyd, 0, 0, n);

    clock_gettime(CLOCK_MONOTONIC, &finish);
    time_taken  =  finish.tv_sec - start.tv_sec;
    time_taken += (finish.tv_nsec - start.tv_nsec) / 1e9;
 


    cout << "Parallel r-Way R-DP Floyd-Warshall with STXXL" << endl;   
    cout << "Cores used: " << __cilkrts_get_nworkers() << endl;
    cout << "Input file: " << inp_filename << endl;
    cout << "Output file: " << op_filename << endl;
    cout << "Time taken: " << time_taken << endl;

    op_file << "Array after execution in Z-morton vector format: " << endl;
    for (fw_vector_type::const_iterator it = zfloyd.begin(); it != zfloyd.end(); ++it)
		op_file << *it << " ";
    op_file << endl;

    /*
    op_file << "Array in matrix format " << endl;
    for (uint64_t i = 0; i < n; i++) {
	for (uint64_t j = 0; j < n; j++) {
	    if (j != 0)
		op_file << " ";
	    op_file << zfloyd[encode2D_to_morton_64bit(i,j)];
	}
	op_file << endl;
    }
    */
    
    op_file.close();

    return 0;
}
