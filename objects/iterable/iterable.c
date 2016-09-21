
#include <moss.h>
#include <objects/list.h>
#include <objects/function.h>
#include <objects/map.h>

int mf_list_sum0(mt_object* x, mt_list* a);
int mf_list_sum1(mt_object* x, mt_list* a, mt_function* f);
int mf_list_prod0(mt_object* x, mt_list* a);
int mf_list_prod1(mt_object* x, mt_list* a, mt_function* f);
int mf_list_count(mt_object* x, mt_list* a, mt_function* f);
int mf_count_element(mt_object* x, mt_list* list, mt_object* t);

int mf_empty();
void mf_raise_empty();
mt_function* mf_iter(mt_object* x);
mt_tuple* mf_raw_tuple(long size);
mt_function* mf_new_function(unsigned char* address);

int iterable_sum(mt_object* x, int argc, mt_object* v){
  int e;
  if(argc==0){
    if(v[0].type==mv_list){
      return mf_list_sum0(x,(mt_list*)v[0].value.p);
    }else{
      mt_list* a = mf_list(v);
      if(a==NULL){
        mf_traceback("sum");
        return 1;
      }
      e=mf_list_sum0(x,a);
      mf_list_dec_refcount(a);
      return e;
    }
  }else if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in sum(a,f): f is not a function.");
      return 1;
    }
    mt_function* f=(mt_function*)v[1].value.p;
    if(v[1].type==mv_list){
      return mf_list_sum1(x,(mt_list*)v[0].value.p,f);
    }else{
      mt_list* a = mf_list(v);
      if(a==NULL){
        mf_traceback("sum");
        return 1;
      }
      e=mf_list_sum1(x,a,f);
      mf_list_dec_refcount(a);
      return e;
    }
  }else{
    mf_argc_error(argc,0,1,"sum");
    return 1;
  }
}

int iterable_prod(mt_object* x, int argc, mt_object* v){
  int e;
  if(argc==0){
    if(v[0].type==mv_list){
      return mf_list_prod0(x,(mt_list*)v[0].value.p);
    }else{
      mt_list* a = mf_list(v);
      if(a==NULL){
        mf_traceback("prod");
        return 1;
      }
      e=mf_list_prod0(x,a);
      mf_list_dec_refcount(a);
      return e;
    }
  }else if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in prod(a,f): f is not a function.");
      return 1;
    }
    mt_function* f=(mt_function*)v[1].value.p;
    if(v[1].type==mv_list){
      return mf_list_prod1(x,(mt_list*)v[0].value.p,f);
    }else{
      mt_list* a = mf_list(v);
      if(a==NULL){
        mf_traceback("prod");
        return 1;
      }
      e=mf_list_prod1(x,a,f);
      mf_list_dec_refcount(a);
      return e;
    }
  }else{
    mf_argc_error(argc,1,2,"prod");
    return 1;
  }
}

int iterable_count(mt_object* x, int argc, mt_object* v){
  int e;
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in count(a,p): p is not a fuction.");
      return 1;
    }
    mt_function* f=(mt_function*)v[1].value.p;
    if(v[0].type==mv_list){
      return mf_list_count(x,(mt_list*)v[0].value.p,f);
    }else{
      mt_list* a = mf_list(v);
      if(a==NULL){
        mf_traceback("count");
        return 1;
      }
      e=mf_list_count(x,a,f);
      mf_list_dec_refcount(a);
      return e;
    }
  }else if(argc==0){
    mt_list* list = mf_list(v);
    if(list==NULL){
      mf_traceback("count");
      return 1;
    }
    long size = list->size;
    mf_list_dec_refcount(list);
    x->type=mv_int;
    x->value.i=size;
    return 0;
  }else{
    mf_argc_error(argc,0,1,"count");
    return 1;
  }
}

