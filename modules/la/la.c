
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
  #include "blas.c"
#else
  #include "blas.c"
  // #include <cblas.h>
#endif
#include <moss.h>
#include <objects/string.h>
#include <objects/list.h>
#include <modules/bs.h>
#include <modules/la.h>

double mf_float(mt_object* x, int* e);
extern mt_table* mv_type_array;

void mf_array_delete(mt_array* a){
  if(a->data->refcount==1){
    mf_free(a->data);
  }else{
    a->data->refcount--;
  }
  mf_free(a);
}

void mf_array_dec_refcount(mt_array* a){
  if(a->refcount==1){
    mf_array_delete(a);
  }else{
    a->refcount--;
  }
}

static
mt_array* mf_new_array(long size, int type){
  mt_array* a = mf_malloc(sizeof(mt_array));
  if(type==mv_array_float){
    a->data = mf_malloc(sizeof(mt_array_data)+size*sizeof(double));
  }else{
    abort();
  }
  a->data->refcount=1;
  a->data->type=type;
  a->base=a->data->a;
  a->refcount=1;
  a->size=size;
  return a;
}

static
mt_array* mf_array_copy(mt_array* a){
  mt_array* b;
  if(a->n==2){
    if(a->plain){
      goto plain;
    }else{
      unsigned long m=a->shape[0];
      unsigned long n=a->shape[1];
      long si=a->stride[0];
      long sj=a->stride[1];
      b = mf_new_array(m*n,mv_array_float);
      b->n=a->n; b->plain=1;
      b->shape[0]=m; b->shape[1]=n;
      b->stride[0]=1; b->stride[1]=m;
      double* pa=(double*)a->base;
      double* pb=(double*)b->base;
      unsigned long i,j,mj;
      long sjj;
      for(j=0; j<n; j++){
        mj=m*j; sjj=sj*j;
        for(i=0; i<m; i++){
          pb[i+mj]=pa[si*i+sjj];
        }
      }
      return b;
    }
  }else if(a->n==1){
    b = mf_new_array(a->shape[0],mv_array_float);
    b->n=1;
    b->shape[0]=a->shape[0];
    b->stride[0]=1;
    cblas_dcopy(a->shape[0],(double*)a->base,a->stride[0],(double*)b->base,1);
    return b;
  }else{
    if(a->plain) goto plain;
    abort();
  }

  plain:
  b = mf_new_array(a->size,mv_array_float);
  b->n=a->n;
  b->plain=1;
  unsigned long i;
  for(i=0; i<a->n; i++){
    b->shape[i]=a->shape[i];
    b->stride[i]=a->stride[i];
  }
  cblas_dcopy(a->size,(double*)a->base,1,(double*)b->base,1);
  return b;
}

void mf_ensure_plain(mt_array* a){
  if(a->n==2){
    if(a->plain) return;
    mt_array_data* data = mf_malloc(sizeof(mt_array_data)+a->size*sizeof(double));
    data->refcount=1;
    data->type=mv_array_float;
    double* pa = (double*)a->base;
    double* pb = (double*)data->a;

    unsigned long m=a->shape[0];
    unsigned long n=a->shape[1];
    long si=a->stride[0];
    long sj=a->stride[1];
    unsigned long i,j,mj;
    long sjj;
    for(j=0; j<n; j++){
      mj=m*j; sjj=sj*j;
      for(i=0; i<m; i++){
        pb[i+mj]=pa[si*i+sjj];
      }
    }
    if(a->data->refcount==1){
      mf_free(a->data);
    }else{
      a->data->refcount--;
    }
    a->stride[0]=1;
    a->stride[1]=m;
    a->data=data;
    a->base=data->a;
    a->plain=1;
  }else if(a->n==1){
    abort();
  }else{
    if(a->plain) return;
    abort();
  }
}

