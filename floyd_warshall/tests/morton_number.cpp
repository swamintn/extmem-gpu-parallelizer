#include <iomanip>
#include <iostream>
#include <stdint.h>

using namespace std;

#define N 4

/* Encoding and decoding Morton codes

   Taken from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
*/

uint32_t encode2D_to_morton_32bit(uint32_t x, uint32_t y)
{
    x &= 0x0000ffff;                  // x = ---- ---- ---- ---- fedc ba98 7654 3210
    x = (x ^ (x <<  8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x <<  4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x <<  2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x <<  1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0

    y &= 0x0000ffff;                  // x = ---- ---- ---- ---- fedc ba98 7654 3210
    y = (y ^ (y <<  8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    y = (y ^ (y <<  4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    y = (y ^ (y <<  2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    y = (y ^ (y <<  1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    
    // This will return row-major order z ordering. If we switch x and y, it will be column-major 
    return (x << 1) | y;
}

uint64_t encode2D_to_morton_64bit(uint64_t x, uint64_t y)
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

int convert_to_morton_recurse(int *source, int *dest, int row_start, int col_start, int size, int full_size, int index_to_use)
{
    if (size == 1) {
        dest[index_to_use] = source[full_size*row_start + col_start];
        return index_to_use + 1;
    }
    index_to_use = convert_to_morton_recurse(source, dest, row_start, col_start, size/2, full_size, index_to_use);
    index_to_use = convert_to_morton_recurse(source, dest, row_start, col_start + size/2, size/2, full_size, index_to_use);
    index_to_use = convert_to_morton_recurse(source, dest, row_start + size/2, col_start, size/2, full_size, index_to_use);
    index_to_use = convert_to_morton_recurse(source, dest, row_start + size/2, col_start + size/2, size/2, full_size, index_to_use);

    return index_to_use;
}

void convert_to_morton_bitmanip(int *source, int *dest, int size)
{
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int z_order = encode2D_to_morton_64bit(i, j);
            dest[z_order] = source[size*i + j];
        }
    }
}

void print_matrix(int *mat, int size)
{
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            cout << setw(2) << mat[size*i + j] << " ";
        }
        cout << endl;
    }
}

void print_array(int *mat, int size)
{
    for (int i = 0; i < size; i++) {
        cout << mat[i] << " ";
    }
    cout << endl;
}

int main()
{
    int *mat = new int[N*N];
    int *z_mat_recurse = new int[N*N];
    int *z_mat_bitmanip = new int[N*N];
    
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            mat[N*i + j] = N*i + j;
        }
    }
    
    cout << "\n\nNORMAL MATRIX:\n" << endl;
    print_matrix(mat, N);

    convert_to_morton_recurse(mat, z_mat_recurse, 0, 0, N, N, 0);
    //cout << "\n\nZ-MORTON MATRIX USING RECURSION:\n" << endl;
    //print_matrix(z_mat_recurse, N);

    cout << "\n\nZ-MORTON ORDER USING RECURSION:\n" << endl;
    print_array(z_mat_recurse, N*N);

    convert_to_morton_bitmanip(mat, z_mat_bitmanip, N);
    //cout << "\n\nZ-MORTON MATRIX USING BIT MANIPULATION:\n" << endl;
    //print_matrix(z_mat_bitmanip, N);

    cout << "\n\nZ-MORTON ORDER USING BIT MANIPULATION:\n" << endl;
    print_array(z_mat_bitmanip, N*N);

    delete [] mat;
    delete [] z_mat_recurse;
    delete [] z_mat_bitmanip;

    return 0;
}
