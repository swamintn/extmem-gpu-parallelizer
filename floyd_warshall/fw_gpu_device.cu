#include <iostream>
#include <stdint.h>
#include <cuda_runtime.h>

#include "fw_gpu_common.h"

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
 * GPU base case
 */
__device__ void parallel_base_case(unsigned long *X, unsigned long *U, unsigned long *V, uint64_t i, uint64_t j, uint64_t k)
{
    uint64_t cur = encode2D_to_morton_64bit(i, j);
    uint64_t first = encode2D_to_morton_64bit(i, k);
    uint64_t second = encode2D_to_morton_64bit(k, j);
    X[cur] = min(X[cur], (U[first] + V[second]));
}

/*
 * GPU Simple Kernel
 */
__global__ void parallel_iterative_kernel(unsigned long *X, unsigned long *U, unsigned long *V,
                                     uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol,
                                     uint64_t n)
{
    __shared__ unsigned long S_3X[3*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;
 
    unsigned long *S_X = S_3X;
    unsigned long *S_U = S_3X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);
    unsigned long *S_V = S_3X + (2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    // The entire array can fit in shared memory. Copy it here
    uint64_t X_off = encode2D_to_morton_64bit(xrow, xcol);
    uint64_t U_off = encode2D_to_morton_64bit(urow, ucol);
    uint64_t V_off = encode2D_to_morton_64bit(vrow, vcol);
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U[n*i + j] = U[U_off + (n*i + j)];
    S_V[n*i + j] = V[V_off + (n*i + j)];
    
    for (uint64_t k = 0; k < n; k++) {
        parallel_base_case(S_X, S_U, S_V, i, j, k);
        __syncthreads();
    }

    // Copy back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];
}

/**
 * A_fw kernels
 */
__global__ void device_GPU_A_fw_A(unsigned long *X, uint64_t xrow, uint64_t xcol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_X[ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;    
    
    // Copy to shared memory
    uint64_t off = encode2D_to_morton_64bit(xrow + n*K, xcol + n*K);
    S_X[n*i + j] = X[off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        parallel_base_case(S_X, S_X, S_X, i, j, k);
        __syncthreads();
    }
    
    // Copy back to global memory
    X[off + (n*i + j)] = S_X[n*i + j];
}

#define B_ROW 0
#define C_ROW 1
__global__ void device_GPU_A_fw_B_C(unsigned long *X, uint64_t xrow, uint64_t xcol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_2X[2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t B_or_C = blockIdx.x;
    uint64_t J_or_I = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    unsigned long *S_X = S_2X;
    unsigned long *S_U_or_V = S_2X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);
    
    uint64_t X_off;
    if (B_or_C == 0)
        X_off = encode2D_to_morton_64bit(xrow + n*K, xcol + n*J_or_I);
    else
        X_off = encode2D_to_morton_64bit(xrow + n*J_or_I, xcol + n*K);
    uint64_t U_or_V_off = encode2D_to_morton_64bit(xrow + n*K, xcol + n*K);
    
    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U_or_V[n*i + j]= X[U_or_V_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        if (B_or_C == 0 && J_or_I != K)
            parallel_base_case(S_X, S_U_or_V, S_X, i, j, k);
        else if (B_or_C == 1 && J_or_I != K)
            parallel_base_case(S_X, S_X, S_U_or_V, i, j, k);
        __syncthreads();
    }

    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];
}

__global__ void device_GPU_A_fw_D(unsigned long *X, uint64_t xrow, uint64_t xcol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_3X[3*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t I = blockIdx.x;
    uint64_t J = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    unsigned long *S_X = S_3X;
    unsigned long *S_U = S_3X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);
    unsigned long *S_V = S_3X + (2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    uint64_t X_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*J);
    uint64_t U_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*K);
    uint64_t V_off = encode2D_to_morton_64bit(xrow + n*K, xcol + n*J);

    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U[n*i + j] = X[U_off + (n*i + j)];
    S_V[n*i + j] = X[V_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        if (I != K && J != K)
            parallel_base_case(S_X, S_U, S_V, i, j, k);
        __syncthreads();
    }
    
    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];   
}