static
mt_array* mf_array_map_dd(mt_array* a, double(*f)(double)){
  mt_array* b;
  if(a->n==2){
    if(a->plain){
      goto plain;
    }else{
      unsigned long m=a->shape[0];
      unsigned long n=a->shape[1];
      long si=a->stride[0];
      long sj=a->stride[1];
      b = mf_new_array(m*n,mv_array_float);
      b->n=a->n; b->plain=1;
      b->shape[0]=m; b->shape[1]=n;
      b->stride[0]=1; b->stride[1]=m;
      double* pa=(double*)a->base;
      double* pb=(double*)b->base;
      unsigned long i,j,mj;
      long sjj;
      for(j=0; j<n; j++){
        mj=m*j; sjj=sj*j;
        for(i=0; i<m; i++){
          pb[i+mj]=f(pa[si*i+sjj]);
        }
      }
      return b;
    }
  }else if(a->n==1){
    unsigned long n=a->shape[0];
    b = mf_new_array(n,mv_array_float);
    b->n=1; b->shape[0]=n;
    b->stride[0]=1;
    long s=a->stride[0];
    unsigned long k;
    double* pa=(double*)a->base;
    double* pb=(double*)b->base;
    for(k=0; k<n; k++){
      pb[k]=f(pa[k*s]);
    }
    return b;
  }else{
    if(a->plain) goto plain;
    abort();
  }

  plain:
  b = mf_new_array(a->size,mv_array_float);
  b->n=a->n;
  b->plain=1;
  unsigned long i;
  for(i=0; i<a->n; i++){
    b->shape[i]=a->shape[i];
    b->stride[i]=a->stride[i];
  }
  double* pa=(double*)a->base;
  double* pb=(double*)b->base;
  unsigned long size=a->size;
  unsigned long k;
  for(k=0; k<size; k++){
    pb[k]=f(pa[k]);
  }
  return b;
}

long mf_array_size(mt_array* a){
  long p;
  unsigned long k;
  switch(a->n){
  case 1:
    return a->shape[0];
  case 2:
    return a->shape[0]*a->shape[1];
  default:
    p=1;
    for(k=0; k<a->n; k++){
      p=p*a->shape[k];
    }
    return p;
  }
}

mt_array* mf_array(long size, mt_object* v){
  mt_array* a;
  if(size>0 && v[0].type==mv_list){
    long i,j;
    mt_list* list = (mt_list*)v[0].value.p;
    long n = list->size;
    a = mf_new_array(size*n,mv_array_float);
    a->n=2;
    a->shape[0]=size; a->shape[1]=n;
    a->stride[0]=1; a->stride[1]=size;
    a->plain=1;
    double* b = (double*)a->base;
    int e=0;
    for(i=0; i<size; i++){
      if(v[i].type!=mv_list) abort();
      list = (mt_list*)v[i].value.p;
      if(list->size!=n) abort();
      for(j=0; j<n; j++){
        b[i+size*j]=mf_float(list->a+j,&e);
        if(e) abort();
      }
    }
  }else{
    a = mf_new_array(size,mv_array_float);
    a->n=1;
    a->shape[0]=size;
    a->stride[0]=1;
    long i;
    double* b = (double*)a->base;
    int e=0;
    for(i=0; i<size; i++){
      b[i] = mf_float(v+i,&e);
    }
  }
  return a;
}

int la_array(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array");
    return 1;
  }
  if(v[1].type!=mv_list){
    mf_type_error1("in array(a): a (type: %s) is not a list.",v+1);
    return 1;
  }
  mt_list* list=(mt_list*)v[1].value.p;
  mt_array* a = mf_array(list->size, list->a);
  if(a==NULL) return 1;
  x->type=mv_array;
  x->value.p=(mt_basic*)a;
  return 0;
}

int la_vector(mt_object* x, int argc, mt_object* v){
  mt_array* a = mf_array(argc,v+1);
  if(a==NULL) return 1;
  x->type=mv_array;
  x->value.p=(mt_basic*)a;
  return 0;
}

int la_matrix(mt_object* x, int argc, mt_object* v){
  mt_array* a = mf_array(argc,v+1);
  if(a==NULL) return 1;
  if(a->n==1){
    a->n=2;
    unsigned long size = a->shape[0];
    a->shape[0]=size;
    a->shape[1]=1;
    a->stride[0]=1;
    a->stride[1]=size;
    a->plain=0;
  }
  x->type=mv_array;
  x->value.p=(mt_basic*)a;
  return 0;
}

