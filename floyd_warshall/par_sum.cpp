#include <algorithm>
#include <stxxl/vector>
#include <iostream>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include <time.h>
#include <pthread.h>

using namespace std;


//	(1)	:	Passing the entire vector	:	Got the following error "WRITE_REQ FOR BID WITH PENDING READ ERROR"
//	(2)	:	Passing an unsigned int pointer which points to the vector, but
//			only  once, to the function	:	Calculates only for the size that fits into RAM, as once the
//			function is invoked, no stxxl comes into play
//	(3)	:	Split stxxl into 4 components, passing unsigned int* each time to
//			the function. Once the 1st component is done, it comes back to main
//			func where stxxl performs the cache replacement, and invokes the
//			function again

//int parallel_sum(int n, stxxl::VECTOR_GENERATOR<unsigned int, 1, 1, 1024*1024*1024, stxxl::RC, stxxl::lru>::result&  my_vector)  (1)
//int parallel_sum(int n, unsigned int* my_vector)			   (2)
int parallel_sum(int n, unsigned int*my_vector)				// (3)
{   
    cilk::reducer< cilk::op_add<int> > sum;
	//pthread_mutex_t m; //define the lock
	//pthread_mutex_init(&m,NULL); //initialize the lock

    cilk_for(int i = 0; i < n/4; i++) {
		//pthread_mutex_lock(&m); //lock - prevents other threads from running this code
		//pthread_mutex_unlock(&m); //unlock - allows other threads to access this code
        *sum += my_vector[i];
    }  
	return sum.get_value();
}


int serial_sum(int n, stxxl::VECTOR_GENERATOR<unsigned int, 1, 1, 1024*1024*1024, stxxl::RC, stxxl::lru>::result& my_vector)
{
	int sum = 0;

	for (int i = 0; i < n; i++) {
		sum += my_vector[i];
	}
	return sum;
}

int main(int argc, char **argv)
{
	// RAM limit is 1 GB
	typedef stxxl::VECTOR_GENERATOR<unsigned int, 1, 1, 1024*1024*1024, stxxl::RC, stxxl::lru>::result vector;
    vector my_vector;

	// Total input size is 4 * 1 GB = 4 GB
    int n = 1024 * 1024 * 1024;
    int ser_sum = 0, par_sum = 0;

    for (int i = 0; i < n; i++)
    {
        my_vector.push_back(1);
    }

	// 3 different methods used, marked (1), (2) and (3), out of which only (3) worked

	//sum = parallel_sum(n, my_vector);							(1)
	//sum = parallel_sum(n, &my_vector[0]);						(2)
	for (int i = 0; i < 4; i++)
		par_sum += parallel_sum(n, &my_vector[i * 256 * 1024 * 1024]);	  //(3)
	cout << par_sum << endl;

	ser_sum = serial_sum(n, my_vector);    
	cout << ser_sum << endl;

	while (!my_vector.empty())
    {
		my_vector.pop_back();
    }

    return 0;
}