static
int list_all(mt_object* x, mt_list* list, mt_function* f){
  long size=list->size;
  mt_object* a=list->a;
  long i;
  if(f==NULL){
    for(i=0; i<size; i++){
      if(a[i].type!=mv_bool){
        mf_type_error("Type error in a.all(): a must contain only booleans.");
        return 1;
      }
      if(!a[i].value.b){
        x->type=mv_bool;
        x->value.b=0;
        return 0;
      }
    }
    x->type=mv_bool;
    x->value.b=1;
    return 0;
  }
  mt_object argv[2];
  argv[0].type=mv_null;
  mt_object y;
  for(i=0; i<size; i++){
    mf_copy(argv+1,a+i);
    if(mf_call(f,&y,1,argv)){
      mf_traceback("all");
      return 1;
    }
    if(y.type!=mv_bool){
      mf_dec_refcount(&y);
      mf_type_error("Type error in a.all(f): return value of f is not of type bool.");
      return 1;
    }
    if(!y.value.b){
      x->type=mv_bool;
      x->value.b=0;
      return 0;
    }
  }
  x->type=mv_bool;
  x->value.b=1;
  return 0;
}

static
int list_any(mt_object* x, mt_list* list, mt_function* f){
  long size=list->size;
  mt_object* a=list->a;
  long i;
  if(f==NULL){
    for(i=0; i<size; i++){
      if(a[i].type!=mv_bool){
        mf_type_error("Type error in a.any(): a must contain only booleans.");
        return 1;
      }
      if(a[i].value.b){
        x->type=mv_bool;
        x->value.b=0;
        return 1;
      }
    }
    x->type=mv_bool;
    x->value.b=0;
    return 0;
  }
  mt_object argv[2];
  argv[0].type=mv_null;
  mt_object y;
  for(i=0; i<size; i++){
    mf_copy(argv+1,a+i);
    if(mf_call(f,&y,1,argv)){
      mf_traceback("any");
      return 1;
    }
    if(y.type!=mv_bool){
      mf_dec_refcount(&y);
      mf_type_error("Type error in a.any(f): return value of f is not of type bool.");
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

int iterable_all(mt_object* x, int argc, mt_object* v){
  mt_list* list;
  mt_function* p;
  int e;
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in a.all(p): p is not a function.");
      return 1;
    }
    p = (mt_function*)v[1].value.p;
    list = mf_list(v);
    if(list==NULL){
      mf_traceback("all");
      return 1;
    }
    e=list_all(x,list,p);
    mf_list_dec_refcount(list);
    return e;
  }else if(argc==0){
    list = mf_list(v);
    if(list==NULL){
      mf_traceback("all");
      return 1;
    }
    e=list_all(x,list,NULL);
    mf_list_dec_refcount(list);
    return e;
  }else{
    mf_argc_error(argc,0,1,"all");
    return 1;
  }
}

int iterable_any(mt_object* x, int argc, mt_object* v){
  mt_list* list;
  mt_function* p;
  int e;
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in a.any(p): p is not a function.");
      return 1;
    }
    p = (mt_function*)v[1].value.p;
    list = mf_list(v);
    if(list==NULL){
      mf_traceback("any");
      return 1;
    }
    e=list_any(x,list,p);
    mf_list_dec_refcount(list);
    return e;
  }else if(argc==0){
    list = mf_list(v);
    if(list==NULL){
      mf_traceback("any");
      return 1;
    }
    e=list_any(x,list,NULL);
    mf_list_dec_refcount(list);
    return e;
  }else{
    mf_argc_error(argc,0,1,"any");
    return 1;
  }
}

static
int list_each(mt_object* x, mt_list* a, mt_function* f){
  mt_object y;
  long i;
  mt_object argv[2];
  argv[0].type=mv_null;
  int e;
  for(i=0; i<a->size; i++){
    mf_copy(argv+1,a->a+i);
    e=mf_call(f,&y,1,argv);
    if(e){
      mf_traceback("each");
      return 1;
    }
    mf_dec_refcount(&y);
  }
  x->type=mv_null;
  return 0;
}

int iterable_each(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"each");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in a.each(f): f is not a function.");
    return 1;
  }
  mt_function* f;
  f = (mt_function*)v[1].value.p;
  mt_list* list = mf_list(v);
  if(list==NULL){
    mf_traceback("each");
    return 1;
  }
  int e;
  e = list_each(x,list,f);
  mf_list_dec_refcount(list);
  return e;
}

