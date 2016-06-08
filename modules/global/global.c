
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <moss.h>
#include <objects/string.h>
#include <objects/list.h>
#include <objects/map.h>
#include <objects/complex.h>
#include <objects/long.h>
#include <objects/set.h>
#include <objects/function.h>
mt_function* mf_new_function(unsigned char* address);
mt_table* mf_load_module(const char* id);
void mf_module_dec_refcount(mt_module* m);
mt_tuple* mf_raw_tuple(long size);

extern mt_table* mv_type_bool;
extern mt_table* mv_type_int;
extern mt_table* mv_type_float;
extern mt_table* mv_type_complex;
extern mt_table* mv_type_string;
extern mt_table* mv_type_bstring;
extern mt_table* mv_type_list;
extern mt_table* mv_type_function;
extern mt_table* mv_type_dict;
extern mt_table* mv_type_long;

extern mt_list* mv_traceback_list;
extern mt_object mv_exception;
extern mt_map* mv_gvtab;

static
long floor_div(long a, long b){
  if(a%b==0 || (a<0)==(b<0)){
    return a/b;
  }else{
    return a/b-1;
  }
}

long mf_floor_mod(long x, long m){
  long r = x%m;
  if(r==0 || (x<0)==(m<0)){
    return r;
  }else{
    return r+m;
  }
}

static
double floor_fmod(double x, double m){
  return x-m*floor(x/m);
}

double mf_float(mt_object* x, int* error){
  if(x->type==mv_int){
    return x->value.i;
  }else if(x->type==mv_float){
    return x->value.f;
  }else{
    *error=1;
    return NAN;
  }
}

void* mf_malloc(size_t size){
  void* p = malloc(size);
  if(p==NULL){
    puts("Memory error in mf_malloc.");
    abort();
  }
  return p;
}

void* mf_malloc_analyze(size_t size,
  const char* file, unsigned int line
){
  // todo
  return NULL;
}

void mf_free_analyze(void* p){
  // todo
  free(p);
}

mt_tuple* mf_tuple_noinc(int argc, mt_object* v){
  mt_tuple* t = mf_malloc(sizeof(mt_tuple)+argc*sizeof(mt_object));
  t->refcount=1;
  t->size=argc;
  int i;
  for(i=0; i<argc; i++){
    mf_copy(t->a+i,v+i);
  }
  return t;
}

void mf_tuple_dec_refcount(mt_tuple* t){
  if(t->refcount==1){
    long i;
    for(i=0; i<t->size; i++){
      mf_dec_refcount(t->a+i);
    }
    mf_free(t);
  }else{
    t->refcount--;
  }
}

void mf_dec_refcount(mt_object* x){
  if(x->type<=mv_complex) return;

  mt_basic* p = x->value.p;
  assert(p->refcount>0);
  switch(x->type){
  case mv_function:
    if(p->refcount==1){
      mf_function_delete((mt_function*)p);
    }else{
      p->refcount--;
    }
    return;
  case mv_list:
    if(p->refcount==1){
      mf_list_delete((mt_list*)p);
    }else{
      p->refcount--;
    }
    return;
  case mv_map:
    if(p->refcount==1){
      mf_map_delete((mt_map*)p);
    }else{
      p->refcount--;
    }
    return;
  case mv_set:
    if(p->refcount==1){
      mf_set_delete((mt_set*)p);
    }else{
      p->refcount--;
    }
    return;
  case mv_string:
    if(p->refcount==1){
      mf_free(p);
    }else{
      p->refcount--;
    }
    return;
  case mv_long:
    if(p->refcount==1){
      mf_long_delete((mt_long*)p);
    }else{
      p->refcount--;
    }
    return;
  case mv_range:
    if(p->refcount==1){
      mt_range* r = (mt_range*)p;
      mf_dec_refcount(&r->a);
      mf_dec_refcount(&r->b);
      mf_dec_refcount(&r->step);
      mf_free(r);
    }else{
      p->refcount--;
    }
    return;
  case mv_tuple:
    mf_tuple_dec_refcount((mt_tuple*)p);
    return;
  case mv_native:{
    if(p->refcount==1){
      mt_native* pn=(mt_native*)p;
      pn->del(pn);
    }else{
      p->refcount--;
    }
    return;
  }
  case mv_table:{
    if(p->refcount==1){
      mt_table* t=(mt_table*)p;
      mf_dec_refcount(&t->prototype);
      if(t->m){
        mf_map_dec_refcount(t->m);
      }
      mf_free(t);
    }else{
      p->refcount--;
    }
    return;
  }
  default:
    printf("x->type==%i\n",x->type);
    puts("Error in mf_dec_refcount.");
    abort();
  }
}

void mf_dec_refcounts(long size, mt_object* a){
  long i;
  for(i=0; i<size; i++){
    mf_dec_refcount(a+i);
  }
}

void mf_inc_refcount(mt_object* x){
  if(x->type>mv_complex){
    x->value.p->refcount++;
  }
}

// == Nomenclature ==
// mf_copy: copy reference
// mf_copy_inc: copy reference and increment refcount
// mf_copy_shallow: shallow copy
// mf_copy_deep: deep copy

void mf_copy(mt_object* x, mt_object* a){
  switch(a->type){
  case mv_null:
    x->type=mv_null;
    return;
  case mv_bool:
    x->type=mv_bool;
    x->value.b=a->value.b;
    return;
  case mv_int:
    x->type=mv_int;
    x->value.i=a->value.i;
    return;
  case mv_float:
    x->type=mv_float;
    x->value.f=a->value.f;
    return;
  case mv_complex:
    x->type=mv_complex;
    x->value.c=a->value.c;
    return;
  default:
    x->type=a->type;
    x->value.p=a->value.p;
  }
}

void mf_copy_inc(mt_object* x, mt_object* a){
  switch(a->type){
  case mv_null:
    x->type=mv_null;
    return;
  case mv_bool:
    x->type=mv_bool;
    x->value.b=a->value.b;
    return;
  case mv_int:
    x->type=mv_int;
    x->value.i=a->value.i;
    return;
  case mv_float:
    x->type=mv_float;
    x->value.f=a->value.f;
    return;
  case mv_complex:
    x->type=mv_complex;
    x->value.c=a->value.c;
    return;
  default:
    x->type=a->type;
    x->value.p=a->value.p;
    a->value.p->refcount++;
  }
}

mt_tuple* mf_tuple_copy(mt_tuple* t){
  long size = t->size;
  mt_tuple* t2 = mf_raw_tuple(size);
  long i;
  for(i=0; i<size; i++){
    mf_copy_inc(t2->a+i,t->a+i);
  }
  return t2;
}

mt_function* mf_function_copy(mt_function* f){
  mt_function* g = mf_new_function(f->address);
  mf_copy_inc(&g->prototype,&f->prototype);
  if(f->m){
    f->m->refcount++;
    g->m=f->m;
  }
  g->fp=f->fp;
  g->data=f->data;
  g->gtab=f->gtab;
  g->argc=f->argc;
  g->min=f->min;
  g->max=f->max;
  g->varcount=f->varcount;
  if(f->context){
    g->context = mf_tuple_copy(f->context);
  }
  if(f->name){
    f->name->refcount++;
    g->name=f->name;
  }
  if(f->module){
    f->module->refcount++;
    g->module=f->module;
  }
  return g;
}

mt_list* mf_list_copy(mt_list* a);
int mf_copy_shallow(mt_object* x, mt_object* a){
  switch(a->type){
  case mv_null:
    x->type=mv_null;
    return 0;
  case mv_bool:
    x->type=mv_bool;
    x->value.b=a->value.b;
    return 0;
  case mv_int:
    x->type=mv_int;
    x->value.i=a->value.i;
    return 0;
  case mv_float:
    x->type=mv_float;
    x->value.f=a->value.f;
    return 0;
  case mv_complex:
    x->type=mv_complex;
    x->value.c=a->value.c;
    return 0;
  case mv_list:{
    x->type=mv_list;
    mt_list* list=(mt_list*)a->value.p;
    x->value.p=(mt_basic*)mf_list_copy(list);
    return 0;
  }
  case mv_string:
  case mv_bstring:
    x->type=mv_string;
    x->value.p=a->value.p;
    a->value.p->refcount++;
    return 0;
  case mv_function:{
    mt_function* f = (mt_function*)a->value.p;
    mt_function* g = mf_function_copy(f);
    x->type=mv_function;
    x->value.p=(mt_basic*)g;
    return 0;
  }
  default:
    mf_type_error("Type error in copy(x): cannot construct shallow copy of x.");
    return 1;
  }
}

int mf_fcopy(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"copy");
    return 1;
  }
  return mf_copy_shallow(x,v+1);
}

const char* mf_type_to_str(mt_object* a){
  switch(a->type){
  case mv_bool:
    return "bool";
  case mv_int:
    return "int";
  case mv_float:
    return "float";
  case mv_complex:
    return "complex";
  case mv_long:
    return "long";
  case mv_list:
    return "list";
  case mv_string:
    return "str";
  case mv_bstring:
    return "bstr";
  case mv_table:
    return "table";
  case mv_range:
    return "range";
  default:
    return "object";
  }
}

static
void unary_op_type_error(const char* s, mt_object* a){
  char buffer[200];
  snprintf(buffer,200,s,mf_type_to_str(a));
  mf_type_error(buffer);
}

