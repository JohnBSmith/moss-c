
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <modules/str.h>
#include <modules/vec.h>
#include <moss.h>
#include <gmp.h>

struct mt_long{
  long refcount;
  mpz_t value;
};

mt_string* mf_cstr_to_str(const char* a);
char* mf_str_to_cstr(mt_string* s);
void mf_reverse(void** a, long size);
mt_list* mf_raw_list(long size);
void mf_list_push(mt_list* list, mt_object* x);

void mf_long_delete(mt_long* x){
  mpz_clear(x->value);
  mf_free(x);
}

void mf_long_dec_refcount(mt_long* x){
  if(x->refcount==1){
    mf_long_delete(x);
  }else{
    x->refcount--;
  }
}

mt_long* mf_long_new(){
  mt_long* x = mf_malloc(sizeof(mt_long));
  x->refcount=1;
  mpz_init(x->value);
  return x;
}

mt_long* mf_string_to_long(mt_string* x){
  long n = x->size;
  char* buffer = mf_str_to_cstr(x);
  mt_long* y = mf_long_new();
  if(n>2 && isalpha(buffer[1])){
    if(buffer[1]=='x'){
      mpz_set_str(y->value,buffer+2,16);
    }else if(buffer[1]=='b'){
      mpz_set_str(y->value,buffer+2,2);
    }else if(buffer[1]=='o'){
      mpz_set_str(y->value,buffer+2,8);
    }else{
      abort();
    }
  }else{
    mpz_set_str(y->value,buffer,10);
  }
  mf_free(buffer);
  return y;
}

mt_string* mf_long_to_string(mt_long* x, int base){
  char* buffer = mpz_get_str(NULL,base,x->value);
  mt_string* s = mf_cstr_to_str(buffer);
  mf_free(buffer);
  return s;
}

mt_long* mf_int_to_long(long n){
  mt_long* x = mf_long_new();
  mpz_set_si(x->value,n);
  return x;
}

int mf_flong(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"long");
    return 1;
  }
  if(v[1].type==mv_int){
    x->type=mv_long;
    x->value.p=(mt_basic*) mf_int_to_long(v[1].value.i);
    return 0;
  }else if(v[1].type==mv_string){
    x->type=mv_long;
    x->value.p=(mt_basic*)mf_string_to_long((mt_string*)v[1].value.p);
    return 0;
  }else if(v[1].type==mv_long){
    mf_copy_inc(x,v+1);
    return 0;
  }else{
    mf_type_error("Type error in long(x): cannot convert x to long.");
    return 1;
  }
}

int mf_long_cmp(mt_long* a, mt_long* b){
  return mpz_cmp(a->value,b->value);
}

int mf_long_cmp_si(mt_long* a, long b){
  return mpz_cmp_si(a->value,b);
}

mt_long* mf_long_copy(mt_long* x){
  mt_long* y = mf_long_new();
  mpz_set(y->value,x->value);
  return y;
}

mt_long* mf_long_neg(mt_long* x){
  mt_long* y = mf_long_new();
  mpz_neg(y->value,x->value);
  return y;
}

mt_long* mf_long_abs(mt_long* x){
  mt_long* y = mf_long_new();
  mpz_abs(y->value,x->value);
  return y;
}

int mf_long_sgn(mt_long* x){
  return mpz_sgn(x->value);
}

mt_long* mf_long_add(mt_long *a, mt_long* b){
  mt_long* y = mf_long_new();
  mpz_add(y->value,a->value,b->value);
  return y;
}

mt_long* mf_long_sub(mt_long* a, mt_long* b){
  mt_long* y = mf_long_new();
  mpz_sub(y->value,a->value,b->value);
  return y;
}

mt_long* mf_long_mpy(mt_long* a, mt_long* b){
  mt_long* y = mf_long_new();
  mpz_mul(y->value,a->value,b->value);
  return y;
}

mt_long* mf_long_div(mt_long* a, mt_long* b){
  mt_long* y = mf_long_new();
  mpz_fdiv_q(y->value,a->value,b->value);
  return y;
}

mt_long* mf_long_mod(mt_long* a, mt_long* b){
  mt_long* y = mf_long_new();
  mpz_fdiv_r(y->value,a->value,b->value);
  return y;
}

mt_long* mf_long_pow(mt_long* a, mt_long* b){
  mt_long* y = mf_long_new();
  int e = mpz_get_si(b->value);
  mpz_pow_ui(y->value,a->value,e);
  return y;
}

mt_long* mf_long_powmod(mt_long* a, mt_long* b, mt_long* m){
  mt_long* y = mf_long_new();
  mpz_powm(y->value,a->value,b->value,m->value);
  return y;
}

void mf_long_print(mt_long* x){
  char* buffer = mpz_get_str(NULL,10,x->value);
  printf("%s",buffer);
  mf_free(buffer);
}

uint32_t mf_long_hash(mt_long* x){
  char* a = mpz_get_str(NULL,16,x->value);
  uint32_t y=0;
  int i;
  for(i=0; a[i]!=0; i++){
    y = a[i]+(y<<6)+(y<<16)-y;
  }
  mf_free(a);
  return y;
}

mt_long* mf_to_long(mt_object* x){
  if(x->type==mv_int){
    return mf_int_to_long(x->value.i);
  }else if(x->type==mv_long){
    x->value.p->refcount++;
    return (mt_long*)x->value.p;
  }else if(x->type==mv_string){
    return mf_string_to_long((mt_string*)x->value.p);
  }else{
    mf_type_error("Type error in long(x): x is not an integer.");
    return NULL;
  }
}

mt_list* mf_long_base(mt_long* a, unsigned int b){
  mpz_t t,x;
  mpz_init(t);
  mpz_init(x);
  mpz_set(x,a->value);
  int r;
  mt_list* list = mf_raw_list(0);
  mt_object u;
  u.type=mv_int;
  while(mpz_cmp_ui(x,0)!=0){
    r = mpz_fdiv_q_ui(t,x,b);
    u.value.i=r;
    mf_list_push(list,&u);
    mpz_set(x,t);
  }
  return list;
}

int mf_long_isprime(mt_long* x){
  return mpz_probab_prime_p(x->value,20);
}

double mf_long_float(mt_long* x){
  return mpz_get_d(x->value);
}

