/*
 * Autor: Datcu Andrei Daniel 331 CC
 * Grupa: 331CC
 * Tema2 ASC
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

enum CBLAS_ORDER    {CblasRowMajor=101, CblasColMajor=102};
enum CBLAS_TRANSPOSE    {CblasNoTrans=111, CblasTrans=112, CblasConjTrans=113};
enum CBLAS_UPLO     {CblasUpper=121, CblasLower=122};
enum CBLAS_DIAG     {CblasNonUnit=131, CblasUnit=132};
enum CBLAS_SIDE     {CblasLeft=141, CblasRight=142};

void my_dtrmv(const enum CBLAS_ORDER Order, const enum CBLAS_UPLO Uplo,
              const enum CBLAS_TRANSPOSE TransA, const enum CBLAS_DIAG Diag,
              const int N, const double *A, const int lda,
              double *X, const int incX){


    if(Order != CblasRowMajor && Uplo != CblasLower && TransA != CblasNoTrans){
        fprintf(stderr, "Operation not supported\n");
        return;
    }

    double *rezx = malloc(N * sizeof(double));
    unsigned int i, j;
    for (i = 0; i < N; ++i){
        rezx[i] = 0.0;

        for (j = 0; j <= i; ++j)
            rezx[i] += A[i * N + j] * X[j];
    }

    memcpy(X, rezx, N * sizeof(double));
    free(rezx);
}

void generateInput(unsigned int N, double *A, double *X, unsigned int seed){

    unsigned int i, j;

    srand(seed);
    for (i = 0; i < N; ++i)
        for (j = 0; j < N; ++j)
            A[i * N + j] =
                i <= j ? (double) (rand() % (2 * N) ) - (double) N : 0.0;

    for (i = 0; i < N; ++i)
        X[i] = (double) (rand() % (2 * N) ) - (double)N;
}

int main(int argc, char **argv){

    int N = atoi(argv[1]);

    size_t uN = N;

    double *A = malloc(uN * uN * (size_t)sizeof(double));
    double *X = malloc(uN * sizeof(double));

    generateInput(N, A, X, 500);


    struct timeval start, end;
    gettimeofday(&start, NULL);

    my_dtrmv(CblasRowMajor, CblasLower, CblasNoTrans, CblasNonUnit, N, A, N, X,
             1);

    gettimeofday(&end, NULL);
    double elapsed= (end.tv_sec - start.tv_sec) +
        (end.tv_usec - start.tv_usec)/1000000.0f;

    fprintf(stdout,"%lg\n", elapsed);

    FILE *rezfile = fopen(argv[2], "w");

    int i;
    for (i = 0; i < N; ++i)
        fprintf(rezfile, "%lg ", X[i]);
    fprintf(rezfile, "\n");
    fclose(rezfile);

    free(A);
    free(X);
    return 0;
}
