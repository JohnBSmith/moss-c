
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <moss.h>
#include <string.h>
#include <objects/string.h>

#define GROWTH_FACTOR 2

long mf_floor_mod(long x, long m);
int mf_add_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_mpy_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_eq(mt_object* x, mt_object* a, mt_object* b);
int mf_lt(mt_object* x, mt_object* a, mt_object* b);
int mf_gt(mt_object* x, mt_object* a, mt_object* b);
mt_list* mf_map_to_list(mt_map* m);
static mt_list* mf_list_filter_map(mt_list* a, mt_function* f);
mt_list* mf_iterator_to_list(mt_function* f, long max);
mt_list* mf_bstring_to_list(mt_bstring* bs);
int mf_ncall(mt_function* f, mt_object* x, mt_object* t);
mt_function* mf_iter(mt_object* x);
void mf_function_dec_refcount(mt_function* f);

mt_list* mf_raw_list(long size){
  mt_list* list = mf_malloc(sizeof(mt_list));
  list->a = mf_malloc(size*sizeof(mt_object));
  list->size=size;
  list->capacity=size;
  list->frozen=0;
  list->refcount=1;
  return list;
}

int mf_new_list(mt_object* x, int argc, mt_object* v){
  mt_list* list = mf_malloc(sizeof(mt_list));
  list->a = mf_malloc(argc*sizeof(mt_object));
  list->size=argc;
  list->capacity=argc;
  list->frozen=0;
  list->refcount=1;
  int i;
  for(i=1; i<=argc; i++){
    mf_copy_inc(list->a+i-1,v+i);
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

mt_list* mf_list_noinc(int argc, mt_object* v){
  mt_list* list = mf_malloc(sizeof(mt_list));
  list->a = mf_malloc(argc*sizeof(mt_object));
  list->size=argc;
  list->capacity=argc;
  list->frozen=0;
  list->refcount=1;
  int i;
  for(i=0; i<argc; i++){
    mf_copy(list->a+i,v+i);
  }
  return list;
}

void mf_list_delete(mt_list* list){
  long size=list->size;
  long i;
  for(i=0; i<size; i++){
    mf_dec_refcount(list->a+i);
  }
  mf_free(list->a);
  mf_free(list);
}

void mf_list_dec_refcount(mt_list* a){
  if(a->refcount==1){
    mf_list_delete(a);    
  }else{
    a->refcount--;
  }
}

mt_list* mf_list_copy(mt_list* a){
  long size=a->size;
  mt_list* b=mf_raw_list(size);
  long i;
  for(i=0; i<size; i++){
    mf_copy_inc(b->a+i,a->a+i);
  }
  return b;
}

int mf_put_repr(mt_object* x);
int mf_buffer_print(long size, mt_object* a){
  long i;
  for(i=0; i<size; i++){
    if(i>0) printf(", ");
    if(mf_put_repr(a+i)){
      printf("\n");
      return 1;
    }
  }
  return 0;
}

void mf_list_push(mt_list* list, mt_object* x){
  long n = list->size;
  long capacity;
  if(n==list->capacity){
    if(n==0){
      capacity=10;
    }else{
      capacity=GROWTH_FACTOR*list->capacity;
    }
    mt_object* a = mf_malloc(capacity*sizeof(mt_object));
    long i;
    for(i=0; i<n; i++){
      mf_copy(a+i,list->a+i);
    }
    mf_copy(a+n,x);
    list->size++;
    list->capacity=capacity;
    mf_free(list->a);
    list->a=a;
  }else{
    mf_copy(list->a+n,x);
    list->size++;
  }
}

mt_list* mf_list_add(mt_list* a, mt_list* b){
  long asize=a->size;
  long bsize=b->size;
  mt_list* x = mf_raw_list(asize+bsize);
  long i;
  for(i=0; i<asize; i++){
    mf_copy_inc(x->a+i,a->a+i);
  }
  for(i=0; i<bsize; i++){
    mf_copy_inc(x->a+asize+i,b->a+i);
  }
  return x;
}

mt_list* mf_list_mpy(mt_list* a, long n){
  long size=a->size;
  mt_list* x = mf_raw_list(size*n);
  long i,j;
  for(i=0; i<n; i++){
    for(j=0; j<size; j++){
      mf_copy_inc(x->a+i*size+j,a->a+j);
    }
  }
  return x;
}

static
mt_list* frange_to_list(mt_range* r){
  double a,b,step;
  if(r->a.type==mv_int){
    a=r->a.value.i;
  }else if(r->a.type==mv_float){
    a=r->a.value.f;
  }else if(r->a.type==mv_null){
    a=0;
  }else{
    mf_type_error1("in list(a..b:step): cannot convert a (type: %s) to float.",&r->a);
    return NULL;
  }
  if(r->b.type==mv_int){
    b=r->b.value.i;
  }else if(r->b.type==mv_float){
    b=r->b.value.f;
  }else if(r->b.type==mv_null){
    mf_value_error("Value error in list: cannot convert infinite range into a list.");
  }else{
    mf_type_error("Type error in list(a..b:step): cannot convert a to float.");
    return NULL;
  }
  if(r->step.type==mv_int){
    step = r->step.value.i;
  }else if(r->step.type==mv_float){
    step = r->step.value.f;
  }else{
    mf_type_error1("in list(a..b:step): cannot convert step (type: %s) to float.",&r->step);
    return NULL;
  }

  long n = 1+round((b-a)/step);
  if(n<=0){
    return mf_raw_list(0);
  }
  long i;
  mt_list* list = mf_raw_list(n);
  for(i=0; i<n; i++){
    list->a[i].type=mv_float;
    list->a[i].value.f=a+i*step;
  }
  return list;
}

static
mt_list* srange_to_list(mt_range* r){
  mt_string* a = (mt_string*)r->a.value.p;
  if(r->b.type!=mv_string){
    mf_type_error1("in list(a..b): b (type: %s) is not a string.",&r->b);
    return NULL;
  }
  mt_string* b = (mt_string*)r->b.value.p;
  if(a->size!=1 || b->size!=1){
    mf_value_error("Value error in list(a..b): a and b must be strings of size one.");
    return NULL;
  }
  unsigned long i=a->a[0];
  unsigned long j=b->a[0];
  mt_list* list = mf_raw_list(0);
  mt_object t;
  t.type=mv_string;
  while(i<=j){
    mt_string* s = mf_raw_string(1);
    s->a[0]=i;
    t.value.p=(mt_basic*)s;
    mf_list_push(list,&t);
    i++;
  }
  return list;
}

static
mt_list* range_to_list(mt_range* r){
  long a,b,step,i;
  switch(r->a.type){
  case mv_int:
    a=r->a.value.i;
    break;
  case mv_null:
    a=0;
    break;
  case mv_float:
    return frange_to_list(r);
  case mv_string:
    return srange_to_list(r);
  default:
    mf_type_error1("in list(a..b): a (type: %s) is not an integer.",&r->a);
    return NULL;
  }
  if(r->b.type==mv_int){
    b=r->b.value.i;
  }else if(r->b.type==mv_null){
    mf_value_error("Value error in list: cannot convert infinite range into a list.");
    return NULL;
  }else if(r->b.type==mv_float){
    return frange_to_list(r);
  }else{
    mf_type_error1("in list(a..b): b (type: %s) is not an integer.",&r->b);
    return NULL;
  }
  if(r->step.type==mv_int){
    step=r->step.value.i;
  }else if(r->step.type==mv_float){
    return frange_to_list(r);
  }else{
    mf_type_error1("in list(a..b:step): step (type: %s) is not an integer and not a float.",&r->step);
    return NULL;
  }
  
  if(step==0){
    mf_value_error("Value error in list(a..b:step): step must not be zero.");
    return NULL;
  }

  mt_list* list;
  long size;
  if((b-a<0)==(step<0)){
    size = (b-a)/step+1;
    if(size<0) abort();
  }else{
    size=0;
  }

  list = mf_raw_list(size);
  for(i=0; i<size; i++){
    list->a[i].type=mv_int;
    list->a[i].value.i=a+i*step;
  }
  return list;
}

static
mt_string* chr(unsigned long n){
  mt_string* s = mf_raw_string(1);
  s->a[0]=n;
  return s;
}

mt_list* mf_list(mt_object* a){
  mt_list* list;
  long i;
  switch(a->type){
  case mv_int:{
    long n=a->value.i;
    if(n<0){
      mf_value_error("Value error in list(n): n is negative.");
      return NULL;
    }
    list = mf_raw_list(n);
    for(i=0; i<n; i++){
      list->a[i].type=mv_int;
      list->a[i].value.i=i;
    }
    return list;
  }
  case mv_range:
    return range_to_list((mt_range*)a->value.p);
  case mv_list:
    list = (mt_list*)a->value.p;
    list->refcount++;
    return list;
  case mv_string:{
    mt_string* s = (mt_string*)a->value.p;
    list = mf_raw_list(s->size);
    for(i=0; i<s->size; i++){
      list->a[i].type=mv_string;
      list->a[i].value.p=(mt_basic*)chr(s->a[i]);
    }
    return list;
  }
  case mv_bstring:
    return mf_bstring_to_list((mt_bstring*)a->value.p);
  case mv_map:{
    mt_map* m = (mt_map*)a->value.p;
    return mf_map_to_list(m);
  }
  case mv_function:{
    mt_function* f = (mt_function*)a->value.p;
    return mf_iterator_to_list(f,-1);
  }
  default:
    break;
  }
  mt_function* f = mf_iter(a);
  if(f==NULL){
    mf_traceback("list");
    return NULL;
  }
  mt_list* y = mf_iterator_to_list(f,-1);
  mf_function_dec_refcount(f);
  // mf_type_error1("in list(x): cannot convert x (type: %s) into a list.",a);
  return y;
}

int mf_flist(mt_object* x, int argc, mt_object* v){
  if(argc!=1 && argc!=2){
    mf_argc_error(argc,1,2,"list");
    return 1;
  }
  mt_list* list = mf_list(v+1);
  if(list==NULL) return 1;
  if(argc==2){
    if(v[2].type!=mv_function){
      mf_list_dec_refcount(list);
      return 1;
    }
    mt_function* f = (mt_function*)v[2].value.p;
    mt_list* list2 = mf_list_filter_map(list,f);
    mf_list_dec_refcount(list);
    if(list2==NULL) return 1;
    list=list2;
  }  
  x->type=mv_list;
  x->value.p = (mt_basic*)list;
  return 0;
}

static
int list_push(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error1("in a.push(x): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* a = (mt_list*)v[0].value.p;
  if(a->frozen){
    mf_value_error("Value error in a.push(x): a is frozen.");
    return 1;
  }
  int i;
  for(i=1; i<=argc; i++){
    mf_inc_refcount(v+i);
    mf_list_push(a,v+i);
  }
  x->type=mv_null;
  return 0;
}

static
int list_pop(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error1("in a.pop(): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  if(list->frozen){
    mf_std_exception("Value error in a.pop(): a is frozen.");
    return 1;
  }
  if(list->size==0){
    mf_index_error("Index error in a.pop(): a is empty.");
    return 1;
  }
  if(argc==1){
    if(v[1].type!=mv_int){
      mf_type_error1("in a.pop(i): i (type %s) is not an integer.",v+1);
      return 1;
    }
    long i = v[1].value.i;
    if(i<0){
      i+=list->size;
      if(i<0){
        mf_index_error2("in a.pop(i): i (value: %li) is out of lower bound (size: %li).",
          v[1].value.i,list->size
        );
        return 1;
      }
    }else if(i>=list->size){
      mf_index_error2("in a.pop(i): i (value: %li) is out of upper bound (size: %li).",
        i,list->size
      );
      return 1;
    }
    mf_copy(x,list->a+i);
    long j;
    for(j=i; j+1<list->size; j++){
      mf_copy(list->a+j,list->a+j+1);
    }
    list->size--;
    return 0;
  }else if(argc==0){
    list->size--;
    mf_copy(x,list->a+list->size);
    return 0;
  }else{
    mf_argc_error(argc,0,1,"pop");
    return 1;
  }
}

static
int list_clear(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error1("in a.clear(): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  if(list->frozen){
    mf_value_error("Value error in a.clear(): a is frozen.");
    return 1;
  }
  if(argc==0){
    long i;
    for(i=0; i<list->size; i++){
      mf_dec_refcount(list->a+i);
    }
    list->size=0;
  }else if(argc==1){
    if(v[1].type!=mv_int){
      mf_type_error1("in a.clear(n): i (type: %s) is not an integer.",v+1);
      return 1;
    }
    long n=v[1].value.i;
    if(n>list->size){
      mf_value_error("Value error in a.clear(n): n is out of upper bound.");
      return 1;
    }
    if(n<0){
      n+=list->size;
      if(n<0){
        mf_value_error("Value error in a.clear(n): n is out of lower bound.");
        return 1;
      }
    }
    long i;
    for(i=n; i<list->size; i++){
      mf_dec_refcount(list->a+i);
    }
    list->size=n;
  }else{
    mf_argc_error(argc,0,1,"clear");
    return 1;
  }
  x->type=mv_null;
  return 0;
}

void mf_reverse(long size, mt_object* a){
  mt_object t;
  long i,m;
  mt_object* b=a+size-1;
  m=size/2;
  for(i=0; i<m; i++){
    mf_copy(&t,a+i);
    mf_copy(a+i,b-i);
    mf_copy(b-i,&t);
  }
}

static
int list_reverse(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"rev");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("Type error in a.rev(): a (type: %s) is not of type list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  if(list->frozen){
    mf_value_error("Value error in a.rev(): a is frozen.");
    return 1;
  }
  mf_reverse(list->size,list->a);
  list->refcount++;
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

static
int list_rot(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"rot");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.rot(n): a (type: %s) is not of type list.",v);
    return 1;    
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in a.rot(n): n (type: %s) is not of type int.",v+1);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  if(list->frozen){
    mf_value_error("Value error in a.rot(): a is frozen.");
    return 1;
  }
  long size = list->size;
  long d = mf_floor_mod(-v[1].value.i,size);
  mf_reverse(d,list->a);
  mf_reverse(size-d,list->a+d);
  mf_reverse(size,list->a);
  list->refcount++;
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

static
int list_map(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"map");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.map(f): a (type: %s) is not a list.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error1("in a.map(f): f (type: %s) is not a function.",&v[1]);
    return 1;
  }
  mt_list* a = (mt_list*)v[0].value.p;
  a->refcount++;
  mt_function* f = (mt_function*)v[1].value.p;
  mt_list* b = mf_raw_list(a->size);
  long i;
  mt_object argv[2];
  argv[0].type=mv_null;
  mt_object t;
  int e;
  for(i=0; i<a->size; i++){
    // mf_copy_inc(argv+1,a->a+i);
    // e = mf_call(f,b->a+i,1,argv);
    // mf_dec_refcount(&argv[1]);
    e = mf_ncall(f,b->a+i,a->a+i);
    if(e){
      // mf_dec_refcount(&argv[0]);
      mf_traceback("map");
      mf_dec_refcounts(i,b->a);
      mf_free(b->a);
      mf_free(b);
      return 1;
    }
  }
  // mf_dec_refcount(&argv[0]);
  x->type=mv_list;
  x->value.p=(mt_basic*)b;
  return 0;
}