static
void vector_str(mt_bs* bs, mt_array* T){
  char buffer[100];
  unsigned long i;
  double* a = (double*)T->base;
  unsigned long size = T->shape[0];
  long step=T->stride[0];

  mf_bs_push_cstr(bs,"[");
  for(i=0; i<size; i++){
    snprintf(buffer,100,i==0?"%.4g":", %.4g",a[step*i]);
    mf_bs_push_cstr(bs,buffer);
  }
  mf_bs_push_cstr(bs,"]");
}

static
void matrix_str(mt_bs* bs, mt_array* T){
  char buffer[100];
  unsigned long i,j;
  double* a = (double*)T->base;
  unsigned long m,n,is,js;
  m = T->shape[0];
  n = T->shape[1];
  is = T->stride[0];
  js = T->stride[1];
  int max=0;
  int size;
  for(i=0; i<m; i++){
    for(j=0; j<n; j++){
      size = snprintf(buffer,100,"%.4g",a[i*is+j*js]);
      if(size>max) max=size;
    }
  }
  mf_bs_push_cstr(bs,"[");
  for(i=0; i<m; i++){
    mf_bs_push_cstr(bs,i==0? "[": ",\n [");
    for(j=0; j<n; j++){
      snprintf(buffer,100,j==0?"%*.4g":", %*.4g",max,a[i*is+j*js]);
      mf_bs_push_cstr(bs,buffer);
    }
    mf_bs_push_cstr(bs,"]");
  }
  mf_bs_push_cstr(bs,"]");
}

static
int array_str(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"array.type.str");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a.str(): a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* T = (mt_array*)v[0].value.p;
  mt_bs bs;
  mf_bs_init(&bs);
  if(T->n==1){
    vector_str(&bs,T);
  }else if(T->n==2){
    matrix_str(&bs,T);
  }else{
    abort();
  }

  x->type = mv_string;
  x->value.p = (mt_basic*)mf_str_new(bs.size,(const char*)bs.a);
  mf_free(bs.a);
  return 0;
}

static
int array_list(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"array.type.list");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a.list(): a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* T = (mt_array*)v[0].value.p;
  mt_list* list = mf_raw_list(T->size);
  unsigned long i;
  double* a = (double*)T->base;
  unsigned long size = T->shape[0];
  long step=T->stride[0];
  for(i=0; i<size; i++){
    list->a[i].type=mv_float;
    list->a[i].value.f=a[step*i];
  }
  
  x->type = mv_list;
  x->value.p = (mt_basic*)list;
  return 0;
}

double addc;
double add(double x){
  return x+addc;
}
double subc;
double sub(double x){
  return x-subc;
}

static
int array_ADD(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array.type.ADD");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a+b: a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* a=(mt_array*)v[0].value.p;
  mt_array* c;
  if(v[1].type!=mv_array){
    int e=0;
    addc = mf_float(&v[1],&e);
    if(e){
      mf_type_error1("in a+r: cannot convert r (type: %s) to float.",&v[1]);
      return 1;
    }
    c = mf_array_map_dd(a,add);
    if(c==NULL) return 1;
    x->type=mv_array;
    x->value.p=(mt_basic*)c;
    return 0;
  }
  mt_array* b=(mt_array*)v[1].value.p;
  if(a->n!=b->n){
    mf_value_error("value error in a+b: a and b have unequal order.");
    return 1;
  }
  if(a->n==1){
    unsigned long size=a->shape[0];
    if(size!=b->shape[0]){
      mf_value_error("Value error in a+b: a and b have unequal shape.");
      return 1;
    }
    c = mf_new_array(size,mv_array_float);
    c->n=1; c->stride[0]=1;
    c->shape[0]=size;
    cblas_dcopy(size,(double*)a->base,a->stride[0],(double*)c->base,1);
    cblas_daxpy(size,1,(double*)b->base,b->stride[0],(double*)c->base,1);
  }else{
    unsigned long i;
    for(i=0; i<a->n; i++){
      if(a->shape[i]!=b->shape[i]) abort();
    }
    c = mf_array_copy(a);
    mf_ensure_plain(b);
    cblas_daxpy(c->size,1,(double*)b->base,1,(double*)c->base,1);
  }
  x->type=mv_array;
  x->value.p=(mt_basic*)c;
  return 0;
}

