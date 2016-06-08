
#include <math.h>
#include <moss.h>

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
  double t;
  if(v[1].type==mv_int){
    t=v[1].value.i;
      }else if(v[1].type==mv_float){
    t=v[1].value.f;
  }else{
    mf_type_error("Type error in sqrt(x): x is of unexpected type.");
    return 1;
  }
  if(t<0){
    x->type=mv_complex;
    x->value.c.re=0;
    x->value.c.im=sqrt(fabs(t));
    return 0;
  }else{
    x->type=mv_float;
    x->value.f=sqrt(t);
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

mt_table* mf_cmath_load(){
  mt_table* cmath = mf_table(NULL);
  cmath->name = mf_cstr_to_str("module cmath");
  cmath->m=mf_empty_map();
  mt_map* m=cmath->m;
  mf_insert_function(m,1,1,"re",cmath_re);
  mf_insert_function(m,1,1,"im",cmath_im);
  mf_insert_function(m,1,1,"arg",cmath_arg);
  mf_insert_function(m,1,1,"sqrt",cmath_sqrt);
  mf_insert_function(m,1,1,"exp",cmath_exp);
  m->frozen=1;
  return cmath;
}