/**
 * Host GPU code - Launcher for A_fw kernels
 */
void host_GPU_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_SHARED) {
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        parallel_iterative_kernel<<<1, threads_per_block>>>(X, X, X, xrow, xcol, xrow, xcol, xrow, xcol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_SHARED;
        uint64_t m = n / r;
        // DEBUG: cout << "A: Splitting GPU global matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;
        
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        dim3 blocks_per_grid_A(1);
        dim3 blocks_per_grid_B_C(2, r);
        dim3 blocks_per_grid_D(r, r);

        for (uint64_t k = 0; k < r; k++) {
            // Step 1: A_step - A(X_kk, U_kk, V_kk), X,U,V are the same
            device_GPU_A_fw_A<<<blocks_per_grid_A, threads_per_block>>>(X, xrow, xcol, k, m);
            
            // Step 2: B_C_step - B(X_kj, U_kk, V_kj), C(X_ik, U_ik, V_kk)
            device_GPU_A_fw_B_C<<<blocks_per_grid_B_C, threads_per_block>>>(X, xrow, xcol, k, m);

            // Step 3: D_step - D_step - D(X_ij, U_ik, V_kj)
            device_GPU_A_fw_D<<<blocks_per_grid_D, threads_per_block>>>(X, xrow, xcol, k, m);
        }
    }
}

/** 
 * B_fw kernels
 */
__global__ void device_GPU_B_fw_B(unsigned long *X, unsigned long *U,
                                  uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_2X[2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t J = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    unsigned long *S_X = S_2X;
    unsigned long *S_U = S_2X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    uint64_t X_off = encode2D_to_morton_64bit(xrow + n*K, xcol + n*J);
    uint64_t U_off = encode2D_to_morton_64bit(urow + n*K, ucol + n*K);

    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U[n*i + j] = U[U_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        parallel_base_case(S_X, S_U, S_X, i, j, k);
        __syncthreads();
    }

    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];    
}

__global__ void device_GPU_B_fw_D(unsigned long *X, unsigned long *U,
                                  uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_3X[3*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t I = blockIdx.x;
    uint64_t J = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    unsigned long *S_X = S_3X;
    unsigned long *S_U = S_3X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);
    unsigned long *S_V = S_3X + (2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    uint64_t X_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*J);
    uint64_t U_off = encode2D_to_morton_64bit(urow + n*I, ucol + n*K);
    uint64_t V_off = encode2D_to_morton_64bit(xrow + n*K, xcol + n*J);

    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U[n*i + j] = U[U_off + (n*i + j)];
    S_V[n*i + j] = X[V_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        if (I != K)
            parallel_base_case(S_X, S_U, S_V, i, j, k);
        __syncthreads();
    }
    
    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];
}

/**
 * Host GPU code - Launcher for B_fw kernels
 */
void host_GPU_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_SHARED) {
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        parallel_iterative_kernel<<<1, threads_per_block>>>(X, U, X, xrow, xcol, urow, ucol, xrow, xcol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_SHARED;
        uint64_t m = n / r;
        // DEBUG: cout << "B: Splitting GPU global matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;
        
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        dim3 blocks_per_grid_B(1, r);
        dim3 blocks_per_grid_D(r, r);
        
        for (uint64_t k = 0; k < r; k++) {
            // Step 1: B_step - B(X_kj, U_kk, Vkj)
            device_GPU_B_fw_B<<<blocks_per_grid_B, threads_per_block>>>(X, U, xrow, xcol, urow, ucol, k, m);

            // Step 2: D_step - D(X_ij, U_ik, V_kj)
            device_GPU_B_fw_D<<<blocks_per_grid_D, threads_per_block>>>(X, U, xrow, xcol, urow, ucol, k, m);
        }
    }
}

