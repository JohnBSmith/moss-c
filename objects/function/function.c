
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <moss.h>
#include <objects/string.h>
#include <objects/list.h>

extern mt_object mv_exception;
extern mt_basic* mv_empty;
double mf_float(mt_object* x, int* error);
int mf_add_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_mpy_dec(mt_object* x, mt_object* a, mt_object* b);

void mf_tuple_dec_refcount(mt_tuple* t);
void mf_map_dec_refcount(mt_map* m);
void mf_module_dec_refcount(mt_module* m);

mt_tuple* mf_raw_tuple(long size){
  mt_tuple* t = mf_malloc(sizeof(mt_tuple)+size*sizeof(mt_object));
  t->size=size;
  t->refcount=1;
  return t;
}

void mf_raise_empty(){
  mf_dec_refcount(&mv_exception);
  mv_empty->refcount++;
  mv_exception.type=mv_table;
  mv_exception.value.p=mv_empty;
}

int mf_empty(){
  return mv_exception.type==mv_table && mv_exception.value.p==mv_empty;
}

void mf_function_delete(mt_function* f){
  if(f->context){
    mf_tuple_dec_refcount(f->context);
  }
  if(f->m){
    mf_map_dec_refcount(f->m);
  }
  if(f->name){
    mf_str_dec_refcount(f->name);
  }
  mf_dec_refcount(&f->prototype);
  if(f->module){
    mf_module_dec_refcount(f->module);
  }
  mf_free(f);
}

void mf_function_dec_refcount(mt_function* f){
  if(f->refcount==1){
    mf_function_delete(f);
  }else{
    f->refcount--;
  } 
}

mt_function* mf_new_function(unsigned char* address){
  mt_function* f = mf_malloc(sizeof(mt_function));
  f->refcount=1;
  f->address=address;
  f->prototype.type=mv_null;
  f->m=NULL;
  f->context=NULL;
  f->name=NULL;
  f->module=NULL;
  return f;
}

static
mt_range* uint_range(long a, long b){
  mt_range* r = mf_malloc(sizeof(mt_range));
  r->refcount=1;
  r->a.type=mv_int;
  r->a.value.i=a;
  if(b<0){
    r->b.type=mv_null;
  }else{
    r->b.type=mv_int;
    r->b.value.i=b;
  }
  r->step.type=mv_int;
  r->step.value.i=1;
  return r;
}

static
int function_argc(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"f.argc");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in f.argc(): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  if(f->argc<0){
    x->type=mv_range;
    x->value.p=(mt_basic*)uint_range(f->min,f->max);
  }else{
    x->type=mv_int;
    x->value.i=f->argc;
  }
  return 0;
}

static
int function_id(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"id");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in f.id(): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  if(f->name==0){
    char a[200];
    snprintf(a,200,"%p",f);
    mt_string* s = mf_cstr_to_str(a);
    x->type=mv_string;
    x->value.p=(mt_basic*)s;
  }else{
    f->name->refcount++;
    x->type=mv_string;
    x->value.p=(mt_basic*)f->name;
  }
  return 0;
}

static
int range_next(mt_object* x, int argc, mt_object* v){
  mt_object* a = function_self->context->a;
  long step = a[2].value.i;
  long index = a[0].value.i;
  long b;
  int inf;
  if(a[1].type==mv_null){
    inf=1;
  }else{
    inf=0;
    b = a[1].value.i;
  }
  if(!inf){
    if((step>0 && index>b) || (step<0 && index<b)){
      mf_raise_empty();
      return 1;
    }
  }
  x->type=mv_int;
  x->value.i=index;
  a[0].value.i=index+step;
  return 0;
}

static
int list_next(mt_object* x, int argc, mt_object* v){
  mt_object* a = function_self->context->a;
  mt_list* list = (mt_list*)a[0].value.p;
  long i = a[1].value.i;
  if(i>=list->size){
    mf_raise_empty();
    return 1;
  }
  a[1].value.i++;
  mf_copy_inc(x,list->a+i);
  return 0;
}

