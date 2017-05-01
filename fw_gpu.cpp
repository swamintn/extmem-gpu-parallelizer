#include <fstream>
#include <iostream>
#include <limits>
#include <ctime>

#include <stxxl/vector>

#include "fw_gpu_common.h"

using namespace std;

typedef stxxl::VECTOR_GENERATOR<unsigned long>::result fw_vector_type;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        cout << "Inadequate parameters, provide input in the format "
                "./fw_gpu <z_morton_input_file>" << endl;
        exit(1);
    }

    //typedef stxxl::VECTOR_GENERATOR<unsigned long, 1, 1, 1024*1024, stxxl::RC, stxxl::lru>::result vector;
    //typedef stxxl::VECTOR_GENERATOR<unsigned long>::result vector;
    
    fw_vector_type zfloyd;
    string inp_filename(argv[1]);
    ifstream inp_file(inp_filename);
     
    string line;
    int size = 0;
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
            size++;
        }
    }
    cout << "Finished reading input file, size=" << size << endl;
    inp_file.close();

    // Printing vector
    //for (fw_vector_type::iterator it = zfloyd.begin(); it != zfloyd.end(); ++it)
    //    cout << *it << " ";
    for (int i = 0; i < zfloyd.size(); ++i)
        cout << zfloyd[i] << " ";
    cout << endl;

    return 1;
    //return kernel_wrapper();	
}