static
int list_filter(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"filter");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.filter(f): a (type: %s) is not a list.",v);
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error1("in a.filter(f): f (type: %s) is not a function.",v+1);
    return 1;
  }
  mt_function* f = (mt_function*)v[1].value.p;
  mt_list* list = (mt_list*)v[0].value.p;
  mt_list* list2 = mf_raw_list(0);
  mt_object y;
  long i;
  for(i=0; i<list->size; i++){
    if(mf_ncall(f,&y,list->a+i)){
      mf_traceback("filter");
      mf_list_dec_refcount(list2);
      return 1;
    }
    if(y.type!=mv_bool){
      mf_type_error1("in a.filter(f): return value (type: %s) of f is not a boolean.",&y);
      mf_list_dec_refcount(list2);
      mf_dec_refcount(&y);
      return 1;
    }
    if(y.value.b==1){
      mf_list_push(list2,list->a+i);
    }
  }
  for(i=0; i<list2->size; i++){
    mf_inc_refcount(list2->a+i);
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list2;
  return 0;
}

static
mt_list* mf_list_filter_map(mt_list* a, mt_function* f){
  mt_list* list = mf_raw_list(0);
  mt_object y;
  long i;
  for(i=0; i<a->size; i++){
    if(mf_ncall(f,&y,a->a+i)){
      mf_traceback("list");
      mf_list_dec_refcount(list);
      return NULL;
    }else if(y.type!=mv_null){
      mf_list_push(list,&y);
    }
  }
  return list;
}

