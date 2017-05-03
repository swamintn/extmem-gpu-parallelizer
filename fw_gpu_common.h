// Common CPU-GPU header files
#ifndef VECADD_H
#define VECADD_H

int kernel_wrapper();

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

#endif
