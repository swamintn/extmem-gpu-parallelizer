
all: fw-gpu

fw-gpu: check-stxxl-lib compile-fw-gpu-device compile-fw-gpu-host link-fw-gpu-host-device

compile-fw-gpu-device:
	nvcc -c -arch=compute_35 -code=sm_35 -o fw_gpu_device.o fw_gpu_device.cu

compile-fw-gpu-host: check-stxxl-lib
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart -o fw_gpu_host.o -c fw_gpu_host.cpp

# For testing
build-fw-gpu-host: check-stxxl-lib
	icpc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG -I${STXXL_LIB}/include -I${STXXL_LIB}/build/include -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart -o fw_gpu_host.o -c fw_gpu_host.cpp
	icpc -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG fw_gpu_host.o -o fw_gpu -rdynamic ${STXXL_LIB}/build/lib/libstxxl.a -lpthread
# End

link-fw-gpu-host-device: check-stxxl-lib
	icpc -W -Wall -pedantic -Wno-long-long -std=c++0x -fopenmp -O3 -DNDEBUG fw_gpu_host.o fw_gpu_device.o -o fw_gpu -rdynamic ${STXXL_LIB}/build/lib/libstxxl.a -lpthread -L/opt/apps/cuda/6.5/lib64 -lcuda -lcudart

clean: clean-fw-gpu

clean-fw-gpu:
	rm fw_gpu_device.o fw_gpu_host.o fw_gpu

check-stxxl-lib:
ifndef STXXL_LIB
	$(error STXXL_LIB is undefined)
endif