int mf_fmap(mt_object* x, int argc, mt_object* v){
  if(argc<2){
    mf_argc_error(argc,1,1,"map");
    return 1;
  }
  int i,j,e;
  for(i=1; i<argc-1; i++){
    if(v[i].type!=mv_list){
      mf_type_error("Type error: map takes lists and a function.");
      return 1;
    }
  }
  if(v[argc].type!=mv_function){
    mf_type_error("Type error: last argument of map is not a function.");
    return 1;
  }
  mt_object y;
  mt_list* a;
  mt_list* list = mf_raw_list(0);
  mt_object* argv = mf_malloc(argc*sizeof(mt_object));
  argv[0].type=mv_null;
  i=0;
  while(1){
    for(j=1; j<argc; j++){
      a = (mt_list*)v[j].value.p;
      if(i>=a->size) goto out;
      mf_copy(argv+j,a->a+i);
    }
    e = mf_call((mt_function*)v[argc].value.p,&y,argc-1,argv);
    if(e){
      if(argc==2){
        mf_traceback("map(a,f)");
      }else{
        mf_traceback("map(a0[,...],f)");
      }
      mf_list_dec_refcount(list);
      mf_free(argv);
      return 1;
    }
    mf_list_push(list,&y);
    i++;
  }
  out:
  mf_free(argv);
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

int mf_list_sum0(mt_object* x, mt_list* a){
  long size=a->size;
  if(size==0){
    x->type=mv_int;
    x->value.i=0;
    return 0;
  }
  long i;
  mt_object y;
  mf_copy_inc(&y,a->a);
  for(i=1; i<size; i++){
    mf_inc_refcount(a->a+i);
    if(mf_add_dec(&y,&y,a->a+i)){
      mf_traceback("sum");
      return 1;
    }
  }
  mf_copy(x,&y);
  return 0;
}

int mf_list_sum1(mt_object* x, mt_list* a, mt_function* f){
  long size=a->size;
  if(size==0){
    x->type=mv_int;
    x->value.i=0;
    return 0;
  }
  long i;
  mt_object t,y;
  if(mf_ncall(f,&y,&a->a[0])){
    mf_traceback("sum");
    return 1;
  }
  for(i=1; i<size; i++){
    if(mf_ncall(f,&t,a->a+i)){
      mf_dec_refcount(&y);
      mf_traceback("sum");
      return 1;
    }
    if(mf_add_dec(&y,&y,&t)){
      mf_traceback("sum");
      return 1;
    }
  }
  mf_copy(x,&y);
  return 0;
}

static
int list_sum(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error("Type error in a.sum(): a is not a list.");
    return 1;
  }
  mt_list* a = (mt_list*)v[0].value.p;
  if(argc==0){
    return mf_list_sum0(x,a);
  }else if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in a.sum(f): f is not a function.");
      return 1;
    }
    mt_function* f = (mt_function*)v[1].value.p;
    return mf_list_sum1(x,a,f);
  }else{
    mf_argc_error(argc,0,1,"sum");
    return 1;
  }
}

