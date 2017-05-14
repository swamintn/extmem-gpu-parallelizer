
all: mm-gpu mm-cpu

mm-gpu: check-stxxl-lib compile-mm-gpu-device compile-mm-gpu-host link-mm-gpu-host-device

mm-cpu: build-mm-cpu

compile-mm-gpu-device:
	nvcc -c -arch=compute_35 -code=sm_35 -o mm_gpu_device.o mm_gpu_device.cu

compile-mm-gpu-host: check-stxxl-lib
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart -o mm_gpu_host.o -c mm_gpu_host.cpp

link-mm-gpu-host-device: check-stxxl-lib
	icpc -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG mm_gpu_host.o mm_gpu_device.o -o mm_gpu -rdynamic ${STXXL_LIB}/build/lib/libstxxl.a -lpthread -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart

build-mm-cpu: check-stxxl-lib
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -lrt -o mm_cpu.o -c mm_cpu.cpp
	icpc -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG mm_cpu.o -o mm_cpu -rdynamic ${STXXL_LIB}/build/lib/libstxxl.a -lpthread -lrt

clean: clean-mm-gpu clean-mm-cpu

clean-mm-gpu:
	rm mm_gpu_device.o mm_gpu_host.o mm_gpu

clean-mm-cpu:
	rm mm_cpu.o mm_cpu

check-stxxl-lib:
ifndef STXXL_LIB
	$(error STXXL_LIB is undefined)
endif
