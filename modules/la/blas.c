
typedef enum {CblasRowMajor=0, CblasColMajor=1} CBLAS_LAYOUT;
typedef enum {CblasNoTrans=0, CblasTrans=1, CblasConjTrans=2} CBLAS_TRANSPOSE;
typedef enum {CblasUpper=0, CblasLower=1} CBLAS_UPLO;
typedef enum {CblasNonUnit=0, CblasUnit=1} CBLAS_DIAG;
typedef enum {CblasLeft=0, CblasRight=1} CBLAS_SIDE;

// y:=x
void cblas_dcopy(int N, const double* x, int incx, double* y, int incy){
    int i,ix,iy;
    ix = 0; iy = 0;
    for(i=0; i<N; i++){
        y[iy] = x[ix];
        ix+=incx; iy+=incy;
    }
}

// y:=alpha*x+y
void cblas_daxpy(int N, double alpha, const double* x, int incx, double* y, int incy){
    int i,ix,iy;
    ix=0; iy=0;
    for(i=0; i<N; i++){
        y[iy] = alpha*x[ix]+y[iy];
        ix+=incx; iy+=incy;
    }
}

// x:=alpha*x
void cblas_dscal(int N, double alpha, double* x, int incx){
    int i,ix;
    ix = 0;
    for(i=0; i<N; i++){
        x[ix]*=alpha;
        ix+=incx;
    }
}

// y:=x*y
double cblas_ddot(int N,
    const double* x, int incx,
    const double* y, int incy
){
    int i,ix,iy;
    double r = 0;
    ix = 0; iy = 0;
    for(i=0; i<N; i++){
        r+=x[ix]*y[iy];
        ix+=incx; iy+=incy;
    }
    return r;
}

// y:=alpha*A*x+beta*y
void cblas_dgemv(CBLAS_LAYOUT layout, CBLAS_TRANSPOSE TransA,
    int M, int N, double alpha, const double *A, int lda,
    const double *x, int incx, double beta,
    double *y, int incy
){
    int i;
    if(beta==0){
        for(i=0; i<M; i++){
            y[i*incy] = alpha*cblas_ddot(N,A+i,lda,x,incx);
        }
    }else{
        // If beta is zero, y may be uninitialized. This is serious,
        // one would need "valgrind --track-origins=yes" to find this bug.
        for(i=0; i<M; i++){
            y[i*incy] = alpha*cblas_ddot(N,A+i,lda,x,incx)+beta*y[i*incy];
        }
    }
}

// C:=alpha*A*B+beta*C
// A: M*K, B: K*N, C: M*N
void cblas_dgemm(CBLAS_LAYOUT layout, CBLAS_TRANSPOSE TransA,
    CBLAS_TRANSPOSE TransB, int M, int N, int K,
    double alpha, const double *A, int lda, const double *B, int ldb,
    double beta, double *C, int ldc
){
    int j;
    for(j=0; j<N; j++){
        cblas_dgemv(CblasColMajor,CblasNoTrans,
            M,K,alpha,A,lda,B+j*ldb,1,beta,C+j*ldc,1);
    }
}