int mf_list_prod0(mt_object* x, mt_list* a){
  long size=a->size;
  if(size==0){
    x->type=mv_int;
    x->value.i=1;
    return 0;
  }
  long i;
  mt_object y;
  mf_copy_inc(&y,a->a);
  for(i=1; i<size; i++){
    mf_inc_refcount(a->a+i);
    if(mf_mpy_dec(&y,&y,a->a+i)){
      mf_traceback("prod");
      return 1;
    }
  }
  mf_copy(x,&y);
  return 0;
}

int mf_list_prod1(mt_object* x, mt_list* a, mt_function* f){
  long size=a->size;
  if(size==0){
    x->type=mv_int;
    x->value.i=1;
    return 0;
  }
  long i;
  mt_object t,y;
  if(mf_ncall(f,&y,&a->a[0])){
    mf_traceback("prod");
    return 1;
  }
  for(i=1; i<size; i++){
    if(mf_ncall(f,&t,a->a+i)){
      mf_dec_refcount(&y);
      mf_traceback("prod");
      return 1;
    }
    if(mf_mpy_dec(&y,&y,&t)){
      mf_traceback("prod");
      return 1;
    }
  }
  mf_copy(x,&y);
  return 0;
}

static
int list_prod(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error("Type error in a.prod(): a is not a list.");
    return 1;
  }
  mt_list* a = (mt_list*)v[0].value.p;
  if(argc==0){
    return mf_list_prod0(x,a);
  }else if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in a.prod(f): f is not a function.");
      return 1;
    }
    mt_function* f = (mt_function*)v[1].value.p;
    return mf_list_prod1(x,a,f);
  }else{
    mf_argc_error(argc,0,1,"prod");
    return 1;
  }
}

int mf_count_element(mt_object* x, mt_list* list, mt_object* t){
  long size=list->size;
  long i,k;
  k=0;
  mt_object y;
  for(i=0; i<size; i++){
    if(mf_eq(&y,list->a+i,t)){
      mf_traceback("count");
      return 1;
    }
    if(y.type!=mv_bool){
      mf_dec_refcount(&y);
      mf_type_error("Type error in a.count(x): result of '==' is not a boolean.");
      return 1;
    }
    if(y.value.b) k++;
  }
  x->type=mv_int;
  x->value.i=k;
  return 0;
}

int mf_list_count(mt_object* x, mt_list* list, mt_function* p){
  mt_object y;
  long counter=0;
  long i;
  for(i=0; i<list->size; i++){
    if(mf_ncall(p,&y,list->a+i)){
      mf_traceback("count(a,p)");
      return 1;
    }
    if(y.type!=mv_bool){
      mf_type_error1("in count(a,p): return value (type: %s) of p is not a boolean.",&y);
      mf_dec_refcount(&y);
      return 1;
    }
    if(y.value.b){
      counter++;
    }
  }
  x->type=mv_int;
  x->value.i=counter;
  return 0;
}

static
int reduce(mt_object* x, mt_list* list, mt_function* f){
  long size = list->size;
  mt_object y;
  mt_object argv[3];
  argv[0].type=mv_null;
  long i;
  if(size>0){
    mf_copy_inc(&y,list->a);
  }else{
    x->type=mv_null;
    return 0;
  }
  int e;
  for(i=1; i<size; i++){
    mf_copy(argv+1,&y);
    mf_copy(argv+2,list->a+i);
    e=mf_call(f,&y,2,argv);
    mf_dec_refcount(argv+1);
    if(e){
      mf_traceback("reduce");
      return 1;
    }
  }
  mf_copy(x,&y);
  return 0;
}

