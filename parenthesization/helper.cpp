/*
  Helper library to read and write matrices from files

  Matrix files will have each row on a separate line and columns are separated
  by spaces. Eg:
  1 2 -5
  -2 4 0
  3 -7 8
 */
#include "helper.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

void write_matrix(long matrix[SIZE][SIZE], const string& filename)
{
    int n = SIZE;
    ofstream file(filename);
    for (int i = 0; i != n; i++) {
        for (int j = 0; j != n; j++) {
            if (j != 0)
                file << " ";
            file << matrix[i][j];
        }
        file << endl;
    }
    file.close();
}

// Returns 0 on success and -1 on failure
int read_matrix(long matrix[SIZE][SIZE], const std::string& filename)
{
    int n = SIZE;
    ifstream file(filename);
    string line;
    int i = 0, j = 0;
    while (getline(file, line) && i < n) {
        stringstream ss(line);
        string buf;
        j = 0;
        while ((ss >> buf) && (j < n)) {
            matrix[i][j] = stol(buf);
            j++;
        }
        if (j != n)
            break;
        i++;
    }
    return (i != n) || (j != n) ? -1 : 0;
}