static
int array_RADD(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"Array.RMPY");
    return 1;
  }
  if(v[1].type!=mv_array){
    mf_type_error1("in r*a: a (type: %s) is not an array.",&v[1]);
    return 1;
  }
  int e=0;
  addc = mf_float(&v[0],&e);
  if(e){
    mf_type_error1("in r+a: cannot convert r (type: %s) to float.",&v[0]);
    return 1;
  }
  mt_array* a = (mt_array*)v[1].value.p;
  mt_array* y = mf_array_map_dd(a,add);
  if(y==NULL) return 1;
  x->type = mv_array;
  x->value.p = (mt_basic*)y;
  return 0;
}

static
int array_SUB(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array.type.SUB");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a-b: a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* a=(mt_array*)v[0].value.p;
  mt_array* c;
  if(v[1].type!=mv_array){
    int e=0;
    subc = mf_float(&v[1],&e);
    if(e){
      mf_type_error1("in a-r: cannot convert r (type: %s) to float.",&v[1]);
      return 1;
    }
    c = mf_array_map_dd(a,sub);
    if(c==NULL) return 1;
    x->type=mv_array;
    x->value.p=(mt_basic*)c;
    return 0;
  }
  mt_array* b=(mt_array*)v[1].value.p;
  if(a->n==1){
    unsigned long size=a->shape[0];
    if(size!=b->shape[0]){
      mf_value_error("Value error in a-b: a and b have unequal shape.");
      return 1;
    }

    c = mf_new_array(size,mv_array_float);
    c->n=1; c->shape[0]=size;
    c->stride[0]=1;
    cblas_dcopy(size,(double*)a->base,a->stride[0],(double*)c->base,1);
    cblas_daxpy(size,-1,(double*)b->base,b->stride[0],(double*)c->base,1);
  }else{
    unsigned long i;
    for(i=0; i<a->n; i++){
      if(a->shape[i]!=b->shape[i]) abort();
    }
    c = mf_array_copy(a);
    mf_ensure_plain(b);
    cblas_daxpy(c->size,-1,(double*)b->base,1,(double*)c->base,1);
  }
  x->type=mv_array;
  x->value.p=(mt_basic*)c;
  return 0;
}

static
mt_array* mf_array_scal(double r, mt_array* a){
  mt_array* b;
  if(a->n==1){
    unsigned long size = a->shape[0];
    b = mf_new_array(size,mv_array_float);
    b->n=1; b->shape[0]=size;
    b->stride[0]=1;
    cblas_dcopy(size,(double*)a->base,a->stride[0],(double*)b->base,1);
  }else{
    b=mf_array_copy(a);
  }
  cblas_dscal(b->size,r,(double*)b->base,1);
  return b;
}

static
mt_array* array_mpy_array(mt_array* a, mt_array* b){
  if(b->n==1){
    unsigned long m = a->shape[0];
    unsigned long n = a->shape[1];
    if(n!=b->shape[0]){
      mf_value_error("Value error in A*x: shape does not match.");
      return NULL;
    }
    if(a->stride[0]!=1){
      mf_ensure_plain(a);
    }
    mt_array* c = mf_new_array(m,mv_array_float);
    c->n=1; c->shape[0]=m;
    c->stride[0]=1;
    cblas_dgemv(CblasColMajor,CblasNoTrans,m,n,1,
      (double*)a->base, a->stride[1],
      (double*)b->base, b->stride[0],
      0,(double*)c->base,1
    );
    return c;
  }else if(b->n==2){
    if(a->n==2){
      if(a->shape[1]!=b->shape[0]){
        mf_value_error("Value error in A*B: incompatible matrices.");
        return NULL;
      }
      if(a->stride[0]!=1){
        mf_ensure_plain(a);
      }
      if(b->stride[0]!=1){
        mf_ensure_plain(b);
      }

      unsigned long m = a->shape[0];
      unsigned long k = a->shape[1];
      unsigned long n = b->shape[1];
      mt_array* c = mf_new_array(m*n,mv_array_float);
      c->n=2; c->shape[0]=m; c->shape[1]=n;
      c->stride[0]=1; c->stride[1]=m;
      c->plain=1;
      cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1,
        (double*)a->base, a->stride[1],
        (double*)b->base, b->stride[1],
        0, (double*)c->base, m
      );
      return c;
    }else{
      abort();
    }
  }else{
    abort();
  }
}

