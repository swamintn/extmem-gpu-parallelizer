#include <iostream>
#include <stdint.h>
#include <cuda_runtime.h>

#include "mm_gpu_common.h"

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

/**
 * D_mm kernels
 */
__global__ void device_GPU_A_mm(long *X, long *U, long *V,
                                  uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t K, uint64_t n)
{
    __shared__ long S_3X[3*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t I = blockIdx.x;
    uint64_t J = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    long *S_X = S_3X;
    long *S_U = S_3X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);
    long *S_V = S_3X + (2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    uint64_t X_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*J);
    uint64_t U_off = encode2D_to_morton_64bit(urow + n*I, ucol + n*K);
    uint64_t V_off = encode2D_to_morton_64bit(vrow + n*K, vcol + n*J);

    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U[n*i + j] = U[U_off + (n*i + j)];
    S_V[n*i + j] = V[V_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        uint64_t cur = encode2D_to_morton_64bit(i, j);
        uint64_t first = encode2D_to_morton_64bit(i, k);
        uint64_t second = encode2D_to_morton_64bit(k, j);
        S_X[cur] += (S_U[first] * S_V[second]);
        __syncthreads();
    }

    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];
}

/**
 * Host GPU code - Launcher for the kernel
 */
void host_GPU_A_mm(long *X, long *U, long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_SHARED) {
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        // This is like r-way with r = 1. So k loop through only one value - '0'
        device_GPU_A_mm<<<1, threads_per_block>>>(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, 0, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_SHARED;
        uint64_t m = n / r;
        // DEBUG: cout << "A: Splitting GPU global matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;
        
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        dim3 blocks_per_grid_A(r, r);
        
        for (uint64_t k = 0; k < r; k++) {
            // Step 1: A_step - D(X_ij, U_ik, V_kj)
            device_GPU_A_mm<<<blocks_per_grid_A, threads_per_block>>>(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, k, m);
        }
    }
}

/*
 * Host RAM code
 */
void host_RAM_A_mm(long *X, long *U, long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        long *G_X = NULL;
        long *G_U = NULL;
        long *G_V = NULL;
        unsigned int bytes = n * n * sizeof(long);
        // DEBUG: cout << "Reached RAM_A base case, size=" << n << endl;
        cudaMalloc(&G_X, bytes);
        cudaMalloc(&G_U, bytes);
        cudaMalloc(&G_V, bytes);
        cudaMemcpy(G_X, X + encode2D_to_morton_64bit(xrow, xcol), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(G_U, U + encode2D_to_morton_64bit(urow, ucol), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(G_V, V + encode2D_to_morton_64bit(vrow, vcol), bytes, cudaMemcpyHostToDevice);
        host_GPU_A_mm(G_X, G_U, G_V, 0, 0, 0, 0, 0, 0, n);
        cudaMemcpy(X + encode2D_to_morton_64bit(xrow, xcol), G_X, bytes, cudaMemcpyDeviceToHost);
        cudaFree(G_X);
        cudaFree(G_U);
        cudaFree(G_V);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        // DEBUG: cout << "A_mm: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        long *G_X  = NULL;
        long *G_U = NULL;
        long *G_V = NULL;
        unsigned int bytes = m * m * sizeof(long);

        cudaMalloc(&G_X, bytes);
        cudaMalloc(&G_U, bytes);
        cudaMalloc(&G_V, bytes);

        for (uint64_t k = 0; k < r; k++) {
            for (uint64_t i = 0; i < r; i++) {
                cudaMemcpy(G_U, U + encode2D_to_morton_64bit(urow + m*i, ucol + m*k), bytes, cudaMemcpyHostToDevice);
                for (uint64_t j = 0; j < r; j++) {
                    cudaMemcpy(G_V, V + encode2D_to_morton_64bit(vrow + m*k, vcol + m*j), bytes, cudaMemcpyHostToDevice);
                    cudaMemcpy(G_X, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                    host_GPU_A_mm(G_X, G_U, G_V, 0, 0, 0, 0, 0, 0, m);
                    cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), G_X, bytes, cudaMemcpyDeviceToHost);
                }
            }
        }
        cudaFree(G_X);
        cudaFree(G_U);
        cudaFree(G_V);
    }
}


/*
 * Serial Matrix Multiplication code (for verification if necessary)
 */
void serial_mm(long *X, long *U, long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    cout << "Serial for n= " << n << endl;
    for (uint64_t k = 0; k < n; k++) {
        for (uint64_t i = 0; i < n; i++) {
            for (uint64_t j = 0; j < n; j++) {
                uint64_t U_ind = encode2D_to_morton_64bit(urow + i, ucol + k);
                uint64_t V_ind = encode2D_to_morton_64bit(vrow + k, vcol + j);
                uint64_t X_ind = encode2D_to_morton_64bit(xrow + i, xcol + j);
                X[X_ind] += (U[U_ind] * V[V_ind]);
            }
        }
    }       
}