static
int string_next(mt_object* x, int argc, mt_object* v){
  mt_object* a = function_self->context->a;
  mt_string* s = (mt_string*)a[0].value.p;
  long i=a[1].value.i;
  if(i>=s->size){
    mf_raise_empty();
    return 1;
  }
  a[1].value.i++;
  mt_string* y=mf_str_new_u32(1,s->a+i);
  x->type=mv_string;
  x->value.p=(mt_basic*)y;
  return 0;
}

static
int crange_next(mt_object* x, int argc, mt_object* v){
  mt_object* a=function_self->context->a;
  long i = a[0].value.i;
  long step = a[2].value.i;
  long b;
  int inf;
  if(a[1].type==mv_null){
    inf=1;
  }else{
    inf=0;
    b = a[1].value.i;
  }
  if(!inf){
    if((step>0 && i>b) || (step<0 && i<b)){
      mf_raise_empty();
      return 0;
    }
  }
  a[0].value.i+=step;
  mt_string* c = mf_raw_string(1);
  c->a[0]=i;
  x->type=mv_string;
  x->value.p=(mt_basic*)c;
  return 0;
}

static
int dict_next(mt_object* x, int argc, mt_object* v){
  mt_object* a=function_self->context->a;
  mt_htab* t=&((mt_map*)a[0].value.p)->htab;
  long i=a[1].value.i;
  while(i<t->capacity){
    if(t->a[i].taken){
      mt_list* list = mf_raw_list(2);
      mf_copy_inc(list->a+0,&t->a[i].key);
      mf_copy_inc(list->a+1,&t->a[i].value);
      a[1].value.i=i+1;
      x->type=mv_list;
      x->value.p=(mt_basic*)list;
      return 0;
    }
    i++;
  }
  a[1].value.i=i;
  mf_raise_empty();
  return 1;
}

static
int set_next(mt_object* x, int argc, mt_object* v){
  mt_object* a=function_self->context->a;
  mt_htab* t = &((mt_set*)a[0].value.p)->htab;  
  long i = a[1].value.i;
  while(i<t->capacity){
    if(t->a[i].taken){
      a[1].value.i=i+1;
      mf_copy_inc(x,&t->a[i].key);
      return 0;
    }
    i++;
  }
  a[1].value.i=i;
  mf_raise_empty();
  return 1;
}

static
int bstring_next(mt_object* x, int argc, mt_object* v){
  mt_object* a=function_self->context->a;
  mt_bstring* s = (mt_bstring*)a[0].value.p;
  long i=a[1].value.i;
  if(i>=s->size){
    mf_raise_empty();
    return 1;
  }
  a[1].value.i++;
  x->type=mv_int;
  x->value.i=s->a[i];
  return 0;
}

static
int float_range_next(mt_object* x, int argc, mt_object* v){
  mt_object* a=function_self->context->a;
  long i = a[1].value.i;
  long n = a[2].value.i;
  if(i>=n){
    mf_raise_empty();
    return 1;
  }
  double t = a[0].value.f;
  double step = a[3].value.f;
  a[1].value.i++;
  x->type=mv_float;
  x->value.f=t+i*step;
  return 0;
}

static
mt_function* float_range_iter(mt_range* r){
  double a,b,step;
  int e=0;
  a=mf_float(&r->a,&e);
  b=mf_float(&r->b,&e);
  step=mf_float(&r->step,&e);
  if(e){
    mf_type_error("Type error in iter(a..b:step): a,b,step must be convertible to float.");
    return NULL;
  }
  mt_function* f = mf_new_function(NULL);
  f->argc=0;
  f->context = mf_raw_tuple(4);
  mt_object* ca=f->context->a;
  ca[0].type=mv_float;
  ca[0].value.f=a;
  ca[1].type=mv_int;
  ca[1].value.i=0;
  ca[2].type=mv_int;
  ca[2].value.i=1+round((b-a)/step);
  ca[3].type=mv_float;
  ca[3].value.f=step;
  f->fp=float_range_next;
  return f;
}