static
int iterable_reduce(mt_object* x, int argc, mt_object* v){
  mt_function *g,*f;
  mt_object last;
  mt_object argv[1];
  argv[0].type=mv_null;
  g = mf_iter(v);
  if(g==NULL){
    mf_traceback("reduce");
    return 1;
  }
  if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in a.reduce(f): f is not a function.");
      goto error;
    }
    f = (mt_function*)v[1].value.p;
    if(mf_call(g,&last,0,argv)){
      if(mf_empty()){
        x->type=mv_null;
        mf_function_dec_refcount(g);
        return 0;
      }
      mf_traceback("reduce");
      goto error;
    }
  }else if(argc==2){
    if(v[2].type!=mv_function){
      mf_type_error("Type error in a.reduce(e,f): f is not a function.");
      goto error;
    }
    f = (mt_function*)v[2].value.p;
    mf_copy_inc(&last,v+1);
  }else{
    mf_argc_error(argc,1,2,"reduce");
    goto error;
  }

  mt_object t;
  mt_object args[3];
  args[0].type=mv_null;
  int e;
  while(1){
    if(mf_call(g,&t,0,argv)){
      if(mf_empty()){
        mf_copy(x,&last);
        mf_function_dec_refcount(g);
        return 0;
      }
      mf_traceback("reduce");
      goto error;
    }
    mf_copy(args+1,&last);
    mf_copy(args+2,&t);
    e = mf_call(f,&last,2,args);
    mf_dec_refcount(args+1);
    mf_dec_refcount(args+2);
    if(e){
      mf_traceback("reduce");
      goto error;
    }
  }
  error:
  mf_function_dec_refcount(g);
  return 1;
}

static
int map_next(mt_object* x, int argc, mt_object* v){
  mt_object* a=function_self->context->a;
  mt_function* g=(mt_function*)a[0].value.p;
  mt_function* f=(mt_function*)a[1].value.p;
  mt_object y;
  mt_object argv[2];
  argv[0].type=mv_null;
  if(mf_call(g,&y,0,argv)){
    return 1;
  }
  mf_copy(argv+1,&y);
  int e=mf_call(f,x,1,argv);
  mf_dec_refcount(&y);
  return e;
}

static
int iterable_map(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"map");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in i.map(f): f is not a function.");
    return 1;
  }
  mt_function* i = mf_iter(v);
  if(i==NULL){
    mf_traceback("map");
    return 1;
  }
  mt_function* f=mf_new_function(NULL);
  f->context=mf_raw_tuple(2);
  mt_object* a=f->context->a;
  a[0].type=mv_function;
  a[0].value.p=(mt_basic*)i;
  mf_copy_inc(a+1,v+1);
  f->argc=0;
  f->fp=map_next;
  x->type=mv_function;
  x->value.p=(mt_basic*)f;
  return 0;
}

static
int filter_next(mt_object* x, int argc, mt_object* v){
  mt_object* a = function_self->context->a;
  mt_function* f = (mt_function*)a[0].value.p;
  mt_function* p = (mt_function*)a[1].value.p;
  mt_object y,c;
  mt_object argv[2];
  argv[0].type=mv_null;
  while(1){
    if(mf_call(f,&y,0,argv)){
      mf_traceback("iterator from filter");
      return 1;
    }
    mf_copy(argv+1,&y);
    if(mf_call(p,&c,1,argv)){
      mf_dec_refcount(&y);
      mf_traceback("iterator from filter");
      return 0;
    }
    if(c.type!=mv_bool){
      mf_dec_refcount(&c);
      mf_type_error("Type error in i.filter(p): return value of p is not a boolean.");
      return 1;
    }
    if(c.value.b==1){
      mf_copy(x,&y);
      return 0;
    }else{
      mf_dec_refcount(&y);   
    }
  }
}

static
int iterable_filter(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"filter");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in i.filter(f): f is not a function.");
    return 1;
  }
  mt_function* i = mf_iter(v);
  if(i==NULL){
    mf_traceback("map");
    return 1;
  }
  mt_function* f=mf_new_function(NULL);
  f->context=mf_raw_tuple(2);
  mt_object* a=f->context->a;
  a[0].type=mv_function;
  a[0].value.p=(mt_basic*)i;
  mf_copy_inc(a+1,v+1);
  f->argc=0;
  f->fp=filter_next;
  x->type=mv_function;
  x->value.p=(mt_basic*)f;
  return 0;
}

