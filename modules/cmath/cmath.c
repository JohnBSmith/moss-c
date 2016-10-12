
#include <math.h>
#include <complex.h>
#include <moss.h>

#ifndef CMPLX
  #define CMPLX(x,y) ((x)+(y)*I)
#endif

double mf_float(mt_object* x, int* e);
double complex mf_cgamma(double complex z);

int mf_complex(mt_object* z, mt_object* x){
  switch(x->type){
  case mv_int:
    z->value.c.re=x->value.i;
    z->value.c.im=0;
    return 0;
  case mv_float:
    z->value.c.re=x->value.f;
    z->value.c.im=0;
    return 0;
  case mv_complex:
    z->value.c.re=x->value.c.re;
    z->value.c.im=x->value.c.im;
    return 0;
  default:
    return 1;
  }
}

static
int cmath_re(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"re");
    return 1;
  }
  if(v[1].type==mv_int){
    x->type=mv_int;
    x->value.i=v[1].value.i;
    return 0;
  }else if(v[1].type==mv_float){
    x->type=mv_float;
    x->value.f=v[1].value.f;
    return 0;
  }else if(v[1].type==mv_complex){
    x->type=mv_float;
    x->value.f=v[1].value.c.re;
    return 0;
  }else{
    mf_type_error("Type rror in re(x): x is of unexpected type.");
    return 1;
  }
}

static
int cmath_im(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"im");
    return 1;
  }
  if(v[1].type==mv_int){
    x->type=mv_int;
    x->value.i=0;
    return 0;
  }else if(v[1].type==mv_float){
    x->type=mv_float;
    x->value.f=0;
    return 0;
  }else if(v[1].type==mv_complex){
    x->type=mv_float;
    x->value.f=v[1].value.c.im;
    return 0;
  }else{
    mf_type_error("Type rror in im(x): x is of unexpected type.");
    return 1;
  }
}

static
int cmath_conj(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"conj");
    return 1;
  }
  if(v[1].type==mv_complex){
    x->type=mv_complex;
    x->value.c.re = v[1].value.c.re;
    x->value.c.im = -v[1].value.c.im;
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in conj(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=t;
    return 0;
  }
}

static
int cmath_arg(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"arg");
    return 1;
  }
  if(v[1].type==mv_complex){
    x->type=mv_float;
    x->value.f = atan2(v[1].value.c.im,v[1].value.c.re);
    return 0;
  }else if(v[1].type==mv_float){
    x->type=mv_float;
    x->value.f=atan2(0,v[1].value.f);
    return 0;
  }else if(v[1].type==mv_int){
    x->type=mv_float;
    x->value.f=atan2(0,v[1].value.i);
    return 0;
  }else{
    mf_type_error("Type error in arg(z): z is not a complex number.");
    return 1;
  }
}

static
int cmath_sqrt(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sqrt");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = csqrt(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re = creal(w);
    x->value.c.im = cimag(w);
    return 0;
  }else{
    int e=0;
    double t=mf_float(v+1,&e);
    if(e){
      mf_type_error1("in sqrt(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    double complex w = csqrt(t);
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }
}

static
int cmath_exp(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"exp");
    return 1;
  }
  double t;
  if(v[1].type==mv_int){
    x->type=mv_float;
    x->value.f=exp(v[1].value.i);
    return 0;
  }else if(v[1].type==mv_float){
    x->type=mv_float;
    x->value.f=exp(v[1].value.f);
    return 0;
  }else if(v[1].type==mv_complex){
    double re=v[1].value.c.re;
    double im=v[1].value.c.im;
    double p = exp(re);
    x->type=mv_complex;
    x->value.c.re=p*cos(im);
    x->value.c.im=p*sin(im);
    return 0;
  }else{
    mf_type_error("Type error in exp(x): x is of unexpected type.");
    return 1;
  }
}

static
int cmath_ln(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"ln");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = clog(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in ln(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=log(t);
    return 0;
  }
}

static
int cmath_sin(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sin");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = csin(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in sin(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=sin(t);
    return 0;
  }
}

static
int cmath_cos(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"cos");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = ccos(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in cos(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=cos(t);
    return 0;
  }
}

static
int cmath_tan(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"tan");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = ctan(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in tan(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=tan(t);
    return 0;
  }
}

static
int cmath_asin(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"asin");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = casin(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in asin(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=asin(t);
    return 0;
  }
}

static
int cmath_acos(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"acos");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = cacos(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in acos(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=acos(t);
    return 0;
  }
}

static
int cmath_atan(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"atan");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = catan(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in atan(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=atan(t);
    return 0;
  }
}

static
int cmath_sinh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sinh");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = csinh(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in sinh(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=sinh(t);
    return 0;
  }
}

static
int cmath_cosh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"cosh");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = ccosh(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in cosh(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=ccosh(t);
    return 0;
  }
}

static
int cmath_tanh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"tanh");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = ctanh(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in tanh(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=tanh(t);
    return 0;
  }
}

static
int cmath_asinh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"asinh");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = casinh(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in asinh(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=asinh(t);
    return 0;
  }
}

static
int cmath_acosh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"acosh");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = cacosh(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in sin(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=acosh(t);
    return 0;
  }
}

static
int cmath_atanh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"atanh");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = catanh(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in atanh(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=atanh(t);
    return 0;
  }
}

static
int cmath_gamma(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"gamma");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = mf_cgamma(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in gamma(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=tgamma(t);
    return 0;
  }
}

mt_table* mf_cmath_load(){
  mt_table* cmath = mf_table(NULL);
  cmath->name = mf_cstr_to_str("module cmath");
  cmath->m=mf_empty_map();
  mt_map* m=cmath->m;
  mf_insert_function(m,1,1,"re",cmath_re);
  mf_insert_function(m,1,1,"im",cmath_im);
  mf_insert_function(m,1,1,"conj",cmath_conj);
  mf_insert_function(m,1,1,"arg",cmath_arg);
  mf_insert_function(m,1,1,"sqrt",cmath_sqrt);
  mf_insert_function(m,1,1,"exp",cmath_exp);
  mf_insert_function(m,1,1,"ln",cmath_ln);
  mf_insert_function(m,1,1,"sin",cmath_sin);
  mf_insert_function(m,1,1,"cos",cmath_cos);
  mf_insert_function(m,1,1,"tan",cmath_tan);
  mf_insert_function(m,1,1,"asin",cmath_asin);
  mf_insert_function(m,1,1,"acos",cmath_acos);
  mf_insert_function(m,1,1,"atan",cmath_atan);
  mf_insert_function(m,1,1,"sinh",cmath_sinh);
  mf_insert_function(m,1,1,"cosh",cmath_cosh);
  mf_insert_function(m,1,1,"tanh",cmath_tanh);
  mf_insert_function(m,1,1,"asinh",cmath_asinh);
  mf_insert_function(m,1,1,"acosh",cmath_acosh);
  mf_insert_function(m,1,1,"atanh",cmath_atanh);
  mf_insert_function(m,1,1,"gamma",cmath_gamma);
  m->frozen=1;
  return cmath;
}