/**
 * C_fw kernels
 */
__global__ void device_GPU_C_fw_C(unsigned long *X, unsigned long *V,
                                  uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_2X[2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t I = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    unsigned long *S_X = S_2X;
    unsigned long *S_V = S_2X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    uint64_t X_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*K);
    uint64_t V_off = encode2D_to_morton_64bit(vrow + n*K, vcol + n*K);

    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_V[n*i + j] = V[V_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        parallel_base_case(S_X, S_X, S_V, i, j, k);
        __syncthreads();
    }

    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];
}

__global__ void device_GPU_C_fw_D(unsigned long *X, unsigned long *V,
                                  uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_3X[3*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t I = blockIdx.x;
    uint64_t J = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    unsigned long *S_X = S_3X;
    unsigned long *S_U = S_3X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);
    unsigned long *S_V = S_3X + (2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    uint64_t X_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*J);
    uint64_t U_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*K);
    uint64_t V_off = encode2D_to_morton_64bit(vrow + n*K, vcol + n*J);

    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U[n*i + j] = X[U_off + (n*i + j)];
    S_V[n*i + j] = V[V_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        if (J != K)
            parallel_base_case(S_X, S_U, S_V, i, j, k);
        __syncthreads();
    }

    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];
}


/**
 * Host GPU code - Launcher for C_fw kernels
 */
void host_GPU_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_SHARED) {
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        parallel_iterative_kernel<<<1, threads_per_block>>>(X, X, V, xrow, xcol, xrow, xcol, vrow, vcol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_SHARED;
        uint64_t m = n / r;
        // DEBUG: cout << "C: Splitting GPU global matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;
        
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        dim3 blocks_per_grid_C(1, r);
        dim3 blocks_per_grid_D(r, r);
        
        for (uint64_t k = 0; k < r; k++) {
            // Step 1: B_step - C(X_ik, U_kk, V_kk)
            device_GPU_C_fw_C<<<blocks_per_grid_C, threads_per_block>>>(X, V, xrow, xcol, vrow, vcol, k, m);

            // Step 2: D_step - D(X_ij, U_ik, V_kj)
            device_GPU_C_fw_D<<<blocks_per_grid_D, threads_per_block>>>(X, V, xrow, xcol, vrow, vcol, k, m);
        }
    }
}

/**
 * D_fw kernels
 */
__global__ void device_GPU_D_fw_D(unsigned long *X, unsigned long *U, unsigned long *V,
                                  uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t K, uint64_t n)
{
    __shared__ unsigned long S_3X[3*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED];
    uint64_t I = blockIdx.x;
    uint64_t J = blockIdx.y;
    uint64_t i = threadIdx.x;
    uint64_t j = threadIdx.y;

    unsigned long *S_X = S_3X;
    unsigned long *S_U = S_3X + (ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);
    unsigned long *S_V = S_3X + (2*ALLOWED_SIZE_GPU_SHARED*ALLOWED_SIZE_GPU_SHARED);

    uint64_t X_off = encode2D_to_morton_64bit(xrow + n*I, xcol + n*J);
    uint64_t U_off = encode2D_to_morton_64bit(urow + n*I, ucol + n*K);
    uint64_t V_off = encode2D_to_morton_64bit(vrow + n*K, vcol + n*J);

    // Copy to shared memory
    S_X[n*i + j] = X[X_off + (n*i + j)];
    S_U[n*i + j] = U[U_off + (n*i + j)];
    S_V[n*i + j] = V[V_off + (n*i + j)];
    __syncthreads();

    for (uint64_t k = 0; k < n; k++) {
        parallel_base_case(S_X, S_U, S_V, i, j, k);
        __syncthreads();
    }

    // Copy only X back to global memory
    X[X_off + (n*i + j)] = S_X[n*i + j];
}

/**
 * Host GPU code - Launcher for D_fw kernels
 */