mt_function* mf_iter_range(mt_range* r){
  long a,b,step;
  if(r->a.type==mv_int){
    a=r->a.value.i;
  }else if(r->a.type==mv_null){
    a=0;
  }else{
    mf_type_error("Type error in iter(a..b): a is not an integer.");
    return NULL;
  }
  if(r->b.type!=mv_int && r->b.type!=mv_null){
    mf_type_error("Type error in iter(a..b): b is not an integer.");
    return NULL;
  }
  if(r->step.type!=mv_int){
    mf_type_error("Type error in iter(a..b:step): step is not an integer.");
    return NULL;
  }
  mt_function* f = mf_new_function(NULL);
  f->argc=0;
  mt_tuple* context = mf_raw_tuple(3);
  context->a[0].type=mv_int;
  context->a[0].value.i=a;
  mf_copy_inc(context->a+1,&r->b);
  mf_copy_inc(context->a+2,&r->step);
  f->context = context;
  f->fp=range_next;
  return f;
}

mt_function* mf_iter(mt_object* x){
  switch(x->type){
  case mv_int:{
    mt_function* f = mf_new_function(NULL);
    f->argc=0;
    mt_tuple* context = mf_raw_tuple(3);
    mt_object* a=context->a;
    a[0].type=mv_int;
    a[0].value.i=0;
    a[1].type=mv_int;
    a[1].value.i=x->value.i-1;
    a[2].type=mv_int;
    a[2].value.i=1;
    f->context = context;
    f->fp=range_next;
    return f;
  } break;
  case mv_range:
    return mf_iter_range((mt_range*)x->value.p);
    break;
  case mv_list:{
    mt_function* f = mf_new_function(NULL);
    f->argc=0;
    f->context=mf_raw_tuple(2);
    mt_object* a=f->context->a;
    mf_copy_inc(a,x);
    a[1].type=mv_int;
    a[1].value.i=0;
    f->fp=list_next;
  } break;
  case mv_string:{
    mt_function* f = mf_new_function(NULL);
    f->argc=0;
    f->context=mf_raw_tuple(2);
    mt_object* a=f->context->a;
    mf_copy_inc(a,x);
    a[1].type=mv_int;
    a[1].value.i=0;
    f->fp=string_next;
  } break;
  case mv_bstring:{
    mt_function* f = mf_new_function(NULL);
    f->argc=0;
    f->context=mf_raw_tuple(2);
    mt_object* a=f->context->a;
    mf_copy_inc(a,x);
    a[1].type=mv_int;
    a[2].value.i=0;
    f->fp=bstring_next;
  } break;
  case mv_map:{
    mt_function* f = mf_new_function(NULL);
    f->argc=0;
    f->context=mf_raw_tuple(2);
    mt_object* a=f->context->a;
    mf_copy_inc(a,x);
    a[1].type=mv_int;
    a[1].value.i=0;
    f->fp=dict_next;
  } break;
  case mv_set:{
    mt_function* f = mf_new_function(NULL);
    f->argc=0;
    f->context=mf_raw_tuple(2);
    mt_object* a=f->context->a;
    mf_copy_inc(a,x);
    a[1].type=mv_int;
    a[1].value.i=0;
    f->fp=set_next;
  } break;
  case mv_function:{
    mt_function* f=(mt_function*)x->value.p;
    f->refcount++;
    return f;
  } break;
  default:
    mf_type_error("Type error in iter(x): x is not iterable.");
    return NULL;
  }
}

int mf_fiter(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"iter");
    return 1;
  }
  mt_function* f = mf_iter(v+1);
  if(f){
    x->type=mv_function;
    x->value.p=(mt_basic*)f;
    return 0;
  }else{
    return 1;
  }
}

