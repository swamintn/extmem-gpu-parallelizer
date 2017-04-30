
all: fw-gpu

fw-gpu: check-stxxl-lib compile-fw-gpu-kernel compile-fw-gpu-host link-fw-gpu-host-kernel

compile-fw-gpu-kernel:
	nvcc -c -arch=compute_35 -code=sm_35 -o fw_gpu_kernel.o fw_gpu_kernel.cu

compile-fw-gpu-host: check-stxxl-lib
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -Wextra -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart -o fw_gpu.o -c fw_gpu.cpp

link-fw-gpu-host-kernel: check-stxxl-lib
	icpc -W -Wall -pedantic -Wno-long-long -Wextra -std=c++0x -fopenmp -O3 -DNDEBUG fw_gpu.o fw_gpu_kernel.o -o fw_gpu -rdynamic ${STXXL_LIB}/build/lib/libstxxl.a -lpthread -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart

clean: clean-fw-gpu

clean-fw-gpu:
	rm fw_gpu_kernel.o fw_gpu.o fw_gpu

check-stxxl-lib:
ifndef STXXL_LIB
	$(error STXXL_LIB is undefined)
endif