void host_GPU_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_SHARED) {
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        parallel_iterative_kernel<<<1, threads_per_block>>>(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_SHARED;
        uint64_t m = n / r;
        // DEBUG: cout << "D: Splitting GPU global matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;
        
        dim3 threads_per_block(ALLOWED_SIZE_GPU_SHARED, ALLOWED_SIZE_GPU_SHARED);
        dim3 blocks_per_grid_D(r, r);
        
        for (uint64_t k = 0; k < r; k++) {
            // Step 1: D_step - D(X_ij, U_ik, V_kj)
            device_GPU_D_fw_D<<<blocks_per_grid_D, threads_per_block>>>(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, k, m);
        }
    }
}

/*
 * Host RAM code
 */
void host_RAM_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n)
{
    // Base case - If possible, read the entire array into GPU
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        unsigned long *G_X = NULL;
        unsigned int bytes = n * n * sizeof(unsigned long);
        // DEBUG: cout << "Reached RAM_A base case, size=" << n << endl;       
        cudaMalloc(&G_X, bytes);
        cudaMemcpy(G_X, X + encode2D_to_morton_64bit(xrow, xcol), bytes, cudaMemcpyHostToDevice);
        host_GPU_A_fw(G_X, 0, 0, n);
        cudaMemcpy(X + encode2D_to_morton_64bit(xrow, xcol), G_X, bytes, cudaMemcpyDeviceToHost);
        cudaFree(G_X);
    }
    // If not, split into r chunks
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        // DEBUG: cout << "A_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;
        
        unsigned long *G_W  = NULL;
        unsigned long *G_R1 = NULL;
        unsigned long *G_R2 = NULL;
        unsigned int bytes = m * m * sizeof(unsigned long);

        cudaMalloc(&G_W, bytes);
        cudaMalloc(&G_R1, bytes);
        cudaMalloc(&G_R2, bytes);
        
        for (uint64_t k = 0; k < r; k++) {
            // Step 1: A_step - A(X_kk, U_kk, V_kk), X,U,V are the same
            cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*k), bytes, cudaMemcpyHostToDevice);
            host_GPU_A_fw(G_W, 0, 0, m);
            // Keep this in device memory itself since B and C can use it. Take care to write it to RAM later
            
            // Step 2: B_C_step - B(X_kj, U_kk, V_kj), C(X_ik, U_ik, V_kk)
            // Note that B's U_kk and C's V_kk are the same
            // For B, X and V are the same, for C, X and U are the same.
            // We already have B's U_kk/C's V_kk in device memory, put it in R1
            unsigned long *G_T = G_R1; G_R1 = G_W; G_W = G_T;
            for (uint64_t j = 0; j < r; j++) {
                if (j != k) {
                    cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                    host_GPU_B_fw(G_W, G_R1, 0, 0, 0, 0, m);
                    cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*j), G_W, bytes, cudaMemcpyDeviceToHost);                
                }
            }
            for (uint64_t i = 0; i < r; i++) {
                if (i != k) {
                    cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*k), bytes, cudaMemcpyHostToDevice);
                    host_GPU_C_fw(G_W, G_R1, 0, 0, 0, 0, m);
                    cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*k), G_W, bytes, cudaMemcpyDeviceToHost);
                }
            }
            // Write the deferred A-step X_kk to RAM
            cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*k), G_R1, bytes, cudaMemcpyDeviceToHost);

            // Step 3: D_step - D(X_ij, U_ik, V_kj)           
            for (uint64_t i = 0; i < r; i++) {
                if (i != k) {
                    // U_ik is same for all j
                    cudaMemcpy(G_R1, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*k), bytes, cudaMemcpyHostToDevice);
                    for (uint64_t j = 0; j < r; j++) {
                        if (j != k) {
                            cudaMemcpy(G_R2, X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                            cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                            host_GPU_D_fw(G_W, G_R1, G_R2, 0, 0, 0, 0, 0, 0, m);
                            cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), G_W, bytes, cudaMemcpyDeviceToHost);
                        }
                    }
                }
            }
        }       
        cudaFree(G_W);
        cudaFree(G_R1);
        cudaFree(G_R2);
    }
}

