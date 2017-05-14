#include <fstream>
#include <iostream>
#include <limits>
#include <ctime>
#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <stxxl/vector>
#include "mm_gpu_common.h"

using namespace std;

/**
 * STXXL Configurations
 */
#define BLOCKS_PER_PAGE     1
#define PAGES_IN_CACHE      3
#define BLOCK_SIZE_IN_BYTES sizeof(long) * ALLOWED_SIZE_RAM * ALLOWED_SIZE_RAM // 32 * 3 MB

typedef stxxl::VECTOR_GENERATOR<long, BLOCKS_PER_PAGE, PAGES_IN_CACHE,
                                BLOCK_SIZE_IN_BYTES,
                                stxxl::RC, stxxl::lru>::result mm_vector_type;

/** 
 * Encoding and decoding Morton codes
 * (Taken from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/)
 */
uint64_t host_encode2D_to_morton_64bit(uint64_t x, uint64_t y)
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
 * Find log of a power of 2 number
 */
unsigned int log_base_2(unsigned int power_of_2) {
    unsigned int ans = 0;
    while (power_of_2 >>= 1) ans++;
    return ans;
}


/**
 * Read from stxxl vector to RAM vector
 */
void read_to_RAM(mm_vector_type& zmatrix, long *X, uint64_t row, uint64_t col, uint64_t n)
{
    // DEBUG: cout << "Reading from row " << row << " and col " << col << " of size " << n << " from disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = host_encode2D_to_morton_64bit(row, col);
    uint64_t end_pos = begin_pos + (n*n) - 1;
    for (mm_vector_type::const_iterator it = zmatrix.begin() + begin_pos; it != zmatrix.begin() + end_pos + 1; ++it, ++i) {
        X[i] = *it;
    }
}

/**
 * Write RAM data to stxxl vector
 */
void write_to_disk(mm_vector_type& zmatrix, long *X, uint64_t row, uint64_t col, uint64_t n)
{   
    // DEBUG: cout << "Writing from row " << row << " and col " << col << " of size " << n << " to disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = host_encode2D_to_morton_64bit(row, col);
    uint64_t end_pos = begin_pos + (n*n) - 1;
    for (mm_vector_type::iterator it = zmatrix.begin() + begin_pos; it != zmatrix.begin() + end_pos + 1; ++it, ++i) {
        *it = X[i];
    }
}

/*
 * Disk code
 */
void host_disk_A_mm(mm_vector_type& zmatrix_X, mm_vector_type& zmatrix_U, mm_vector_type& zmatrix_V,
                    uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol,
                    uint64_t n)
{
    // Base case - If possible, read the entire array into RAM
    if (n <= ALLOWED_SIZE_RAM) {
        long *X = (long *)mallocCudaHostMemory(n*n*sizeof(long), HOST_MEMORY_TYPE);
        long *U = (long *)mallocCudaHostMemory(n*n*sizeof(long), HOST_MEMORY_TYPE);
        long *V = (long *)mallocCudaHostMemory(n*n*sizeof(long), HOST_MEMORY_TYPE);
        read_to_RAM(zmatrix_X, X, xrow, xcol, n);
        read_to_RAM(zmatrix_U, U, urow, ucol, n);
        read_to_RAM(zmatrix_V, V, vrow, vcol, n);
        host_RAM_A_mm(X, U, V, 0, 0, 0, 0, 0, 0, n);
        write_to_disk(zmatrix_X, X, xrow, xcol, n);
        freeCudaHostMemory((void *)X,  HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *)U,  HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *)V,  HOST_MEMORY_TYPE);
    }
    // If not, split into r chunks
    else {
        uint64_t r = n / ALLOWED_SIZE_RAM;
        uint64_t m = n / r;
        // DEBUG: cout << "Splitting disk matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        // Our RAM size is chosen such that it holds upto 3 times the allowed size
        long *X  = (long *)mallocCudaHostMemory(m*m*sizeof(long), HOST_MEMORY_TYPE);
        long *U = (long *)mallocCudaHostMemory(m*m*sizeof(long), HOST_MEMORY_TYPE);
        long *V = (long *)mallocCudaHostMemory(m*m*sizeof(long), HOST_MEMORY_TYPE);

        for (uint64_t k = 0; k < r; k++) {
            for (uint64_t i = 0; i < r; i++) {
               // U_ik is same for all j
               read_to_RAM(zmatrix_U, U, urow + (m*i), ucol + (m*k), m);
               for (uint64_t j = 0; j < r; j++) { 
                   read_to_RAM(zmatrix_V, V, vrow + (m*k), vcol + m*j, m);
                   read_to_RAM(zmatrix_X, X, xrow + (m*i), xcol + m*j, m);
                   host_RAM_A_mm(X, U, V, 0, 0, 0, 0, 0, 0, m);
                   write_to_disk(zmatrix_X, X, xrow + (m*i), xcol + m*j, m);
                }
            }

        }
        freeCudaHostMemory((void *)X,  HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *)U, HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *)V, HOST_MEMORY_TYPE);
    }
}


