#include <iostream>

#include <cuda_runtime.h>

using namespace std;

/**
 * GPU Kernel
 */
__global__ void vectorAdd(const float *A, const float *B, float *C, int numElements)
{
	int i = blockDim.x * blockIdx.x + threadIdx.x;

	if (i < numElements)
	{
		C[i] = A[i] + B[i];
	}
}

int main()
{
	// Error code checking
	cudaError_t err = cudaSuccess;

	int numElements = 50000;
	size_t size = numElements * sizeof(float);
	cout << "Vector addition of " << numElements << " elements\n...";

	// Host vectors
	float *h_A = (float *)malloc(size);
	float *h_B = (float *)malloc(size);
	float *h_C = (float *)malloc(size);
	if (h_A == NULL || h_B == NULL || h_C == NULL)
	{
		cout << "ERROR: Host vector memory allocation failed\n";
		exit(EXIT_FAILURE);
	}

	// Initialize host vectors
	for (int i = 0; i < numElements; ++i) {
		h_A[i] = (i + 1);
		h_B[i] = (i + 1) * 2;
	}

	// Device vectors
	float *d_A = NULL;
	float *d_B = NULL;
	float *d_C = NULL;
	cudaMalloc(&d_A, size);	
	cudaMalloc(&d_B, size);	
	cudaMalloc(&d_C, size);
	
	// Copy memory from host to device
	cudaMemcpy(d_A, h_A, size, cudaMemcpyHostToDevice);
	cudaMemcpy(d_B, h_B, size, cudaMemcpyHostToDevice);

	// Kernel properties
	int threadsPerBlock = 256;
	int blocksPerGrid = (numElements + threadsPerBlock - 1) / threadsPerBlock;
	cout << "Launching kernel with " << blocksPerGrid << " blocks and " << threadsPerBlock << " threads...\n";
	vectorAdd<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, numElements);
	err = cudaGetLastError();

	if (err != cudaSuccess)
	{
	cout << "ERROR: Failed to launch vectorAdd kernel (error is '" << cudaGetErrorString(err) << "')\n";
        exit(EXIT_FAILURE);
	}

	// Copy memory from device to host
	cudaMemcpy(h_C, d_C, size, cudaMemcpyDeviceToHost);

	// Print vector
	for (int i = 0; i < numElements; ++i) {
		cout << "h_A[" << i << "] = " << h_A[i] << ", h_A[" << i << "] = " << h_B[i] << ", h_C[ " << i << "] = " << h_C[i] << endl;
	} 

}