void host_RAM_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        unsigned long *G_X = NULL;
        unsigned long *G_U = NULL;
        unsigned int bytes = n * n * sizeof(unsigned long);
        // DEBUG: cout << "Reached RAM_B base case, size=" << n << endl;       
        cudaMalloc(&G_X, bytes);
        cudaMalloc(&G_U, bytes);
        cudaMemcpy(G_X, X + encode2D_to_morton_64bit(xrow, xcol), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(G_U, U + encode2D_to_morton_64bit(urow, ucol), bytes, cudaMemcpyHostToDevice);
        host_GPU_B_fw(G_X, G_U, 0, 0, 0, 0, n);
        cudaMemcpy(X + encode2D_to_morton_64bit(xrow, xcol), G_X, bytes, cudaMemcpyDeviceToHost);
        cudaFree(G_X);
        cudaFree(G_U);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        // DEBUG: cout << "B_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        unsigned long *G_W  = NULL;
        unsigned long *G_R1 = NULL;
        unsigned long *G_R2 = NULL;
        unsigned int bytes = m * m * sizeof(unsigned long);

        cudaMalloc(&G_W, bytes);
        cudaMalloc(&G_R1, bytes);
        cudaMalloc(&G_R2, bytes);

        for (uint64_t k = 0; k < r; k++) {
            cudaMemcpy(G_R1, U + encode2D_to_morton_64bit(urow + m*k, ucol + m*k), bytes, cudaMemcpyHostToDevice);
            for (uint64_t j = 0; j < r; j++) {
                cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                host_GPU_B_fw(G_W, G_R1, 0, 0, 0, 0, m);
                cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*j), G_W, bytes, cudaMemcpyDeviceToHost);
            }
            for (uint64_t i = 0; i < r; i++) {
                cudaMemcpy(G_R1, U + encode2D_to_morton_64bit(urow + m*i, ucol + m*k), bytes, cudaMemcpyHostToDevice);
                for (uint64_t j = 0; j < r; j++) {
                    if (i != k) {
                        cudaMemcpy(G_R2, X + encode2D_to_morton_64bit(xrow + m*k, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                        cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                        host_GPU_D_fw(G_W, G_R1, G_R2, 0, 0, 0, 0, 0, 0, m);
                        cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), G_W, bytes, cudaMemcpyDeviceToHost);
                    }
                }
            }
        }
        cudaFree(G_W);
        cudaFree(G_R1);
        cudaFree(G_R2);
    }
}

void host_RAM_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        unsigned long *G_X = NULL;
        unsigned long *G_V = NULL;
        unsigned int bytes = n * n * sizeof(unsigned long);
        // DEBUG: cout << "Reached RAM_C base case, size=" << n << endl;
        cudaMalloc(&G_X, bytes);
        cudaMalloc(&G_V, bytes);
        cudaMemcpy(G_X, X + encode2D_to_morton_64bit(xrow, xcol), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(G_V, V + encode2D_to_morton_64bit(vrow, vcol), bytes, cudaMemcpyHostToDevice);
        host_GPU_C_fw(G_X, G_V, 0, 0, 0, 0, n);
        cudaMemcpy(X + encode2D_to_morton_64bit(xrow, xcol), G_X, bytes, cudaMemcpyDeviceToHost);
        cudaFree(G_X);
        cudaFree(G_V);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        // DEBUG: cout << "C_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        unsigned long *G_W  = NULL;
        unsigned long *G_R1 = NULL;
        unsigned long *G_R2 = NULL;
        unsigned int bytes = m * m * sizeof(unsigned long);

        cudaMalloc(&G_W, bytes);
        cudaMalloc(&G_R1, bytes);
        cudaMalloc(&G_R2, bytes);

        for (uint64_t k = 0; k < r; k++) {
            cudaMemcpy(G_R1, V + encode2D_to_morton_64bit(vrow + m*k, vcol + m*k), bytes, cudaMemcpyHostToDevice);
            for (uint64_t i = 0; i < r; i++) {
                cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*k), bytes, cudaMemcpyHostToDevice);
                host_GPU_C_fw(G_W, G_R1, 0, 0, 0, 0, m);
                cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*k), G_W, bytes, cudaMemcpyDeviceToHost);
            }
            for (uint64_t i = 0; i < r; i++) {
                cudaMemcpy(G_R1, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*k), bytes, cudaMemcpyHostToDevice);
                for (uint64_t j = 0; j < r; j++) {
                    if (j != k) {
                        cudaMemcpy(G_R2, V + encode2D_to_morton_64bit(xrow + m*k, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                        cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                        host_GPU_D_fw(G_W, G_R1, G_R2, 0, 0, 0, 0, 0, 0, m);
                        cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), G_W, bytes, cudaMemcpyDeviceToHost);
                    }
                }
            }
        }
        cudaFree(G_W);
        cudaFree(G_R1);
        cudaFree(G_R2);
    }
}