static
mt_array* matrix_identity(unsigned long n){
  mt_array* a = mf_new_array(n*n,mv_array_float);
  a->n=2; a->plain=1;
  a->shape[0]=n; a->shape[1]=n;
  a->stride[0]=1; a->stride[1]=n;
  double* b = (double*)a->base;
  unsigned long i,j,nj;
  for(j=0; j<n; j++){
    nj=n*j;
    for(i=0; i<n; i++){
      b[i+nj] = i==j? 1: 0;
    }
  }
  return a;
}

static
mt_array* square_matrix_mpy(mt_array* a, mt_array* b){
  if(a->stride[0]!=1){
    mf_ensure_plain(a);
  }
  if(b->stride[0]!=1){
    mf_ensure_plain(b);
  }
  unsigned long n = a->shape[0];
  mt_array* c = mf_new_array(n*n,mv_array_float);
  c->n=2; c->shape[0]=n; c->shape[1]=n;
  c->stride[0]=1; c->stride[1]=n;
  c->plain=1;
  cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, n, n, n, 1,
    (double*)a->base, a->stride[1],
    (double*)b->base, b->stride[1],
    0, (double*)c->base, n
  );
  return c;
}

static
mt_array* square_matrix_pow(mt_array* a, unsigned long n){
  if(n==0){
    return matrix_identity(a->shape[0]);
  }
  mt_array *p,*h;
  p = mf_array_copy(a);
  a = p;
  a->refcount++;
  n--;
  while(n){
    if(n&1){
      h=p;
      p=square_matrix_mpy(p,a);
      mf_array_dec_refcount(h);
    }
    n>>=1;
    if(n==0) break;
    h=a;
    a=square_matrix_mpy(a,a);
    mf_array_dec_refcount(h);
  }
  mf_array_dec_refcount(a);
  return p;
}

static
int array_MPY(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array.type.RMPY");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a*b: a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* a=(mt_array*)v[0].value.p;
  mt_array* b;
  double r;
  if(v[1].type==mv_array){
    b=(mt_array*)v[1].value.p;
    if(a->n==2){
      if(b->n==2 && a->shape[0]==1 && b->shape[1]==1){
        if(a->shape[1]!=b->shape[0]){
          mf_value_error("Value error in a*b: incompatible matrices.");
          return 1;
        }
        // todo: strides
        r = cblas_ddot(a->shape[1],
          (double*)a->base,a->stride[1],
          (double*)b->base,b->stride[0]
        );
        x->type=mv_float;
        x->value.f=r;
        return 0;
      }
      mt_array* c = array_mpy_array(a,b);
      if(c==NULL) return 1;
      x->type=mv_array;
      x->value.p=(mt_basic*)c;
      return 0;
    }
    if(a->n!=1){
      mf_value_error("Value error in a*b: a and b have incompatible shape.");
      return 1;
    }
    if(b->n==2){
      mt_array* c = array_mpy_array(b,a);
      if(c==NULL) return 1;
      x->type=mv_array;
      x->value.p=(mt_basic*)c;
      return 0;
    }
    if(a->shape[0]!=b->shape[0]){
      mf_value_error("Value error in a*b: a and b have unequal shape.");
      return 1;
    }
    r = cblas_ddot(a->shape[0],
      (double*)a->base,a->stride[0],
      (double*)b->base,b->stride[0]
    );
    x->type=mv_float;
    x->value.f=r;
    return 0;
  }else{
    int e=0;
    r = mf_float(&v[1],&e);
    if(e){
      mf_type_error1("in a*r: cannot convert r (type: %s) to float.",&v[1]);
      return 1;
    }
    x->type=mv_array;
    x->value.p=(mt_basic*)mf_array_scal(r,a);
    return 0;
  }
}