static
void binary_op_type_error(const char* s, mt_object* a, mt_object* b){
  char buffer[200];
  snprintf(buffer,200,s,mf_type_to_str(a),mf_type_to_str(b));
  mf_type_error(buffer);
}

#ifdef USE_BUILTIN_OVERFLOW
#define add_overflow __builtin_add_overflow
#define sub_overflow __builtin_sub_overflow
#define mpy_overflow __builtin_mpy_overflow
#else

// Have a look at
// Henry S. Warren: "Hacker's Delight", 2nd ed., 2013
// Chapter 2 "Basics", section 13 "Overflow Detection"
static
int add_overflow(long a, long b, long* c){
  if(a>0 && b>LONG_MAX-a) return 1;
  if(a<0 && b<LONG_MIN-a) return 1;
  *c = a+b;
  return 0;
}

static
int sub_overflow(long a, long b, long* c){
  if(b>0 && a<LONG_MIN+b) return 1;
  if(b<0 && a>LONG_MAX+b) return 1;
  *c = a-b;
  return 0;
}

static
int mpy_overflow(long a, long b, long* c){
  if(b==LONG_MIN && a<0) return 1;
  long y=a*b;
  if(b!=0 && y/b!=a) return 1;
  *c=y;
  return 0;
}
#endif

static
int pow_overflow(long a, long n, long* c){
  long y=1;
  while(n){
    if(n&1){
      if(mpy_overflow(y,a,&y)) return 1;
    }
    n>>=1;
    if(n==0) break;
    if(mpy_overflow(a,a,&a)) return 1;
  }
  *c=y;
  return 0;
}

int mf_object_get(mt_object* x, mt_object* a, mt_object* key);

int mf_object_get_memory(mt_object* x, mt_object* a,
  long size, const char* id
){
  // todo: does not need to be allocated
  mt_object s;
  s.type=mv_string;
  s.value.p=(mt_basic*) mf_str_new(size,id);
  int e=mf_object_get(x,a,&s);
  mf_dec_refcount(&s);
  return e;
}

static
int op_method_error;

static
int unary_op_method(mt_object* x, mt_object* a,
  int size, const char* id
){
  mt_object f;
  if(mf_object_get_memory(&f,a,size,id)){
    op_method_error=0;
    return 1;
  }
  char buffer[200];
  if(f.type!=mv_function){
    snprintf(buffer,200,"Type error in a.%s(): %s is not a function.",id,id);
    mf_type_error(buffer);
    mf_dec_refcount(a);
    op_method_error=1;
    return 1;
  }
  mt_object y;
  if(mf_call((mt_function*)f.value.p,&y,0,a)){
    mf_dec_refcount(a);
    op_method_error=1;
    return 1;
  }
  mf_dec_refcount(a);
  mf_copy(x,&y);
  return 0;
}

static
int binary_op_method(mt_object* x, mt_object* a, mt_object* b,
  mt_object* p, int size, const char* id
){
  mt_object f;
  if(mf_object_get_memory(&f,p,size,id)){
    op_method_error=0;
    return 1;
  }
  char buffer[200];
  if(f.type!=mv_function){
    snprintf(buffer,200,"Type error in a.%s(b): %s is not a function.",id,id);
    mf_type_error(buffer);
    mf_dec_refcount(a);
    mf_dec_refcount(b);
    op_method_error=1;
    return 1;
  }
  mt_object argv[2];
  mf_copy(argv,a);
  mf_copy(argv+1,b);
  mt_object y;
  if(mf_call((mt_function*)f.value.p,&y,1,argv)){
    mf_dec_refcount(a);
    mf_dec_refcount(b);
    op_method_error=1;
    return 1;
  }
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  mf_copy(x,&y);
  return 0;
}