void host_RAM_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n)
{
    if (n <= ALLOWED_SIZE_GPU_GLOBAL) {
        unsigned long *G_X = NULL;
        unsigned long *G_U = NULL;
        unsigned long *G_V = NULL;
        unsigned int bytes = n * n * sizeof(unsigned long);
        // DEBUG: cout << "Reached RAM_D base case, size=" << n << endl;
        cudaMalloc(&G_X, bytes);
        cudaMalloc(&G_U, bytes);
        cudaMalloc(&G_V, bytes);
        cudaMemcpy(G_X, X + encode2D_to_morton_64bit(xrow, xcol), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(G_U, U + encode2D_to_morton_64bit(urow, ucol), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(G_V, V + encode2D_to_morton_64bit(vrow, vcol), bytes, cudaMemcpyHostToDevice);
        host_GPU_D_fw(G_X, G_U, G_V, 0, 0, 0, 0, 0, 0, n);
        cudaMemcpy(X + encode2D_to_morton_64bit(xrow, xcol), G_X, bytes, cudaMemcpyDeviceToHost);
        cudaFree(G_X);
        cudaFree(G_U);
        cudaFree(G_V);
    }
    else {
        uint64_t r = n / ALLOWED_SIZE_GPU_GLOBAL;
        uint64_t m = n / r;
        // DEBUG: cout << "D_fw: Splitting RAM matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        unsigned long *G_W  = NULL;
        unsigned long *G_R1 = NULL;
        unsigned long *G_R2 = NULL;
        unsigned int bytes = m * m * sizeof(unsigned long);

        cudaMalloc(&G_W, bytes);
        cudaMalloc(&G_R1, bytes);
        cudaMalloc(&G_R2, bytes);

        for (uint64_t k = 0; k < r; k++) {
            for (uint64_t i = 0; i < r; i++) {
                cudaMemcpy(G_R1, U + encode2D_to_morton_64bit(urow + m*i, ucol + m*k), bytes, cudaMemcpyHostToDevice);
                for (uint64_t j = 0; j < r; j++) {
                    cudaMemcpy(G_R2, V + encode2D_to_morton_64bit(vrow + m*k, vcol + m*j), bytes, cudaMemcpyHostToDevice);
                    cudaMemcpy(G_W, X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), bytes, cudaMemcpyHostToDevice);
                    host_GPU_D_fw(G_W, G_R1, G_R2, 0, 0, 0, 0, 0, 0, m);
                    cudaMemcpy(X + encode2D_to_morton_64bit(xrow + m*i, xcol + m*j), G_W, bytes, cudaMemcpyDeviceToHost);
                }
            }
        }
        cudaFree(G_W);
        cudaFree(G_R1);
        cudaFree(G_R2);
    }
}