static
int array_POW(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array.type.POW");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a^n: a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* a=(mt_array*)v[0].value.p;
  
  double r;
  if(a->n!=1){
    if(a->n==2 && v[1].type==mv_int){
      long n = v[1].value.i;
      if(a->shape[0]!=a->shape[1]){
        mf_value_error("Value error in A^n: A is not a square matrix");
        return 1;
      }
      x->type=mv_array;
      x->value.p=(mt_basic*)square_matrix_pow(a,(unsigned long)n);
      return 0;
    }
    mf_value_error("Value error in a^n: a is not a vector.");
    return 1;
  }
  r = cblas_ddot(a->shape[0],
    (double*)a->base,a->stride[0],
    (double*)a->base,a->stride[0]
  );
  x->type=mv_float;
  if(v[1].type==mv_int && v[1].value.i==2){
    x->value.f=r;
  }else{
    int e=0;
    double n = mf_float(&v[1],&e);
    if(e){
      mf_type_error1("in a^n: cannot convert n (type: %s) to float.",&v[1]);
      return 1;
    }
    x->value.f=pow(r,n);
  }
  return 0;
}

static
int array_RMPY(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array.type.RMPY");
    return 1;
  }
  if(v[1].type!=mv_array){
    mf_type_error1("in r*a: a (type: %s) is not an array.",&v[1]);
    return 1;
  }
  int e=0;
  double r = mf_float(&v[0],&e);
  if(e){
    mf_type_error1("in r*a: cannot convert r (type: %s) to float.",&v[0]);
    return 1;
  }
  mt_array* a=(mt_array*)v[1].value.p;
  x->type=mv_array;
  x->value.p=(mt_basic*)mf_array_scal(r,a);;
  return 0;
}

static
int array_NEG(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"array.type.NEG");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in -a: a (type: %s) is not an array.",&v[1]);
    return 1;
  }
  mt_array* a=(mt_array*)v[0].value.p;
  x->type=mv_array;
  x->value.p=(mt_basic*)mf_array_scal(-1,a);;
  return 0;
}

static
int array_DIV(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array.type.DIV");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a/r: a (type: %s) is not an array.",&v[1]);
    return 1;
  }
  int e=0;
  double r = mf_float(&v[1],&e);
  if(e){
    mf_type_error1("in a/r: cannot convert r (type: %s) to float.",&v[0]);
    return 1;
  }
  mt_array* a=(mt_array*)v[0].value.p;
  x->type=mv_array;
  x->value.p=(mt_basic*)mf_array_scal(1/r,a);;
  return 0;
}

static
int array_T(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"array.type.T");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a.T(): a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* a = (mt_array*)v[0].value.p;
  if(a->n==2){
    mt_array* b = mf_malloc(sizeof(mt_array));
    b->refcount=1;
    b->n=2;
    a->data->refcount++;
    b->data=a->data;
    b->base=a->base;
    b->size=a->size;
    b->plain=0;
    b->shape[0]=a->shape[1];
    b->shape[1]=a->shape[0];
    b->stride[0]=a->stride[1];
    b->stride[1]=a->stride[0];
    x->type=mv_array;
    x->value.p=(mt_basic*)b;
  }else{
    a->refcount++;
    x->type=mv_array;
    x->value.p=(mt_basic*)a;
  }
  return 0;
}

static
int array_plain(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_array) abort();
  mt_array* a = (mt_array*)v[0].value.p;
  mf_ensure_plain(a);
  a->refcount++;
  x->type=mv_array;
  x->value.p=(mt_basic*)a;
  return 0;
}

static
int array_copy(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_array) abort();
  mt_array* a = (mt_array*)v[0].value.p;
  x->type=mv_array;
  x->value.p=(mt_basic*)mf_array_copy(a);
  return 0;
}