int function_list(mt_object* x, int argc, mt_object* v){
  long max;
  if(argc==1){
    if(v[1].type!=mv_int){
      mf_type_error("Type error in i.list(n): n is not an integer.");
      return 1;
    }
    max=v[1].value.i;
  }else if(argc==0){
    max=-1;
  }else{
    mf_argc_error(argc,0,1,"i.list");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.list(): i is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  mt_list* list = mf_raw_list(0);
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object y;
  long k=0;
  while(1){
    if(max>=0){
      if(k>=max){
        x->type=mv_list;
        x->value.p=(mt_basic*)list;
        return 0;
      }
    }
    if(mf_call(f,&y,0,argv)){
      if(mf_empty()){
        x->type=mv_list;
        x->value.p=(mt_basic*)list;
        return 0;
      }
      mf_traceback("list");
      return 1;
    }
    mf_list_push(list,&y);
    k++;
  }
}

static
int function_all(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.all(p): i is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object y;
  if(argc==0){
    while(1){
      if(mf_call(f,&y,0,argv)){
        if(mf_empty()){
          x->type=mv_bool;
          x->value.b=1;
          return 0;
        }
        mf_traceback("all");
        return 1;
      }
      if(y.type!=mv_bool){
        mf_dec_refcount(&y);
        mf_type_error("Type error in i.all(): return value of i is not a boolean.");
        return 1;
      }
      if(y.value.b==0){
        x->type=mv_bool;
        x->value.b=0;
        return 0;
      }
    }
  }else if(argc!=1){
    mf_argc_error(argc,0,1,"all");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in i.all(p): p is not a function.");
    return 1;
  }
  mt_function* p = (mt_function*)v[1].value.p;
  mt_object c;
  mt_object args[2];
  args[0].type=mv_null;
  while(1){
    if(mf_call(f,&y,0,argv)){
      if(mf_empty()){
        x->type=mv_bool;
        x->value.b=1;
        return 0;
      }
      mf_traceback("all");
      return 1;
    }
    mf_copy(args+1,&y);
    if(mf_call(p,&c,1,args)){
      mf_dec_refcount(&y);
      mf_traceback("all");
      return 1;
    }
    mf_dec_refcount(&y);
    if(c.type!=mv_bool){
      mf_type_error("Type error in i.all(p): return value of p is not a boolean.");
      mf_dec_refcount(&c);
      return 1;
    }
    if(c.value.b==0){
      mf_dec_refcount(&c);
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  }
}

static
int function_any(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.any(p): i is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object y;
  if(argc==0){
    while(1){
      if(mf_call(f,&y,0,argv)){
        if(mf_empty()){
          x->type=mv_bool;
          x->value.b=0;
          return 0;
        }
        mf_traceback("any");
        return 1;
      }
      if(y.type!=mv_bool){
        mf_dec_refcount(&y);
        mf_type_error("Type error in i.any(): return value of i is not a boolean.");
        return 1;
      }
      if(y.value.b==1){
        x->type=mv_bool;
        x->value.b=1;
        return 0;
      }
    }
  }else if(argc!=1){
    mf_argc_error(argc,0,1,"any");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in i.any(p): p is not a function.");
    return 1;
  }
  mt_function* p = (mt_function*)v[1].value.p;
  mt_object c;
  mt_object args[2];
  args[0].type=mv_null;
  while(1){
    if(mf_call(f,&y,0,argv)){
      if(mf_empty()){
        x->type=mv_bool;
        x->value.b=0;
        return 0;
      }
      mf_traceback("any");
      return 1;
    }
    mf_copy(args+1,&y);
    if(mf_call(p,&c,1,args)){
      mf_dec_refcount(&y);
      mf_traceback("any");
      return 1;
    }
    mf_dec_refcount(&y);
    if(c.type!=mv_bool){
      mf_type_error("Type error in i.any(p): return value of p is not a boolean.");
      mf_dec_refcount(&c);
      return 1;
    }
    if(c.value.b==1){
      mf_dec_refcount(&c);
      x->type=mv_bool;
      x->value.b=1;
      return 0;
    }
  }
}

static
long simple_count(mt_function* f){
  mt_object x;
  mt_object argv[1];
  argv[0].type=mv_null;
  long k=0;
  while(1){
    if(mf_call(f,&x,0,argv)){
      if(mf_empty()){
        return k;
      }else{
        return -1;
      }
    }
    mf_dec_refcount(&x);
    k++;
  }
}

static
int function_count(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.count(p): i is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  if(argc==0){
    long count = simple_count(f);
    if(count<0){
      mf_traceback("count");
      return 1;
    }else{
      x->type=mv_int;
      x->value.i=count;
      return 0;
    }
  }
  if(argc!=1){
    mf_argc_error(argc,1,1,"count");
    return 0;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in i.count(p): p is not a function.");
    return 1;
  }
  mt_function* p = (mt_function*)v[1].value.p;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object y,c;
  mt_object args[2];
  args[0].type=mv_null;
  long k=0;
  while(1){
    if(mf_call(f,&y,0,argv)){
      if(mf_empty()){
        x->type=mv_int;
        x->value.i=k;
        return 0;
      }
      mf_traceback("count");
      return 1;
    }
    mf_copy(args+1,&y);
    if(mf_call(p,&c,1,args)){
      mf_dec_refcount(&y);
      mf_traceback("count");
      return 1;
    }
    mf_dec_refcount(&y);
    if(c.type!=mv_bool){
      mf_type_error("Type error in i.count(p): return value of p is not a boolean.");
      mf_dec_refcount(&c);
      return 1;
    }
    if(c.value.b==1){
      k++;
    }
  }
}

static
int until_next(mt_object* x, int argc, mt_object* v){
  mt_object* a = function_self->context->a;
  mt_function* f = (mt_function*)a[0].value.p;
  mt_function* p = (mt_function*)a[1].value.p;
  mt_object y,c;
  mt_object argv[2];
  argv[0].type=mv_null;

  if(mf_call(f,&y,0,argv)){
    mf_traceback("iterator from until");
    return 1;
  }
  mf_copy(argv+1,&y);
  if(mf_call(p,&c,1,argv)){
    mf_dec_refcount(&y);
    mf_traceback("iterator from until");
    return 1;
  }
  if(c.type!=mv_bool){
    mf_dec_refcount(&y);
    mf_dec_refcount(&c);
    mf_type_error("Type error in i.until(p): return value of p is not a boolean.");
    return 1;
  }
  if(c.value.b==0){
    mf_copy(x,&y);
    return 0;
  }else{
    mf_dec_refcount(&y);
    mf_raise_empty();
    return 1;
  }
}

static
int function_until(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"until");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.until(f): i is not a function.");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in i.until(f): f is not a function.");
    return 1;
  }
  mt_function* f = mf_new_function(NULL);
  f->context=mf_raw_tuple(2);
  mt_object* a=f->context->a;
  mf_copy_inc(a,v);
  mf_copy_inc(a+1,v+1);
  f->argc=0;
  f->fp=until_next;
  x->type=mv_function;
  x->value.p=(mt_basic*)f;
  return 0;
}

