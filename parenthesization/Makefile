CC=icpc
CFLAGS=-std=c++11 -O3 -lrt 

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
