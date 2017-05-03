// Common CPU-GPU header files
#ifndef GPUCOMMON_H
#define GPUCOMMON_H

#include <stdint.h>

/**
 * Different memory hierarchy configurations
 */
#define ALLOWED_SIZE_RAM        (1 << 3) // use 11
#define ALLOWED_SIZE_GPU_GLOBAL (1 << 2) // use 8
#define INFINITY_LENGTH         (1 << 20)

#define NORMAL_HOST_MEMORY      0
#define PINNED_HOST_MEMORY      1
#define HOST_MEMORY_TYPE        PINNED_HOST_MEMORY

/**
 * GPU memory allocator on host - Allows pinned/normal memory
 */
void *mallocCudaHostMemory(unsigned int bytes, int type);
void freeCudaHostMemory(void *memory, int type);

/**
 * GPU functions on host
 */
void host_GPU_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n);
void host_GPU_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n);
void host_GPU_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n);
void host_GPU_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n);

/**
 * RAM functions on host
 */
void host_RAM_A_fw(unsigned long *X,
                   uint64_t xrow, uint64_t xcol, uint64_t n);
void host_RAM_B_fw(unsigned long *X, unsigned long *U,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t n);
void host_RAM_C_fw(unsigned long *X, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t vrow, uint64_t vcol, uint64_t n);
void host_RAM_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   uint64_t xrow, uint64_t xcol, uint64_t urow, uint64_t ucol, uint64_t vrow, uint64_t vcol, uint64_t n);

#endif