static
int iterable_dict0(mt_object* x, mt_object* a){
  mt_function* i = mf_iter(a);
  if(i==NULL){
    mf_traceback("dict");
    return 1;
  }
  mt_object y;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_map* m = mf_empty_map();
  long k=0;
  while(1){
    if(mf_call(i,&y,0,argv)){
      if(mf_empty()) break;
      mf_traceback("dict");
      goto error;
    }
    if(y.type!=mv_list){
      mf_dec_refcount(&y);
      mf_type_error("Type error in a.dict(): an element of a is not a list.");
      goto error;
    }
    mt_list* t = (mt_list*)y.value.p;
    if(t->size!=2){
      mf_list_dec_refcount(t);
      mf_value_error("Value error in a.dict(): an element of a is not a pair.");
      goto error;
    }
    if(mf_map_set(m,&t->a[0],&t->a[1])){
      mf_list_dec_refcount(t);
      mf_traceback("dict");
      goto error;
    }
    mf_list_dec_refcount(t);
    k++;
  }
  x->type=mv_map;
  x->value.p=(mt_basic*)m;
  return 0;
  error:
  mf_map_dec_refcount(m);
  mf_function_dec_refcount(i);
  return 1;
}

static
int iterable_dict1(mt_object* x, mt_object* a, mt_function* f){
  mt_function* i = mf_iter(a);
  if(i==NULL){
    mf_traceback("dict");
    return 1;
  }
  mt_object y,u;
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object args[2];
  args[0].type=mv_null;
  mt_map* m = mf_empty_map();
  long k=0;
  while(1){
    if(mf_call(i,&args[1],0,argv)){
      if(mf_empty()) break;
      mf_traceback("dict");
      goto error;
    }
    if(mf_call(f,&y,1,args)){
      mf_dec_refcount(&args[1]);
      mf_traceback("dict");
      goto error;
    }
    mf_dec_refcount(&args[1]);
    if(y.type!=mv_list){
      mf_dec_refcount(&y);
      mf_type_error("Type error in a.dict(f): return value of f is not a list.");
      goto error;
    }
    mt_list* t = (mt_list*)y.value.p;
    if(t->size!=2){
      mf_list_dec_refcount(t);
      mf_value_error("Value error in a.dict(f): return value of f is not a pair.");
      goto error;
    }
    if(mf_map_set(m,&t->a[0],&t->a[1])){
      mf_list_dec_refcount(t);
      mf_traceback("dict");
      goto error;
    }
    mf_list_dec_refcount(t);
    k++;
  }
  x->type=mv_map;
  x->value.p=(mt_basic*)m;
  return 0;
  error:
  mf_map_dec_refcount(m);
  mf_function_dec_refcount(i);
  return 1;
}

static
int iterable_dict(mt_object* x, int argc, mt_object* v){
  if(argc==0){
    return iterable_dict0(x,v);
  }else if(argc==1){
    if(v[1].type!=mv_function){
      mf_type_error("Type error in a.dict(f): f is not a function.");
      return 1;
    }
    mt_function* f = (mt_function*)v[1].value.p;
    return iterable_dict1(x,v,f);
  }else{
    mf_argc_error(argc,0,1,"dict");
    return 1;
  }
}

int mf_fdict(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"dict");
    return 1;
  }
  return iterable_dict0(x,v+1);
}

void mf_init_type_iterable(mt_table* type){
  type->name=mf_cstr_to_str("iterable");
  type->m=mf_empty_map();
  mt_map* m=type->m;
  mf_insert_function(m,0,1,"sum",iterable_sum);
  mf_insert_function(m,0,1,"prod",iterable_prod);
  mf_insert_function(m,0,1,"count",iterable_count);
  mf_insert_function(m,0,1,"all",iterable_all);
  mf_insert_function(m,0,1,"any",iterable_any);
  mf_insert_function(m,0,1,"each",iterable_each);
  mf_insert_function(m,1,2,"reduce",iterable_reduce);
  mf_insert_function(m,1,1,"map",iterable_map);
  mf_insert_function(m,1,1,"filter",iterable_filter);
  mf_insert_function(m,1,1,"call",iterable_filter);
  mf_insert_function(m,1,1,"dict",iterable_dict);
}