int main(int argc, char *argv[])
{
    if (argc != 4) {
        cout << "Inadequate parameters, provide input in the format "
                "./mm_gpu <z_morton_U_input_file> <z_morton_V_input_file> <z_morton_X_output_file>" << endl;
        exit(1);
    }
    
    mm_vector_type zmatrix_U;
    mm_vector_type zmatrix_V;
    mm_vector_type zmatrix_X;
    string inp_filename1(argv[1]);
    string inp_filename2(argv[2]);
    string outp_filename(argv[3]);
    
    ifstream inp_file1(inp_filename1); 
    string line;
    uint64_t full_size = 0;
    long value = 0;
    cout << "Reading input file, " << inp_filename1 << endl;
    while (getline(inp_file1, line)) {
        stringstream ss(line);
        string buf;
        while (ss >> buf) {
            value = stol(buf);
            zmatrix_U.push_back(value);
            full_size++;
        }
    }
    cout << "Finished reading input file, full_size=" << full_size << endl;
    inp_file1.close();

    ifstream inp_file2(inp_filename2);
    cout << "Reading input file, " << inp_filename2 << endl;
    full_size = 0;
    while (getline(inp_file2, line)) {
        stringstream ss(line);
        string buf;
        while (ss >> buf) {
            value = stol(buf);
            zmatrix_V.push_back(value);
            full_size++;
        }
    }
    cout << "Finished reading input file, full_size=" << full_size << endl;
    inp_file2.close();
    
    cout << "Initializing output matrix with zeros..." << endl;
    for (uint64_t i = 0; i < full_size; i++)
        zmatrix_X.push_back(0);    

    uint64_t n = sqrt(full_size);

    struct timespec start, finish;
    double time_taken;

    clock_gettime(CLOCK_MONOTONIC, &start);
    host_disk_A_mm(zmatrix_X, zmatrix_U, zmatrix_V, 0, 0, 0, 0, 0, 0, n);
    clock_gettime(CLOCK_MONOTONIC, &finish);

    time_taken  =  finish.tv_sec - start.tv_sec;
    time_taken += (finish.tv_nsec - start.tv_nsec) / 1e9;
    
    cout << "Parallel r-Way R-DP Matrix Multiplication using GPUs with STXXL" << endl;
    cout << "Input matrix size: " << "2^" << log_base_2(n) << ", " << n << endl;
    cout << "RAM matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_RAM) << ", " << ALLOWED_SIZE_RAM << endl;
    cout << "GPU global matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_GPU_GLOBAL) << ", " << ALLOWED_SIZE_GPU_GLOBAL << endl;
    cout << "GPU shared matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_GPU_SHARED) << ", " << ALLOWED_SIZE_GPU_SHARED << endl;
    cout << "Input file, U: " << inp_filename1 << endl;
    cout << "Input file, V: " << inp_filename2 << endl;
    cout << "Output file, X: " << outp_filename << endl;
    cout << "Time taken: " << time_taken << endl;

    ofstream outp_file(outp_filename);
    for (mm_vector_type::const_iterator it = zmatrix_X.begin(); it != zmatrix_X.end(); ++it) {
        if (it != zmatrix_X.begin())
            outp_file << " ";
        outp_file << *it;
    }
    outp_file << "\n";
    outp_file.close();

    return 0;
}
