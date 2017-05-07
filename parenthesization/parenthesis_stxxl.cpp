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
#define PARALLEL_ITERATIVE_THRESHOLD 64

/**
 * STXXL Configurations
 */
#define BLOCKS_PER_PAGE     1
#define PAGES_IN_CACHE      3
#define BLOCK_SIZE_IN_BYTES sizeof(unsigned long) * ALLOWED_SIZE_RAM * ALLOWED_SIZE_RAM

typedef stxxl::VECTOR_GENERATOR<unsigned long, BLOCKS_PER_PAGE, PAGES_IN_CACHE,
                                BLOCK_SIZE_IN_BYTES,
                                stxxl::RC, stxxl::lru>::result par_vector_type;

void host_RAM_A_fw( unsigned long *X, uint64_t x_row, uint64_t x_col, uint64_t n);

void host_RAM_B_fw( unsigned long *X, unsigned long *U, unsigned long *V,
uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow,
uint64_t vcol, uint64_t n);

void host_RAM_C_fw( unsigned long *X, unsigned long *U, unsigned long *V,
uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow,
uint64_t vcol, uint64_t n);

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


/*
 * RAM code
 */
void host_RAM_A_fw( unsigned long *X, uint64_t x_row, uint64_t x_col, uint64_t n)


void host_RAM_B_fw( unsigned long *X, unsigned long *U, unsigned long *V,
					uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow,
					uint64_t vcol, uint64_t n) {

}


void host_RAM_C_fw( unsigned long *X, unsigned long *U, unsigned long *V,
					uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow,
					uint64_t vcol, uint64_t n) {

}

/**
 * Read from stxxl vector to RAM vector
 */
void read_to_RAM(par_vector_type& zmatrix, unsigned long *X, uint64_t row, uint64_t col, uint64_t n)
{
    //cout << "Reading from row " << row << " and col " << col << " of size " << n << " from disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = encode2D_to_morton_64bit(row, col);
    uint64_t end_pos = begin_pos + (n*n) - 1;
    for (par_vector_type::const_iterator it = zmatrix.begin() + begin_pos; it != zmatrix.begin() + end_pos + 1; ++it, ++i) {
        X[i] = *it;
    }
}

/**
 * Write RAM data to stxxl vector
 */
void write_to_disk(par_vector_type& zmatrix, unsigned long *X, uint64_t row, uint64_t col, uint64_t n)
{   
    //cout << "Writing from row " << row << " and col " << col << " of size " << n << " to disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = encode2D_to_morton_64bit(row, col);
    uint64_t end_pos = begin_pos + (n*n) - 1;
    for (par_vector_type::iterator it = zmatrix.begin() + begin_pos; it != zmatrix.begin() + end_pos + 1; ++it, ++i) {
        *it = X[i];
    }
}

/*
 * Disk code
 */