static
int function_each(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"each");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.all(f): i is not a function.");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in i.all(f): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  mt_function* p = (mt_function*)v[1].value.p;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object y,y2;
  mt_object args[2];
  args[0].type=mv_null;
  while(1){
    if(mf_call(f,&y,0,argv)){
      if(mf_empty()){
        x->type=mv_null;
        return 0;
      }
      mf_traceback("each");
      return 1;
    }
    mf_copy(args+1,&y);
    if(mf_call(p,&y2,1,args)){
      mf_dec_refcount(&y);
      mf_traceback("each");
      return 1;
    }
    mf_dec_refcount(&y);
    mf_dec_refcount(&y2);
  }
}

static
int function_sum(mt_object* x, int argc, mt_object* v){
  mt_function *g,*f;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object t,y,s;
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.sum(f): i is not a function.");
    return 1;
  }
  g = (mt_function*)v[0].value.p;
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in i.sum(f): f is not a function.");
      return 1;
    }
    f = (mt_function*)v[1].value.p;
  }else if(argc==0){
    s.type=mv_int;
    s.value.i=0;
    while(1){
      if(mf_call(g,&t,0,argv)){
        if(mf_empty){
          mf_copy(x,&s);
          return 0;
        }
        mf_traceback("sum");
        return 1;
      }
      if(mf_add_dec(&s,&s,&t)){
        mf_traceback("sum");
        return 1;
      }
    }
  }else{
    mf_argc_error(argc,0,1,"sum");
    return 1;
  }

  mt_object args[2];
  args[0].type=mv_null;
  s.type=mv_int;
  s.value.i=0;
  while(1){
    if(mf_call(g,&t,0,argv)){
      if(mf_empty()){
        mf_copy(x,&s);
        return 0;
      }
      mf_traceback("sum");
      return 1;
    }
    mf_copy(args+1,&t);
    if(mf_call(f,&y,1,args)){
      mf_dec_refcount(&t);
      mf_traceback("sum");
      return 1;
    }
    mf_dec_refcount(&t);
    if(mf_add_dec(&s,&s,&y)){
      mf_traceback("sum");
      return 1;
    }
  }
}