int mf_add_dec(mt_object* x, mt_object* a, mt_object* b){
  double re,im;
  mt_string* sx;
  mt_list* list;
  long y;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      if(add_overflow(a->value.i,b->value.i,&y)){
        mt_long *aL,*bL,*xL;
        aL=mf_int_to_long(a->value.i);
        bL=mf_int_to_long(b->value.i);
        xL=mf_long_add(aL,bL);
        mf_long_delete(aL);
        mf_long_delete(bL);
        x->type=mv_long;
        x->value.p=(mt_basic*)xL;
      }else{
        x->type=mv_int;
        x->value.i=y;
      }
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.i+b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re = a->value.i+b->value.c.re;
      im = b->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    case mv_long:{
      mt_long *aL,*bL,*xL;
      aL=mf_int_to_long(a->value.i);
      bL=(mt_long*)b->value.p;
      xL=mf_long_add(aL,bL);
      mf_long_delete(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    }
    default:
      goto radd;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_float;
      x->value.f=a->value.f+b->value.i;
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.f+b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re = a->value.f+b->value.c.re;
      im = b->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    default:
      goto radd;
    }
  case mv_complex:
    switch(b->type){
    case mv_int:
      x->type=mv_complex;
      x->value.c.re=a->value.c.re+b->value.i;
      x->value.c.im=a->value.c.im;
      return 0;
    case mv_float:
      x->type=mv_complex;
      x->value.c.re=a->value.c.re+b->value.f;
      x->value.c.im=a->value.c.im;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re = a->value.c.re+b->value.c.re;
      im = a->value.c.im+b->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    default:
      goto radd;
    }
  case mv_long:{
    mt_long *aL,*bL,*xL;
    switch(b->type){
    case mv_int:
      aL=(mt_long*)a->value.p;
      bL=mf_int_to_long(b->value.i);
      xL=mf_long_add(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_delete(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    case mv_long:
      aL=(mt_long*)a->value.p;
      bL=(mt_long*)b->value.p;
      xL=mf_long_add(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    default:
      goto radd;
    }
  }
  case mv_string:
    switch(b->type){
    case mv_string:
      sx = mf_str_add((mt_string*)a->value.p,(mt_string*)b->value.p);
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_string;
      x->value.p = (mt_basic*)sx;
      return 0;
    default:
      goto radd;
    }
    break;
  case mv_list:
    switch(b->type){
    case mv_list:
      list = mf_list_add((mt_list*)a->value.p,(mt_list*)b->value.p);
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_list;
      x->value.p = (mt_basic*)list;
      return 0;
    default:
      goto radd;
    }
  default:
    goto add;
  }
  add:
  if(binary_op_method(x,a,b,a,3,"add")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  radd:
  if(binary_op_method(x,a,b,b,4,"radd")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a+b: '+' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_sub_dec(mt_object* x, mt_object* a, mt_object* b){
  double re,im;
  long y;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      if(sub_overflow(a->value.i,b->value.i,&y)){
        mt_long *aL,*bL,*xL;
        aL=mf_int_to_long(a->value.i);
        bL=mf_int_to_long(b->value.i);
        xL=mf_long_sub(aL,bL);
        mf_long_delete(aL);
        mf_long_delete(bL);
        x->type=mv_long;
        x->value.p=(mt_basic*)xL;
      }else{
        x->type=mv_int;
        x->value.i=y;
      }
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.i-b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re=a->value.i-b->value.c.re;
      im=-b->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    case mv_long:{
      mt_long *aL,*bL,*xL;
      aL=mf_int_to_long(a->value.i);
      bL=(mt_long*)b->value.p;
      xL=mf_long_sub(aL,bL);
      mf_long_delete(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    }
    default:
      goto rsub;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_float;
      x->value.f=a->value.f-b->value.i;
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.f-b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re =a->value.f-b->value.c.re;
      im = -b->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    default:
      goto rsub;
    }
  case mv_complex:
    switch(b->type){
    case mv_int:
      x->type=mv_complex;
      re = a->value.c.re-b->value.i;
      im = a->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    case mv_float:
      x->type=mv_complex;
      re = a->value.c.re-b->value.f;
      im = a->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re = a->value.c.re-b->value.c.re;
      im = a->value.c.im-b->value.c.im;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    default:
      goto rsub;
    }
  case mv_long:{
    mt_long *aL,*bL,*xL;
    switch(b->type){
    case mv_int:
      aL=(mt_long*)a->value.p;
      bL=mf_int_to_long(b->value.i);
      xL=mf_long_sub(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_delete(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    case mv_long:
      aL=(mt_long*)a->value.p;
      bL=(mt_long*)b->value.p;
      xL=mf_long_sub(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    default:
      goto rsub;
    }
  }
  case mv_set:
    if(b->type==mv_set){
      mt_set* seta = (mt_set*)a->value.p;
      mt_set* setb = (mt_set*)b->value.p;
      x->type=mv_set;
      x->value.p=(mt_basic*)mf_set_difference(seta,setb);
      mf_set_dec_refcount(seta);
      mf_set_dec_refcount(setb);
      return 0;
    }else{
      goto rsub;
    }
  default:
    goto sub;
  }
  sub:
  if(binary_op_method(x,a,b,a,3,"sbt")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rsub:
  if(binary_op_method(x,a,b,b,4,"rsbt")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a-b: '-' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_mpy_dec(mt_object* x, mt_object* a, mt_object* b){
  double re,im,re1,re2,im1,im2;
  mt_string* sx;
  mt_list* list;
  long y;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      if(mpy_overflow(a->value.i,b->value.i,&y)){
        mt_long *aL,*bL,*xL;
        aL=mf_int_to_long(a->value.i);
        bL=mf_int_to_long(b->value.i);
        xL=mf_long_mpy(aL,bL);
        mf_long_delete(aL);
        mf_long_delete(bL);
        x->type=mv_long;
        x->value.p=(mt_basic*)xL;
      }else{
        x->type=mv_int;
        x->value.i=y;
      }
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.i*b->value.f;
      return 0;
    case mv_complex:
      re=a->value.i*b->value.c.re;
      im=a->value.i*b->value.c.im;
      x->type=mv_complex;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    case mv_long:{
      mt_long *aL,*bL,*xL;
      aL=mf_int_to_long(a->value.i);
      bL=(mt_long*)b->value.p;
      xL=mf_long_mpy(aL,bL);
      mf_long_delete(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    }
    case mv_string:
      sx = mf_str_mpy((mt_string*)b->value.p,a->value.i);
      mf_dec_refcount(b);
      x->type=mv_string;
      x->value.p = (mt_basic*)sx;
      return 0;
    case mv_list:
      list = mf_list_mpy((mt_list*)b->value.p,a->value.i);
      mf_dec_refcount(b);
      x->type=mv_list;
      x->value.p = (mt_basic*)list;
      return 0;
    default:
      goto rmpy;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_float;
      x->value.f=a->value.f*b->value.i;
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.f*b->value.f;
      return 0;
    case mv_complex:
      re=a->value.f*b->value.c.re;
      im=a->value.f*b->value.c.im;
      x->type=mv_complex;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    default:
      goto rmpy;
    }
  case mv_complex:
    switch(b->type){
    case mv_int:
      x->type=mv_complex;
      re=a->value.c.re*b->value.i;
      im=a->value.c.im*b->value.i;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    case mv_float:
      x->type=mv_complex;
      re=a->value.c.re*b->value.f;
      im=a->value.c.im*b->value.f;
      x->value.c.re=re;
      x->value.c.im=im;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re1 = a->value.c.re; im1 = a->value.c.im;
      re2 = b->value.c.re; im2 = b->value.c.im;
      x->value.c.re=re1*re2-im1*im2;
      x->value.c.im=re1*im2+re2*im1;
      return 0;
    default:
      goto rmpy;
    }
  case mv_long:{
    mt_long *aL,*bL,*xL;
    switch(b->type){
    case mv_int:
      aL=(mt_long*)a->value.p;
      bL=mf_int_to_long(b->value.i);
      xL=mf_long_mpy(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_delete(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    case mv_long:
      aL=(mt_long*)a->value.p;
      bL=(mt_long*)b->value.p;
      xL=mf_long_mpy(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    default:
      goto rmpy;
    }
  }
  case mv_string:
    switch(b->type){
    case mv_int:
      sx = mf_str_mpy((mt_string*)a->value.p,b->value.i);
      mf_dec_refcount(a);
      x->type=mv_string;
      x->value.p = (mt_basic*)sx;
      return 0;
    default:
      goto rmpy;
    }
  case mv_list:
    switch(b->type){
    case mv_int:
      list = mf_list_mpy((mt_list*)a->value.p,b->value.i);
      mf_dec_refcount(a);
      x->type=mv_list;
      x->value.p = (mt_basic*)list;
      return 0;
    default:
      goto rmpy;
    }
  default:
    goto mpy;
  }
  mpy:
  if(binary_op_method(x,a,b,a,3,"mpy")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rmpy:
  if(binary_op_method(x,a,b,b,4,"rmpy")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a*b: '*' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_div_dec(mt_object* x, mt_object* a, mt_object* b){
  double r,re,im,re1,re2,im1,im2;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      x->type=mv_float;
      x->value.f=((double)a->value.i)/b->value.i;
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.i/b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      r = b->value.c.re+b->value.c.re+b->value.c.im*b->value.c.im;
      re = a->value.i*b->value.c.re;
      im = -a->value.i*b->value.c.im;
      x->value.c.re=re/r;
      x->value.c.im=im/r;
      return 0;
    default:
      goto rdiv;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_float;
      x->value.f=a->value.f/b->value.i;
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=a->value.f/b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      r = b->value.c.re*b->value.c.re+b->value.c.im*b->value.c.im;
      re = a->value.f*b->value.c.re;
      im = -a->value.f*b->value.c.im;
      x->value.c.re=re/r;
      x->value.c.im=im/r;
      return 0;
    default:
      goto rdiv;
    }
  case mv_complex:
    switch(b->type){
    case mv_int:
      x->type=mv_complex;
      x->value.c.re=a->value.c.re/b->value.i;
      x->value.c.im=a->value.c.im/b->value.i;
      return 0;
    case mv_float:
      x->type=mv_complex;
      x->value.c.re=a->value.c.re/b->value.f;
      x->value.c.im=a->value.c.im/b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_complex;
      re1 = a->value.c.re; im1 = a->value.c.im;
      re2 = b->value.c.re; im2 = b->value.c.im;
      r = re2*re2+im2*im2;
      x->value.c.re=(re1*re2+im1*im2)/r;
      x->value.c.im=(im1*re2-re1*im2)/r;
      return 0;
    default:
      goto rdiv;
    }
  default:
    goto div;
  }
  div:
  if(binary_op_method(x,a,b,a,3,"div")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rdiv:
  if(binary_op_method(x,a,b,b,4,"rdiv")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a/b: '/' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_idiv_dec(mt_object* x, mt_object* a, mt_object* b){
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      if(b->value.i==0){
        mf_value_error("Value error in a//b: b==0.");
        return 1;
      }
      x->type=mv_int;
      x->value.i=floor_div(a->value.i,b->value.i);
      return 0;
    case mv_long:{
      mt_long *aL,*bL,*xL;
      aL=mf_int_to_long(a->value.i);
      bL=(mt_long*)b->value.p;
      xL=mf_long_div(aL,bL);
      mf_long_delete(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    }
    default:
      goto ridiv;
    }
  case mv_long:{
    mt_long *aL,*bL,*xL;
    switch(b->type){
    case mv_int:
      aL=(mt_long*)a->value.p;
      bL=mf_int_to_long(b->value.i);
      xL=mf_long_div(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_delete(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    case mv_long:
      aL=(mt_long*)a->value.p;
      bL=(mt_long*)b->value.p;
      xL=mf_long_div(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    default:
      goto ridiv;
    }
  }
  default:
    goto idiv;
  }
  idiv:
  if(binary_op_method(x,a,b,a,3,"idiv")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  ridiv:
  if(binary_op_method(x,a,b,b,4,"ridiv")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a//b: '//' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_mod_dec(mt_object* x, mt_object* a, mt_object* b){
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      x->type=mv_int;
      x->value.i=mf_floor_mod(a->value.i,b->value.i);
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=floor_fmod(a->value.i,b->value.f);
      return 0;
    case mv_long:{
      mt_long *aL,*bL,*xL;
      aL=mf_int_to_long(a->value.i);
      bL=(mt_long*)b->value.p;
      xL=mf_long_mod(aL,bL);
      mf_long_delete(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    }
    default:
      goto rmod;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_float;
      x->value.f=floor_fmod(a->value.f,b->value.i);
      return 0;
    case mv_float:
      x->type=mv_float;
      x->value.f=floor_fmod(a->value.f,b->value.f);
      return 0;
    default:
      goto rmod;
    }
  case mv_long:{
    mt_long *aL,*bL,*xL;
    switch(b->type){
    case mv_int:
      aL=(mt_long*)a->value.p;
      bL=mf_int_to_long(b->value.i);
      xL=mf_long_mod(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_delete(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    case mv_long:
      aL=(mt_long*)a->value.p;
      bL=(mt_long*)b->value.p;
      xL=mf_long_mod(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    default:
      goto rmod;
    }
  }
  default:
    goto mod;
  }
  mod:
  if(binary_op_method(x,a,b,a,3,"mod")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rmod:
  if(binary_op_method(x,a,b,b,4,"rmod")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a%%b: '%%' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_neg_dec(mt_object* x, mt_object* a){
  switch(a->type){
  case mv_int:
    x->type=mv_int;
    x->value.i=-a->value.i;
    return 0;
  case mv_float:
    x->type=mv_float;
    x->value.f=-a->value.f;
    return 0;
  case mv_complex:
    x->type=mv_complex;
    x->value.c.re=-a->value.c.re;
    x->value.c.im=-a->value.c.im;
    return 0;
  case mv_long:
    x->type=mv_long;
    x->value.p=(mt_basic*)mf_long_neg((mt_long*)a->value.p);
    return 0;
  default:
    goto neg;
  }
  neg:
  if(unary_op_method(x,a,3,"neg")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  unary_op_type_error(
    "Type error in -a: '-' is not defined for a of type '%s'.",a
  );
  mf_dec_refcount(a);
  x->type=mv_null;
  return 1;
}

int mf_tilde_dec(mt_object* x, mt_object* a){
  switch(a->type){
  case mv_bool:
    x->type=mv_bool;
    x->value.b=!a->value.b;
    return 0;
  case mv_int:
    x->type=mv_int;
    x->value.i=~a->value.i;
    return 0;
  default:
    goto tilde;
  }
  tilde:
  if(unary_op_method(x,a,5,"tilde")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  unary_op_type_error(
    "Type error in ~a: '~' is not defined for a of type '%s'.",a
  );
  mf_dec_refcount(a);
  x->type=mv_null;
  return 1;
}

int mf_not_dec(mt_object* x, mt_object* a){
  if(a->type==mv_bool){
    x->type=mv_bool;
    x->value.b=!a->value.b;
    return 0;
  }else{
    unary_op_type_error(
      "Type error in not a: 'not' is not defined for a of type '%s'.",a
    );
    mf_dec_refcount(a);
    x->type=mv_null;
    return 1;
  }
}

int mf_pow_dec(mt_object* x, mt_object* a, mt_object* b){
  long y;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      if(b->value.i<0){
        x->type=mv_float;
        x->value.f=pow(a->value.i,b->value.i);
        return 0;
      }
      if(pow_overflow(a->value.i,b->value.i,&y)){
        mt_long *aL,*bL,*xL;
        aL=mf_int_to_long(a->value.i);
        bL=mf_int_to_long(b->value.i);
        xL=mf_long_pow(aL,bL);
        mf_long_delete(aL);
        mf_long_delete(bL);
        x->type=mv_long;
        x->value.p=(mt_basic*)xL;
      }else{
        x->type=mv_int;
        x->value.i=y;
      }
      return 0;
    case mv_float:
      if(a->value.i<0){
        x->type=mv_complex;
        mf_complex_powrr(x,a->value.i,b->value.f);
      }else{
        x->type=mv_float;
        x->value.f = pow(a->value.i,b->value.f);
      }
      return 0;
    case mv_complex:
      x->type=mv_complex;
      mf_complex_powrc(x,a->value.i,b->value.c.re,b->value.c.im);
      return 0;
    case mv_long:{
      mt_long *aL,*bL,*xL;
      aL=mf_int_to_long(a->value.i);
      bL=(mt_long*)b->value.p;
      xL=mf_long_pow(aL,bL);
      mf_long_delete(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    }
    default:
      goto rpow;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_float;
      x->value.f = pow(a->value.f,b->value.i);
      return 0;
    case mv_float:
      if(a->value.f<0){
        x->type=mv_complex;
        mf_complex_powrr(x,a->value.f,b->value.f);
      }else{
        x->type=mv_float;
        x->value.f = pow(a->value.f,b->value.f);
      }
      return 0;
    case mv_complex:
      x->type=mv_complex;
      mf_complex_powrc(x,a->value.f,b->value.c.re,b->value.c.im);
      return 0;
    default:
      goto rpow;
    }
  case mv_complex:
    switch(b->type){
    case mv_int:
      x->type=mv_complex;
      mf_complex_powcr(x,a->value.c.re,a->value.c.im,b->value.i);
      return 0;
    case mv_float:
      x->type=mv_complex;
      mf_complex_powcr(x,a->value.c.re,a->value.c.im,b->value.f);
      return 0;
    case mv_complex:
      x->type=mv_complex;
      mf_complex_powcc(x,a->value.c.re,a->value.c.im,
        b->value.c.re,b->value.c.im);
      return 0;
    default:
      goto rpow;
    }
  case mv_long:{
    mt_long *aL,*bL,*xL;
    switch(b->type){
    case mv_int:
      aL=(mt_long*)a->value.p;
      bL=mf_int_to_long(b->value.i);
      xL=mf_long_pow(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_delete(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    case mv_long:
      aL=(mt_long*)a->value.p;
      bL=(mt_long*)b->value.p;
      xL=mf_long_pow(aL,bL);
      mf_long_dec_refcount(aL);
      mf_long_dec_refcount(bL);
      x->type=mv_long;
      x->value.p=(mt_basic*)xL;
      return 0;
    default:
      goto rpow;
    }
  }
  case mv_list:
    if(b->type==mv_int){
      mt_list* list=(mt_list*)a->value.p;
      x->type=mv_list;
      x->value.p=(mt_basic*)mf_list_pow(list,b->value.i);
      return 0;
    }else{
      goto rpow;
    }
  default:
    goto pow;  
  }
  pow:
  if(binary_op_method(x,a,b,a,3,"pow")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rpow:
  if(binary_op_method(x,a,b,b,4,"rpow")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a^b: '^' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  return 1;
}

int mf_lt_dec(mt_object* x, mt_object* a, mt_object* b){
  int t;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.i<b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.i<b->value.f;
      return 0;
    case mv_long:
      t = mf_long_cmp_si((mt_long*)b->value.p,a->value.i)>0;
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    default:
      goto rlt;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.f<b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.f<b->value.f;
      return 0;
    default:
      goto rlt;
    }
  case mv_long:
    switch(b->type){
    case mv_int:
      t = mf_long_cmp_si((mt_long*)a->value.p,b->value.i)<0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_long:
      t = mf_long_cmp((mt_long*)a->value.p,(mt_long*)b->value.p)<0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto rlt;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_string:
    if(b->type==mv_string){
      mt_string* s1=(mt_string*)a->value.p;
      mt_string* s2=(mt_string*)b->value.p;
      x->type=mv_bool;
      x->value.b=mf_str_cmp(s1,s2)<0;
      mf_str_dec_refcount(s1);
      mf_str_dec_refcount(s2);
      return 0;
    }else{
      goto rlt;
    }
  default:
    goto lt;
  }
  lt:
  if(binary_op_method(x,a,b,a,3,"lt")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rlt:
  if(binary_op_method(x,a,b,b,4,"rlt")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a<b: '<' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_gt_dec(mt_object* x, mt_object* a, mt_object* b){
  int t;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.i>b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.i>b->value.f;
      return 0;
    case mv_long:
      t = mf_long_cmp_si((mt_long*)b->value.p,a->value.i)<0;
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    default:
      goto rgt;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.f>b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.f>b->value.f;
      return 0;
    default:
      goto rgt;
    }
  case mv_long:
    switch(b->type){
    case mv_int:
      t = mf_long_cmp_si((mt_long*)a->value.p,b->value.i)>0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_long:
      t = mf_long_cmp((mt_long*)a->value.p,(mt_long*)b->value.p)>0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto rgt;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_string:
    if(b->type==mv_string){
      mt_string* s1=(mt_string*)a->value.p;
      mt_string* s2=(mt_string*)b->value.p;
      x->type=mv_bool;
      x->value.b=mf_str_cmp(s1,s2)>0;
      mf_str_dec_refcount(s1);
      mf_str_dec_refcount(s2);
      return 0;
    }else{
      goto rgt;
    }
  default:
    goto gt;
  }
  gt:
  if(binary_op_method(x,a,b,a,3,"gt")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rgt:
  if(binary_op_method(x,a,b,b,4,"rgt")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a>b: '>' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_le_dec(mt_object* x, mt_object* a, mt_object* b){
  int t;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.i<=b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.i<=b->value.f;
      return 0;
    case mv_long:
      t = mf_long_cmp_si((mt_long*)b->value.p,a->value.i)>=0;
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    default:
      goto rle;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.f<=b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.f<=b->value.f;
      return 0;
    default:
      goto rle;
    }
  case mv_long:
    switch(b->type){
    case mv_int:
      t = mf_long_cmp_si((mt_long*)a->value.p,b->value.i)<=0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_long:
      t = mf_long_cmp((mt_long*)a->value.p,(mt_long*)b->value.p)<=0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto rle;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_string:
    if(b->type==mv_string){
      mt_string* s1=(mt_string*)a->value.p;
      mt_string* s2=(mt_string*)b->value.p;
      x->type=mv_bool;
      x->value.b=mf_str_cmp(s1,s2)<=0;
      mf_str_dec_refcount(s1);
      mf_str_dec_refcount(s2);
      return 0;
    }else{
      goto rle;
    }
  default:
    goto le;
  }
  le:
  if(binary_op_method(x,a,b,a,3,"le")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rle:
  if(binary_op_method(x,a,b,b,4,"rle")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a<=b: '<=' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_ge_dec(mt_object* x, mt_object* a, mt_object* b){
  int t;
  switch(a->type){
  case mv_int:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.i>=b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.i>=b->value.f;
      return 0;
    case mv_long:
      t = mf_long_cmp_si((mt_long*)b->value.p,a->value.i)<=0;
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    default:
      goto rge;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.f>=b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.f>=b->value.f;
      return 0;
    default:
      goto rge;
    }
  case mv_long:
    switch(b->type){
    case mv_int:
      t = mf_long_cmp_si((mt_long*)a->value.p,b->value.i)>=0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_long:
      t = mf_long_cmp((mt_long*)a->value.p,(mt_long*)b->value.p)>=0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto rge;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_string:
    if(b->type==mv_string){
      mt_string* s1=(mt_string*)a->value.p;
      mt_string* s2=(mt_string*)b->value.p;
      x->type=mv_bool;
      x->value.b=mf_str_cmp(s1,s2)>=0;
      mf_str_dec_refcount(s1);
      mf_str_dec_refcount(s2);
      return 0;
    }else{
      goto rge;
    }
  default:
    goto ge;
  }
  ge:
  if(binary_op_method(x,a,b,a,3,"ge")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rge:
  if(binary_op_method(x,a,b,b,4,"rge")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a>=b: '>=' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_null;
  return 1;
}

int mf_eq_dec(mt_object* x, mt_object* a, mt_object* b){
  int t;
  switch(a->type){
  case mv_null:
    if(b->type==mv_null){
      x->type=mv_bool;
      x->value.b=1;
      return 0;
    }else{
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_bool:
    switch(b->type){
    case mv_bool:
      x->type=mv_bool;
      x->value.b=a->value.b==b->value.b;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_int:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.i==b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.i==b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_bool;
      x->value.b=a->value.i==b->value.c.re && b->value.c.im==0;
      return 0;
    case mv_long:
      t = mf_long_cmp_si((mt_long*)b->value.p,a->value.i)==0;
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_float:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.f==b->value.i;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.f==b->value.f;
      return 0;
    case mv_complex:
      x->type=mv_bool;
      x->value.b=a->value.f==b->value.c.re && b->value.c.im==0;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_complex:
    switch(b->type){
    case mv_int:
      x->type=mv_bool;
      x->value.b=a->value.c.re==b->value.i && a->value.c.im==0;
      return 0;
    case mv_float:
      x->type=mv_bool;
      x->value.b=a->value.c.re==b->value.f && a->value.c.im==0;
      return 0;
    case mv_complex:
      x->type=mv_bool;
      x->value.b =
        a->value.c.re==b->value.c.re &&
        a->value.c.im==b->value.c.im;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_long:
    switch(b->type){
    case mv_int:
      t = mf_long_cmp_si((mt_long*)a->value.p,b->value.i)==0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_long:
      t = mf_long_cmp((mt_long*)a->value.p,(mt_long*)b->value.p)==0;
      mf_long_dec_refcount((mt_long*)a->value.p);
      mf_long_dec_refcount((mt_long*)b->value.p);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_string:
    switch(b->type){
    case mv_string:
      t = mf_str_eq((mt_string*)a->value.p,(mt_string*)b->value.p);
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_list:
    switch(b->type){
    case mv_list:
      t = mf_list_eq((mt_list*)a->value.p,(mt_list*)b->value.p);
      mf_list_dec_refcount((mt_list*)a->value.p);
      mf_list_dec_refcount((mt_list*)b->value.p);
      if(t<0) return 1;
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_set:
    switch(b->type){
    case mv_set:
       t = mf_set_eq((mt_set*)a->value.p,(mt_set*)b->value.p);
       mf_set_dec_refcount((mt_set*)a->value.p);
       mf_set_dec_refcount((mt_set*)b->value.p);
       x->type=mv_bool;
       x->value.b=t;
       return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_map:
    switch(b->type){
    case mv_map:
       t = mf_map_eq((mt_map*)a->value.p,(mt_map*)b->value.p);
       mf_map_dec_refcount((mt_map*)a->value.p);
       mf_map_dec_refcount((mt_map*)b->value.p);
       if(t<0) return 1;
       x->type=mv_bool;
       x->value.b=t;
       return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  case mv_table:
    if(b->type==mv_null){
      mf_dec_refcount(a);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }else{
      goto eq;
    }
  default:
    switch(b->type){
    case mv_null:
      mf_dec_refcount(a);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    case mv_table:
      goto req;
    default:
      mf_dec_refcount(a);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  }
  eq:
  if(binary_op_method(x,a,b,a,3,"eq")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  req:
  if(binary_op_method(x,a,b,b,4,"req")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  if(a->type!=b->type){
    t=0;
  }else{
    t=a->value.p==b->value.p;
  }
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  x->type=mv_bool;
  x->value.b=t;
  return 0;
}

int mf_ne_dec(mt_object* x, mt_object* a, mt_object* b){
  if(mf_eq_dec(x,a,b)){
    return 1;
  }
  if(x->type!=mv_bool){
    // todo
    abort();
  }else{
    x->value.b=!x->value.b;
  }
  return 0;
}

int mf_eq(mt_object* x, mt_object* a, mt_object* b){
  mf_inc_refcount(a);
  mf_inc_refcount(b);
  return mf_eq_dec(x,a,b);
}

int mf_lt(mt_object* x, mt_object* a, mt_object* b){
  mf_inc_refcount(a);
  mf_inc_refcount(b);
  return mf_lt_dec(x,a,b);
}

int mf_gt(mt_object* x, mt_object* a, mt_object* b){
  mf_inc_refcount(a);
  mf_inc_refcount(b);
  return mf_gt_dec(x,a,b);
}

int mf_is_dec(mt_object* x, mt_object* a, mt_object* b){
  int t;
  if(a->type!=b->type){
    mf_dec_refcount(a);
    mf_dec_refcount(b);
    x->type=mv_bool;
    x->value.b=0;
    return 0;
  }
  switch(a->type){
  case mv_null:
    x->type=mv_bool;
    x->value.b=1;
    return 0;
  case mv_bool:
    x->type=mv_bool;
    x->value.b=a->value.b==b->value.b;
    return 0;
  case mv_int:
    x->type=mv_bool;
    x->value.b=a->value.i==b->value.i;
    return 0;
  case mv_float:
  case mv_complex:
    x->type=mv_bool;
    x->value.b=0;
    return 0;
  default:
    t=a->value.p==b->value.p;
    mf_dec_refcount(a);
    mf_dec_refcount(b);
    x->type=mv_bool;
    x->value.b=t;
    return 0;
  }
}

static
int list_in(mt_object* x, mt_object* a, mt_list* list){
  long size=list->size;
  mt_object* b=list->a;
  long i;
  mt_object y;
  for(i=0; i<size; i++){
    if(mf_eq(&y,a,b+i)){
      mf_traceback("operator 'in'");
      return 1;
    }
    if(y.type!=mv_bool){
      mf_type_error("Type error in 'a in b': return value of '==' is not a boolean.");
      mf_dec_refcount(&y);
      return 1;
    }
    if(y.value.b){
      x->type=mv_bool;
      x->value.b=1;
      return 0;
    }
  }
  x->type=mv_bool;
  x->value.b=0;
  return 0;
}

int mf_in_dec(mt_object* x, mt_object* a, mt_object* b){
  int e;
  switch(b->type){
  case mv_list:
    e=list_in(x,a,(mt_list*)b->value.p);
    mf_dec_refcount(a);
    mf_dec_refcount(b);
    return e;
  case mv_int:{
    if(a->type!=mv_int){
      mf_type_error("Type error in 'a in b': a is not an integer.");
      mf_dec_refcount(a);
      return 1;
    }
    x->type=mv_bool;
    x->value.b=a->value.i>=0 && a->value.i<b->value.i;
    return 0;
  }
  case mv_map:{
    mt_map* m=(mt_map*)b->value.p;
    mt_object y;
    e=mf_map_get(&y,m,a);
    mf_dec_refcount(a);
    mf_dec_refcount(b);
    if(e){
      if(e==1){
        x->type=mv_bool;
        x->value.b=0;
        return 0;
      }else{
        mf_traceback("'a in b'");
        return 1;
      }
    }
    x->type=mv_bool;
    x->value.b=1;
    return 0;
  }
  case mv_set:{
    mt_set* m = (mt_set*)b->value.p;
    int t = mf_set_in(a,m);
    if(t<0){
      mf_traceback("'in'");
      return 1;
    }
    mf_dec_refcount(a);
    mf_set_dec_refcount((mt_set*)b->value.p);
    x->type=mv_bool;
    x->value.b=t;
    return 0;
  }
  case mv_string:{
    if(a->type!=mv_string){
      mf_type_error("Type error in 'a in b': b is a string but a is not a string.");
      return 1;
    }
    mt_string* bs = (mt_string*)b->value.p;
    mt_string* as = (mt_string*)a->value.p;
    x->type=mv_bool;
    x->value.b = mf_str_in_str(as,bs);
    mf_str_dec_refcount(as);
    mf_str_dec_refcount(bs);
    return 0;
  }
  case mv_range:{
    mt_range* r = (mt_range*)b->value.p;
    if(r->a.type==mv_int){
      if(r->b.type!=mv_int){
        mf_type_error("Type error in 'x in a..b: b is not an integer.");
        return 1;
      }
      if(a->type!=mv_int){
        mf_type_error("Type error in 'x in a..b: x is not an integer.");
        return 1;
      }
      long n = a->value.i;
      int t = r->a.value.i<=n && n<=r->b.value.i;
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    }else if(r->a.type==mv_string){
      if(r->b.type!=mv_string){
        mf_type_error("Type error in 's in a..b': b is not a string.");
        return 1;
      }
      if(a->type!=mv_string){
        mf_type_error("Type error in 's in a..b': s is not a string.");
        return 1;
      }
      mt_string* s = (mt_string*)a->value.p;
      mt_string* s1 = (mt_string*)r->a.value.p;
      mt_string* s2 = (mt_string*)r->b.value.p;
      int t = mf_str_in_range(s,s1,s2);
      mf_str_dec_refcount(s);
      mf_dec_refcount(b);
      x->type=mv_bool;
      x->value.b=t;
      return 0;
    }
  }
  default:
    goto in;
  }
  in:
  if(binary_op_method(x,a,b,a,3,"in")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  if(binary_op_method(x,a,b,b,4,"rin")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a in b: 'in' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  return 1;
}

int mf_notin_dec(mt_object* x, mt_object* a, mt_object* b){
  if(mf_in_dec(x,a,b)) return 1;
  x->value.b=!x->value.b;
  return 0;
}

static
int is_table(mt_object* a, mt_table* t){
  return a->type==mv_table && (mt_table*)a->value.p==t;
}

static
int mf_is(mt_object* a, mt_object* b){
  if(a->type!=b->type) return 0;
  switch(a->type){
  case mv_null:
    return 1;
  case mv_bool:
    return a->value.b==b->value.b;
  case mv_int:
    return a->value.i==b->value.i;
  case mv_float:
    return a->value.f==b->value.f;
  case mv_complex:
    return
      a->value.c.re==b->value.c.re &&
      a->value.c.im==b->value.c.im;
  default:
    return a->value.p==b->value.p;
  }
}

int mf_isin(mt_object* x, mt_object* a, mt_object* b){
  switch(a->type){
  case mv_null:
    x->value.b=0;
    x->type=mv_bool;
    return 0;
  case mv_bool:
    x->value.b=is_table(b,mv_type_bool);
    x->type=mv_bool;
    return 0;
  case mv_int:
    x->value.b=is_table(b,mv_type_int);
    x->type=mv_bool;
    return 0;
  case mv_float:
    x->value.b=is_table(b,mv_type_float);
    x->type=mv_bool;
    return 0;
  case mv_complex:
    x->value.b=is_table(b,mv_type_complex);
    x->type=mv_complex;
    return 0;
  case mv_string:
    x->value.b=is_table(b,mv_type_string);
    x->type=mv_bool;
    return 0;
  case mv_bstring:
    x->value.b=is_table(b,mv_type_bstring);
    x->type=mv_bool;
    return 0;
  case mv_list:
    x->value.b=is_table(b,mv_type_list);
    x->type=mv_bool;
    return 0;
  case mv_function:
    x->value.b=is_table(b,mv_type_function);
    x->type=mv_bool;
    return 0;
  case mv_table:{
    mt_table* t=(mt_table*)a->value.p;
    if(t->prototype.type==mv_null){
      x->value.b=0;
      x->type=mv_bool;
      return 0;
    }else if(mf_is(&t->prototype,b)){
      x->value.b=1;
      x->type=mv_bool;
      return 0;
    }else{
      return mf_isin(x,&t->prototype,b);
    }
  }
  default:
    abort();
  }
}

int mf_isin_dec(mt_object* x, mt_object* a, mt_object* b){
  mt_object t;
  int e=mf_isin(&t,a,b);
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  mf_copy(x,&t);
  return e;
}

int mf_bit_and_dec(mt_object* x, mt_object* a, mt_object* b){
  switch(a->type){
  case mv_bool:
    if(b->type==mv_bool){
      x->type=mv_bool;
      x->value.b=a->value.b&b->value.b;
      return 0;
    }else{
      goto rbit_and;
    }
  case mv_int:
    if(b->type==mv_int){
      x->type=mv_int;
      x->value.i=a->value.i&b->value.i;
      return 0;
    }else{
      goto rbit_and;
    }
  case mv_set:
    if(b->type==mv_set){
      mt_set* seta = (mt_set*)a->value.p;
      mt_set* setb = (mt_set*)b->value.p;
      x->type=mv_set;
      x->value.p=(mt_basic*)mf_set_intersection(seta,setb);
      mf_set_dec_refcount(seta);
      mf_set_dec_refcount(setb);
      return 0;
    }else{
      goto rbit_and;
    }
  default:
    goto bit_and;
  }
  bit_and:
  if(binary_op_method(x,a,b,a,3,"band")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rbit_and:
  if(binary_op_method(x,a,b,b,4,"rband")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a&b: '&' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  return  1;
}

int mf_bit_or_dec(mt_object* x, mt_object* a, mt_object* b){
  switch(a->type){
  case mv_bool:
    if(b->type==mv_bool){
      x->type=mv_bool;
      x->value.b=a->value.b|b->value.b;
      return 0;
    }else{
      goto rbit_or;
    }
  case mv_int:
    if(b->type==mv_int){
      x->type=mv_int;
      x->value.i=a->value.i|b->value.i;
      return 0;
    }else{
      goto rbit_or;
    }
  case mv_set:
    if(b->type==mv_set){
      mt_set* seta = (mt_set*)a->value.p;
      mt_set* setb = (mt_set*)b->value.p;
      x->type=mv_set;
      x->value.p=(mt_basic*)mf_set_union(seta,setb);
      mf_set_dec_refcount(seta);
      mf_set_dec_refcount(setb);
      return 0;
    }else{
      goto rbit_or;
    }
  default:
    goto bit_or;
  }
  bit_or:
  if(binary_op_method(x,a,b,a,3,"bor")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rbit_or:
  if(binary_op_method(x,a,b,b,4,"rbor")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a|b: '|' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  return  1;
}

int mf_bit_xor_dec(mt_object* x, mt_object* a, mt_object* b){
  switch(a->type){
  case mv_bool:
    if(b->type==mv_bool){
      x->type=mv_bool;
      x->value.b=a->value.b^b->value.b;
      return 0;
    }else{
      goto rbit_xor;
    }
  case mv_int:
    if(b->type==mv_int){
      x->type=mv_int;
      x->value.i=a->value.i^b->value.i;
      return 0;
    }else{
      goto rbit_xor;
    }
  case mv_set:
    if(b->type==mv_set){
      mt_set* seta = (mt_set*)a->value.p;
      mt_set* setb = (mt_set*)b->value.p;
      x->type=mv_set;
      x->value.p=(mt_basic*)mf_set_symmetric_difference(seta,setb);
      mf_set_dec_refcount(seta);
      mf_set_dec_refcount(setb);
      return 0;
    }else{
      goto rbit_xor;
    }
  default:
    goto bit_xor;
  }
  bit_xor:
  if(binary_op_method(x,a,b,a,3,"bxor")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  rbit_xor:
  if(binary_op_method(x,a,b,b,4,"rbxor")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  binary_op_type_error(
    "Type error in a$b: '$' is not defined for\n"
    "a of type '%s' and b of type '%s'.", a,b
  );
  mf_dec_refcount(a);
  mf_dec_refcount(b);
  return  1;
}

int mf_get(mt_object* x, mt_object* a, mt_object* key){
  int type=a->type;
  if(type==mv_list){
    mt_list* list = (mt_list*)a->value.p;
    if(key->type==mv_range){
      mt_range* r=(mt_range*)key->value.p;
      int e=mf_list_slice(x,list,r);
      if(e) return 1;
      mf_list_dec_refcount(list);
      mf_dec_refcount(key);
      return 0;
    }else if(key->type!=mv_int){
      mf_type_error("Type error in a[i]: a is a list but i is not an integer.");
      return 1;
    }
    long index = key->value.i;
    if(index<0){
      index+=list->size;
      if(index<0){
        mf_index_error("Index error in a[i]: i is out of lower bound.");
        return 1;
      }
    }else if(index>=list->size){
      mf_index_error("Index error in a[i]: i is out of upper bound.");
      return 1;
    }
    mf_copy_inc(x,list->a+index);
    mf_list_dec_refcount(list);
    return 0;
  }else if(type==mv_map){
    mt_map* m = (mt_map*)a->value.p;
    int e = mf_map_get(x,m,key);
    if(e){
      if(e==1){
        mf_index_error("Index error in d[key]: key is not in d.");
        return 1;
      }else{
        mf_traceback("d[key]");
        return 1;
      }
    }
    mf_inc_refcount(x);
    return 0;
  }else if(type==mv_string){
    mt_string* s = (mt_string*)a->value.p;
    return mf_str_get(x,s,key);
  }
  if(binary_op_method(x,a,key,a,3,"get")){
    if(op_method_error) return 1;
  }else{
    return 0;
  }
  mf_type_error("Type error in a[i]: a is not a list and not a dictionary.");
  return 1;
}



int mf_fsize(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"size");
    return 1;
  }
  mt_object* a=v+1;
  switch(a->type){
  case mv_list:{
    mt_list* list=(mt_list*)a->value.p;
    x->type=mv_int;
    x->value.i=list->size;
    return 0;
  } break;
  case mv_map:{
    mt_map* m = (mt_map*)a->value.p;
    x->type=mv_int;
    x->value.i=m->htab.size;
    return 0;
  } break;
  case mv_set:{
    mt_set* m = (mt_set*)a->value.p;
    x->type=mv_int;
    x->value.i=m->htab.size;
    return 0;
  } break;
  case mv_string:{
    mt_string* s = (mt_string*)a->value.p;
    x->type=mv_int;
    x->value.i=s->size;
    return 0;
  } break;
  default:
    mf_type_error("Type error in size(a): cannot determine the size of a.");
    return 1;
  }
}

int mf_frecord(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"record");
    return 1;
  }
  if(v[1].type!=mv_table){
    mf_type_error("Type error: expected a table object.");
    return 1;
  }
  mt_table* t = (mt_table*)v[1].value.p;
  mt_map* m;
  if(t->m){
    m=t->m;
  }else{
    m=mf_empty_map();
  }
  m->refcount++;
  x->type=mv_map;
  x->value.p=(mt_basic*)m;
  return 0;
}

mt_table* mf_table(mt_object* prototype){
  mt_table* t = mf_malloc(sizeof(mt_table));
  t->refcount=1;
  if(prototype){
    mf_copy_inc(&t->prototype,prototype);
  }else{
    t->prototype.type=mv_null;
  }
  t->m=NULL;
  t->name=NULL;
  return t;
}

int mf_table_literal(mt_object* x, mt_object* prototype, mt_object* m){
  if(m!=0 && m->type!=mv_map){
    mf_type_error("Type error in table: expected dictionary.");
    return 1;
  }
  mt_table* t = mf_malloc(sizeof(mt_table));
  t->refcount=1;
  mf_copy(&t->prototype,prototype);
  t->m=(mt_map*)m->value.p;
  t->name=NULL;
  x->type=mv_table;
  x->value.p=(mt_basic*)t;
  return 0;
}

int mf_fobject(mt_object* x, int argc, mt_object* v){
  mt_table* t;
  if(argc==0){
    t=mf_table(NULL);
    x->type=mv_table;
    x->value.p=(mt_basic*)t;
    return 0;
  }else if(argc==1){
    t=mf_table(v+1);
    x->type=mv_table;
    x->value.p=(mt_basic*)t;
    return 0;
  }else if(argc==2){
    if(v[2].type!=mv_map){
      mf_type_error("Type error in object(type,d): d is not a dictionary.");
      return 1;
    }
    mt_map* m=(mt_map*)v[2].value.p;
    m->refcount++;
    t=mf_table(v+1);
    t->m=m;
    x->type=mv_table;
    x->value.p=(mt_basic*)t;
    return 0;
  }else{
    mf_argc_error(argc,0,2,"object");
    return 1;
  }
}

void mf_type(mt_object* x, mt_object* a){
  switch(a->type){
  case mv_bool:
    mv_type_bool->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_bool;
    break;
  case mv_int:
    mv_type_int->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_int;
    break;
  case mv_float:
    mv_type_float->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_float;
    break;
  case mv_complex:
    mv_type_complex->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_complex;
    break;
  case mv_long:
    mv_type_long->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_long;
    break;
  case mv_list:
    mv_type_list->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_list;
    break;
  case mv_string:
    mv_type_string->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_string;
    break;
  case mv_function:
    mv_type_function->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_function;
    break;
  case mv_map:
    mv_type_dict->refcount++;
    x->type=mv_table;
    x->value.p=(mt_basic*)mv_type_dict;
    break;
  case mv_table:{
    mt_table* t=(mt_table*)a->value.p;
    mf_copy_inc(x,&t->prototype);
  } break;
  default:
    x->type=mv_null;
  }
}

int mf_ftype(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"type");
    return 1;
  }
  mf_type(x,v+1);
  return 0;
}

int mf_fint(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"int");
    return 1;
  }
  switch(v[1].type){
  case mv_bool:
    x->type=mv_int;
    x->value.i=v[1].value.b;
    return 0;
  case mv_int:
    x->type=mv_int;
    x->value.i=v[1].value.i;
    return 0;
  case mv_float:
    x->type=mv_int;
    x->value.i=lround(v[1].value.f);
    return 0;
  case mv_string:
    return mf_str_to_int(x,(mt_string*)v[1].value.p);
  default:
    mf_type_error("Type error in int(x): cannot convert x into an integer.");
    return 1;
  }
}

mt_range* mf_range(mt_object* a, mt_object* b, mt_object* step){
  mt_range* r = mf_malloc(sizeof(mt_range));
  r->refcount=1;
  mf_copy(&r->a,a);
  mf_copy(&r->b,b);
  mf_copy(&r->step,step);
  return r;
}

mt_tuple* mf_tuple_raw(long n){
  mt_tuple* t = mf_malloc(sizeof(mt_tuple)+n*sizeof(mt_object));
  t->refcount=1;
  t->size=n;
  return t;
}

mt_function* mf_insert_function(mt_map* m, int min, int max,
  const char* id, mt_plain_fn pf
){
  mt_function* f = (mt_function*) mf_new_function(NULL);
  f->fp=pf;
  if(min==max){
    f->argc=min;
  }else{
    f->argc=-1;
    f->min=min;
    f->max=max;
  }
  mt_string* key = mf_str_new(strlen(id),id);
  mt_object x;
  x.type=mv_function;
  x.value.p=(mt_basic*)f;
  mf_map_set_str(m,key,&x);
  return f;
}

void mf_insert_object(mt_map* m, const char* id, mt_object* x){
  mt_string* key = mf_cstr_to_str(id);
  mf_map_set_str(m,key,x);
}

void mf_cstr_exception(const char* s){
  mf_dec_refcount(&mv_exception);
  mv_exception.type=mv_string;
  mv_exception.value.p = (mt_basic*)mf_cstr_to_str(s);
}

void mf_argc_error(int argc, int min, int max, const char* s){
  mv_traceback_list = mf_raw_list(0);
  char a[200];
  if(min==max){
    if(min==0){
      snprintf(a,200,"Error in %s: expected no argument but got %i.",s,argc);
    }else if(min==1){
      snprintf(a,200,"Error in %s: expected exactly one argument but got %i.",s,argc);
    }else if(min==2){
      snprintf(a,200,"Error in %s: expected exactly %i arguments but got %i.",s,min,argc);
    }
  }else{
    snprintf(a,200,"Error in %s: expected %i..%i arguments but got %i.",s,min,max,argc);
  }
  mf_cstr_exception(a);
}

static
void mf_new_traceback_list(){
  if(mv_traceback_list){
    mf_list_dec_refcount(mv_traceback_list);
  }
  mv_traceback_list = mf_raw_list(0);
}

void mf_type_error(const char* s){
  mf_new_traceback_list();
  mf_cstr_exception(s);
}

void mf_value_error(const char* s){
  mf_new_traceback_list();
  mf_cstr_exception(s);
}

void mf_index_error(const char* s){
  mf_new_traceback_list();
  mf_cstr_exception(s);
}

void mf_std_exception(const char* s){
  mf_new_traceback_list();
  mf_cstr_exception(s);
}

void mf_traceback(const char* s){
  if(mv_traceback_list){
    mt_object t;
    t.type=mv_string;
    t.value.p=(mt_basic*)mf_cstr_to_str(s);
    mf_list_push(mv_traceback_list,&t);
  }
}

mt_table* mf_math_load();
mt_table* mf_cmath_load();
mt_table* mf_ma_load();
#ifdef __linux__
mt_table* mf_sys_load();
mt_table* mf_time_load();
mt_table* mf_os_load();
mt_table* mf_gx_load();
#else
mt_table* mf_sys_load(){
  mf_std_exception("Error: module 'sys' is not implemented.");
  return NULL;
}
mt_table* mf_time_load(){
  mf_std_exception("Error: module 'time' is not implemented.");
  return NULL;
}
mt_table* mf_os_load(){
  mf_std_exception("Error: module 'os' is not implemented.");
  return NULL;
}
mt_table* mf_gx_load(){
  mf_std_exception("Error: module 'gx' is not implemented.");
  return NULL;
}
#endif


typedef struct{
  int size;
  const char* a;
  mt_table* (*load)(void);
  mt_table* t;
} mt_load_tab_element;

static
mt_load_tab_element load_tab[] = {
  {5, "cmath", mf_cmath_load, NULL},
  {2, "gx", mf_gx_load, NULL},
  {2, "ma", mf_ma_load, NULL},
  {4, "math", mf_math_load, NULL},
  {2, "os", mf_os_load, NULL},
  {3, "sys", mf_sys_load, NULL},
  {4, "time", mf_time_load, NULL}
};

static
mt_load_tab_element* load_tab_find(int size, const char* a){
  int n = sizeof(load_tab)/sizeof(mt_load_tab_element);
  int i;
  for(i=0; i<n; i++){
    if(size==load_tab[i].size){
      if(memcmp(a,load_tab[i].a,size)==0) return load_tab+i;
    }
  }
  return NULL;
}

static
mt_table* mf_load(mt_object* x){
  if(x->type!=mv_string){
    mf_type_error("Type error in load(id): id is not a string.");
    return NULL;
  }
  mt_string* s = (mt_string*)x->value.p;
  mt_bstr id;
  mf_encode_utf8(&id,s->a,s->size);
  mt_table* t;
  mt_load_tab_element* p;
  p=load_tab_find(id.size,id.a);
  if(p){
    if(p->t){
      t=p->t;
      t->refcount++;
    }else{
      p->t=p->load();
      t=p->t;
    }
    mf_free(id.a);
    return t;
  }else{
    t=mf_load_module(id.a);
    mf_free(id.a);
    return t;
  }
}

int mf_fload(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"load");
    return 1;
  }
  mt_table* t=mf_load(v+1);
  if(t==NULL){
    mf_traceback("load");
    return 1;
  }
  x->type=mv_table;
  x->value.p=(mt_basic*)t;
  return 0;
}

static
int mf_use(mt_table* t, mt_map* d){
  mt_htab_element* a = d->htab.a;
  int n = d->htab.capacity;
  mt_map* m = t->m;
  char buffer[200];
  if(m==NULL){
    puts("Error in mf_use.");
    abort();
  }
  mt_object key;
  mt_object value;
  int e;
  long i;
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(a[i].key.type!=mv_string){
        mf_type_error("Type error in import: some key is not of type string.");
        return 1;
      }
      if(a[i].value.type==mv_string){
        mf_copy(&key,&a[i].value);
      }else{
        mf_copy(&key,&a[i].key);
      }
      e = mf_map_get(&value,m,&key);
      if(e){
        mt_string* s = (mt_string*)key.value.p;
        mt_bstr bs;
        mf_encode_utf8(&bs,s->a,s->size);
        snprintf(buffer,200,"Error in import: '%s' was not found.",bs.a);
        free(bs.a);
        mf_std_exception(buffer);
        return 1;
      }
      mf_map_set_hash(mv_gvtab,&a[i].key,&value,a[i].hash);
    }
  }
  return 0;
}

int mf_fimport(mt_object* x, int argc, mt_object* v){
  if(argc!=1 && argc!=2){
    mf_argc_error(argc,1,2,"__import__");
    return 1;
  }
  if(v[1].type!=mv_map){
    mf_type_error("Type error in __import__(d): d is not a dictionary.");
    return 1;
  }
  if(argc==2){
    if(v[2].type!=mv_map){
      mf_type_error("Type error in __import__(d,i): i is not a dictionary.");
      return 1;
    }
  }
  mt_map* d = (mt_map*)v[1].value.p;
  mt_htab_element* a = d->htab.a;
  long n = d->htab.capacity;
  long i;
  mt_table* module;
  mt_object t;
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(a[i].value.type==mv_string){
        module = mf_load(&a[i].value);
      }else{
        module = mf_load(&a[i].key);
      }
      if(module==NULL){
        mf_traceback("import");
        return 1;
      }
      t.type=mv_table;
      t.value.p=(mt_basic*)module;
      mf_map_set_hash(mv_gvtab,&a[i].key,&t,a[i].hash);
    }
  }
  if(argc==2 && module){
    if(mf_use(module,(mt_map*)v[2].value.p)){
      return 1;
    }
  }
  x->type=mv_null;
  return 0;
}

int mf_eval_string(mt_object* x, mt_string* s);
int mf_feval(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"eval");
    return 0;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in eval(s): s is not a string.");
    return 0;
  }
  mt_string* s = (mt_string*)v[1].value.p;
  return mf_eval_string(x,s);
}

int mf_fgtab(mt_object* x, int argc, mt_object* v){
  if(argc!=0 && argc!=1){
    mf_argc_error(argc,0,1,"gtab");
    return 1;
  }
  mt_map* gtab;
  if(argc==0){
    gtab = mv_gvtab;
  }else{
    gtab = function_self->gtab;
  }
  assert(gtab);
  gtab->refcount++;
  x->type=mv_map;
  x->value.p=(mt_basic*)gtab;
  return 0;
}

int mf_fmin(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"min");
    return 1;
  }
  mt_object y;
  if(mf_lt(&y,v+1,v+2)){
    mf_traceback("min");
    return 1;
  }
  if(y.type!=mv_bool){
    mf_type_error("Type error in min(a,b): return value of a<b is not a boolean.");
    return 1;
  }
  if(y.value.b){
    mf_copy_inc(x,v+1);
  }else{
    mf_copy_inc(x,v+2);
  }
  return 0;
}

int mf_fmax(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"max");
    return 1;
  }
  mt_object y;
  if(mf_lt(&y,v+1,v+2)){
    mf_traceback("max");
    return 1;
  }
  if(y.type!=mv_bool){
    mf_type_error("Type error in max(a,b): return value of a<b is not a boolean.");
    return 1;
  }
  if(y.value.b){
    mf_copy_inc(x,v+2);
  }else{
    mf_copy_inc(x,v+1);
  }
  return 0;
}

int mf_ffloat(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"float");
    return 1;
  }
  int type=v[1].type;
  double t;
  if(type==mv_float){
    t=v[1].value.f;
  }else if(type==mv_int){
    t=v[1].value.i;
  }else if(type==mv_bool){
    t=v[1].value.b;
  }else{
    mf_type_error("Type error in float(x): cannot convert x to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=t;
  return 0;
}

int mf_ford(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"ord");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in ord(c): c is not a string.");
    return 1;
  }
  mt_string* s = (mt_string*)v[1].value.p;
  if(s->size!=1){
    mf_value_error("Value error in ord(c): size(c)!=1.");
    return 1;
  }
  x->type=mv_int;
  x->value.i = (long)s->a[0];
  return 0;
}

int mf_fchr(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"chr");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error("Type error in chr(n): n is not an integer.");
    return 1;
  }
  mt_string* s = mf_raw_string(1);
  s->a[0]=(uint32_t)v[1].value.i;
  x->type=mv_string;
  x->value.p=(mt_basic*)s;
  return 0;
}

int mf_const(mt_object* x){
  switch(x->type){
  case mv_null:
  case mv_bool:
  case mv_int:
  case mv_complex:
  case mv_string:
  case mv_bstring:
  case mv_long:
    return 0;
  case mv_list:{
    mt_list* list = (mt_list*)x->value.p;
    list->frozen=1;
    long size=list->size;
    long i;
    for(i=0; i<size; i++){
      if(mf_const(list->a+i)){
        mf_traceback("const(x: list)");
        return 1;
      }
    }
    return 0;
  }
  case mv_map:{
    mt_map* m = (mt_map*)x->value.p;
    m->frozen=1;
    mt_htab_element* a = m->htab.a;
    long n = m->htab.capacity;
    long i;
    for(i=0; i<n; i++){
      if(a[i].taken){
        if(mf_const(&a[i].value)){
          mf_traceback("const(x: dict)");
          return 1;
        }
      }
    }
    return 0;
  }
  case mv_set:{
    mt_set* m = (mt_set*)x->value.p;
    m->frozen=1;
    return 0;
  }
  default:
    mf_type_error("Type error in const(x): cannot freeze x.");
    return 1;
  }
}

int mf_fconst(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"const");
    return 1;
  }
  if(mf_const(v+1)){
    return 1;
  }
  mf_copy_inc(x,v+1);
  return 0;
}

int mf_fextend(mt_object* x, int argc, mt_object* v){
  if(argc<2){
    mf_argc_error(argc,2,-1,"extend");
    return 1;
  }
  int i;
  if(v[1].type!=mv_table && v[1].type!=mv_function){
    mf_type_error("Type error in extend(a,b): a is not a table and not a function.");
    return 1;
  }
  mt_table* t = (mt_table*)v[1].value.p;
  if(t->m==NULL){
    t->m = mf_empty_map();
  }
  for(i=2; i<=argc; i++){
    if(v[i].type!=mv_table && v[i].type!=mv_function){
      mf_type_error("Type error in extend(a,*argv): argv[i] is not a table and not a function.");
      return 1;
    }
    if(((mt_table*)v[i].value.p)->m){
      mf_map_extend(t->m,((mt_table*)v[i].value.p)->m);
    }
  }
  x->type=mv_null;
  return 0;
}

static
mt_long* long_int(mt_object* x){
  if(x->type==mv_int){
    return mf_int_to_long(x->value.i);
  }else if(x->type==mv_long){
    x->value.p->refcount++;
    return (mt_long*)x->value.p;
  }else{
    return NULL;
  }
}

int mf_fpow(mt_object* x, int argc, mt_object* v){
  if(argc==3){
    mt_long *a,*n,*m,*y;
    a = long_int(v+1);
    if(a==NULL){
      mf_type_error("Type error in pow(a,n,m): cannot convert a to long.");
      return 1;
    }
    n = long_int(v+2);
    if(n==NULL){
      mf_type_error("Type error in pow(a,n,m): cannot convert n to long.");
      mf_long_dec_refcount(a);
      return 1;
    }
    m = long_int(v+3);
    if(m==NULL){
      mf_type_error("Type error in pow(a,n,m): cannot convert m to long.");
      mf_long_dec_refcount(a);
      mf_long_dec_refcount(n);
      return 1;
    }
    y = mf_long_powmod(a,n,m);
    mf_long_dec_refcount(a);
    mf_long_dec_refcount(n);
    mf_long_dec_refcount(m);
    x->type=mv_long;
    x->value.p=(mt_basic*)y;
    return 0;
  }else if(argc==2){
    mf_inc_refcount(v+1);
    mf_inc_refcount(v+2);
    return mf_pow_dec(x,v+1,v+2);
  }else{
    mf_argc_error(argc,2,3,"pow");
    return 1;
  }
}

int mf_fabs(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"abs");
    return 1;
  }
  switch(v[1].type){
  case mv_int:{
    long i=v[1].value.i;
    x->type=mv_int;
    x->value.i=i<0? -i: i;
    return 0;
  }
  case mv_float:{
    double f=v[1].value.f;
    x->type=mv_float;
    x->value.f=f<0.0? -f: f;
    return 0;
  }
  case mv_complex:{
    x->type=mv_float;
    x->value.f=hypot(v[1].value.c.re,v[1].value.c.im);
    return 0;
  }
  case mv_long:{
    mt_long* iL = (mt_long*)v[1].value.p;
    x->type=mv_long;
    x->value.p=(mt_basic*)mf_long_abs(iL);
    return 0;
  }
  default:
    mf_type_error("Type error in abs(x): x must be of type int, float or complex.");
    return 1;
  }
}

int mf_fsgn(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sgn");
    return 1;
  }
  switch(v[1].type){
  case mv_int:{
    long i=v[1].value.i;
    x->type=mv_int;
    x->value.i=i<0? -1: 1;
    return 0;
  }
  case mv_float:{
    double f=v[1].value.f;
    x->type=mv_float;
    x->value.f=f<0.0? -1.0: 1.0;
    return 0;
  }
  case mv_complex:{
    double re=v[1].value.c.re;
    double im=v[1].value.c.im;
    double r;
    x->type=mv_complex;
    if(re==0 && im==0){
      x->value.c.re=0;
      x->value.c.im=0;
    }else{
      r = hypot(re,im);
      x->value.c.re=re/r;
      x->value.c.im=im/r;
    }
    return 0;
  }
  case mv_long:{
    mt_long* iL = (mt_long*)v[1].value.p;
    x->type=mv_int;
    x->value.i=mf_long_sgn(iL);
    return 0;
  }
  default:
    mf_type_error("Type error in math.sgn(x): cannot convert x to float.");
    return 1;
  }
}

