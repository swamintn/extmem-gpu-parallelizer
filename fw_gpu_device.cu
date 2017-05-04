#include <iostream>
#include <stdint.h>
#include <cuda_runtime.h>

#include "fw_gpu_common.h"

using namespace std;

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

/**
 * Host GPU code
 */
void host_GPU_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n){}
void host_GPU_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n){}
void host_GPU_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n){}
void host_GPU_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n){}

/*
 * Host RAM code
 */
void host_RAM_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        host_GPU_A_fw(X, xrow, xcol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        cout << "A_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        for (uint64_t k = 0; k < r; k++) {
            host_GPU_A_fw(X, xrow + m*k, xcol + m*k, m);
            for (uint64_t j = 0; j < r; j++) {
                if (j != k) {
                    host_GPU_B_fw(X, X, xrow + m*k, xcol + m*j, xrow + m*k, xcol + m*k, m);
                }
            }
            for (uint64_t i = 0; i < r; i++) {
                if (i != k) {
                    host_GPU_C_fw(X, X, xrow + m*i, xcol + m*k, xrow + m*k, xcol + m*k, m);
                }
            }
            for (uint64_t i = 0; i < r; i++) {
                for (uint64_t j = 0; j < r; j++) {
                    if (i != k && j != k) {
                        host_GPU_D_fw(X, X, X, xrow + m*i, xcol + m*j, xrow + m*i, xcol + m*k, xrow + m*k, xcol + m*j, m);
                    }
                }
            }
        }
    }
}

void host_RAM_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        host_GPU_B_fw(X, U, xrow, xcol, urow, ucol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        cout << "B_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        for (uint64_t k = 0; k < r; k++) {
            for (uint64_t j = 0; j < r; j++) {
                host_GPU_B_fw(X, U, xrow + m*k, xcol + m*j, urow + m*k, ucol + m*k, m);
            }
            for (uint64_t i = 0; i < r; i++) {
                for (uint64_t j = 0; j < r; j++) {
                    if (i != k) {
                        host_GPU_D_fw(X, U, X, xrow + m*i, xcol + m*j, urow + m*i, ucol + m*k, xrow + m*k, xcol + m*j, m);
                    }
                }
            }
        }
    }
}

void host_RAM_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        host_GPU_C_fw(X, V, xrow, xcol, vrow, vcol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        cout << "C_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        for (uint64_t k = 0; k < r; k++) {
            for (uint64_t i = 0; i < r; i++) {
                host_GPU_C_fw(X, V, xrow + m*i, xcol + m*k, vrow + m*k, vcol + m*k, m);
            }
            for (uint64_t i = 0; i < r; i++) {
                for (uint64_t j = 0; j < r; j++) {
                    if (j != k) {
                        host_GPU_D_fw(X, X, V, xrow + m*i, xcol + m*j, xrow + m*i, xcol + m*k, vrow + m*k, vcol + m*j, m);
                    }
                }
            }
        }
    }
}

void host_RAM_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        host_GPU_D_fw(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        cout << "D_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        for (uint64_t k = 0; k < r; k++) {
            for (uint64_t i = 0; i < r; i++) {
                for (uint64_t j = 0; j < r; j++) {
                    host_GPU_D_fw(X, U, V, xrow + m*i, xcol + m*j, urow + m*i, ucol + m*k, vrow + m*k, vcol + m*j, m);
                }
            }
        }
    }
}