static
int function_prod(mt_object* x, int argc, mt_object* v){
  mt_function *g,*f;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object t,y,p;
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.prod(f): i is not a function.");
    return 1;
  }
  g = (mt_function*)v[0].value.p;
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in i.prod(f): f is not a function.");
      return 1;
    }
    f = (mt_function*)v[1].value.p;
  }else if(argc==0){
    p.type=mv_int;
    p.value.i=1;
    while(1){
      if(mf_call(g,&t,0,argv)){
        if(mf_empty){
          mf_copy(x,&p);
          return 0;
        }
        mf_traceback("prod");
        return 1;
      }
      if(mf_mpy_dec(&p,&p,&t)){
        mf_traceback("prod");
        return 1;
      }
    }
  }else{
    mf_argc_error(argc,0,1,"prod");
    return 1;
  }

  mt_object args[2];
  args[0].type=mv_null;
  p.type=mv_int;
  p.value.i=1;
  while(1){
    if(mf_call(g,&t,0,argv)){
      if(mf_empty()){
        mf_copy(x,&p);
        return 0;
      }
      mf_traceback("prod");
      return 1;
    }
    mf_copy(args+1,&t);
    if(mf_call(f,&y,1,args)){
      mf_dec_refcount(&t);
      mf_traceback("prod");
      return 1;
    }
    mf_dec_refcount(&t);
    if(mf_mpy_dec(&p,&p,&y)){
      mf_traceback("prod");
      return 1;
    }
  }
}

static
int accu_next(mt_object* x, int argc, mt_object* v){
  mt_object* a = function_self->context->a;
  mt_function* g = (mt_function*)a[0].value.p;
  mt_function* f = (mt_function*)a[1].value.p;
  mt_object argv[1];
  mt_object args[3];
  argv[0].type=mv_null;
  args[0].type=mv_null;
  mt_object t,y;
  if(a[2].type==mv_null){
    if(a[3].type!=mv_null){
      mf_copy(a+2,a+3);
      a[3].type=mv_null;
      mf_copy_inc(x,a+2);
      return 0;
    }
    if(mf_call(g,&t,0,argv)){
      if(mf_empty()){
        return 1;
      }
      goto error;
    }
    mf_dec_refcount(a+2);
    mf_copy(a+2,&t);
    mf_copy_inc(x,&t);
    return 0;
  }else{
    if(mf_call(g,&t,0,argv)){
      mf_dec_refcount(a+2);
      a[2].type=mv_null;
      if(mf_empty()){
        return 0;
      }
      goto error;
    }
    mf_copy(args+1,a+2);
    mf_copy(args+2,&t);
    if(mf_call(f,&y,2,args)){
      goto error;
    }
    mf_dec_refcount(a+2);
    mf_copy(a+2,&y);
    mf_copy_inc(x,&y);
    return 0;
  }
  error:
  mf_traceback("iterator from accu");
  return 0;
}

static
int function_accu(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_function){
    mf_type_error("Type error in i.accu: i is not a function.");
    return 1;
  }
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in i.accu(f): f is not a function.");
      return 1;
    }
  }else if(argc==2){
    if(v[2].type!=mv_function){
      mf_type_error("Type error in i.accu(e,f): f is not a function.");
      return 1;
    }
  }else{
    mf_argc_error(argc,1,2,"accu");
    return 0;
  }
  mt_function* f = mf_new_function(NULL);
  f->context=mf_raw_tuple(4);
  mt_object* a=f->context->a;
  mf_copy_inc(a,v);
  mf_copy_inc(a+1,v+argc);
  if(argc==1){
    a[2].type=mv_null;
    a[3].type=mv_null;
  }else{
    a[2].type=mv_null;
    mf_copy_inc(a+3,v+1);
  }
  f->argc=0;
  f->fp=accu_next;
  x->type=mv_function;
  x->value.p=(mt_basic*)f;
  return 0;
}

