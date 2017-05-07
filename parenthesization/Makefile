CC=icpc
CFLAGS=-std=c++11 -O0 -lrt 

serial:
	$(CC) helper.cpp serial_parenthesis.cpp $(CFLAGS) -o serial_parenthesis_2_power_13_maxcores

	
parallel:
	$(CC) helper.cpp parallel_parenthesis.cpp $(CFLAGS) -o parallel_parenthesis

granular_parallel:
	$(CC) helper.cpp granular_parallel_parenthesis.cpp $(CFLAGS) -o granular_parallel_parenthesis_m_32

best_granular_parallel:
	$(CC) helper.cpp best_granular_parallel_parenthesis.cpp $(CFLAGS) -o best_granular_parallel_parenthesis_2_power_13_maxcores

clean_serial:
	rm serial_parenthesis_2_power_13_maxcores

clean_parallel:
	rm parallel_parenthesis

clean_granular_parallel:
	rm granular_parallel_parenthesis_m_32

clean_best_granular_parallel:
	rm best_granular_parallel_parenthesis_2_power_13_maxcores
