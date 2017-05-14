#include <fstream>
#include <iostream>
#include <limits>
#include <ctime>
#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <stxxl/vector>
#include "par_gpu_common.h"

#define min(a, b) ((a) > (b))? (b): (a)
#define encode2D_to_morton_64bit host_encode2D_to_morton_64bit

using namespace std;

/**
 * STXXL Configurations
 */
#define BLOCKS_PER_PAGE     1
#define PAGES_IN_CACHE      3
#define BLOCK_SIZE_IN_BYTES sizeof(unsigned long) * ALLOWED_SIZE_RAM * ALLOWED_SIZE_RAM // 32 * 3 MB

typedef stxxl::VECTOR_GENERATOR<unsigned long, BLOCKS_PER_PAGE, PAGES_IN_CACHE,
                                BLOCK_SIZE_IN_BYTES,
                                stxxl::RC, stxxl::lru>::result par_vector_type;

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
void read_to_RAM(par_vector_type& zmatrix, unsigned long *X, uint64_t row, uint64_t col, uint64_t n)
{
    uint64_t i = 0;
    uint64_t begin_pos = host_encode2D_to_morton_64bit(row, col);
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
    uint64_t i = 0;
    uint64_t begin_pos = host_encode2D_to_morton_64bit(row, col);
    uint64_t end_pos = begin_pos + (n*n) - 1;
    for (par_vector_type::iterator it = zmatrix.begin() + begin_pos; it != zmatrix.begin() + end_pos + 1; ++it, ++i) {
        *it = X[i];
    }
}


/* DELETE LATER */
#ifdef NOGPU
void *mallocCudaHostMemory(unsigned int bytes, int type)
{
    void *memory;
    memory = malloc(bytes);
    return memory;
}

void freeCudaHostMemory(void *memory, int type)
{
    free(memory);
}

void serial_A_par(unsigned long *X,
                  uint64_t xrow, uint64_t xcol, uint64_t n)
{
    for (int64_t t = 2; t <= n-1; t++) {
        for (int64_t i = 0; i <= n - t - 1; i++) {
            int64_t j = t + i;
            for (int64_t k = i + 1; k <= j; k++) {
                uint64_t cur = encode2D_to_morton_64bit(xrow + i, xcol + j);
                uint64_t first = encode2D_to_morton_64bit(xrow + i, xcol + k);
                uint64_t second = encode2D_to_morton_64bit(xrow + k, xcol + j);
                X[cur] = min(X[cur], X[first] + X[second]);
            }
        }
    }
}

void host_RAM_A_par(unsigned long *X,
                    uint64_t xrow, uint64_t xcol, uint64_t n)
{
    cout << "RAM A\n";
    serial_A_par(X, xrow, xcol, n);
}


void serial_B_par(unsigned long *X, unsigned long *U, unsigned long *V,
                  uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    cout << "serial B\n";  
    for (int64_t t = n-1; t >= 0; t--) {
        for (int64_t i = t; i <= n-1; i++) {
            int64_t j = i - t;
            for (int64_t k = i; k <= n - 1; k++) {
                if (j == 1) {
                cout << "serial B first loop t,i,j,k " << t << ", " << i << ", " << j << ", " << k << endl;
                }
                uint64_t cur = encode2D_to_morton_64bit(xrow + i, xcol + j);
                uint64_t first = encode2D_to_morton_64bit(urow + i, ucol + k);
                uint64_t second = encode2D_to_morton_64bit(xrow + k, xcol + j);
                if (j == 1) {
                cout << "serial B first loop cur=" << cur << ", first=" << first << ", second=" << second << endl;
                cout << "serial B first loop X[cur]=" << X[cur] << ", U[first]=" << U[first] << ", X[second]=" << X[second] << endl;
                }
                X[cur] = min(X[cur], U[first] + X[second]);
            }
            for (int64_t k = 0; k <= j; k++) {
                if (j == 1) {
                //cout << "serial B second loop t,i,j,k " << t << ", " << i << ", " << j << ", " << k << endl;
                }
                uint64_t cur = encode2D_to_morton_64bit(xrow + i, xcol + j);
                uint64_t first = encode2D_to_morton_64bit(xrow + i, xcol + k);
                uint64_t second = encode2D_to_morton_64bit(vrow + k, vcol + j);
                if (j == 1) {
                //cout << "serial B second loop cur=" << cur << ", first=" << first << ", second=" << second << endl;
                //cout << "serial B second loop X[cur]=" << X[cur] << ", U[first]=" << U[first] << ", V[second]=" << V[second] << endl;
                }
                X[cur] = min(X[cur], X[first] + V[second]);
            }
        }
    }
}