static
mt_array* vector_slice(mt_array* a, mt_range* r){
  long i,j;
  if(r->a.type!=mv_int || r->b.type!=mv_int){
    mf_type_error("Type error in a[r]: r is not an integer range.");
    return NULL;
  }
  i=r->a.value.i;
  j=r->b.value.i;
  unsigned long n=a->shape[0];
  if(i<0 || j<0){
    mf_value_error("Value error in a[r]: range with negative values.");
    return NULL;
  }
  if((unsigned long)i>=n || (unsigned long)j>=n){
    mf_value_error("Value error in a[r]: range is out of bounds.");
    return NULL;
  }
  mt_array* b = mf_malloc(sizeof(mt_array));
  b->refcount=1;
  b->size=a->size;
  a->data->refcount++;
  b->data=a->data;
  a->data=b->data;
  b->n=1;
  b->base=(unsigned char*)((double*)a->base+a->stride[0]*i);
  b->shape[0]=j-i+1;
  b->stride[0]=a->stride[0];
  return b;
}

static
int array_GET(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_array){
    mf_type_error1("in a[i]: a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* a = (mt_array*)v[0].value.p;
  if(argc==1){
    if(a->n!=1){
      mf_value_error("Value error in a[i]: a is not a vector.");
      return 1;
    }
    if(v[1].type!=mv_int){
      if(v[1].type==mv_range){
         mt_array* b = vector_slice(a,(mt_range*)v[1].value.p);
         if(b==NULL) return 1;
         x->type=mv_array;
         x->value.p=(mt_basic*)b;
         return 0;
      }
      mf_type_error1("in a[i]: i (type: %s) is not an integer.",&v[1]);
      return 1;
    }
    long index = v[1].value.i;
    unsigned long n = a->shape[0];
    if(index<0 || (unsigned long)index>=n){
      mf_value_error("Value error in a[i]: i is out of bounds.");
      return 1;
    }
    double* b = (double*)a->base;
    double t = b[index*a->stride[0]];
    x->type=mv_float;
    x->value.f=t;
    return 0;
  }else{
    mf_argc_error(argc,1,1,"array.GET");
    return 1;
  }
}

static
int la_idm(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"idm");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in idm(n): n (type: %s) is not an integer.",v+1);
    return 1;
  }
  long n = v[1].value.i;
  x->type=mv_array;
  x->value.p=(mt_basic*)matrix_identity(n);
  return 0;
}

static
mt_array* diag(mt_array* v){
  if(v->n!=1){
    mf_value_error("Value error in diag(v): v is not a vector.");
    return NULL;
  }
  unsigned long n = v->shape[0];
  mt_array* a = mf_new_array(n*n,mv_array_float);
  a->n=2; a->plain=1;
  a->shape[0]=n; a->shape[1]=n;
  a->stride[0]=1; a->stride[1]=n;
  double* b = (double*)a->base;
  double* vb = (double*)v->base;
  long vs = v->stride[0];
  unsigned long i,j,nj;
  for(j=0; j<n; j++){
    nj=n*j;
    for(i=0; i<n; i++){
      b[i+nj] = i==j? vb[i*vs]: 0;
    }
  }
  return a;
}

static
int la_diag(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"diag");
    return 1;
  }
  if(v[1].type!=mv_array){
    mf_type_error1("in diag(a): a is not an array.",v+1);
    return 1;
  }
  mt_array* u = (mt_array*)v[1].value.p;
  mt_array* a = diag(u);
  if(a==NULL) return 1;
  x->type=mv_array;
  x->value.p=(mt_basic*)a;
  return 0;
}

static
mt_array* diag_slice(mt_array* a){
  if(a->n!=2 || a->shape[0]!=a->shape[1]){
    mf_value_error("Value error in a.diag(): a is not an quadratic matrix.");
    return NULL;
  }
  if(a->stride[1]!=1){
    mf_ensure_plain(a);
  }
  mt_array* v = mf_malloc(sizeof(mt_array));
  v->refcount=1;
  v->n=1;
  a->data->refcount++;
  v->data=a->data;
  v->base=a->base;
  v->shape[0]=a->shape[0];
  v->stride[0]=(a->shape[0]+1)*a->stride[0];
  return v;
}

