#include <fstream>
#include <iostream>
#include <limits>
#include <ctime>
#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <stxxl/vector>
#include "fw_gpu_common.h"

using namespace std;

/**
 * STXXL Configurations
 */
#define BLOCKS_PER_PAGE     1
#define PAGES_IN_CACHE      3
#define BLOCK_SIZE_IN_BYTES sizeof(unsigned long) * ALLOWED_SIZE_RAM * ALLOWED_SIZE_RAM // 32 * 3 MB

typedef stxxl::VECTOR_GENERATOR<unsigned long, BLOCKS_PER_PAGE, PAGES_IN_CACHE,
                                BLOCK_SIZE_IN_BYTES,
                                stxxl::RC, stxxl::lru>::result fw_vector_type;

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

#ifdef NO_GPU
/* Non-GPU base-case code to be used if GPU is not present in the machine */

/**
 * Serial Floyd-Warshall's algorithm
 */
void serial_fw(unsigned long *X, unsigned long *U, unsigned long *V,
               uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol,
               uint64_t n)
{
    for (uint64_t k = 0; k < n; k++) {
        for (uint64_t i = 0; i < n; i++) {
            for (uint64_t j = 0; j < n; j++) {
                uint64_t Xi = xrow + i; uint64_t Xj = xcol + j;
                uint64_t Ui = urow + i; uint64_t Vj = vcol + j;
                uint64_t K  = ucol + k;
                uint64_t cur = host_encode2D_to_morton_64bit(Xi, Xj);
                uint64_t first = host_encode2D_to_morton_64bit(Ui, K);
                uint64_t second = host_encode2D_to_morton_64bit(K, Vj);
                X[cur] = min(X[cur], (U[first] + V[second]));
            }
        }
    }
}

/*
 * RAM launcher code
 */
void host_RAM_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n)
{
    serial_fw(X, X, X, xrow, xcol, xrow, xcol, xrow, xcol, n);
}

void host_RAM_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n)
{
    serial_fw(X, U, X, xrow, xcol, urow, ucol, xrow, xcol, n);
}

void host_RAM_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    serial_fw(X, X, V, xrow, xcol, xrow, xcol, vrow, vcol, n);
}

void host_RAM_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    serial_fw(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
}
#endif

/**
 * Read from stxxl vector to RAM vector
 */
void read_to_RAM(fw_vector_type& zfloyd, unsigned long *X, uint64_t row, uint64_t col, uint64_t n)
{
    // DEBUG: cout << "Reading from row " << row << " and col " << col << " of size " << n << " from disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = host_encode2D_to_morton_64bit(row, col);
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
    // DEBUG: cout << "Writing from row " << row << " and col " << col << " of size " << n << " to disk" << endl; 
    uint64_t i = 0;
    uint64_t begin_pos = host_encode2D_to_morton_64bit(row, col);
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
#ifdef NO_GPU
        unsigned long *X = new unsigned long[n*n];
#else
        unsigned long *X = (unsigned long *)mallocCudaHostMemory(n*n*sizeof(unsigned long), HOST_MEMORY_TYPE);
#endif
        read_to_RAM(zfloyd, X, xrow, xcol, n);
        host_RAM_A_fw(X, 0, 0, n);
        write_to_disk(zfloyd, X, xrow, xcol, n);
#ifdef NO_GPU
        delete [] X;
#else
        freeCudaHostMemory((void *)X,  HOST_MEMORY_TYPE);
#endif
    }
    // If not, split into r chunks
    else {
        uint64_t r = n / ALLOWED_SIZE_RAM;
        uint64_t m = n / r;
        // DEBUG: cout << "Splitting disk matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        // Our RAM size is chosen such that it holds upto 3 times the allowed size
#ifdef NO_GPU
        unsigned long *W  = new unsigned long[m*m];
        unsigned long *R1 = new unsigned long[m*m];
        unsigned long *R2 = new unsigned long[m*m];
#else
        // DEBUG: cout << "Allocating Host memory...\n";
        unsigned long *W  = (unsigned long *)mallocCudaHostMemory(m*m*sizeof(unsigned long), HOST_MEMORY_TYPE);
        unsigned long *R1 = (unsigned long *)mallocCudaHostMemory(m*m*sizeof(unsigned long), HOST_MEMORY_TYPE);
        unsigned long *R2 = (unsigned long *)mallocCudaHostMemory(m*m*sizeof(unsigned long), HOST_MEMORY_TYPE);
#endif

        for (uint64_t k = 0; k < r; k++) {
            // Step 1: A_step - A(X_kk, U_kk, V_kk), X,U,V are the same
            // DEBUG: cout << "A\n";
            read_to_RAM(zfloyd, W, xrow + (m*k), xcol + m*k, m);
            host_RAM_A_fw(W, 0, 0, m);
            // Don't write to RAM yet, as B and C can use this directly

            // Step 2: B_C_step - B(X_kj, U_kk, V_kj), C(X_ik, U_ik, V_kk)
            // Note that B's U_kk and C's V_kk are the same
            // For B, X and V are the same, for C, X and U are the same.
            // We already have B's U_kk/C's V_kk in RAM, put it in R1
            // DEBUG: cout << "B\n";
            unsigned long *T = R1; R1 = W; W = T;
            for (uint64_t j = 0; j < r; j++) {
                if (j != k) {
                    // DEBUG: cout << "Bj " << j << endl;
                    read_to_RAM(zfloyd, W, xrow + (m*k), xcol + m*j, m);
                    host_RAM_B_fw(W, R1, 0, 0, 0, 0, m);
                    write_to_disk(zfloyd, W, xrow + (m*k), xcol + m*j, m);
                }
            }
            // DEBUG: cout << "C\n";
            for (uint64_t i = 0; i < r; i++) {
                if (i != k) {
                    // DEBUG: cout << "Ci " << i << endl;
                    read_to_RAM(zfloyd, W, xrow + (m*i), xcol + m*k, m);
                    host_RAM_C_fw(W, R1, 0, 0, 0, 0, m);
                    write_to_disk(zfloyd, W, xrow + (m*i), xcol + m*k, m);
                }
            }
            // Write the deffered A-step data to disk
            write_to_disk(zfloyd, R1, xrow + (m*k), xcol + m*k, m);

            // Step 3: D_step - D(X_ij, U_ik, V_kj)
            // DEBUG: cout << "D\n";
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
#ifdef NO_GPU
        delete [] W;
        delete [] R1;
        delete [] R2;
#else
        freeCudaHostMemory((void *)W,  HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *)R1, HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *)R2, HOST_MEMORY_TYPE);
