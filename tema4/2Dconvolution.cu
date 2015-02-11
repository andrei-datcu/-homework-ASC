/* Nume: Datcu Andrei Daniel
   Grupa: 331CC
   Tema 4 ASC
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <helper_cuda.h>
#include <helper_timer.h>
#include <helper_functions.h>
#include <helper_math.h>

// includes, project
#include "2Dconvolution.h"


////////////////////////////////////////////////////////////////////////////////
// declarations, forward

extern "C"
void computeGold(float*, const float*, const float*, unsigned int, unsigned int);

Matrix AllocateDeviceMatrix(int width, int height);
Matrix AllocateMatrix(int width, int height);
void FreeDeviceMatrix(Matrix* M);
void FreeMatrix(Matrix* M);

void ConvolutionOnDevice(const Matrix M, const Matrix N, Matrix P);
void ConvolutionOnDeviceShared(const Matrix M, const Matrix N, Matrix P);


__device__ inline bool in_bounds(int row, int col, int height, int width){
    return row >= 0 && row < height && col >=0 && col < width;
}

////////////////////////////////////////////////////////////////////////////////
// Înmulțirea fără memorie partajată
////////////////////////////////////////////////////////////////////////////////
__global__ void ConvolutionKernel(Matrix M, Matrix N, Matrix P)
{

    float Cvalue = 0;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    if (!in_bounds(row, col, N.height, N.width))
        return;//Daca nu fac parte din matrice nu am ce calcula

    for (int i = row - KERNEL_SIZE / 2, ci = 0; i <=  row + KERNEL_SIZE / 2; ++i, ++ci)
        for (int j = col - KERNEL_SIZE / 2, cj = 0; j <=  col + KERNEL_SIZE /2; ++j, ++cj)
            Cvalue += M[ci][cj] * (in_bounds(i, j, N.height, N.width) ? N[i][j] : 0);

    P[row][col] = Cvalue;
}


////////////////////////////////////////////////////////////////////////////////
// Înmulțirea cu memorie partajată
////////////////////////////////////////////////////////////////////////////////
__global__ void ConvolutionKernelShared(Matrix M, Matrix N, Matrix P)
{

    unsigned int startOffsetY = blockIdx.y * BLOCK_SIZE;
    unsigned int startOffsetX = blockIdx.x * BLOCK_SIZE;

    if (startOffsetY >= N.height || startOffsetX  >= N.width)
        return; //Acest block nu face parte din imagine

    //Dimensiunile actuale ale blocului, la margini - cand nu mai e loc pentru
    //block full
    int actualBSX = min(N.width - startOffsetX, (unsigned int)BLOCK_SIZE);
    int actualBSY = min(N.height - startOffsetY, (unsigned int)BLOCK_SIZE);

    //liniile si coloanele relative la bloc
    int col = threadIdx.x, row = threadIdx.y;

    __shared__ float Ms[KERNEL_SIZE][KERNEL_SIZE];

    //Matricea M va fi copiata in memoria shared de KERNEL_SIZE * KERNEL_SIZE
    //threaduri incepand de la threadul KERNEL_SIZE/2, KERNEL_SIZE/2

    if (row >= KERNEL_SIZE / 2 && row < KERNEL_SIZE + KERNEL_SIZE / 2 &&
            col >= KERNEL_SIZE / 2 && col < KERNEL_SIZE + KERNEL_SIZE / 2)
        Ms[row - KERNEL_SIZE / 2][col - KERNEL_SIZE / 2] =
            M[row - KERNEL_SIZE / 2][col - KERNEL_SIZE / 2];

    if (col >= actualBSX || row >= actualBSY)
        return;// threadul din acest bloc nu corespunde unui pixel din imagine

    __shared__ float Ns[BLOCK_SIZE+ 2 * (KERNEL_SIZE / 2)]
                        [BLOCK_SIZE + 2 * (KERNEL_SIZE / 2)];

    //Calculam indicii relativ la coltul dreapta sus al imaginii
    int fullCol = startOffsetX + col, fullRow = startOffsetY + row;

    // Aduc datele in memoria shared

    // Fiecare thread isi aduce pixelul corespunzator in memoria shared
    Ns[row + KERNEL_SIZE / 2][col + KERNEL_SIZE / 2] = N[fullRow][fullCol];

    // Threadurile de pe randurile din margine aduc
    //fiecare cate unul in plus pentru bordare sus
    if (actualBSY >= KERNEL_SIZE / 2){
        if (row < KERNEL_SIZE / 2)
            Ns[row][col + KERNEL_SIZE / 2] =
                (fullRow - KERNEL_SIZE / 2 >= 0) ?
                    N[fullRow - KERNEL_SIZE / 2][fullCol] : 0;
    }
    else //Daca nu sunt suficiente pe margine, unul bordeaza tot
        if (row == 0)
            for (int i = 0; i < KERNEL_SIZE / 2; ++i)
                Ns[i][col + KERNEL_SIZE / 2] =
                    (fullRow + i - KERNEL_SIZE / 2 >= 0) ?
                        N[fullRow + i - KERNEL_SIZE / 2][fullCol] : 0;

    // Threadurile de pe coloanele din margine aduc fiecare cate un pixel
    //plus pentru bordare la stanga
    if (actualBSX >= KERNEL_SIZE / 2){
        if (col < KERNEL_SIZE / 2)
            Ns[row + KERNEL_SIZE / 2][col] =
                (fullCol - KERNEL_SIZE / 2 >= 0) ?
                    N[fullRow][fullCol - KERNEL_SIZE / 2] : 0;
    }
    else if (col == 0) // Daca nu sunt suficiente pe margine unu bordeaza tot
        for (int j = 0; j < KERNEL_SIZE / 2; ++j)
            Ns[row + KERNEL_SIZE / 2][j] =
                (fullCol +j - KERNEL_SIZE / 2 >= 0) ?
                    N[fullRow][fullCol +j - KERNEL_SIZE / 2] : 0;


    // Threadurile din coltul stanga sus bordeaza stanga sus
    if (col == 0 && row == 0)
        for (int dy = - KERNEL_SIZE / 2, sy = 0; dy < 0; ++dy, ++sy)
            for (int dx = - KERNEL_SIZE /2, sx = 0; dx < 0; ++dx, ++sx)
                if (fullRow + dy >= 0 && fullCol + dx >= 0)
                    Ns[sy][sx] = N[fullRow + dy][fullCol + dx];
                else
                    Ns[sy][sx] = 0;

    // Threadurile din coltul stanga jos bordeaza stanga jos
    if (col == 0 && row == actualBSY - 1)
        for (int dy = 1, sy = actualBSY + KERNEL_SIZE / 2;
             dy <= KERNEL_SIZE / 2; ++dy, ++sy)
            for (int dx = - KERNEL_SIZE /2, sx = 0; dx < 0; ++dx, ++sx)
                if (fullRow + dy < N.height && fullCol + dx >= 0)
                    Ns[sy][sx] = N[fullRow + dy][fullCol + dx];
                else
                    Ns[sy][sx] = 0;

    //Bordare jos la fel ca mai sus
    if (actualBSY >= KERNEL_SIZE / 2){
        if (row >= actualBSY - KERNEL_SIZE / 2)
            Ns[row + 2 * (KERNEL_SIZE/2)][col + KERNEL_SIZE / 2] =
                (fullRow + KERNEL_SIZE / 2 < N.height) ?
                    N[fullRow + KERNEL_SIZE / 2][fullCol] : 0;
    }
    else if (row == actualBSY - 1)
        for (int i = 0; i < KERNEL_SIZE / 2; ++i)
            Ns[i + actualBSY + KERNEL_SIZE / 2][col + KERNEL_SIZE / 2] =
                (fullRow + i + 1 < N.height) ? N[fullRow + i + 1][fullCol] : 0;


    //Bordare dreapta la fel ca mai sus
    if (actualBSX >= KERNEL_SIZE / 2){
        if (col >= actualBSX - KERNEL_SIZE / 2)
            Ns[row + KERNEL_SIZE / 2][col + 2 * (KERNEL_SIZE / 2)] =
                (fullCol + KERNEL_SIZE / 2 < N.width) ?
                    N[fullRow][fullCol + KERNEL_SIZE / 2] : 0;
    }
    else if (col == actualBSX - 1)
        for (int j = 0; j < KERNEL_SIZE / 2; ++j)
            Ns[row + KERNEL_SIZE / 2][actualBSX + j + KERNEL_SIZE / 2] =
                (fullCol + j + 1 < N.width) ? N[fullRow][fullCol + j + 1] : 0;

    //Bordare dreapta sus
    if (col == actualBSX - 1 && row == 0)
        for (int dy = - KERNEL_SIZE / 2, sy = 0; dy < 0; ++dy, ++sy)
            for (int dx = 1, sx = actualBSX + KERNEL_SIZE / 2;
                    dx <= KERNEL_SIZE / 2; ++dx, ++sx)
                if (fullRow + dy >= 0 && fullCol + dx < N.width)
                    Ns[sy][sx] = N[fullRow + dy][fullCol + dx];
                else
                    Ns[sy][sx] = 0;

    //Bordare stanga sus
    if (col == actualBSX - 1 && row == actualBSY - 1)
        for (int dy = 1, sy = actualBSY + KERNEL_SIZE / 2;
                dy <= KERNEL_SIZE / 2; ++dy, ++sy)
            for (int dx = 1, sx = actualBSX + KERNEL_SIZE / 2;
                    dx <= KERNEL_SIZE / 2; ++dx, ++sx)
                if (fullRow + dy < N.height && fullCol + dx < N.width)
                    Ns[sy][sx] = N[fullRow + dy][fullCol + dx];
                else
                    Ns[sy][sx] = 0;

    __syncthreads();

    float C = 0;

    for (int dy = row, cy = 0; dy < row + KERNEL_SIZE; ++dy, ++cy)
        for (int dx = col, cx = 0; dx < col + KERNEL_SIZE; ++dx, ++cx)
            C += Ms[cy][cx] * Ns[dy][dx];

    //Punem valorea finala a pixelului in imagine
    P[fullRow][fullCol] = C;
}

////////////////////////////////////////////////////////////////////////////////
// Returnează 1 dacă matricele sunt ~ egale
////////////////////////////////////////////////////////////////////////////////
int CompareMatrices(Matrix A, Matrix B)
{
    int i;
    if(A.width != B.width || A.height != B.height || A.pitch != B.pitch)
        return 0;
    int size = A.width * A.height;
    for(i = 0; i < size; i++)
        if(fabs(A.elements[i] - B.elements[i]) > MAX_ERR)
            return 0;
    return 1;
}
void GenerateRandomMatrix(Matrix m)
{
    int i;
    int size = m.width * m.height;

    srand(time(NULL));

    for(i = 0; i < size; i++)
        m.elements[i] = rand() / (float)RAND_MAX;
}

////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    int width = 0, height = 0;
    FILE *f, *out;
    if(argc < 2)
    {
        printf("Argumente prea puține, trimiteți id-ul testului care trebuie rulat\n");
        return 0;
    }
    char name[100];
    sprintf(name, "./tests/test_%s.txt", argv[1]);
    f = fopen(name, "r");
    out = fopen("out.txt", "a");
    fscanf(f, "%d%d", &width, &height);
    Matrix M;//kernel de pe host
    Matrix N;//matrice inițială de pe host
    Matrix P;//rezultat fără memorie partajată calculat pe GPU
    Matrix PS;//rezultatul cu memorie partajată calculat pe GPU

    M = AllocateMatrix(KERNEL_SIZE, KERNEL_SIZE);
    N = AllocateMatrix(width, height);
    P = AllocateMatrix(width, height);
    PS = AllocateMatrix(width, height);

    GenerateRandomMatrix(M);
    GenerateRandomMatrix(N);


    // M * N pe device
    ConvolutionOnDevice(M, N, P);

    // M * N pe device cu memorie partajată
    ConvolutionOnDeviceShared(M, N, PS);

    // calculează rezultatul pe CPU pentru comparație
    Matrix reference = AllocateMatrix(P.width, P.height);
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);
    computeGold(reference.elements, M.elements, N.elements, N.height, N.width);
    gettimeofday(&t2, NULL);

    fprintf (stderr, "Timp execuție cpu: %lf ms\n",(t2.tv_sec - (t1).tv_sec) *
             1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0);

    // verifică dacă rezultatul obținut pe device este cel așteptat
    int res = CompareMatrices(reference, P);
    printf("Test global %s\n", (1 == res) ? "PASSED" : "FAILED");
    fprintf(out, "Test global %s %s\n", argv[1], (1 == res) ? "PASSED" : "FAILED");

    // verifică dacă rezultatul obținut pe device cu memorie partajată este cel așteptat
    int ress = CompareMatrices(reference, PS);
    printf("Test shared %s\n", (1 == ress) ? "PASSED" : "FAILED");
    fprintf(out, "Test shared %s %s\n", argv[1], (1 == ress) ? "PASSED" : "FAILED");

    // Free matrices
    FreeMatrix(&M);
    FreeMatrix(&N);
    FreeMatrix(&P);
    FreeMatrix(&PS);

    fclose(f);
    fclose(out);
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
//! Run a simple test for CUDA
////////////////////////////////////////////////////////////////////////////////
void ConvolutionOnDevice(const Matrix M, const Matrix N, Matrix P)
{
    Matrix Md, Nd, Pd; //matricele corespunzătoare de pe device

    //pentru măsurarea timpului de execuție în kernel
    StopWatchInterface *kernelTime = NULL;
    sdkCreateTimer(&kernelTime);
    sdkResetTimer(&kernelTime);
    //: alocați matricele de pe device

    Md = AllocateDeviceMatrix(M.width, M.height);
    Nd = AllocateDeviceMatrix(N.width, N.height);
    Pd = AllocateDeviceMatrix(P.width, P.height);

    //: copiați datele de pe host (M, N) pe device (MD, Nd)
    cudaMemcpy(Md.elements, M.elements, M.sizeInBytes(), cudaMemcpyHostToDevice);
    cudaMemcpy(Nd.elements, N.elements, N.sizeInBytes(), cudaMemcpyHostToDevice);

    //: setați configurația de rulare a kernelului
    dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);
    dim3 dimGrid(N.width / dimBlock.x + 1, N.height / dimBlock.y + 1);

    sdkStartTimer(&kernelTime);
    //: lansați în execuție kernelul
    ConvolutionKernel<<<dimGrid, dimBlock>>>(Md, Nd, Pd);
    cudaThreadSynchronize();
    sdkStopTimer(&kernelTime);
    fprintf (stderr, "Timp execuție kernel: %f ms\n", sdkGetTimerValue(&kernelTime));
    //: copiaţi rezultatul pe host
    cudaMemcpy(P.elements, Pd.elements, P.sizeInBytes(), cudaMemcpyDeviceToHost);
    //: eliberați memoria matricelor de pe device
    FreeDeviceMatrix(&Md);
    FreeDeviceMatrix(&Nd);
    FreeDeviceMatrix(&Pd);
}


void ConvolutionOnDeviceShared(const Matrix M, const Matrix N, Matrix P)
{
    Matrix Md, Nd, Pd; //matricele corespunzătoare de pe device

    //pentru măsurarea timpului de execuție în kernel
    StopWatchInterface *kernelTime = NULL;
    sdkCreateTimer(&kernelTime);
    sdkResetTimer(&kernelTime);
    //: alocați matricele de pe device
    Md = AllocateDeviceMatrix(M.width, M.height);
    Nd = AllocateDeviceMatrix(N.width, N.height);
    Pd = AllocateDeviceMatrix(P.width, P.height);

    //: copiați datele de pe host (M, N) pe device (MD, Nd)
    cudaMemcpy(Md.elements, M.elements, M.sizeInBytes(), cudaMemcpyHostToDevice);
    cudaMemcpy(Nd.elements, N.elements, N.sizeInBytes(), cudaMemcpyHostToDevice);

    //: setați configurația de rulare a kernelului
    dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);
    dim3 dimGrid(N.width / dimBlock.x + 1, N.height / dimBlock.y + 1);

    sdkStartTimer(&kernelTime);
    //: lansați în execuție kernelul
    ConvolutionKernelShared<<<dimGrid, dimBlock>>>(Md, Nd, Pd);

    cudaThreadSynchronize();
    sdkStopTimer(&kernelTime);
    fprintf (stderr, "Timp execuție kernel cu memorie partajată: %f ms\n",
             sdkGetTimerValue(&kernelTime));
    //: copiaţi rezultatul pe host
    cudaMemcpy(P.elements, Pd.elements, P.sizeInBytes(), cudaMemcpyDeviceToHost);
    //: eliberați memoria matricelor de pe device

    FreeDeviceMatrix(&Md);
    FreeDeviceMatrix(&Nd);
    FreeDeviceMatrix(&Pd);
}


// Alocă o matrice de dimensiune height*width pe device
Matrix AllocateDeviceMatrix(int width, int height)
{
    Matrix M;
    M.width = M.pitch = width;
    M.height = height;
    int size = M.width * M.height;
    cudaMalloc((void**)&M.elements, size * sizeof(float));
    return M;
}

// Alocă matrice pe host de dimensiune height*width
Matrix AllocateMatrix(int width, int height)
{
    Matrix M;
    M.width = M.pitch = width;
    M.height = height;
    int size = M.width * M.height;
    M.elements = (float*) malloc(size*sizeof(float));
    return M;
}

// Eliberează o matrice de pe device
void FreeDeviceMatrix(Matrix* M)
{
    cudaFree(M->elements);
    M->elements = NULL;
}

// Eliberează o matrice de pe host
void FreeMatrix(Matrix* M)
{
    free(M->elements);
    M->elements = NULL;
}