static
int function_join(mt_object* x, int argc, mt_object* v){
  mt_object argv[2];
  if(argc==1){
    mf_copy(argv+1,v+1);
  }else if(argc!=0){
    mf_argc_error(argc,0,1,"join");
    return 1;
  }
  puts("Error in function_join: todo");
  abort();
  mt_list* a;
  // a = mf_function_list(0,v);
  if(a==0){
    mf_traceback("join");
    return 1;
  }
  /*
  argv[0]=a;
  Object* s = mf_list_join(argc,argv);
  if(s==0){
    mf_traceback("join");
  }
  */
  
  return 0;
}

static
int fn_pow_lambda(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"function returned by operator '^'");
    return 1;
  }
  mt_object* a=function_self->context->a;
  mt_function* f = (mt_function*)a[0].value.p;
  long n = a[1].value.i;
  int i;
  mt_object argv[2];
  argv[0].type=mv_null;
  mf_copy(argv+1,v+1);
  mt_object y;
  mf_copy_inc(&y,argv+1);
  for(i=0; i<n; i++){
    if(mf_call(f,&y,1,argv)){
      mf_dec_refcount(argv+1);
      mf_traceback("function returned by operator '^'");
      return 1;
    }
    mf_dec_refcount(argv+1);
    mf_copy(argv+1,&y);
  }
  mf_copy(x,&y);
  return 0;
}

static
int function_pow(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"pow");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in f^n: f is not a function.");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error("Type error in f^n: n is not an integer.");
    return 1;
  }
  long n=v[1].value.i;
  if(n<0){
    mf_value_error("Value error in f^n: negative values of n are not allowed.");
    return 0; 
  }
  mt_function* f = mf_new_function(NULL);
  f->argc=1;
  f->context = mf_raw_tuple(2);
  mt_object* a=f->context->a;
  mf_copy_inc(a,v);
  a[1].type=mv_int;
  a[1].value.i=n;
  f->fp=fn_pow_lambda;
  x->type=mv_function;
  x->value.p=(mt_basic*)f;
  return 0;
}

static
int compositum(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"function returned by compose");
    return 1;
  }
  mt_object* a = function_self->context->a;
  mt_list* list = (mt_list*)a[0].value.p;
  long size=list->size;
  long i;
  mt_object argv[2];
  argv[0].type=mv_null;
  mt_object y;
  mf_copy_inc(&y,v+1);
  int e;
  mt_function* f;
  for(i=size-1; i>=0; i--){
    mf_copy(argv+1,&y);
    f=(mt_function*)list->a[i].value.p;
    e = mf_call(f,&y,1,argv);
    mf_dec_refcount(argv+1);
    if(e){
      mf_traceback("function returned by compose");
      return 1;
    }
  }
  mf_copy(x,&y);
  return 0;
}

static
int function_compose(mt_object* x, int argc, mt_object* v){
  mt_list* list;
  long i;
  if(argc!=1){
    list = mf_raw_list(argc);
    for(i=0; i<argc; i++){
      mf_copy_inc(list->a+i,v+i+1);
    }
  }else if(v[1].type==mv_list){
    list=(mt_list*)v[1].value.p;
    list->refcount++;
  }else{
    list = mf_raw_list(1);
    mf_copy_inc(list->a,v+1);
  }
  for(i=0; i<list->size; i++){
    if(list->a[i].type!=mv_function){
      mf_type_error("Type error in compose: a[i] is not a function.");
      mf_list_dec_refcount(list);
      return 1;
    }
  }
  mt_function* f = mf_new_function(NULL);
  f->context=mf_raw_tuple(1);
  mt_object* a=f->context->a;
  a[0].type=mv_list;
  a[0].value.p=(mt_basic*)list;
  f->argc=1;
  f->fp=compositum;
  x->type=mv_function;
  x->value.p=(mt_basic*)f;
  return 0;
}

