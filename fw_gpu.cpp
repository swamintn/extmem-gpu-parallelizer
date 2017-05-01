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
 * Different memory hierarchy configurations
 */
#define ALLOWED_SIZE_RAM (1 << 3)
#define INFINITY_LENGTH (1 << 20)


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

/**
 * RAM code
 */
void host_RAM_A_fw(unsigned long *X,
                   int xrow, int xcol, int n)
{
    serial_fw(X, X, X, xrow, xcol, xrow, xcol, xrow, xcol, n);
}

void host_RAM_B_fw(unsigned long *X, unsigned long *U,
                   int xrow, int xcol, int urow, int ucol, int n)
{
    serial_fw(X, U, X, xrow, xcol, urow, ucol, xrow, xcol, n);
}

void host_RAM_C_fw(unsigned long *X, unsigned long *V,
                   int xrow, int xcol, int vrow, int vcol, int n)
{
    serial_fw(X, X, V, xrow, xcol, xrow, xcol, vrow, vcol, n);
}

void host_RAM_D_fw(unsigned long *X, unsigned long *U, unsigned long *V,
                   int xrow, int xcol, int urow, int ucol, int vrow, int vcol, int n)
{
    serial_fw(X, U, V, xrow, xcol, urow, ucol, vrow, vcol, n);
}


/**
 * Read from stxxl vector to RAM vector
 */
void read_to_RAM(fw_vector_type& zfloyd, unsigned long *X, int row, int col, int n)
{
    cout << "Reading from row " << row << " and col " << col << " of size " << n << " from disk" << endl; 
    int i = 0;
    int begin_pos = encode2D_to_morton_32bit(row, col);
    int end_pos = begin_pos + (n*n) - 1;
    for (fw_vector_type::const_iterator it = zfloyd.begin() + begin_pos; it != zfloyd.begin() + end_pos + 1; ++it, ++i) {
        X[i] = *it;
    }
}

/**
 * Write RAM data to stxxl vector
 */
void write_to_disk(fw_vector_type& zfloyd, unsigned long *X, int row, int col, int n)
{   
    cout << "Writing from row " << row << " and col " << col << " of size " << n << " to disk" << endl; 
    int i = 0;
    int begin_pos = encode2D_to_morton_32bit(row, col);
    int end_pos = begin_pos + (n*n) - 1;
    for (fw_vector_type::iterator it = zfloyd.begin() + begin_pos; it != zfloyd.begin() + end_pos + 1; ++it, ++i) {
        *it = X[i];
    }
}

/*
 * Disk code
 */
void host_disk_A_fw(fw_vector_type& zfloyd,
                    int xrow, int xcol, int n)
{
    // Base case - If possible, read the entire array into RAM
    if (n <= ALLOWED_SIZE_RAM) {
        unsigned long *X = new unsigned long[n*n];
        read_to_RAM(zfloyd, X, xrow, xcol, n);
        host_RAM_A_fw(X, xrow, xcol, n);
        write_to_disk(zfloyd, X, xrow, xcol, n);
        delete [] X;
    }
    // If not, split into r chunks
    else {
        int r = n / ALLOWED_SIZE_RAM;
        int m = n / r;
        cout << "Splitting disk matrix into r=" << r << " chunks, each submatrix size, m=" << m << endl;

        // Our RAM size is chosen such that it holds upto 3 times the allowed size
        unsigned long *W  = new unsigned long[m*m];
        unsigned long *R1 = new unsigned long[m*m];
        unsigned long *R2 = new unsigned long[m*m];

        for (int k = 0; k < r; k++) {
            cout << "k " << k << endl;

            // Step 1: A_step - A(X_kk, U_kk, V_kk), X,U,V are the same
            cout << "A\n";
            read_to_RAM(zfloyd, W, xrow + (m*k), xcol + m*k, m);
            host_RAM_A_fw(W, 0, 0, m);
            write_to_disk(zfloyd, W, xrow + (m*k), xcol + m*k, m);

            // Step 2: B_C_step - B(X_kj, U_kk, V_kj), C(X_ik, U_ik, V_kk)
            // Note that B's U_kk and C's V_kk are the same
            // For B, X and V are the same, for C, X and U are the same
            cout << "B\n";
            read_to_RAM(zfloyd, R1, xrow + (m*k), xcol + m*k, m);
            for (int j = 0; j < r; j++) {
                if (j != k) {
                    cout << "Bj " << j << endl;
                    read_to_RAM(zfloyd, W, xrow + (m*k), xcol + m*j, m);
                    host_RAM_B_fw(W, R1, 0, 0, 0, 0, m);
                    write_to_disk(zfloyd, W, xrow + (m*k), xcol + m*j, m);
                }
            }
            cout << "C\n";
            for (int i = 0; i < r; i++) {
                if (i != k) {
                    cout << "Ci " << i << endl;
                    read_to_RAM(zfloyd, W, xrow + (m*i), xcol + m*k, m);
                    host_RAM_C_fw(W, R1, 0, 0, 0, 0, m);
                    write_to_disk(zfloyd, W, xrow + (m*i), xcol + m*k, m);
                }
            }

            // Step 3: D_step - D(X_ij, U_ik, V_kj)
            cout << "D\n";
            for (int i = 0; i < r; i++) {
                if (i != k) {
                    // U_ik is same for all j
                    read_to_RAM(zfloyd, R1, xrow + (m*i), xcol + m*k, m);
                    for (int j = 0; j < r; j++) {
                        if (j != k) {
                            cout << "Di " << i << ", Dj " << j << endl; 
                            read_to_RAM(zfloyd, R2, xrow + (m*k), xcol + m*j, m);
                            read_to_RAM(zfloyd, W, xrow + (m*i), xcol + m*j, m);
                            host_RAM_D_fw(W, R1, R2, 0, 0, 0, 0, 0, 0, m);
                            write_to_disk(zfloyd, W, xrow + (m*i), xcol + m*j, m);
                        }
                    }
                }
            }

        }

        delete [] W;
        delete [] R1;
        delete [] R2;
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
                value = INFINITY_LENGTH;
            else
                value = stol(buf);
            zfloyd.push_back(value);
            full_size++;
        }
    }
    cout << "Finished reading input file, full_size=" << full_size << endl;
    inp_file.close();
    
    ///*
    cout << "Array before execution: " << endl;
    for (fw_vector_type::const_iterator it = zfloyd.begin(); it != zfloyd.end(); ++it)
        cout << *it << " ";
    cout << endl;

    int n = sqrt(full_size);
    host_disk_A_fw(zfloyd, 0, 0, n);

    cout << "Array after execution: " << endl;
    for (fw_vector_type::const_iterator it = zfloyd.begin(); it != zfloyd.end(); ++it)
        cout << *it << " ";
    cout << endl;
    //*/
    /*    
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
    serial_fw(X, X, X, 0, 0, 0, 0, 0, 0, sqrt(full_size));
    for (int i = 0; i < full_size ; ++i) {
        cout << X[i] << " ";
    }
    cout << endl;

    delete [] X;
    */

    return 0;
    //return kernel_wrapper();	
}
