
// Only X submatrix is used here. X is an upper triangular submatrix.

// The inner loop can be parallelized as X[i][j], X[i+1][j+1],
// X[i+2][j+2],...X[n - steps][n] can all be computed in parallel.
// X[i][j] depends only on X[i][1] and X[1][j], X[i][2] and X[2][j] and so on
// X[i+1][j+1] depends only on X[i+1][2] and X[2][j], X[i+2][3] and X[3][j] ...
// .... and so on
void A_par(int M, long (*X)[SIZE], int start_row, int start_col, int end_row, int end_col) {

    for (int steps = 2; steps <= M-1; steps++) {
	    cilk_for (int i = 0; i <= M - steps - 1; i++) {			/***** PARALLEL  *****/
		int j = steps + i;
	        for (int k = i+1; k <= j; k++) {
	            X[start_row + i][start_col + j] = min(X[start_row + i][start_col + j], X[start_row + i][start_col + k] + X[start_row + k][start_col + j]);
	        }
	    }	
    }
}





// X, U and V are the 3 submatrices. X is a square submatrix, while U and V are upper triangular submatrices.
// X, U and V are denoted by different start-row and start-column numbers.
// All X[i][j] can be computed in parallel as they don't depend on any other X[i][j]
void B_par(int M, long (*X)[SIZE], int start_row_X, int start_col_X, int end_row_X, int end_col_X,
	   int start_row_U, int start_col_U, int end_row_U, int end_col_U,
	   int start_row_V, int start_col_V, int end_row_V, int end_col_V) {

	for (int steps = M-1; steps >= 0; steps--) {
	    for (int i = steps; i <= M-1; i++) {
	        int j = i - steps;

			for (int k = i; k <= M - 1; k++) {
			    X[start_row_X + i][start_col_X + j] = min(X[start_row_X + i][start_col_X + j],
								 X[start_row_U + i][start_col_U + k] + X[start_row_V + k][start_col_V + j]);
			}
			for (int k = 0; k <= j; k++) {
				X[start_row_X + i][start_col_X + j] = min(X[start_row_X + i][start_col_X + j],
								 X[start_row_U + i][start_col_U + k] + X[start_row_V + k][start_col_V + j]);
			}
	    }
	}
}






// X, U and V are the 3 square submatrices. 
// X, U and V are denoted by different start-row and start-column numbers.
// All X[i][j] can be computed in parallel as they don't depend on any other X[i][j]
void C_par(int M, long (*X)[SIZE], int start_row_X, int start_col_X, int end_row_X, int end_col_X,
	   int start_row_U, int start_col_U, int end_row_U, int end_col_U,
	   int start_row_V, int start_col_V, int end_row_V, int end_col_V) {
 
    cilk_for (int i = 0; i < M; i++) {				/***** PARALLEL  *****/
	    cilk_for (int j = 0; j < M; j++) {			/***** PARALLEL  *****/
			for (int k = 0; k < M; k++) {
				X[start_row_X + i][start_col_X + j] = min(X[start_row_X + i][start_col_X + j],
								 X[start_row_U + i][start_col_U + k] + X[start_row_V + k][start_col_V + j]);	
			}
	    }
	}
}