static
int orbit_next(mt_object* x, int argc, mt_object* v){
  mt_object* a = function_self->context->a;
  mt_function* f = (mt_function*)a[0].value.p;
  mt_object argv[2];
  argv[0].type=mv_null;
  mf_copy(argv+1,a+1);
  mt_object y;
  if(mf_call(f,&y,1,argv)){
    mf_traceback("iterator from orbit");
    return 1;
  }
  mf_copy(a+1,&y);
  mf_copy(x,argv+1);
  return 0;
}

static
int function_orbit(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"orbit");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in f.orbit(x): f is not a function.");
    return 1;
  }
  mt_function* i = mf_new_function(NULL);
  i->context=mf_raw_tuple(2);
  i->argc=0;
  mt_object* a=i->context->a;
  mf_copy_inc(a,v);
  mf_copy_inc(a+1,v+1);
  i->fp=orbit_next;
  x->type=mv_function;
  x->value.p=(mt_basic*)i;
  return 0;
}

static
int function_call(mt_object* x, int argc, mt_object* v){
  if(argc<1){
    mf_argc_error(argc,1,-1,"call");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in f.call(self,...): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  int i;
  if(f->argc==-1){
    mt_list* list = mf_raw_list(argc-1);
    for(i=2; i<=argc; i++){
      mf_copy_inc(list->a+i-2,v+i);
    }
    mt_object argv[2];
    argv[0].type=mv_null;
    argv[1].type=mv_list;
    argv[1].value.p=(mt_basic*)list;
    if(mf_call(f,x,2,argv)){
      mf_traceback("call");
      return 1;
    }
    mf_list_dec_refcount(list);
    return 0;
  }else{
    mt_object* args = mf_malloc(argc*sizeof(mt_object));
    for(i=0; i<argc; i++){
      mf_copy(args+i,v+i+1);
    }
    if(mf_call(f,x,argc-1,args)){
      mf_traceback("call");
      mf_free(args);
      return 1;
    }
    free(args);
    return 0;
  }
}

static
int function_apply(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"apply");
    return 1;
  }
  if(v[0].type!=mv_function){
    mf_type_error("Type error in f.apply(self,a): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[0].value.p;
  if(v[2].type!=mv_list){
    mf_type_error("Type error in f.apply(self,a): a is not a list.");
    return 1;
  }
  mt_object y;
  if(f->argc==-1){
    abort();
  }else{
    mt_list* list = (mt_list*)v[2].value.p;
    mt_object* args = mf_malloc((list->size+1)*sizeof(mt_object));
    mf_copy(args,v+1);
    long i;
    for(i=0; i<list->size; i++){
      mf_copy(args+i+1,list->a+i);
    }
    if(mf_call(f,x,list->size,args)){
      mf_traceback("apply");
      mf_free(args);
      return 1;
    }
    free(args);
    return 0;
  }
}

void mf_init_type_function(mt_table* type){
  type->name = mf_cstr_to_str("function");
  type->m= mf_empty_map();
  mt_map* m = type->m;

  mf_insert_function(m,1,-1,"call",function_call);
  mf_insert_function(m,2,2,"apply",function_apply);
  mf_insert_function(m,0,0,"argc",function_argc);
  mf_insert_function(m,0,0,"id",function_id);
  mf_insert_function(m,0,1,"list",function_list);
  mf_insert_function(m,0,1,"all",function_all);
  mf_insert_function(m,0,1,"any",function_any);
  mf_insert_function(m,0,1,"count",function_count);
  mf_insert_function(m,1,1,"until",function_until);
  mf_insert_function(m,1,1,"each",function_each);
  mf_insert_function(m,0,1,"sum",function_sum);
  mf_insert_function(m,0,1,"prod",function_prod);
  mf_insert_function(m,1,2,"acuu",function_accu);
  mf_insert_function(m,0,1,"join",function_join);
  mf_insert_function(m,1,1,"pow",function_pow);
  mf_insert_function(m,0,-1,"compose",function_compose);
  mf_insert_function(m,1,1,"orbit",function_orbit);
}