static
int list_reduce(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error("Type error in a.reduce(f): a is not a list.");
    return  1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in a.reduce(f): f is not a function.");
      return 1;
    }
    mt_function* f = (mt_function*)v[1].value.p;
    return reduce(x,list,f);
  }
  if(argc!=2){
    mf_argc_error(argc,1,2,"reduce");
    return 1;
  }
  if(v[2].type!=mv_function){
    mf_type_error("Type error in a.reduce(start,f): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[2].value.p;
  mt_object y;
  mt_object argv[3];
  argv[0].type=mv_null;
  mf_copy_inc(&y,v+1);
  long size = list->size;
  long i;
  int e;
  for(i=0; i<size; i++){
    mf_copy(argv+1,&y);
    mf_copy(argv+2,list->a+i);
    e=mf_call(f,&y,2,argv);
    mf_dec_refcount(argv+1);
    if(e){
      mf_traceback("reduce");
      return 1;
    }
  }
  mf_copy(x,&y);
  return 0;
}

static
int list_shuffle(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"shuffle");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.shuffle(): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  if(list->frozen){
    mf_value_error("Value error in a.shuffle(): a is frozen.");
    return 1;
  }
  long size=list->size;
  mt_object* a=list->a;
  mt_object h;
  long i,j;
  for(i=size-1; i>0; i--){
    j=rand()%(i+1);
    mf_copy(&h,a+j);
    mf_copy(a+j,a+i);
    mf_copy(a+i,&h);
  }
  list->refcount++;
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

int mf_list_slice(mt_object* x, mt_list* a, mt_range* r){
  long i,j,step;
  if(r->step.type==mv_int){
    step=r->step.value.i;
  }else{
    goto error;
  }
  if(step==0){
    mf_value_error("Value error in a[i..j:step]: step must not be zero.");
    return 1;
  }
  if(r->a.type==mv_int){
    i=r->a.value.i;
  }else if(r->a.type==mv_null){
    if(step<0){
      i=a->size-1;
    }else{
      i=0;
    }
  }else{
    goto error;
  }
  if(i<0) i+=a->size;
  if(r->b.type==mv_null){
    if(step<0){
      j=0;
    }else{
      j=a->size-1;
    }
  }else if(r->b.type==mv_int){
    j=r->b.value.i;
    if(j<-1) j=a->size+j;
  }else{
    goto error;
  }
  mt_list* list = mf_raw_list(0);
  int k;
  if(step>0){
    for(k=i; k<=j; k+=step){
      if(k>=0 && k<a->size){
        mf_inc_refcount(a->a+k);
        mf_list_push(list,a->a+k);
      }
    }
  }else{
    for(k=i; k>=j; k+=step){
      if(k>=0 && k<a->size){
        mf_inc_refcount(a->a+k);
        mf_list_push(list,a->a+k);
      }
    }
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
  error:
  mf_type_error("Type error in a[r]: expected integer range r.");
  return 1;
}

// todo: zip([1,2],[3]) fails
int mf_fzip(mt_object* x, int argc, mt_object* v){
  long i,j;
  for(i=1; i<=argc; i++){
    if(v[i].type!=mv_list){
      char s[400];
      snprintf(s,400,"in zip(*a): a[%li] (type: %%s) is not a list.",i-1);
      mf_type_error1(s,v+i);
      return 1;
    }
  }
  mt_list* a;
  mt_list *list,*list2;
  int size,size2;
  if(argc==0){
    size=0;
  }else{
    list=(mt_list*)v[1].value.p;
    size=list->size;
  }
  for(i=2; i<=argc; i++){
    list2=(mt_list*)v[i].value.p;
    size2=list->size;
    if(size2<size) size=size2;
  }
  list = mf_raw_list(0);
  mt_object t;
  t.type=mv_list;
  for(i=0; i<size; i++){
    a=mf_raw_list(argc);
    for(j=0; j<argc; j++){
      list2=(mt_list*)v[j+1].value.p;
      mf_copy_inc(a->a+j,list2->a+i);
    }
    t.value.p=(mt_basic*)a;
    mf_list_push(list,&t);
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

int mf_funzip(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"unzip");
    return 1;
  }
  mt_list* a = mf_list(v+1);
  if(a==NULL){
    mf_type_error1("in unzip(a): cannot convert a (type: %s) into a list.",v+1);
    return 1;
  }
  mt_list* t;
  int i,j;
  for(i=0; i<a->size; i++){
    if(a->a[i].type!=mv_list){
      mf_type_error1("in unzip(a): a[k] (type: %s) is not a list.",a->a+i);
      mf_list_dec_refcount(a);
      return 1;
    }
  }
  mt_list* r = mf_raw_list(0);
  if(a->size==0){
    x->type=mv_list;
    x->value.p=(mt_basic*)r;
    mf_list_dec_refcount(a);
    return 0;
  }
  t=(mt_list*)a->a[0].value.p;
  long size=t->size;
  mt_object y;
  y.type=mv_list;
  for(j=0; j<size; j++){
    y.value.p=(mt_basic*)mf_raw_list(0);
    mf_list_push(r,&y);
  }
  for(i=0; i<a->size; i++){
    t = (mt_list*)a->a[i].value.p;
    for(j=0; j<size && j<t->size; j++){
      mf_copy_inc(&y,t->a+j);
      mf_list_push((mt_list*)r->a[j].value.p,&y);
    }
  }
  mf_list_dec_refcount(a);
  x->type=mv_list;
  x->value.p=(mt_basic*)r;
  return 0;
}

static
int list_swap(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"a.swap");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.swap(i,j): a (type: %s) is not a list.",v);
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in a.swap(i,j): i (type: %s) is not an integer.",v+1);
    return 1;
  }
  if(v[2].type!=mv_int){
    mf_type_error1("in a.swap(i,j) j (type: %s) is not an integer.",v+2);
    return 1;
  }
  long i,j;
  mt_object t;
  mt_list* list = (mt_list*)v[0].value.p;
  i=v[1].value.i;
  j=v[2].value.i;
  if(i<0) i=i+list->size;
  if(j<0) j=j+list->size;
  if(i<0 || i>=list->size){
    mf_index_error("Error in a.swap(i,j): i is out of bounds.");
    return 0;
  }
  if(j<0 || j>=list->size){
    mf_index_error("Error in a.swap(i,j): j is out of bounds.");
    return 0;
  }
  mf_copy(&t,list->a+i);
  mf_copy(list->a+i,list->a+j);
  mf_copy(list->a+j,&t);
  list->refcount++;
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

static
int list_extend(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"extend");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.extend(b): a (type: %s) is not a list.",v);
    return 1;
  }
  if(v[1].type!=mv_list){
    mf_type_error1("in a.extend(b): b (type: %s) is not a list.",v+1);
    return 1;
  }
  mt_list* p = (mt_list*)v[0].value.p;
  mt_list* list = (mt_list*)v[1].value.p;
  long size = list->size;
  mt_object* a = list->a;
  long i;
  for(i=0; i<size; i++){
    mf_inc_refcount(a+i);
    mf_list_push(p,a+i);
  }
  x->type=mv_null;
  return 0;
}

static
int cmp_error;

static
int list_std_cmp(const void* a, const void* b){
  if(cmp_error) return 0;
  mt_object y;
  mt_object* p1=(mt_object*)a;
  mt_object* p2=(mt_object*)b;
  if(mf_lt(&y,p1,p2)){
    cmp_error=1;
    return 0;
  }
  if(y.type!=mv_bool){
    mf_dec_refcount(&y);
    cmp_error=1;
    return 0;
  }
  if(y.value.b){
    return -1;
  }else{
    return 1;
  }
}

