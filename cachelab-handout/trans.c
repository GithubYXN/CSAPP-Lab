/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

#define ROW_STEP 8
#define COL_STEP 8

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
  int a1, a2, a3, a4, a5, a6, a7, a8;
  int i, j, k, l;

  if (M == 32) {
    for (i = 0; i < N; i += 8) {
      for (j = 0; j < M; j += 8) {
        for (k = 0; k < 8; k++) {
          a1 = A[i + k][j];
          a2 = A[i + k][j + 1];
          a3 = A[i + k][j + 2];
          a4 = A[i + k][j + 3];
          a5 = A[i + k][j + 4];
          a6 = A[i + k][j + 5];
          a7 = A[i + k][j + 6];
          a8 = A[i + k][j + 7];

          B[j + k][i] = a1;
          B[j + k][i + 1] = a2;
          B[j + k][i + 2] = a3;
          B[j + k][i + 3] = a4;
          B[j + k][i + 4] = a5;
          B[j + k][i + 5] = a6;
          B[j + k][i + 6] = a7;
          B[j + k][i + 7] = a8;
        }

        for (k = 0; k < 8; k++) {
          for (l = 0; l < k; l++) {
            a1 = B[j + k][i + l];
            B[j + k][i + l] = B[j + l][i + k];
            B[j + l][i + k] = a1;
          }
        }
      }
    }
  } else if (M == 0) {
    return;
  }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

char trans_1_desc[] = "v1";
void trans_1(int M, int N, int A[N][M], int B[M][N]) {
  int i, j, ii, jj, tmp;
  int x = (N / 4) * 4, y = (M / 4) * 4;

  for (i = 0; i < x; i += 4) {
    for (j = 0; j < y; j += 4) {
      for (ii = i; ii < i + 4; ii++) {
        for (jj = j; jj < j + 4; jj++) {
          tmp = A[ii][jj];
          B[jj][ii] = tmp;
        }
      }
    }
  }

  for (int k = 0; k < i; k++) {
    for (int t = j; t < M; t++) {
      tmp = A[k][t];
      B[t][k] = tmp;
    }
  }
  for ( ; i < N; i++) {
    for (int k = 0; k < M; k++) {
      tmp = A[i][k];
      B[k][i] = tmp;
    }
  }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions() {
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 
    registerTransFunction(trans_1, trans_1_desc); 
    // registerTransFunction(trans_2, trans_2_desc); 
    // registerTransFunction(trans_3, trans_3_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