#endif
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
    string outp_filename(argv[2]);
    ifstream inp_file(inp_filename);
     
    string line;
    uint64_t full_size = 0;
    unsigned long value = 0;
    cout << "Reading input file, " << inp_filename << endl;
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
    cout << "Finished reading input file, full_size=" << full_size << endl;
    inp_file.close();
    
    uint64_t n = sqrt(full_size);

    struct timespec start, finish;
    double time_taken;

    clock_gettime(CLOCK_MONOTONIC, &start);
    host_disk_A_fw(zfloyd, 0, 0, n);
    clock_gettime(CLOCK_MONOTONIC, &finish);

    time_taken  =  finish.tv_sec - start.tv_sec;
    time_taken += (finish.tv_nsec - start.tv_nsec) / 1e9;
    
    /* SERIAL CODE 
    unsigned long *X = new unsigned long[full_size];
    int index = 0;
    for (fw_vector_type::const_iterator it = zfloyd.begin(); it != zfloyd.end(); ++it, ++index) {
        X[index] = *it;
    }
    serial_fw(X, X, X, 0, 0, 0, 0, 0, 0, sqrt(full_size));
    write_to_disk(zfloyd, X, 0, 0, sqrt(full_size));
    delete [] X;
    */
    
    cout << "Parallel r-Way R-DP Floyd-Warshall using GPUs with STXXL" << endl;
    cout << "Input matrix size: " << "2^" << log_base_2(n) << ", " << n << endl;
    cout << "RAM matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_RAM) << ", " << ALLOWED_SIZE_RAM << endl;
    cout << "GPU global matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_GPU_GLOBAL) << ", " << ALLOWED_SIZE_GPU_GLOBAL << endl;
    cout << "GPU shared matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_GPU_SHARED) << ", " << ALLOWED_SIZE_GPU_SHARED << endl;
    cout << "Input file: " << inp_filename << endl;
    cout << "Output file: " << outp_filename << endl;
    cout << "Time taken: " << time_taken << endl;

    ofstream outp_file(outp_filename);
    for (fw_vector_type::const_iterator it = zfloyd.begin(); it != zfloyd.end(); ++it) {
        if (it != zfloyd.begin())
            outp_file << " ";
        if (*it == INFINITY_LENGTH)
            outp_file << "inf";
        else
            outp_file << *it;
    }
    outp_file << "\n";
    outp_file.close();

    return 0;
}