typedef struct{
  mt_object key;
  mt_object value;
} mt_pair;

static
mt_pair* decorate(mt_list* list, mt_function* f){
  long size = list->size;
  mt_pair* a = mf_malloc(size*sizeof(mt_pair));
  long i,j;
  mt_object y;
  mt_object argv[2];
  argv[0].type=mv_null;
  for(i=0; i<size; i++){
    mf_copy(argv+1,list->a+i);
    if(mf_call(f,&y,1,argv)){
      for(j=0; j<i; j++){
        mf_dec_refcount(&a[j].key);
      }
      mf_free(a);
      return NULL;
    }
    mf_copy(&a[i].key,&y);
    mf_copy(&a[i].value,list->a+i);
  }
  return a;
}

static
int cmp_keys(const void* pa, const void* pb){
  if(cmp_error) return 0;
  mt_pair *a,*b;
  a = (mt_pair*)pa;
  b = (mt_pair*)pb;
  mt_object y;
  if(mf_lt(&y,&a->key,&b->key)){
    cmp_error=1;
    return 0;
  }
  if(y.type!=mv_bool){
    mf_dec_refcount(&y);
    cmp_error=1;
    return 0;
  }
  if(y.value.b){
    return -1;
  }else{
    return 1;
  }
}

static
int list_sort(mt_object* x, int argc, mt_object* v){
  if(argc!=0 && argc!=1){
    mf_argc_error(argc,0,1,"sort");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.sort(f): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  if(list->frozen){
    mf_value_error("Value error in a.sort(): a is frozen.");
    return 1;
  }
  if(argc==0){
    cmp_error=0;
    qsort(list->a,list->size,sizeof(mt_object),list_std_cmp);
    if(cmp_error){
      mf_traceback("sort");
      return 1;
    }
  }else{
    if(v[1].type!=mv_function){
      mf_type_error1("in a.sort(f): f (type: %s) is not a function.",v+1);
      return 1;
    }
    mt_function* f = (mt_function*)v[1].value.p;
    cmp_error=0;
    mt_pair* a = decorate(list,f);
    if(a==NULL){
      mf_traceback("sort");
      return 1;
    }
    qsort(a,list->size,sizeof(mt_pair),cmp_keys);
    if(cmp_error){
      mf_traceback("sort");
      return 1;
    }
    long i;
    for(i=0; i<list->size; i++){
      mf_dec_refcount(&a[i].key);
      mf_copy(list->a+i,&a[i].value);
    }
    mf_free(a);
  }
  list->refcount++;
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

int mf_join(mt_vec* buffer, mt_list* list, mt_string* sep){
  long sepsize;
  if(sep==NULL){
    sepsize=0;
  }else{
    sepsize=sep->size;
  }
  long i,j;
  mt_string* es;
  for(i=0; i<list->size; i++){
    if(i>0){
      for(j=0; j<sepsize; j++){
        mf_vec_push(buffer,sep->a+j);
      }
    }
    if(list->a[i].type==mv_string){
      es = (mt_string*)list->a[i].value.p;
      for(j=0; j<es->size; j++){
        mf_vec_push(buffer,es->a+j);
      }
    }else{
      es = mf_str(list->a+i);
      if(es==NULL){
        mf_traceback("join");
        return 1;
      }
      for(j=0; j<es->size; j++){
        mf_vec_push(buffer,es->a+j);
      }
      mf_str_dec_refcount(es);
    }
  }
  return 0;
}

static
int list_join(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error1("in a.join(): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  mt_string* sep;
  if(argc==1){
    if(v[1].type!=mv_string){
      mf_type_error1("in a.join(sep): sep (type: %s) is not a string.",v+1);
      return 1;
    }
    sep=(mt_string*)v[1].value.p;
  }else if(argc==0){
    sep=NULL;
  }else{
    mf_argc_error(argc,0,1,"join");
    return 1;
  }
  mt_vec buffer;
  mf_vec_init(&buffer,4);
  if(mf_join(&buffer,list,sep)){
    mf_vec_delete(&buffer);
    return 1;
  }
  mt_string* s = mf_raw_string(buffer.size);
  memcpy(s->a,buffer.a,buffer.size*4);
  mf_vec_delete(&buffer);

  x->type=mv_string;
  x->value.p=(mt_basic*)s;
  return 0;
}

mt_string* mf_list_to_string(mt_list* list){
  mt_vec buffer;
  mf_vec_init(&buffer,4);
  mt_string* sep = mf_cstr_to_str(", ");
  uint32_t c='[';
  mf_vec_push(&buffer,&c);
  if(mf_join(&buffer,list,sep)){
    mf_str_dec_refcount(sep);
    mf_vec_delete(&buffer);
    return NULL;
  }
  c=']';
  mf_vec_push(&buffer,&c);
  mt_string* s = mf_raw_string(buffer.size);
  memcpy(s->a,buffer.a,buffer.size*4);
  mf_str_dec_refcount(sep);
  mf_vec_delete(&buffer);
  return s;
}

static
int list_chain(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"chain");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.chain(): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* elist;
  mt_list* list = (mt_list*)v[0].value.p;
  mt_list* list2 = mf_raw_list(0);
  mt_object t;
  long i,j;
  for(i=0; i<list->size; i++){
    if(list->a[i].type==mv_list){
      elist=(mt_list*)list->a[i].value.p;
      for(j=0; j<elist->size; j++){
        mf_copy_inc(&t,elist->a+j);
        mf_list_push(list2,&t);
      }
    }else if(list->a[i].type==mv_range){
      elist = mf_list(&list->a[i]);
      if(elist==NULL){
        mf_list_dec_refcount(list2);
        mf_traceback("chain");
        return 1;
      }
      for(j=0; j<elist->size; j++){
        mf_copy_inc(&t,elist->a+j);
        mf_list_push(list2,&t);
      }
    }else{
      mf_copy_inc(&t,list->a+i);
      mf_list_push(list2,&t);
    }
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list2;
  return 0;
}

static
mt_list* list_partition(mt_list* list, mt_function* f){
  long size=list->size;
  long* a = mf_malloc(sizeof(long)*size);
  int counter=0;
  mt_object y;
  mt_object argv[3];
  argv[0].type=mv_null;
  mt_object t;
  long i,j,c;
  for(i=0; i<size; i++){
    mf_copy(argv+1,list->a+i);
    c=-1;
    for(j=0; j<i; j++){
      mf_copy(argv+2,list->a+j);
      if(mf_call(f,&y,2,argv)){
        mf_traceback("a.chunks(f)");
        mf_free(a);
        return NULL;
      }
      if(y.type!=mv_bool){
        mf_type_error1("in a.chunks(f): return value (type: %s) of f is not a boolean.",&y);
        mf_free(a);
        return NULL;
      }
      if(y.value.b==1){
        c=a[j];
        break;
      }
    }
    if(c<0){
      a[i]=counter;
      counter++;
    }else{
      a[i]=c;
    }
  }
  mt_list* list2 = mf_raw_list(counter);
  for(i=0; i<list2->size; i++){
    list2->a[i].type=mv_list;
    list2->a[i].value.p = (mt_basic*)mf_raw_list(0);
  }
  for(i=0; i<size; i++){
    c=a[i];
    if(c<0 || c>=list2->size){
      puts("Error in list_partition.");
      abort();
    }
    mf_copy_inc(&t,list->a+i);
    mf_list_push((mt_list*)list2->a[c].value.p,&t);
  }
  mf_free(a);
  return list2;
}

static
int list_chunks(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"chunks");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.chunks(n): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  mt_list* list2;
  mt_list* elist;
  mt_object t;
  long i,j,k;
  if(v[1].type==mv_int){
    k = v[1].value.i;
    if(k<=0){
      mf_value_error("Value error in a.chunks(n): n<=0, but expected n>0.");
      return 0;
    }
    list2 = mf_raw_list(0);
    for(i=0; i<list->size; i+=k){
      elist=mf_raw_list(0);
      for(j=0; j<k; j++){
        if(i+j>=list->size) break;
        mf_copy_inc(&t,list->a+i+j);
        mf_list_push(elist,&t);
      }
      t.type=mv_list;
      t.value.p=(mt_basic*)elist;
      mf_list_push(list2,&t);
    }
    x->type=mv_list;
    x->value.p=(mt_basic*)list2;
    return 0;
  }else if(v[1].type==mv_list){
    mt_list* klist = (mt_list*)v[1].value.p;
    list2 = mf_raw_list(0);
    long index=0;
    i=0;
    while(i<list->size){
      if(klist->a[index].type!=mv_int){
        mf_type_error("Type error in a.chunks(index_list): "
        "element of index_list is not an integer.");
        mf_list_dec_refcount(list2);
        return 1;
      }
      k = klist->a[index].value.i;
      index++;
      if(index>=klist->size) index=0;
      if(k<=0){
        mf_value_error("Value error in a.chunks(index_list): "
        "index_list[i]<=0, but expected index_list[i]>0.");
        mf_list_dec_refcount(list2);
        return 1;
      }
      elist=mf_raw_list(0);
      for(j=0; j<k; j++){
        if(i+j>=list->size) break;
        mf_copy_inc(&t,list->a+i+j);
        mf_list_push(elist,&t);
      }
      t.type=mv_list;
      t.value.p=(mt_basic*)elist;
      mf_list_push(list2,&t);
      i+=k;
    }
    x->type=mv_list;
    x->value.p=(mt_basic*)list2;
    return 0;
  }else if(v[1].type==mv_function){
    mt_function* f=(mt_function*)v[1].value.p;
    list2 = list_partition(list,f);
    if(list2==NULL) return 1;
    x->type=mv_list;
    x->value.p=(mt_basic*)list2;
    return 0;
  }else{
    mf_type_error1("in a.chunks(n): n (type: %s) is not an integer.",v+1);
    return 1;  
  }
}

static
int list_match(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"match");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.match(f): a (type: %s) is not a list.",v);
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error1("in a.match(f): f (type: %s) is not a function.",v+1);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  mt_function* f = (mt_function*)v[1].value.p;
  mt_object argv[2];
  argv[0].type=mv_null;
  long i;
  mt_object y;
  for(i=0; i<list->size; i++){
    mf_copy(argv+1,list->a+i);
    if(mf_call(f,&y,1,argv)){
      mf_traceback("match");
      return 1;
    }
    if(y.type==mv_bool && y.value.b==1){
      mf_copy_inc(x,list->a+i);
      return 0;
    }
  }
  x->type=mv_null;
  return 0;
}

static
int list_index(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"index");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.index(f): a (type: %s) is not a list.",v);
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error1("in a.index(f): f (type: %s) is not a function.",v+1);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  mt_function* f = (mt_function*)v[1].value.p;
  mt_object argv[2];
  argv[0].type=mv_null;
  long i;
  mt_object y;
  for(i=0; i<list->size; i++){
    mf_copy(argv+1,list->a+i);
    if(mf_call(f,&y,1,argv)){
      mf_traceback("index");
      return 1;
    }
    if(y.type==mv_bool && y.value.b==1){
      x->type=mv_int;
      x->value.i=i;
      return 0;
    }
  }
  x->type=mv_null;
  return 0;
}

