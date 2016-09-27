
#include <stdio.h>
#include <stdlib.h>
#include <moss.h>
#include <objects/list.h>
int mf_eq(mt_object* x, mt_object* a, mt_object* b);
int mf_mod(mt_object* x, mt_object* a, mt_object* b);
int mf_mpy(mt_object* x, mt_object* a, mt_object* b);
int mf_idiv(mt_object* x, mt_object* a, mt_object* b);

static
int mf_gcd(mt_object* x, mt_object* a, mt_object* b){
  mt_object zero,c,h;
  zero.type=mv_int;
  zero.value.i=0;
  while(1){
    if(mf_eq(&c,&zero,b)){
      mf_traceback("gcd");
      return 1;
    }
    if(c.type!=mv_bool){
      mf_type_error1("in gcd(a,b): value (type: %s) of b==0 is not a boolean.",&c);
      mf_dec_refcount(&c);
      return 1;
    }
    if(c.value.b) break;
    if(mf_mod(&h,a,b)){
      mf_traceback("gcd");
      return 1;
    }
    mf_dec_refcount(a);
    mf_copy(a,b);
    mf_copy(b,&h);
  }
  mf_copy_inc(x,a);
  return 0;
}

static
int mf_lcm(mt_object* x, mt_object* a, mt_object* b){
  mt_object p,q;
  if(mf_mpy(&p,a,b)) goto error;
  if(mf_gcd(&q,a,b)){
    mf_dec_refcount(&p);
    goto error;
  }
  if(mf_idiv(x,&p,&q)){
    mf_dec_refcount(&p);
    mf_dec_refcount(&q);
    goto error;
  }
  mf_dec_refcount(&p);
  mf_dec_refcount(&q);
  return 0;
  error:
  mf_traceback("lcm");
  return 1;
}

static
int nt_gcd(mt_object* x, int argc, mt_object* v){
  if(argc==2){
    return mf_gcd(x,v+1,v+2);
  }else{
    mf_argc_error(argc,2,2,"gcd");
    return 1;
  }
}

static
int nt_lcm(mt_object* x, int argc, mt_object* v){
  if(argc==2){
    return mf_lcm(x,v+1,v+2);
  }else{
    mf_argc_error(argc,2,2,"lcm");
    return 1;
  }
}

static
long isprime(long n){
  if(n<2) return 0;
  long i;
  for(i=2; i*i<=n; i++){
    if(n%i==0) return 0;
  }
  return 1;
}

static
int nt_isprime(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"isprime");
    return 1;
  }
  if(v[1].type==mv_int){
    x->type=mv_bool;
    x->value.b=isprime(v[1].value.i);
    return 0;
  }else{
    mf_type_error1("in isprime(n): n (type: %s) is not an integer.",v+1);
    return 1;
  }
}

static
int nt_divisors(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"divisors");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in divisors(n): n (type %s) is not an integer.",v+1);
    return 1;
  }
  long n = v[1].value.i;
  if(n<0) n=-n;
  long i;
  mt_list* list = mf_raw_list(0);
  mt_object t; t.type=mv_int;
  for(i=1; i<=n; i++){
    if(n%i==0){
      t.value.i=i;
      mf_list_push(list,&t);
    }
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

static
long next_prime(long p){
  while(1){
    p++;
    if(isprime(p)) return p;
  }
}

static
void push_pe(mt_list* list, long p, long e){
  mt_object x;
  x.type=mv_list;
  mt_list* t = mf_raw_list(2);
  t->a[0].type=mv_int;
  t->a[0].value.i=p;
  t->a[1].type=mv_int;
  t->a[1].value.i=e;
  x.value.p=(mt_basic*)t;
  mf_list_push(list,&x);
}

static
mt_list* factor(long n){
  mt_list* list = mf_raw_list(0);
  mt_list* t;
  mt_object x;
  x.type=mv_list;
  if(n<0){
    n=-n;
    push_pe(list,-1,1);
  }
  long e;
  long p=2;
  while(n!=1){
    if(p*p>n) break;
    e=0;
    while(n%p==0){
      n/=p;
      e++;
    }
    if(e>0){
      push_pe(list,p,e);
    }
    p=next_prime(p);
  }
  if(n>1){
    push_pe(list,n,1);
  }
  return list;
}

static
int nt_factor(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"factor");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in factor(n): n (type: %s) is not an integer.",v+1);
    return 1;
  }
  if(v[1].value.i==0){
    mf_value_error("Value error in factor(n): n==0.");
    return 1;
  }
  mt_list* list = factor(v[1].value.i);
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

mt_table* mf_nt_load(){
  mt_table* nt = mf_table(NULL);
  nt->name = mf_cstr_to_str("module nt");
  nt->m=mf_empty_map();
  mt_map* m=nt->m;
  mf_insert_function(m,2,2,"gcd",nt_gcd);
  mf_insert_function(m,2,2,"lcm",nt_lcm);
  mf_insert_function(m,1,1,"isprime",nt_isprime);
  mf_insert_function(m,1,1,"divisors",nt_divisors);
  mf_insert_function(m,1,1,"factor",nt_factor);
  m->frozen=1;
  return nt;
}