void host_disk_A_fw(par_vector_type& zmatrix,
                    uint64_t xrow, uint64_t xcol, uint64_t n)
{

    // Base case - If possible, read the entire array into RAM
    if (n <= ALLOWED_SIZE_RAM) {
        unsigned long *X = new unsigned long[n*n];
        read_to_RAM(zmatrix, X, xrow, xcol, n);
        host_RAM_A_fw(X, xrow, xcol, n);
        write_to_disk(zmatrix, X, xrow, xcol, n);
        delete [] X;
    }     // If not, split into r chunks
    else {
        uint64_t r = n / ALLOWED_SIZE_RAM;
        uint64_t m = n / r;
        //cout << "Splitting disk matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        // Our RAM size is chosen such that it holds upto 3 times the allowed size
        unsigned long *X  = new unsigned long[m*m];
        unsigned long *U = new unsigned long[m*m];
        unsigned long *V = new unsigned long[m*m];

        for (uint64_t k = 0; k < r; k++) {

            // Step 1: A_step - A(X_kk, U_kk, V_kk), X,U,V are the same
            read_to_RAM(zmatrix, X, xrow + (m*k), xcol + (m*k), m);
            host_RAM_A_fw(X, 0, 0, m);
            write_to_disk(zmatrix, X, xrow + (m*k), xcol + (m*k), m);
		}


		for (uint64_t k = 1; k <= r-1; k++) {

			// Step 2a) : C_step - X_ij, U_i(k+i-1), V_(k+i-1)j, where (j - i)
			// ranges from k to min(2k - 2, r - 1)
            for (uint64_t i = 0; i < r; i++) {
				uint64_t end = min((2*k) - 2, r - 1);

				if ((k + i - 1) >= r)	//	Index out out bounds
					break;

				read_to_RAM(zmatrix, U, xrow + (m*i), xcol + m*(k + i - 1), m);
				for (uint64_t j = k + i; (j < end + i) && (j < r); j++) {
                    read_to_RAM(zmatrix, X, xrow + (m*i), xcol + (m*j), m);
					read_to_RAM(zmatrix, V, xrow + (m*(k + i - 1)), xcol + (m*j), m);
                    host_RAM_C_fw(X, U, V, 0, 0, 0, 0, 0, 0, m);
                    write_to_disk(zmatrix, X, xrow + (m*i), xcol + (m*j), m);
				}
            }

			// Step 2b) : C_step - X_ij, U_i(1+i), V_(1+i)j, where (j - i)
			// ranges from k to min(2k - 3, r - 1)
            for (uint64_t i = 0; i < r; i++) {
				uint64_t end = min((2*k) - 3, r - 1);

				if ((1 + i) >= r)		// Index out of bounds
					break;
				read_to_RAM(zmatrix, U, xrow + (m*i), xcol + m*(1 + i), m);
				for (uint64_t j = k + i; (j < end + i) && (j < r); j++) {
                    read_to_RAM(zmatrix, X, xrow + (m*i), xcol + (m*j), m);
					read_to_RAM(zmatrix, V, xrow + (m*(1 + i)), xcol + (m*j), m);
                    host_RAM_C_fw(X, U, V, 0, 0, 0, 0, 0, 0, m);
                    write_to_disk(zmatrix, X, xrow + (m*i), xcol + (m*j), m);
				}
            }


			// Step 3: B_step - X_ij, U_ii, V_jj, where (j - i) = k
            for (uint64_t i = 0; i < r; i++) {
				read_to_RAM(zmatrix, U, xrow + (m*i), xcol + (m*i), m);
				uint64_t j = k + i;
				if (j < r) {
					read_to_RAM(zmatrix, V, xrow + (m*j), xcol + (m*j), m);
					read_to_RAM(zmatrix, X, xrow + (m*i), xcol + (m*j), m);
					host_RAM_B_fw(X, U, V, 0, 0, 0, 0, 0, 0, m);
					write_to_disk(zmatrix, X, xrow + (m*i), xcol + (m*j), m);
				} else {
					break;
				}
            }

		}

        delete [] X;
        delete [] U;
        delete [] V;
	}
}


int main(int argc, char *argv[])
{
    if (argc != 3) {
        cout << "Inadequate parameters, provide input in the format "
                "./parallel_parenthesis <z_morton_input_file> <z_morton_output_file>" << endl;
        exit(1);
    }
    
    par_vector_type zmatrix;
    string inp_filename(argv[1]);
    string op_filename(argv[2]);
    ifstream inp_file(inp_filename);
    ofstream op_file(op_filename);
     
    string line;
    uint64_t full_size = 0;
    unsigned long value = 0;
    op_file << "Reading input file in Z-morton format, " << inp_filename << endl;
    while (getline(inp_file, line)) {
        stringstream ss(line);
        string buf;
        while (ss >> buf) {
            if (buf == "inf")
                value = INFINITY_LENGTH;
            else
                value = stol(buf);
            zmatrix.push_back(value);
            full_size++;
        }
    }
    op_file << "Finished reading input file, full_size=" << full_size << endl;
    inp_file.close();
    

    uint64_t n = sqrt(full_size);

    struct timespec start, finish;
    double time_taken;
    clock_gettime(CLOCK_MONOTONIC, &start);

    host_disk_A_fw(zmatrix, 0, 0, n);

    clock_gettime(CLOCK_MONOTONIC, &finish);
    time_taken  =  finish.tv_sec - start.tv_sec;
    time_taken += (finish.tv_nsec - start.tv_nsec) / 1e9;
 


    cout << "Parallel r-Way R-DP Parenthesization with STXXL" << endl;   
    cout << "Cores used: " << __cilkrts_get_nworkers() << endl;
    cout << "Input file: " << inp_filename << endl;
    cout << "Output file: " << op_filename << endl;
    cout << "Time taken: " << time_taken << endl;

    op_file << "Array after execution in Z-morton vector format: " << endl;
    for (par_vector_type::const_iterator it = zmatrix.begin(); it != zmatrix.end(); ++it)
		op_file << *it << " ";
    op_file << endl;

    op_file.close();

    return 0;
}