mt_list* mf_list_slice_index_list(mt_list* list, mt_list* ilist){
  long i,index;
  char s[200];
  mt_list* list2 = mf_raw_list(0);
  long ilist_size=ilist->size;
  long size=list->size;
  mt_object* a=ilist->a;
  mt_object t;
  for(i=0; i<ilist_size; i++){
    if(a[i].type!=mv_int){
      snprintf(s,200,"in index list a: a[%li] (type: %%s) is not an integer.",i);
      mf_type_error1(s,a+i);
      mf_list_dec_refcount(list2);
      return NULL;
    }
    index = a[i].value.i;
    if(index<0) index+=size;
    if(index>=0 && index<size){
      mf_copy_inc(&t,list->a+index);
      mf_list_push(list2,&t);
    }
  }
  return list2;
}

static
int list_flip(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"flip");
    return 1;
  }
  if(v[0].type!=mv_list){
    mf_type_error1("in a.flip(): a (type: %s) is not a list.",v);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  char s[200];
  long i,j,size;
  long max=0;
  for(i=0; i<list->size; i++){
    if(list->a[i].type!=mv_list){
      snprintf(s,200,"in a.flip(): a[%li] (type: %%s) is not a list.",i);
      mf_type_error1(s,list->a+i);
      return 1;
    }
    size = ((mt_list*)list->a[i].value.p)->size;
    if(size>max) max=size;
  }
  mt_list* list2 = mf_raw_list(0);
  mt_list *elist, *elist2;
  mt_object t;
  for(i=0; i<max; i++){
    elist2 = mf_raw_list(0);
    for(j=0; j<list->size; j++){
      elist = (mt_list*)list->a[j].value.p;
      if(i>=elist->size){
        if(j!=list->size-1){
          t.type=mv_null;
          mf_list_push(elist2,&t);
        }
      }else{
        mf_copy_inc(&t,elist->a+i);
        mf_list_push(elist2,&t);
      }
    }
    t.type=mv_list;
    t.value.p=(mt_basic*)elist2;
    mf_list_push(list2,&t);
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list2;
  return 0;
}

static
int list_insert(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_list){
    mf_type_error1("in a.insert(i,x): a (type: %s) is not a list.",v);
    return 1;
  }
  if(argc!=2){
    mf_argc_error(argc,2,2,"insert");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in a.insert(i,x): i (type: %s) is not an integer.",v+1);
    return 1;
  }
  mt_list* list = (mt_list*)v[0].value.p;
  long size=list->size;
  long index = v[1].value.i;
  if(index<0){
    index+=size;
    if(index<0){
      mf_index_error2("in a.insert(i,x): i (value: %li) is out of lower bound (size: %li).",
        v[1].value.i,size
      );
      return 1;
    }
  }else if(index>size){
    mf_index_error2("in a.insert(i,x): i (value: %li) is out of upper bound (size: %li).",
      index,size
    );
    return 1;
  }
  if(index==size){
    mf_inc_refcount(v+2);
    mf_list_push(list,v+2);
    x->type=mv_null;
    return 0;
  }
  long i;
  mt_object* a;
  if(size<list->capacity){
    a=list->a;
    for(i=size-1; i>=index; i--){
      mf_copy(a+i+1,a+i);
    }
    mf_inc_refcount(v+2);
    mf_copy(a+index,v+2);
    list->size++;
    x->type=mv_null;
    return 0;
  }
  long size2=size*GROWTH_FACTOR;
  a = mf_malloc(size2*sizeof(mt_object));
  for(i=0; i<index; i++){
    mf_copy(a+i,list->a+i);
  }
  mf_inc_refcount(v+2);
  mf_copy(a+index,v+2);
  for(i=index; i<size; i++){
    mf_copy(a+i+1,list->a+i);
  }
  mf_free(list->a);
  list->size=size+1;
  list->capacity=size2;
  list->a=a;
  x->type=mv_null;
  return 0;
}

