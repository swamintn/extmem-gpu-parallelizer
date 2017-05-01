#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <math.h>
using namespace std;

#define TWO_POWER 11
#define size (1 << TWO_POWER)

void zmorton(int n, int row, int col, long floyd[size][size], vector<long>& zmorton_vector) {

	if (n == 1) {
		zmorton_vector.push_back(floyd[row][col]);
		return;
	}	
	zmorton(n/2, row, col, floyd, zmorton_vector);
	zmorton(n/2, row, col + (n/2), floyd, zmorton_vector);
	zmorton(n/2, row + (n/2), col, floyd, zmorton_vector);
	zmorton(n/2, row + (n/2), col + (n/2), floyd, zmorton_vector);
	return;
}


int main(int argc, char *argv[])
{
	if (argc != 3) {
		cout << "Use proper input format : ./a.out <input-file> <output-file>" << endl;
		exit(1);
	}
	int row = 0, col = 0;
	long (*normal_matrix)[size] = new long[size][size]();
	cout << "Starting\n";
	//int normal_matrix[size][size];
	vector<long> zmorton_vector;
	
	string ipfile_name(argv[1]);
	string opfile_name(argv[2]);
	string line;
	ifstream ipfile(ipfile_name);
	ofstream opfile(opfile_name);	
	
	row = 0;
	while (getline(ipfile,line) && row < size) {
		stringstream ss(line);
		string buf;
		col = 0;
		while ( (ss >> buf) && (col < size) ) {
            if (buf == "inf")
                normal_matrix[row][col] = numeric_limits<long>::max();
            else
			    normal_matrix[row][col] = stol(buf);
			col++;
		}
		if (col != size)
			break;
		row++;
	}

	if (row != size || col != size) {
		cout << "Inadequate input" << endl;
		exit(1);
	}

	row = col = 0;
	zmorton(size, row, col, normal_matrix, zmorton_vector);	

	for (long i = 0; i < size * size; i++) {
		if (i != 0) {
			opfile << " ";
		}
        if (zmorton_vector[i] == numeric_limits<long>::max())
            opfile << "inf";
        else
		    opfile << zmorton_vector[i];
	}
	opfile << endl;

	ipfile.close();
	opfile.close();

	delete[] normal_matrix;
    return 0;
}
