#include <fstream>
#include <iostream>
#include <limits>
#include <ctime>
#include <algorithm>
#include <math.h>

#include <stxxl/vector>

#include "fw_gpu_common.h"

using namespace std;

/**
 * STXXL Configurations
 */
#define BLOCKS_PER_PAGE     1
#define PAGES_IN_CACHE      1
#define BLOCK_SIZE_IN_BYTES 1024 // 1KB

typedef stxxl::VECTOR_GENERATOR<unsigned long, BLOCKS_PER_PAGE, PAGES_IN_CACHE,
                                BLOCK_SIZE_IN_BYTES,
                                stxxl::RC, stxxl::lru>::result fw_vector_type;

/** 
 * Encoding and decoding Morton codes
 * (Taken from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/)
 */
uint32_t encode2D_to_morton_32bit(uint32_t x, uint32_t y)
{
    x &= 0x0000ffff;
    x = (x ^ (x <<  8)) & 0x00ff00ff;
    x = (x ^ (x <<  4)) & 0x0f0f0f0f;
    x = (x ^ (x <<  2)) & 0x33333333;
    x = (x ^ (x <<  1)) & 0x55555555;

    y &= 0x0000ffff;
    y = (y ^ (y <<  8)) & 0x00ff00ff;
    y = (y ^ (y <<  4)) & 0x0f0f0f0f;
    y = (y ^ (y <<  2)) & 0x33333333;
    y = (y ^ (y <<  1)) & 0x55555555;

    return (x << 1 ) | y;

}

/**
 * Serial Floyd-Warshall's algorithm
 */
void serial_fw(unsigned long *X, unsigned long *U, unsigned long *V,
               int xrow, int xcol, int urow, int ucol, int vrow, int vcol,
               int n)
{
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                int Xi = xrow + i; int Xj = xcol + j;
                int Ui = urow + i; int Vj = vcol + j;
                int K  = ucol + k;
                int cur = encode2D_to_morton_32bit(Xi, Xj);
                int first = encode2D_to_morton_32bit(Ui, K);
                int second = encode2D_to_morton_32bit(K, Vj);
                X[cur] = min(X[cur], (U[first] + V[second]));
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        cout << "Inadequate parameters, provide input in the format "
                "./fw_gpu <z_morton_input_file>" << endl;
        exit(1);
    }
    
    fw_vector_type zfloyd;
    string inp_filename(argv[1]);
    ifstream inp_file(inp_filename);
     
    string line;
    int full_size = 0;
    unsigned long value = 0;
    cout << "Reading input file, " << inp_filename << endl;
    while (getline(inp_file, line)) {
        stringstream ss(line);
        string buf;
        while (ss >> buf) {
            if (buf == "inf")
                value = numeric_limits<unsigned long>::max();
            else
                value = stol(buf);
            zfloyd.push_back(value);
            full_size++;
        }
    }
    cout << "Finished reading input file, full_size=" << full_size << endl;
    inp_file.close();

    int size = sqrt(full_size);
    unsigned long *X = new unsigned long[full_size];
    int index = 0;
    for (fw_vector_type::const_iterator it = zfloyd.begin(); it != zfloyd.end(); ++it, ++index) {
        X[index] = *it;
    }
    cout << endl;

    for (int i = 0; i < full_size ; ++i) {
        cout << X[i] << " ";
    }
    cout << endl;
    serial_fw(X, X, X, 0, 0, 0, 0, 0, 0, size);
    for (int i = 0; i < full_size ; ++i) {
        cout << X[i] << " ";
    }
    cout << endl;

    delete [] X;

    return 0;
    //return kernel_wrapper();	
}