void host_RAM_B_par(unsigned long *X, unsigned long *U, unsigned long *V,
                    uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    cout << "RAM B\n";
    serial_B_par(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
}

void host_RAM_C_par(unsigned long *X, unsigned long *U, unsigned long *V,
                    uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    cout << "RAM C\n";
    //serial_C_par(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
}
#endif
/* END OF DELETE */






/*
 * Disk code
 */
void host_disk_A_par(par_vector_type& zmatrix,
                    uint64_t xrow, uint64_t xcol, uint64_t n)
{
    /* Base case - If possible, read the entire array into RAM */
    if (n <= ALLOWED_SIZE_RAM) {
        cout << "host_disk_A: Calling base case\n";
        unsigned long *X = (unsigned long *) mallocCudaHostMemory(n*n*sizeof(unsigned long), HOST_MEMORY_TYPE);
        read_to_RAM(zmatrix, X, xrow, xcol, n);
        host_RAM_A_par(X, 0, 0, n);
        write_to_disk(zmatrix, X, xrow, xcol, n);
        freeCudaHostMemory((void *)X,  HOST_MEMORY_TYPE);
    }
    /* If not, split into r chunks */
    else {
        uint64_t r = n / ALLOWED_SIZE_RAM;
        uint64_t m = n / r;
        cout << "Splitting disk matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        // Our RAM size is chosen such that it holds upto 3 times the allowed size
        unsigned long *W  = (unsigned long *) mallocCudaHostMemory(m*m*sizeof(unsigned long), HOST_MEMORY_TYPE);
        unsigned long *R1 = (unsigned long *) mallocCudaHostMemory(m*m*sizeof(unsigned long), HOST_MEMORY_TYPE);
        unsigned long *R2 = (unsigned long *) mallocCudaHostMemory(m*m*sizeof(unsigned long), HOST_MEMORY_TYPE);

        for (int64_t k = 0; k < r; k++) {
            // Step 1: A_step - A(X_kk, U_kk, V_kk), X,U,V are the same
            read_to_RAM(zmatrix, W, xrow + (m*k), xcol + m*k, m);
            host_RAM_A_par(W, 0, 0, m);
            write_to_disk(zmatrix, W, xrow + (m*k), xcol + (m*k), m);
        }

        for (int64_t k = 1; k <= r-1; k++) {

            // Step 2a) : C_step - X_ij, U_i(k+i-1), V_(k+i-1)j, where (j - i) ranges from k to min(2k - 2, r - 1)
            for (int64_t i = 0; i < r; i++) {
                int64_t end = min((2*k) - 2, r - 1);

                if ((k > end) || (k + i) >= r) // j index of X_ij will be out of bounds, since j starts from (k + i)
                    break;

                read_to_RAM(zmatrix, R1, xrow + (m*i), xcol + m*(k + i - 1), m);
                for (int64_t j = k + i; (j < end + i) && (j < r); j++) {
                    read_to_RAM(zmatrix, W, xrow + (m*i), xcol + (m*j), m);
                    read_to_RAM(zmatrix, R2, xrow + (m*(k + i - 1)), xcol + (m*j), m);
                    cout << "k=" << k << ", C1\n";
                    host_RAM_C_par(W, R1, R2, 0, 0, 0, 0, 0, 0, m);
                    write_to_disk(zmatrix, W, xrow + (m*i), xcol + (m*j), m);
                }
            }
            
            // Step 2b) : C_step - X_ij, U_i(1+i), V_(1+i)j, where (j - i) ranges from k to min(2k - 3, r - 1)
            for (int64_t i = 0; i < r; i++) {
                int64_t end = min((2*k) - 3, r - 1);
                cout << "end= "<< end << endl;

                if ((k > end) || (k + i) >= r) // j index of X_ij will be out of bounds, since j starts from (k + i)
                    break;

                read_to_RAM(zmatrix, R1, xrow + (m*i), xcol + m*(1 + i), m);
                for (int64_t j = k + i; (j < end + i) && (j < r); j++) {
                    read_to_RAM(zmatrix, W, xrow + (m*i), xcol + (m*j), m);
                    read_to_RAM(zmatrix, R2, xrow + (m*(1 + i)), xcol + (m*j), m);
                    cout << "k=" << k << ", C2\n";
                    host_RAM_C_par(W, R1, R2, 0, 0, 0, 0, 0, 0, m);
                    write_to_disk(zmatrix, W, xrow + (m*i), xcol + (m*j), m);
                }
            }
            
            // Step 3: B_step - X_ij, U_ii, V_jj, where (j - i) = k
            for (int64_t i = 0; i < r; i++) {

                if ((k + i) >= r) // j index of X_ij will be out of bounds, since j starts from (k + i)
                    break;
                
                int64_t j = k + i;
                read_to_RAM(zmatrix, R1, xrow + (m*i), xcol + (m*i), m);
                read_to_RAM(zmatrix, R2, xrow + (m*j), xcol + (m*j), m);
                read_to_RAM(zmatrix, W, xrow + (m*i), xcol + (m*j), m);
                cout << "k=" << k << ", B1, i=" << i << ",j=" << j << "\n";
                host_RAM_B_par(W, R1, R2, 0, 0, 0, 0, 0, 0, m);
                write_to_disk(zmatrix, W, xrow + (m*i), xcol + (m*j), m);
            }
        }

        freeCudaHostMemory((void *) W,  HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *) R1, HOST_MEMORY_TYPE);
        freeCudaHostMemory((void *) R2, HOST_MEMORY_TYPE);
    }
}


