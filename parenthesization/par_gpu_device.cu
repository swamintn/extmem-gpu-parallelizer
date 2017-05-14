#include <iostream>
#include <stdint.h>
#include <cuda_runtime.h>

#include "par_gpu_common.h"

#define min(a, b) ((a) > (b))? (b): (a)

using namespace std;

/**
 * Encoding and decoding Morton codes
 * (Taken from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/)
 */
__host__ __device__ uint64_t encode2D_to_morton_64bit(uint64_t x, uint64_t y)
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
 * GPU memory allocator
 *
 * Allocates "pinned" host memory if type is pinned, else normal host memory
 */
void *mallocCudaHostMemory(unsigned int bytes, int type)
{
    void *memory;
    if (type == PINNED_HOST_MEMORY)
        cudaMallocHost((void**) &memory, bytes);
    else
        memory = malloc(bytes);
    return memory;
}

void freeCudaHostMemory(void *memory, int type)
{
    if (type == PINNED_HOST_MEMORY)
       cudaFreeHost(memory);
    else
        free(memory);
}

/*
 * Serial code
 */
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

void serial_B_par(unsigned long *X, unsigned long *U, unsigned long *V,
                  uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    for (int64_t t = n-1; t >= 0; t--) {
        for (int64_t i = t; i <= n-1; i++) {
            int64_t j = i - t;
            for (int64_t k = i; k <= n - 1; k++) {
                cout << "serial B first loop t,i,j,k " << t << ", " << i << ", " << j << ", " << k << endl;
                uint64_t cur = encode2D_to_morton_64bit(xrow + i, xcol + j);
                uint64_t first = encode2D_to_morton_64bit(urow + i, ucol + k);
                uint64_t second = encode2D_to_morton_64bit(xrow + k, xcol + j);
                cout << "serial B first loop cur=" << cur << ", first=" << first << ", second=" << second << endl;
                cout << "serial B first loop X[cur]=" << X[cur] << ", U[first]=" << U[first] << ", V[second]=" << V[second] << endl;
                X[cur] = min(X[cur], U[first] + X[second]);
            }
            for (int64_t k = 0; k <= j; k++) {
                cout << "serial B second loop t,i,j,k " << t << ", " << i << ", " << j << ", " << k << endl;
                uint64_t cur = encode2D_to_morton_64bit(xrow + i, xcol + j);
                uint64_t first = encode2D_to_morton_64bit(xrow + i, xcol + k);
                uint64_t second = encode2D_to_morton_64bit(vrow + k, vcol + j);
                cout << "serial B second loop cur=" << cur << ", first=" << first << ", second=" << second << endl;
                cout << "serial B second loop X[cur]=" << X[cur] << ", U[first]=" << U[first] << ", V[second]=" << V[second] << endl;
                X[cur] = min(X[cur], X[first] + V[second]);
            }
        }
    }
}

void serial_C_par(unsigned long *X, unsigned long *U, unsigned long *V,
                  uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    for (int64_t i = 0; i < n; i++) {
        for (int64_t j = 0; j < n; j++) {
            for (int64_t k = 0; k < n; k++) {
                uint64_t cur = encode2D_to_morton_64bit(xrow + i, xcol + j);
                uint64_t first = encode2D_to_morton_64bit(urow + i, ucol + k);
                uint64_t second = encode2D_to_morton_64bit(vrow + k, vcol + j);
                X[cur] = min(X[cur], U[first] + V[second]); 
            }
        }
    }
}

/*
 * Host RAM code
 */
void host_RAM_A_par(unsigned long *X,
                    uint64_t xrow, uint64_t xcol, uint64_t n)
{
    cout << "RAM A\n";
    serial_A_par(X, xrow, xcol, n);
}

void host_RAM_B_par(unsigned long *X, unsigned long *U, unsigned long *V,
                    uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    cout << "Inside RAM B\n";
    cout << xrow << xcol << urow << ucol << vrow << vcol << endl;
    cout << "RAM B X input\n";
    for(int i = 0; i < n*n; i++) {
        cout << X[i] << " ";
    }
    cout << endl;
    cout << "RAM B U input\n";
    for(int i = 0; i < n*n; i++) {
        cout << U[i] << " ";
    }
    cout << endl;
    cout << "RAM B V input\n";
    for(int i = 0; i < n*n; i++) {
        cout << V[i] << " ";
    }
    cout << endl; 
    serial_B_par(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
    cout << "RAM B X output\n";
    for(int i = 0; i < n*n; i++) {
        cout << X[i] << " ";
    }
    cout << endl;
}

void host_RAM_C_par(unsigned long *X, unsigned long *U, unsigned long *V,
                    uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    cout << "RAM C\n";
    serial_C_par(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
}