static
int array_diag(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"diag");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a.diag(): a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  mt_array* a = (mt_array*)v[0].value.p;
  mt_array* u = diag_slice(a);
  if(u==NULL) return 1;
  x->type=mv_array;
  x->value.p=(mt_basic*)u;
  return 0;
}

int mf_array_map(mt_array* a, mt_function* f){
  mt_object argv[2];
  argv[0].type=mv_null;
  double* ba = (double*)a->base;
  long size = a->size;
  long i;
  mt_object x;
  int e=0;
  for(i=0; i<size; i++){
    argv[1].type=mv_float;
    argv[1].value.f=ba[i];
    if(mf_call(f,&x,1,argv)){
      mf_traceback("map");
      return 1;
    }
    ba[i]=mf_float(&x,&e);
    if(e){
      mf_type_error1("in a.map(f): cannot convert f(a[k]) (type: %s) to float.",&x);
      return 1;
    }
  }
  return 0;
}

static
int array_map(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"map");
    return 1;
  }
  if(v[0].type!=mv_array){
    mf_type_error1("in a.map(f): a (type: %s) is not an array.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error1("in a.map(f): f (type: %s) is not a function.",&v[1]);
    return 1;
  }
  mt_array* a = (mt_array*)v[0].value.p;
  mt_function* f = (mt_function*)v[1].value.p;
  mt_array* b = mf_array_copy(a);
  if(mf_array_map(b,f)) return 1;
  x->type=mv_array;
  x->value.p=(mt_basic*)b;
  return 0;
}

static
int mf_trace(mt_object* x, mt_array* a){
  if(a->n!=2 || a->shape[0]!=a->shape[1]){
    mf_value_error("Value error in trace(a): a is not a quadratic matrix.");
    return 1;
  }
  double s=0;
  long n=a->shape[0];
  double* ba=(double*)a->base;
  long i,m;
  m=a->stride[0]+a->stride[1];
  for(i=0; i<n; i++){
    s+=ba[i*m];
  }
  x->type=mv_float;
  x->value.f=s;
  return 0;
}

static
int la_trace(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"trace");
    return 1;
  }
  if(v[1].type!=mv_array){
    mf_type_error1("in la.trace(a): a (type: %s) is not an array.",&v[1]);
    return 1;
  }
  mt_array* a = (mt_array*)v[1].value.p;
  if(mf_trace(x,a)) return 1;
  return 0;
}

mt_table* mf_la_load(){
  mt_map* m = mv_type_array->m;
  mf_insert_function(m,0,0,"str",array_str);
  mf_insert_function(m,0,0,"list",array_list);
  mf_insert_function(m,1,1,"ADD",array_ADD);
  mf_insert_function(m,1,1,"RADD",array_RADD);
  mf_insert_function(m,1,1,"SUB",array_SUB);
  mf_insert_function(m,1,1,"MPY",array_MPY);
  mf_insert_function(m,1,1,"RMPY",array_RMPY);
  mf_insert_function(m,0,0,"NEG",array_NEG);
  mf_insert_function(m,1,1,"DIV",array_DIV);
  mf_insert_function(m,1,1,"POW",array_POW);
  mf_insert_function(m,1,-1,"GET",array_GET);
  mf_insert_function(m,0,0,"T",array_T);
  mf_insert_function(m,0,0,"plain",array_plain);
  mf_insert_function(m,0,0,"copy",array_copy);
  mf_insert_function(m,0,0,"diag",array_diag);
  mf_insert_function(m,1,1,"map",array_map);

  mt_table* la = mf_table(NULL);
  la->name = mf_cstr_to_str("module la");
  la->m = mf_empty_map();
  m = la->m;
  mf_insert_function(m,1,1,"ha",la_array);
  mf_insert_function(m,1,1,"hv",la_vector);
  mf_insert_function(m,1,1,"hm",la_matrix);
  mf_insert_function(m,1,1,"idm",la_idm);
  mf_insert_function(m,1,1,"diag",la_diag);
  mf_insert_function(m,1,1,"trace",la_trace);
  m->frozen=1;
  return la;
}