int main(int argc, char *argv[])
{
    if (argc != 3) {
        cout << "Inadequate parameters, provide input in the format "
                "./par_gpu <z_morton_input_file> <z_morton_output_file>" << endl;
        exit(1);
    }
    
    par_vector_type zmatrix;
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
            value = stol(buf);
            if (value == INFINITY_USED_IN_FILE)
                value = INFINITY_LENGTH;
            zmatrix.push_back(value);
            full_size++;
        }
    }
    cout << "Finished reading input file, full_size=" << full_size << endl;
    inp_file.close();
    
    uint64_t n = sqrt(full_size);

    struct timespec start, finish;
    double time_taken;

    clock_gettime(CLOCK_MONOTONIC, &start);
    host_disk_A_par(zmatrix, 0, 0, n);
    clock_gettime(CLOCK_MONOTONIC, &finish);

    time_taken  =  finish.tv_sec - start.tv_sec;
    time_taken += (finish.tv_nsec - start.tv_nsec) / 1e9;
    
    cout << "Parallel r-Way R-DP Parenthesis Problem Solver using GPUs with STXXL" << endl;
    cout << "Input matrix size: " << "2^" << log_base_2(n) << ", " << n << endl;
    cout << "RAM matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_RAM) << ", " << ALLOWED_SIZE_RAM << endl;
    cout << "GPU global matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_GPU_GLOBAL) << ", " << ALLOWED_SIZE_GPU_GLOBAL << endl;
    cout << "GPU shared matrix size: " << "2^" << log_base_2(ALLOWED_SIZE_GPU_SHARED) << ", " << ALLOWED_SIZE_GPU_SHARED << endl;
    cout << "Input file: " << inp_filename << endl;
    cout << "Output file: " << outp_filename << endl;
    cout << "Time taken: " << time_taken << endl;

    ofstream outp_file(outp_filename);
    for (par_vector_type::const_iterator it = zmatrix.begin(); it != zmatrix.end(); ++it) {
        if (it != zmatrix.begin())
            outp_file << " ";
        if (*it == INFINITY_LENGTH)
            outp_file << "if"; // Change to INFINITY_USED_IN_FILE later on 
        else
            outp_file << *it;
    }
    outp_file << "\n";
    outp_file.close();

    return 0;
}
