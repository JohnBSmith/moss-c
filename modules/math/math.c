
#include <math.h>
#include <moss.h>

double mf_float(mt_object* x, int* error);

static
int math_floor(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"floor");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in floor(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=floor(t);
  return 0;
}

static
int math_ceil(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"ceil");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in ceil(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=ceil(t);
  return 0;
}

static
int math_sqrt(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sqrt");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in sqrt(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=sqrt(t);
  return 0;
}

static
int math_exp(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"exp");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in exp(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=exp(t);
  return 0;
}

static
int math_log2(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"log2");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in log2(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=log2(t);
  return 0;
}

static
int math_lg(mt_object* x, int argc, mt_object* v){
  int e;
  double a,b;
  if(argc==1){
    e=0;
    a = mf_float(v+1,&e);
    if(e){
      mf_type_error("Type error in lg(x): cannot convert x to float.");
      return 1;
    }
    x->type=mv_float;
    x->value.f=log10(a);
    return 0;
  }else if(argc==2){
    e=0;
    a = mf_float(v+1,&e);
    if(e){
      mf_type_error("Type error in lg(x,b): cannot convert x to float.");
      return 1;
    }
    b = mf_float(v+2,&e);    
    if(e){
      mf_type_error("Type error in lg(x,b): cannot convert b to float.");
      return 1;
    }
    x->type=mv_float;
    x->value.f=log(a)/log(b);
    return 0;
  }else{
    mf_argc_error(argc,1,2,"lg");
    return 1;
  }
}

static
int math_ln(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"ln");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in ln(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=log(t);
  return 0;
}

static
int math_sin(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sin");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in sin(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=sin(t);
  return 0;
}

static
int math_cos(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"cos");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in cos(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=cos(t);
  return 0;
}

static
int math_tan(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"tan");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in tan(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=tan(t);
  return 0;
}

static
int math_asin(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"asin");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in asin(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=asin(t);
  return 0;
}

static
int math_acos(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"acos");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in acos(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=acos(t);
  return 0;
}

static
int math_atan(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"atn");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in atan(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=atan(t);
  return 0;
}

static
int math_sinh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sinh");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in sinh(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=sinh(t);
  return 0;
}

static
int math_cosh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"cosh");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in cosh(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=cosh(t);
  return 0;
}

static
int math_tanh(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"tanh");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in tanh(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=tanh(t);
  return 0;
}

static
int math_fac(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"fac");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in fac(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=tgamma(t+1);
  return 0;
}

static
int math_gamma(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"gamma");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in gamma(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=tgamma(t);
  return 0;
}

static
int math_erf(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"erf");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in erf(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=erf(t);
  return 0;
}

mt_table* mf_math_load(){
  mt_table* math = mf_table(NULL);
  math->name = mf_cstr_to_str("module math");
  math->m=mf_empty_map();
  mt_map* m=math->m;
  mt_object t;
  t.type=mv_float;
  t.value.f=M_PI;
  mf_insert_object(m,"pi",&t);
  t.value.f=M_E;
  mf_insert_object(m,"e",&t);
  t.value.f=NAN;
  mf_insert_object(m,"nan",&t);
  t.value.f=INFINITY;
  mf_insert_object(m,"inf",&t);

  mf_insert_function(m,1,1,"floor",math_floor);
  mf_insert_function(m,1,1,"ceil",math_ceil);
  mf_insert_function(m,1,1,"sqrt",math_sqrt);
  mf_insert_function(m,1,1,"exp",math_exp);
  mf_insert_function(m,1,1,"log2",math_log2);
  mf_insert_function(m,1,2,"lg",math_lg);
  mf_insert_function(m,1,1,"ln",math_ln);
  mf_insert_function(m,1,1,"sin",math_sin);
  mf_insert_function(m,1,1,"cos",math_cos);
  mf_insert_function(m,1,1,"tan",math_tan);
  mf_insert_function(m,1,1,"asin",math_asin);
  mf_insert_function(m,1,1,"acos",math_acos);
  mf_insert_function(m,1,1,"atan",math_atan);
  mf_insert_function(m,1,1,"sinh",math_sinh);
  mf_insert_function(m,1,1,"cosh",math_cosh);
  mf_insert_function(m,1,1,"tanh",math_tanh);
  mf_insert_function(m,1,1,"fac",math_fac);
  mf_insert_function(m,1,1,"gamma",math_gamma);
  mf_insert_function(m,1,1,"erf",math_erf);
  m->frozen=1;
  return math;
}
