
typedef enum {CblasRowMajor=0, CblasColMajor=1} CBLAS_LAYOUT;
typedef enum {CblasNoTrans=0, CblasTrans=1, CblasConjTrans=2} CBLAS_TRANSPOSE;
typedef enum {CblasUpper=0, CblasLower=1} CBLAS_UPLO;
typedef enum {CblasNonUnit=0, CblasUnit=1} CBLAS_DIAG;
typedef enum {CblasLeft=0, CblasRight=1} CBLAS_SIDE;

void cblas_dcopy(int N, const double* x, int incx, double* y, int incy){
  int i,ix,iy;
  ix=0; iy=0;
  for(i=0; i<N; i++){
    y[iy]=x[ix];
    ix+=incx; iy+=incy;
  }
}

void cblas_daxpy(int N, double alpha, const double* x, int incx, double* y, int incy){
  int i,ix,iy;
  ix=0; iy=0;
  for(i=0; i<N; i++){
    y[iy]=alpha*x[ix]+y[iy];
    ix+=incx; iy+=incy;
  }
}

void cblas_dscal(int N, double alpha, double* x, int incx){
  int i,ix;
  ix=0;
  for(i=0; i<N; i++){
    x[ix]*=alpha;
    ix+=incx;
  }
}

double cblas_ddot(int N,
  const double* x, int incx,
  const double* y, int incy
){
  int i,ix,iy;
  double r=0;
  ix=0; iy=0;
  for(i=0; i<N; i++){
    r+=x[ix]*y[iy];
    ix+=incx; iy+=incy;
  }
  return r;
}

void cblas_dgemv(CBLAS_LAYOUT layout, CBLAS_TRANSPOSE TransA,
  int M, int N, double alpha, const double *A, int lda,
  const double *X, int incX, double beta,
  double *Y, int incY
){
}

void cblas_dgemm(CBLAS_LAYOUT layout, CBLAS_TRANSPOSE TransA,
  CBLAS_TRANSPOSE TransB, int M, int N, int K,
  double alpha, const double *A, int lda, const double *B, int ldb,
  double beta, double *C, int ldc
){
}