static
long ipow(long a, long n){
  long y=1;
  while(n!=0){
    if(n%2==1) y*=a;
    n=n/2; a*=a;
  }
  return y;
}

mt_list* mf_list_pow(mt_list* a, long n){
  if(n<0){
    mf_value_error("Value error in [...]^n: n<0.");
    return NULL;
  }
  long size = a->size;
  long m = ipow(size,n);
  mt_list* list = mf_raw_list(m);
  long i,j;
  for(i=0; i<m; i++){
    list->a[i].type=mv_list;
    list->a[i].value.p = (mt_basic*)mf_raw_list(n);
  }
  mt_list* t;
  long index;
  long k=1;
  for(j=n-1; j>=0; j--){
    for(i=0; i<m; i++){
      t = (mt_list*)list->a[i].value.p;
      index = (i/k)%size;
      mf_copy_inc(t->a+j,a->a+index);
    }
    k=k*size;
  }
  return list;
}

mt_list* mf_list_cart(mt_list* a, mt_list* b){
  mt_list* pair;
  mt_list* list = mf_raw_list(0);
  long i,j;
  mt_object t;
  t.type=mv_list;
  for(i=0; i<a->size; i++){
    for(j=0; j<b->size; j++){
      pair = mf_raw_list(2);
      mf_copy_inc(pair->a,a->a+i);
      mf_copy_inc(pair->a+1,b->a+j);
      t.value.p=(mt_basic*)pair;
      mf_list_push(list,&t);
    }
  }
  return list;
}

int mf_list_eq(mt_list* La, mt_list* Lb){
  if(La->size!=Lb->size) return 0;
  long size=La->size;
  long i;
  mt_object* a=La->a;
  mt_object* b=Lb->a;
  mt_object y;
  for(i=0; i<size; i++){
    if(mf_eq(&y,a+i,b+i)){
      mf_traceback("a==b");
      return -1;
    }
    if(y.type!=mv_bool){
      mf_type_error1("in a==b: value of a[i]==b[i] (type: %s) is not a boolean.",&y);
      mf_dec_refcount(&y);
      return -1;
    }
    if(y.value.b==0){
      return 0;
    }
  }
  return 1;
}

void mf_init_type_list(mt_table* type){
  type->name=mf_cstr_to_str("list");
  type->m=mf_empty_map();
  mt_map* m=type->m;
  mf_insert_function(m,1,-1,"push",list_push);
  mf_insert_function(m,0,1,"pop",list_pop);
  mf_insert_function(m,0,1,"clear",list_clear);
  mf_insert_function(m,0,0,"rev",list_reverse);
  mf_insert_function(m,0,0,"rot",list_rot);
  mf_insert_function(m,1,1,"map",list_map);
  mf_insert_function(m,1,1,"filter",list_filter);
  mf_insert_function(m,1,1,"call",list_filter);
  mf_insert_function(m,0,0,"shuffle",list_shuffle);
  mf_insert_function(m,2,2,"swap",list_swap);
  mf_insert_function(m,1,1,"extend",list_extend);
  mf_insert_function(m,0,1,"sort",list_sort);
  mf_insert_function(m,0,1,"join",list_join);
  mf_insert_function(m,0,0,"chain",list_chain);
  mf_insert_function(m,1,1,"chunks",list_chunks);
  mf_insert_function(m,1,1,"DIV",list_chunks);
  mf_insert_function(m,1,1,"match",list_match);
  mf_insert_function(m,1,1,"index",list_index);
  mf_insert_function(m,0,0,"flip",list_flip);
  mf_insert_function(m,2,2,"insert",list_insert);
}
