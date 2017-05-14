CC=icpc
CFLAGS=-std=c++11 -O3 -lrt 

all: par-gpu

serial:
	$(CC) helper.cpp serial_parenthesis.cpp $(CFLAGS) -o serial_parenthesis

	
parallel:
	$(CC) helper.cpp parallel_parenthesis.cpp $(CFLAGS) -o parallel_parenthesis

granular_parallel:
	$(CC) helper.cpp granular_parallel_parenthesis.cpp $(CFLAGS) -o granular_parallel_parenthesis

best_granular_parallel:
	$(CC) helper.cpp best_granular_parallel_parenthesis.cpp $(CFLAGS) -o best_granular_parallel_parenthesis

clean_serial:
	rm serial_parenthesis

clean_parallel:
	rm parallel_parenthesis

clean_granular_parallel:
	rm granular_parallel_parenthesis_m_32

clean_best_granular_parallel:
	rm best_granular_parallel_parenthesis

par-gpu: check-stxxl-lib compile-par-gpu-device compile-par-gpu-host link-par-gpu-host-device

compile-par-gpu-device:
	nvcc -c -arch=compute_35 -code=sm_35 -o par_gpu_device.o par_gpu_device.cu

compile-par-gpu-host: check-stxxl-lib
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart -o par_gpu_host.o -c par_gpu_host.cpp

link-par-gpu-host-device: check-stxxl-lib
	icpc -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG par_gpu_host.o par_gpu_device.o -o par_gpu -rdynamic ${STXXL_LIB}/build/lib/libstxxl.a -lpthread -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart

build-no-gpu: check-stxxl-lib
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -lrt -o par_gpu_host_no_gpu.o -c par_gpu_host.cpp
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -lrt -o par_gpu_device_no_gpu.o -c par_gpu_device_no_gpu.cpp
	icpc -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG par_gpu_host_no_gpu.o par_gpu_device_no_gpu.o -o par_gpu_no_gpu -rdynamic ${STXXL_LIB}/build/lib/libstxxl.a -lpthread -lrt

clean: clean-par-gpu

clean-par-gpu:
	rm par_gpu_device.o par_gpu_host.o par_gpu

clean-no-gpu:
	rm par_gpu_host_no_gpu.o par_gpu_device_no_gpu.o par_gpu_no_gpu

check-stxxl-lib:
ifndef STXXL_LIB
	$(error STXXL_LIB is undefined)
endif
