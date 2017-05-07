#ifndef GUARD_HELPER_H
#define GUARD_HELPER_H

#include <string>
#include <vector>

#define TWO_POWER 13
#define SIZE (1 << TWO_POWER)

void write_matrix(long matrix[SIZE][SIZE], const std::string&);
int read_matrix(long matrix[SIZE][SIZE], const std::string&);

#endif
