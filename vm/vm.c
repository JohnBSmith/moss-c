
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
// #define NDEBUG
#include <assert.h>
#include <moss.h>
#include <vm.h>
#include <objects/list.h>
#include <objects/string.h>
#include <objects/map.h>
#include <objects/long.h>
#include <objects/set.h>
#include <modules/global.h>

// TODO: module nt
// TODO: sparate frame->rescue from ip
// TODO: variadic index operator
// TODO: f->data refcount?
// TODO: type bstr, convert from and to list

#define STACK_SIZE 800000
#define EVAL_STACK_SIZE 200000

typedef struct{
  long ip; // instruction pointer
  long sp; // stack pointer
  mt_object* bp; // base pointer
  mt_object* vp; // variables pointer
  mt_function* fp; // self function pointer
  long sp_try; // try-catch stack pointer
  int rescue; // rescue from raise
  int ret; // choose between return and jump
  int argcp1; // argument count plus one
  int varcount; // number of local variables
  mt_object* px;
  unsigned char* data;
} mt_stack_frame;

int mode_unsafe;
int mode_network;
int gv_argc;
char** gv_argv;

static int stack_pointer;
static mt_object* base_pointer;
static mt_object* var_pointer;
static mt_function* global_function;
mt_function* function_self;
static mt_object* vm_stack;
static mt_list* string_pool;
mt_map* mv_mvtab;
mt_map* mv_gvtab;
static mt_stack_frame* frame;
static mt_stack_frame* eval_stack;
static int eval_sp;
mt_object mv_exception;
mt_basic* mv_empty;
mt_object iter_value;
mt_list* mv_traceback_list;
static unsigned char* exception_address;
static mt_module* exception_module;

int mf_list_print(mt_list* a);
void mf_print_string(long size, uint32_t* a);
void mf_print_string_lit(long size, uint32_t* a);
int mf_put_repr(mt_object* x);
void mf_init_type_function(mt_table* t);
void mf_init_type_iterable(mt_table* t);
mt_function* mf_new_function(unsigned char* address);
int mf_fiter(mt_object* x, int argc, mt_object* v);
int mf_frand(mt_object* x, int argc, mt_object* v);
int mf_finput(mt_object* x, int argc, mt_object* v);
int mf_fabs(mt_object* x, int argc, mt_object* v);
int mf_fsgn(mt_object* x, int argc, mt_object* v);
int mf_empty();

mt_table* mv_type_bool;
mt_table* mv_type_int;
mt_table* mv_type_float;
mt_table* mv_type_complex;
mt_table* mv_type_string;
mt_table* mv_type_bstring;
mt_table* mv_type_list;
mt_table* mv_type_function;
mt_table* mv_type_long;
mt_table* mv_type_dict;
mt_table* mv_type_iterable;
mt_table* mv_type_range;
mt_table* mv_type_set;

static
void print_float(double x){
  char buffer[100];
  snprintf(buffer,100,"%.16g",x);
  printf("%s",buffer);
  if(isfinite(x) && strchr(buffer,'.')==NULL && strchr(buffer,'e')==NULL){
    printf(".0");
  }
}

static
void print_complex(double re, double im){
  if(re==0){
    printf("%.16gi",im);
  }else{
    if(im<0){
      printf("%.16g-%.16gi",re,-im);
    }else{
      printf("%.16g+%.16gi",re,im);
    }
  }
}

int mf_put(mt_object* x){
  mt_string* s;
  mt_range* r;
  switch(x->type){
  case mv_null:
    printf("null");
    break;
  case mv_bool:
    printf(x->value.b? "true": "false");
    break;
  case mv_int:
    printf("%li",x->value.i);
    break;
  case mv_float:
    print_float(x->value.f);
    break;
  case mv_complex:
    print_complex(x->value.c.re,x->value.c.im);
    break;
  case mv_long:
    mf_long_print((mt_long*)x->value.p);
    break;
  case mv_list:
    if(mf_list_print((mt_list*)x->value.p)){
      mf_traceback("put");
      return 1;
    }
    break;
  case mv_map:
    if(mf_map_print((mt_map*)x->value.p)){
      mf_traceback("put");
      return 1;
    }
    break;
  case mv_string:
    s = (mt_string*)x->value.p;
    mf_print_string(s->size,s->a);
    break;
  case mv_function:
    printf("function");
    break;
  case mv_range:
    r = (mt_range*)x->value.p;
    if(r->a.type==mv_null || r->b.type==mv_null){
      printf("(");
    }
    if(r->a.type!=mv_null){
      if(mf_put_repr(&r->a)) goto error;
    }
    printf("..");
    if(r->b.type!=mv_null){
      if(mf_put_repr(&r->b)) goto error;
    }
    if(r->step.type!=mv_int || r->step.value.i!=1){
      printf(":");
      if(mf_put_repr(&r->step)) goto error;
    }
    if(r->a.type==mv_null || r->b.type==mv_null){
      printf(")");
    }
    break;
  case mv_set:{
    s = mf_set_to_string((mt_set*)x->value.p);
    mf_print_string(s->size,s->a);
    mf_str_dec_refcount(s);
  } break;
  default:
    s = mf_str(x);
    if(s){
      mf_print_string(s->size,s->a);
      mf_str_dec_refcount(s);
    }else{
      return 1;
    }
  }
  return 0;
  error:
  printf("\n");
  return 1;
}

int mf_put_repr(mt_object* x){
  if(x->type==mv_string){
    mt_string* s = (mt_string*)x->value.p;
    printf("\"");
    mf_print_string_lit(s->size,s->a);
    printf("\"");
    return 0;
  }else{
    return mf_put(x);
  }
}

int mf_print(mt_object* x){
  int e=mf_put(x);
  printf("\n");
  return e;
}

int mf_fput(mt_object* x, int argc, mt_object* v){
  int i;
  for(i=1; i<=argc; i++){
    if(mf_put(v+i)){
      printf("\n");
      return 1;
    }
  }
  x->type=mv_null;
  return 0;
}

int mf_fprint(mt_object* x, int argc, mt_object* v){
  int i;
  for(i=1; i<=argc; i++){
    if(mf_put(v+i)){
      printf("\n");
      return 1;
    }
  }
  printf("\n");
  x->type=mv_null;
  return 0;
}

int mf_object_get(mt_object* x, mt_object* a, mt_object* key){
  int e;
  mt_table* t;
  mt_function * f;
  mt_map* m;
  switch(a->type){
  case mv_null:
    return 1;
  case mv_bool:
    return mf_map_get(x,mv_type_bool->m,key);
  case mv_int:
    m = mv_type_int->m;
    goto iterable_get;
  case mv_float:
    return mf_map_get(x,mv_type_float->m,key);
  case mv_string:
    m = mv_type_string->m;
    goto iterable_get;
  case mv_list:
    m = mv_type_list->m;
    goto iterable_get;
  case mv_map:
    m = mv_type_dict->m;
    goto iterable_get;
  case mv_range:
    m = mv_type_range->m;
    goto iterable_get;
  case mv_set:
    m = mv_type_set->m;
    goto iterable_get;
  case mv_function:
    f = (mt_function*)a->value.p;
    if(f->m){
      e = mf_map_get(x,f->m,key);
    }else{
      m = mv_type_function->m;
      goto iterable_get;
    }
    if(e){
      if(e==1){
        m = mv_type_function->m;
        goto iterable_get;
      }else{
        mf_traceback("operator '.'");
        return 1;
      }
    }
    return 0;
  case mv_table:
    t = (mt_table*)a->value.p;
    if(t->m){
      e = mf_map_get(x,t->m,key);
    }else{
      e=1;
    }
    if(e){
      if(e==1){
        return mf_object_get(x,&t->prototype,key);
      }else{
        mf_traceback("operator '.'");
        return 1;
      }
    }
    return 0;
  case mv_native:{
    mt_native* n = (mt_native*)a->value.p;
    return mf_object_get(x,&n->prototype,key);
  }
  default:
    puts("Error in mf_object_get.");
    abort();
  }
  iterable_get:
  e = mf_map_get(x,m,key);
  if(e){
    if(e==1){
      return mf_map_get(x,mv_type_iterable->m,key);
    }else{
      mf_traceback("'.'");
      return 1;
    }
  }
  return 0;
}

static
mt_object* mf_get_ref(mt_object* a, mt_object* key){
  int type=a->type;
  if(type==mv_list){
    if(key->type!=mv_int){
      mf_type_error("Type error in a[i]=value: a is a list but i is not an integer.");
      return NULL;
    }
    long index = key->value.i;
    mt_list* list = (mt_list*)a->value.p;
    if(index<0){
      index+=list->size;
      if(index<0){
        mf_index_error("Index error in a[i]=value: i is out of lower bound.");
        return NULL;
      }
    }else if(index>=list->size){
      mf_index_error("Index error in a[i]=value: i is out of upper bound.");
      return NULL;
    }
    if(list->frozen){
      mf_value_error("Value error in a[i]=value: a is frozen.");
      return NULL;
    }
    if(list->refcount==1){
      mf_std_exception("Error in a[i]=value: a will be deleted after assignment.");
      return NULL;
    }else{
      list->refcount--;
    }
    mf_dec_refcount(list->a+index);
    return list->a+index;
  }else if(type==mv_map){
    mt_map* m=(mt_map*)a->value.p;
    if(m->frozen){
      mf_value_error("Value error in d[i]=value: d is frozen.");
      return NULL;
    }
    return mf_map_set_key(m,key);
  }else{
    mf_type_error("Type error in a[i]=value: a is not a list.");
    return NULL;
  }
}

static
mt_object* mf_object_get_ref(mt_object* x, mt_object* id){
  if(id->type!=mv_string){
    mf_type_error("Type error in assignment to m[s]: s is not a string.");
    return NULL;
  }
  if(x->type!=mv_table && x->type!=mv_function){
    mf_type_error("Type error: only objects of type 'object' and "
      "'function' can have a slot table.");
    return NULL;
  }
  mt_table* t = (mt_table*)x->value.p;
  if(t->refcount==1){
    mf_std_exception("Error in a.id=...: a will be deleted after assignment.");
    return NULL;
  }else{
    t->refcount--;
  }
  mt_object* p;
  if(t->m){
    if(t->m->frozen){
      mf_std_exception("Error in x.id=value: x is frozen.");
      return NULL;
    }
    p=mf_map_set_key(t->m,id);
  }else{
    t->m = mf_empty_map();
    p=mf_map_set_key(t->m,id);
  }
  mf_dec_refcount(id);
  return p;
}

static
void mf_insert_table(mt_map* m, const char* id, mt_table* t){
  mt_string* key = mf_str_new(strlen(id),id);
  mt_object x;
  x.type=mv_table;
  x.value.p=(mt_basic*)t;
  mf_map_set_str(m,key,&x);
}

void mf_vm_init_gvtab(){
  mt_map* m=mv_mvtab;
  mt_function* f;

  f=mf_insert_function(m,1,1,"int",mf_fint);
  f->m=mf_empty_map();
  mv_type_int->refcount++;
  mf_insert_table(f->m,"type",mv_type_int);
  
  f=mf_insert_function(m,1,1,"float",mf_ffloat);
  f->m=mf_empty_map();
  mv_type_float->refcount++;
  mf_insert_table(f->m,"type",mv_type_float);

  f=mf_insert_function(m,1,1,"long",mf_flong);
  f->m=mf_empty_map();
  mv_type_long->refcount++;
  mf_insert_table(f->m,"type",mv_type_long);

  f=mf_insert_function(m,1,2,"list",mf_flist);
  f->m=mf_empty_map();
  mv_type_list->refcount++;
  mf_insert_table(f->m,"type",mv_type_list);

  f=mf_insert_function(m,1,1,"str",mf_fstr);
  f->m=mf_empty_map();
  mv_type_string->refcount++;
  mf_insert_table(f->m,"type",mv_type_string);
  
  mf_insert_function(m,1,1,"set",mf_fset);

  mt_table* function = mf_table(NULL);
  function->m=mf_empty_map();
  mf_insert_table(function->m,"type",mv_type_function);
  mf_insert_table(m,"function",function);

  mf_insert_function(m,0,-1,"put",mf_fput);
  mf_insert_function(m,0,-1,"print",mf_fprint);
  mf_insert_function(m,2,-1,"map",mf_fmap);
  mf_insert_function(m,2,-1,"zip",mf_fzip);
  mf_insert_function(m,1,1,"unzip",mf_funzip);
  mf_insert_function(m,1,1,"iter",mf_fiter);
  mf_insert_function(m,1,1,"copy",mf_fcopy);
  mf_insert_function(m,0,3,"rand",mf_frand);
  mf_insert_function(m,1,1,"size",mf_fsize);
  mf_insert_function(m,1,1,"record",mf_frecord);
  mf_insert_function(m,0,2,"object",mf_fobject);
  mf_insert_function(m,1,1,"type",mf_ftype);
  mf_insert_function(m,1,1,"load",mf_fload);
  mf_insert_function(m,1,2,"__import__",mf_fimport);
  mf_insert_function(m,1,1,"eval",mf_feval);
  mf_insert_function(m,0,1,"input",mf_finput);
  mf_insert_function(m,0,1,"gtab",mf_fgtab);
  mf_insert_function(m,2,2,"max",mf_fmax);
  mf_insert_function(m,2,2,"min",mf_fmin);
  mf_insert_function(m,1,1,"abs",mf_fabs);
  mf_insert_function(m,1,1,"sgn",mf_fsgn);
  mf_insert_function(m,1,1,"chr",mf_fchr);
  mf_insert_function(m,1,1,"ord",mf_ford);
  mf_insert_function(m,1,1,"const",mf_fconst);
  mf_insert_function(m,2,-1,"extend",mf_fextend);
  mf_insert_function(m,2,3,"pow",mf_fpow);
}

void mf_vm_init(){
  vm_stack = mf_malloc(STACK_SIZE*sizeof(mt_object));
  eval_stack = mf_malloc(EVAL_STACK_SIZE*sizeof(mt_stack_frame));
  frame = eval_stack;
  mv_mvtab = mf_empty_map();
  mv_gvtab = mf_empty_map();
  global_function = mf_new_function(NULL);
  iter_value.type=mv_null;
  mv_empty=(mt_basic*) mf_table(NULL);

  mv_type_iterable = mf_table(NULL);
  mf_init_type_iterable(mv_type_iterable);
  mt_object iterable;
  iterable.type=mv_table;
  iterable.value.p=(mt_basic*)mv_type_iterable;

  mv_type_bool = mf_table(NULL);
  mv_type_bool->name = mf_cstr_to_str("bool");
  mv_type_bool->m = mf_empty_map();

  mv_type_int = mf_table(NULL);
  mv_type_int->name = mf_cstr_to_str("int");
  mv_type_int->m = mf_empty_map();

  mv_type_float = mf_table(NULL);
  mv_type_float->name = mf_cstr_to_str("float");
  mv_type_float->m = mf_empty_map();

  mv_type_complex = mf_table(NULL);
  mv_type_complex->name = mf_cstr_to_str("complex");
  mv_type_complex->m = mf_empty_map();

  mv_type_long = mf_table(NULL);
  mv_type_long->name = mf_cstr_to_str("long");
  mv_type_long->m = mf_empty_map();

  mv_type_string = mf_table(&iterable);
  mf_init_type_string(mv_type_string);

  mv_type_list = mf_table(&iterable);
  mf_init_type_list(mv_type_list);

  mv_type_function = mf_table(&iterable);
  mf_init_type_function(mv_type_function);

  mv_type_dict = mf_table(&iterable);
  mf_init_type_dict(mv_type_dict);

  mv_type_range = mf_table(&iterable);
  mv_type_range->name = mf_cstr_to_str("range");
  mv_type_range->m = mf_empty_map();

  mv_type_set = mf_table(&iterable);
  mv_type_set->name = mf_cstr_to_str("set");
  mv_type_set->m = mf_empty_map();
}

void mf_argc_error_sub(int argc, mt_function* f){
  char a[100];
  mt_bstr s;
  if(f->name){
    mf_encode_utf8(&s,f->name->a,f->name->size);
    mf_argc_error(argc,f->argc,f->argc,s.a);
    mf_free(s.a);
  }else{
    snprintf(a,100,"function %p",f);
    mf_argc_error(argc,f->argc,f->argc,a);
  }
}

static
void traceback_lambda(mt_function* f, unsigned char* instruction, mt_function* context){
  char a[200];
  mt_bstr s;
  int line, col;
  if(instruction){
    line = *(uint16_t*)(instruction+1);
    col = *(uint8_t*)(instruction+3);
  }
  char id[100];
  if(context->module && context->module->id){
    snprintf(id,100," (%s)",context->module->id);
  }else{
    snprintf(id,100,"%s","");
  }
  if(mv_traceback_list){
    if(instruction){
      if(f->name){
        mf_encode_utf8(&s,f->name->a,f->name->size);
        snprintf(a,200,"%s (%i:%i)%s",s.a,line,col,id);
        mf_free(s.a);
        mf_traceback(a);
      }else{
        snprintf(a,200,"function %p (%i:%i)%s ",f,line,col,id);
        mf_traceback(a);
      }
    }else{
      if(f->name){
        mf_encode_utf8(&s,f->name->a,f->name->size);
        snprintf(a,200,"%s",s.a);
        mf_free(s.a);
        mf_traceback(a);
      }else{
        snprintf(a,200,"function %p",f);
        mf_traceback(a);
      }
    }
  }
}

struct{
  int n;
  const char* s;
} byte_code_tab[] = {
  {ADD, "ADD"}, {SUB, "SUB"}, {MPY, "MPY"}, {DIV, "DIV"},
  {MOD, "MOD"}, {NEG, "NEG"}, {NOT, "NOT"},
  {EQ, "EQ"}, {NE, "NE"}, {LT, "LT"}, {GT, "GT"}, {LE, "LE"}, {GE, "GE"},
  {POW, "POW"}, {CONST_INT, "CONST_INT"},
  {CONST_FLOAT, "CONST_FLOAT"}, {CONST_NULL, "CONST_NULL"},
  {CONST_BOOL, "CONST_BOOL"}, {CONST_STR, "CONST_STR"},
  {HALT, "HALT"}, {DATA, "DATA"},
  {LOAD, "LOAD"}, {LOAD_REF, "LOAD_REF"}, {STORE, "STORE"}, {STN, "STN"},
  {LOAD_LOCAL, "LOAD_LOCAL"}, {LOAD_REF_LOCAL, "LOAD_REF_LOCAL"},
  {LOAD_ARG, "LOAD_ARG"}, {LOAD_ARG_REF, "LOAD_ARG_REF"},
  {PRINT, "PRINT"}, {PUT, "PUT"}, {JMP, "JMP"}, {JPZ, "JPZ"},
  {LIST, "LIST"}, {CALL, "CALL"},
  {GET, "GET"}, {GETREF, "GETREF"}, {RSTORE, "RSTORE"},
  {STORE_LOCAL, "STORE_LOCAL"}, {CONST_FN, "CONST_FN"},
  {RET, "RET"}, {POP, "POP"}, {OBGET, "OBGET"}, {OBGETREF, "OBGETREF"},
  {LOAD_CONTEXT, "LOAD_CONTEXT"}, {IS, "IS"}, {IN, "IN"}, {NOTIN, "NOTIN"},
  {TUPLE, "TUPLE"}, {RANGE, "RANGE"}, {DUP, "DUP"}, {SWAP, "SWAP"},
  {FNSELF, "FNSELF"}, {LIST_POP, "LIST_POP"}, {MAP, "MAP"},
  {TABLE, "TABLE"}, {ITER_NEXT, "ITER_NEXT"}, {ITER_VALUE, "ITER_VALUE"},
  {BIT_AND, "BIT_AND"}, {BIT_OR, "BIT_OR"}, {BIT_XOR, "BIT_XOR"}
};

static
void print_byte_code(long ip, unsigned char* a){
  int n=*a;
  int line=*(uint16_t*)(a+1);
  int col=*(uint8_t*)(a+3);
  int i;
  printf("%02li [%02i:%02i] ",ip,line,col);
  for(i=0; i<sizeof(byte_code_tab)/sizeof(byte_code_tab[0]); i++){
    if(byte_code_tab[i].n==n){
      printf("%s\n",byte_code_tab[i].s);
      return;
    }
  }
  printf("unknown byte code\n");
}

static
void argc_error_str(int argc, int min, int max, mt_function* f){
  mt_bstr s;
  char a[100];
  if(f->name){
    mf_encode_utf8(&s,f->name->a,f->name->size);
    mf_argc_error(argc,min,max,(char*)s.a);
    mf_free(s.a);
  }else{
    snprintf(a,100,"function %p",f);
    mf_argc_error(argc,min,max,a);
  }
}

int mf_call(mt_function* f, mt_object* x, int argc, mt_object* v){
  int e;
  if(f->address==NULL){
    frame->fp=function_self;
    frame++;
    function_self=f;
    e = f->fp(x,argc,v);
    frame--;
    function_self=frame->fp;
    return e;
  }
  if(f->argc!=argc){
    if(f->argc<0){
      int n = -f->argc-1;
      if(n>argc){
        argc_error_str(argc,n,-1,f);
        return 1;
      }
      mt_list* list = mf_raw_list(argc-n);
      int i;
      for(i=0; i<argc-n; i++){
        mf_copy_inc(list->a+i,v+n+i+1);
      }
      v[n+1].type=mv_list;
      v[n+1].value.p=(mt_basic*)list;
    }else{
      argc_error_str(argc,f->argc,f->argc,f);
      return 1;
    }
  }
  frame->fp=function_self;
  function_self=f;
  base_pointer=v;
  int varcount = f->varcount;
  mt_object vars[100];
  int i;
  for(i=0; i<varcount; i++){
    vars[i].type=mv_null;
  }
  var_pointer=vars;
  eval_sp++;
  frame++;
  e = mf_vm_eval(f->address,0);
  eval_sp--;
  frame--;
  function_self=frame->fp;
  for(i=0; i<varcount; i++){
    mf_dec_refcount(vars+i);
  }
  assert(stack_pointer>0);
  stack_pointer--;
  if(e){
    traceback_lambda(f,NULL,frame->fp);
    return 1;
  }else{
    mf_copy(x,vm_stack+stack_pointer);
    return 0;
  }
}

static
int list_get(mt_object* x, mt_list* a, long i){
  if(i>=a->size){
    mf_value_error("Value error in unpacking assignment: out of upper bound.");
    return 1;
  }
  mf_copy_inc(x,a->a+i);
  return 0;
}

static
int iter_next(mt_function* f){
  mt_object argv[1];
  argv[0].type=mv_null;
  mt_object x;
  if(mf_call(f,&x,0,argv)){
    if(mf_empty()){
      return 0;
    }else{
      return -1;
    }
  }
  mf_dec_refcount(&iter_value);
  mf_copy(&iter_value,&x);
  return 1;
}

static
void undefined_variable(mt_object* x){
  assert(x->type==mv_string);
  mt_string* id = (mt_string*)x->value.p;
  mt_bstr s;
  mf_encode_utf8(&s,id->a,id->size);
  char a[200];
  snprintf(a,200,"Error: undefined variable: '%s'.",s.a);
  mf_free(s.a);
  mf_std_exception(a);
}

int mf_vm_eval(unsigned char* a, long ip){
  mt_object* stack = vm_stack;
  long int sp;
  int e,i;
  long v1;
  int argc;
  mt_object x1;
  mt_object *p1,*p2;
  mt_object* px;
  int argcp1;
  unsigned char* address;
  mt_object* vars;
  mt_object* bp;
  mt_object* vp;
  int retv;

  frame->rescue=0;
  frame->ret=1;
  sp=stack_pointer;
  bp=base_pointer;
  vp=var_pointer;

  loop:
  while(1){
    // print_byte_code(ip,a+ip);
    switch(a[ip]){
    case CONST_NULL:
      stack[sp].type=mv_null;
      sp++;
      ip+=BC;
      break;
    case CONST_BOOL:
      stack[sp].type=mv_bool;
      stack[sp].value.b = *(int32_t*)(a+ip+BC);
      sp++;
      ip+=BCp4;
      break;
    case CONST_INT:
      stack[sp].type=mv_int;
      stack[sp].value.i = *(int32_t*)(a+ip+BC);
      sp++;
      ip+=BCp4;
      break;
    case CONST_FLOAT:
      stack[sp].type=mv_float;
      stack[sp].value.f = *(double*)(a+ip+BC);
      sp++;
      ip+=BCp8;
      break;
    case CONST_IMAG:
      stack[sp].type=mv_complex;
      stack[sp].value.c.re=0;
      stack[sp].value.c.im=*(double*)(a+ip+BC);
      sp++;
      ip+=BCp8;
      break;
    case CONST_STR:
      v1 = *(int32_t*)(a+ip+BC);
      mf_copy_inc(stack+sp,function_self->data+v1);
      // mf_copy_inc(stack+sp,string_pool->a+v1);
      sp++;
      ip+=BCp4;
      break;
    case CONST_FN:
      assert(sp>1);
      p1=stack+sp-2; p2=stack+sp-1;
      sp--;
      mt_function* f = mf_new_function(a+ip+*(int32_t*)(a+ip+BC));
      f->argc = *(int32_t*)(a+ip+BCp4);
      f->varcount = *(int32_t*)(a+ip+BCp8);
      // f->data = string_pool->a;
      // f->gtab = gvtab;
      f->data = function_self->data;
      f->gtab = function_self->gtab;
      f->module = function_self->module;
      f->module->refcount++;
      if(p1->type==mv_null){
        f->context=0;
      }else if(p1->type!=mv_tuple){
        puts("Error in CONST_FN: expected mv_tuple.");
        abort();
      }else{
        f->context=(mt_tuple*)p1->value.p;
      }
      if(p2->type==mv_null){
        // pass
      }else if(p2->type!=mv_string){
        puts("Error in CONST_FN: expected mv_string.");
        abort();
      }else{
        f->name=(mt_string*)p2->value.p;
      }
      p1->type=mv_function;
      p1->value.p=(mt_basic*)f;
      ip+=BCp12;
      break;
    case LONG:
      assert(sp>0);
      assert(stack[sp-1].type==mv_string);
      static mt_long* xL;
      xL = mf_string_to_long((mt_string*)stack[sp-1].value.p);
      mf_dec_refcount(stack+sp-1);
      stack[sp-1].type=mv_long;
      stack[sp-1].value.p=(mt_basic*)xL;
      ip+=BC;
      break;
    case FNSELF:
      function_self->refcount++;
      stack[sp].type=mv_function;
      stack[sp].value.p=(mt_basic*)function_self;
      sp++;
      ip+=BC;
      break;
    case LOAD:
      v1 = *(int32_t*)(a+ip+BC);
      // vm_print_gvtab();
      e = mf_map_get(stack+sp,function_self->gtab,function_self->data+v1);
      // e = mf_map_get(stack+sp,gvtab,string_pool->a+v1);
      if(e){
        e = mf_map_get(stack+sp,mv_mvtab,function_self->data+v1);
        if(e){
          undefined_variable(function_self->data+v1);
          goto error;
        }
      }
      mf_inc_refcount(stack+sp);
      // if(stack[sp]==0) goto error;
      // stack[sp]->refcount++;
      sp++;
      ip+=BCp4;
      break;
    case LOAD_REF:
      v1=*(int32_t*)(a+ip+BC);
      px = mf_map_set_key(function_self->gtab,function_self->data+v1);
      // px = mf_map_set_key(gvtab,string_pool->a+v1);
      ip+=BCp4;
      break;
    case LOAD_ARG:
      v1 = *(int32_t*)(a+ip+BC);
      mf_copy_inc(stack+sp,bp+v1);
      sp++;
      ip+=BCp4;
      break;
    case LOAD_ARG_REF:
      v1=*(int32_t*)(a+ip+BC);
      px=bp+v1;
      ip+=BCp4;
      break;
    case LOAD_LOCAL:
      v1 = *(int32_t*)(a+ip+BC);
      mf_copy_inc(stack+sp,vp+v1);
      sp++;
      ip+=BCp4;
      break;
    case LOAD_REF_LOCAL:
      v1=*(int32_t*)(a+ip+BC);
      px=vp+v1;
      ip+=BCp4;
      break;
    case LOAD_CONTEXT:{
      v1=*(int32_t*)(a+ip+BC);
      assert(function_self!=NULL);
      mt_tuple* t = function_self->context;
      assert(t!=NULL);
      assert(v1>=0 && v1<t->size);
      mf_copy_inc(stack+sp,t->a+v1);
      sp++;
      ip+=BCp4;
    } break;
    case LOAD_REF_CONTEXT:{
      v1=*(int32_t*)(a+ip+BC);
      assert(function_self!=NULL);
      mt_tuple* t = function_self->context;
      assert(t!=NULL);
      assert(v1>=0 && v1<t->size);
      px=t->a+v1;
      ip+=BCp4;
    } break;
    case STORE:
      assert(sp>0);
      assert(px!=NULL);
      mf_dec_refcount(px);
      mf_copy(px,stack+sp-1);
      sp--;
      ip+=BC;
      break;
    case LOAD_POINTER:
      assert(px!=NULL);
      mf_copy_inc(stack+sp,px);
      sp++;
      ip+=BC;
      break;
    case ADD:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_add_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case MPY:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_mpy_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case SUB:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_sub_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case DIV:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_div_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case IDIV:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_idiv_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case MOD:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_mod_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case NEG:
      assert(sp>0);
      stack_pointer=sp;
      if(mf_neg_dec(stack+sp-1,stack+sp-1)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case TILDE:
      assert(sp>0);
      stack_pointer=sp;
      if(mf_tilde_dec(stack+sp-1,stack+sp-1)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case NOT:
      assert(sp>0);
      stack_pointer=sp;
      if(mf_not_dec(stack+sp-1,stack+sp-1)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case POW:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_pow_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case LT:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_lt_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case GT:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_gt_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case LE:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_le_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case GE:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_ge_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case EQ:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_eq_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case NE:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_ne_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case IS:
      assert(sp>1);
      sp--;
      if(mf_is_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case IN:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_in_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case NOTIN:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_notin_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case ISIN:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_isin_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case BIT_AND:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_bit_and_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case BIT_OR:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_bit_or_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case BIT_XOR:
      assert(sp>1);
      stack_pointer=sp;
      sp--;
      if(mf_bit_xor_dec(stack+sp-1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      ip+=BC;
      break;
    case JMP:
      ip+=*(int32_t*)(a+ip+BC);
      break;
    case JPZ:
      if(stack[sp-1].type!=mv_bool){
        mf_type_error("Type error in condition: expected boolean value.");
        goto error;
      }else{
        if(stack[sp-1].value.b){
          ip+=BCp4;
        }else{
          ip+=*(int32_t*)(a+ip+BC);
        }
      }
      sp--;
      break;
    case AND:
      if(stack[sp-1].type!=mv_bool){
        mf_type_error("Type error: operator 'and' expected boolean value.");
        goto error;
      }
      if(stack[sp-1].value.b){
        ip+=BCp4;
        sp--;
      }else{
        ip+=*(int32_t*)(a+ip+BC);
      }
      break;
    case OR:
      if(stack[sp-1].type!=mv_bool){
        mf_type_error("Type error: operator 'or' expected boolean value.");
        goto error;
      }
      if(stack[sp-1].value.b){
        ip+=*(int32_t*)(a+ip+BC);
      }else{
        ip+=BCp4;
        sp--;
      }
      break;
    case CALL:
      argc = *(int32_t*)(a+ip+BC);
      argcp1 = argc+1;
      assert(sp>argc);
      p1=stack+sp-1-argcp1;
      // [..., function, self, arg1, arg2, ..., argn, ...]
      // stack[sp]....................................^
      if(p1->type!=mv_function){
        mf_type_error("Type error: expected function.");
        goto error;
      }else{
        f = (mt_function*)p1->value.p;
      }
      address = f->address;
      if(address==NULL){
        mt_object y;
        stack_pointer=sp;

        frame->fp=function_self;
        function_self=f;
        frame++;
        e = f->fp(&y,argc,stack+sp-argcp1);
        frame--;
        function_self=frame->fp;

        if(e) goto error;
        mf_dec_refcounts(argcp1+1,stack+sp-argcp1-1);
        sp=sp-argcp1;
        mf_copy(stack+sp-1,&y);
        ip+=BCp4;
        break;
      }
      if(f->argc<0){
        int n = -f->argc-1;
        if(n>argc){
          puts("Error: function takes more arguments.");
          abort();
          goto error;
        }
        mt_list* list = mf_raw_list(argc-n);
        sp=sp-argc+n;
        for(i=0; i<argc-n; i++){
          list->a[i]=stack[sp+i];
          // CRITICAL
          // list->a[i]->refcount++;
        }
        argcp1=2+n;
        stack[sp].type=mv_list;
        stack[sp].value.p=(mt_basic*)list;
        sp++;
      }else if(f->argc!=argc){
        mf_argc_error_sub(argc,f);
        goto error;
      }
      int varcount = f->varcount;
      base_pointer=stack+sp-argcp1;
      vars=stack+sp;
      var_pointer=vars;
      sp+=varcount;
      stack_pointer=sp;
      for(i=0; i<varcount; i++){
        vars[i].type=mv_null;
      }

      frame->fp=function_self;
      function_self=f;
      frame->ip=ip;
      frame->sp=sp;
      frame->bp=bp;
      frame->vp=vp;
      frame->px=px;
      frame->argcp1=argcp1;
      frame->varcount=varcount;
      frame->data=a;
      eval_sp++;
      frame++;
      if(eval_sp>=EVAL_STACK_SIZE){
        puts("Virtual machine error: stack overflow.");
        abort();
      }

      frame->ret=0;
      a=address;
      ip=0;

      sp=stack_pointer;
      bp=base_pointer;
      vp=var_pointer;
      goto loop;

      jmp_return:
      eval_sp--;
      frame--;
      ip=frame->ip;
      sp=frame->sp;
      bp=frame->bp;
      vp=frame->vp;
      px=frame->px;
      argcp1=frame->argcp1;
      varcount=frame->varcount;
      a=frame->data;

      sp=stack_pointer;
      if(retv>0){
        traceback_lambda(function_self,a+ip,frame->fp);
        function_self=frame->fp;
        goto error;
      }
      function_self=frame->fp;
      p1 = stack+sp-1;
      // Decrement reference counters of
      // function object, self, args, local variables.
      // argcp1 == argc_without_self+1.
      // So the number of objects to decrement is
      // 1+1+argc_without_self+varcount.
      // We must not use argc+1 instead of argcp1, because argc
      // was not saved on the stack frame.
      v1=argcp1+1+varcount;
      mf_dec_refcounts(v1,stack+sp-1-v1);

      sp-=v1;
      mf_copy(stack+sp-1,p1);
      ip+=BCp4;
      break;
    case GET:
      assert(sp>1);
      sp--;
      if(mf_get(&x1,stack+sp-1,stack+sp)){
        sp--; goto error;
      }
      mf_copy_inc(stack+sp-1,&x1);
      ip+=BC;
      break;
    case OBGET:
      assert(sp>1);
      e = mf_object_get(&x1,stack+sp-2,stack+sp-1);
      if(e){
        if(e==1){
          p1 = stack+sp-1;
          if(p1->type==mv_string){
            mt_string* s = (mt_string*)p1->value.p;
            mt_bstr buffer;
            mf_encode_utf8(&buffer,s->a,s->size);
            char a[200];
            snprintf(a,200,"Error: cannot find member '%s' in prototype chain.",buffer.a);
            mf_free(buffer.a);
            mf_std_exception(a);
          }
        }
        goto error;
      }
      mf_dec_refcount(stack+sp-2);
      mf_dec_refcount(stack+sp-1);
      mf_copy_inc(stack+sp-2,&x1);
      sp--;
      ip+=BC;
      break;
    case GETREF:
      assert(sp>1);
      px = mf_get_ref(stack+sp-2,stack+sp-1);
      if(px==NULL) goto error;
      sp-=2;
      ip+=BC;
      break;
    case OBGETREF:
      assert(sp>1);
      px = mf_object_get_ref(stack+sp-2,stack+sp-1);
      if(px==NULL) goto error;
      sp-=2;
      ip+=BC;
      break;
    case LIST_POP:
      assert(sp>0);
      v1=*(int32_t*)(a+ip+BC);
      if(stack[sp-1].type!=mv_list){
        mf_type_error("Type error in unpacking assignment: right hand side is not a list.");
        goto error;
      }
      if(list_get(stack+sp,(mt_list*)stack[sp-1].value.p,v1)){
        goto error;
      }
      sp++;
      ip+=BCp4;
      break;
    case LIST:{
      argc = *(int32_t*)(a+ip+BC);
      assert(sp>=argc);
      mt_list* list = mf_list_noinc(argc,stack+sp-argc);
      sp-=argc;
      stack[sp].type=mv_list;
      stack[sp].value.p=(mt_basic*)list;
      sp++;
      ip+=BCp4;
      } break;
    case TUPLE:{
      argc = *(int32_t*)(a+ip+BC);
      assert(sp>=argc);
      mt_tuple* t = mf_tuple_noinc(argc,stack+sp-argc);
      sp-=argc;
      stack[sp].type=mv_tuple;
      stack[sp].value.p=(mt_basic*)t;
      sp++;
      ip+=BCp4;
    } break;
    case MAP:{
      argc = *(int32_t*)(a+ip+BC);
      assert(sp>=argc);
      mt_map* m = mf_map(argc,stack+sp-argc);
      mf_dec_refcounts(argc,stack+sp-argc);
      sp-=argc;
      stack[sp].type=mv_map;
      stack[sp].value.p=(mt_basic*)m;
      sp++;
      ip+=BCp4;
    } break;
    case TABLE:{
      assert(sp>1);
      if(mf_table_literal(stack+sp-2,stack+sp-2,stack+sp-1)){
        goto error;
      }
      sp--;
      ip+=BC;
    } break;
    case RANGE:
      assert(sp>2);
      stack[sp-3].value.p =
        (mt_basic*)mf_range(stack+sp-3,stack+sp-2,stack+sp-1);
      stack[sp-3].type=mv_range;
      sp-=2;
      ip+=BC;
      break;
    case POP:
      assert(sp>0);
      mf_dec_refcount(stack+sp-1);
      sp--;
      ip+=BC;
      break;
    case DUP:
      assert(sp>0);
      mf_copy_inc(stack+sp,stack+sp-1);
      sp++;
      ip+=BC;
      break;
    case SWAP:
      assert(sp>1);
      mf_copy(&x1,stack+sp-1);
      mf_copy(stack+sp-1,stack+sp-2);
      mf_copy(stack+sp-2,&x1);
      ip+=BC;
      break;
    case ITER_NEXT:
      assert(sp>0);
      if(stack[sp-1].type!=mv_function){
        mf_type_error("Type error in for loop: expected function.");
        goto error;
      }
      v1 = iter_next((mt_function*)stack[sp-1].value.p);
      if(v1<0) goto error;
      mf_dec_refcount(stack+sp-1);
      stack[sp-1].type=mv_bool;
      stack[sp-1].value.b=v1;
      ip+=BC;
      break;
    case ITER_VALUE:
      mf_copy_inc(stack+sp,&iter_value);
      sp++;
      ip+=BC;
      break;
    case RET:
      stack_pointer=sp;
      retv=0;
      if(frame->ret){
        return 0;
      }else{
        goto jmp_return;
      }
    case RAISE:
      assert(sp>0);
      mf_dec_refcount(&mv_exception);
      mf_copy(&mv_exception,stack+sp-1);
      sp--;
      goto error;
      break;
    case EXCEPTION:
      mf_copy_inc(stack+sp,&mv_exception);
      sp++;
      ip+=BC;
      break;
    case TRY:
      frame->rescue=ip+*(int32_t*)(a+ip+BC);
      frame->sp_try=sp;
      ip+=BCp4;
      break;
    case TRYEND:
      frame->rescue=0;
      ip+=BC;
      break;
    case HALT:
      stack_pointer=sp;
      return 0;
    #ifndef NDEBUG
    default:
      printf("Error in vm_eval: unknown byte code, value=%i.\n",a[ip]);
      abort();
    #endif
    }
  }
  error:
  stack_pointer=sp;
  if(!exception_address){
    exception_address=a+ip;
    exception_module=function_self->module;
  }
  if(frame->rescue){
    exception_address=NULL;
    for(i=frame->sp_try; i<sp; i++){
      mf_dec_refcount(stack+i);
    }
    sp=frame->sp_try;
    ip=frame->rescue;
    goto loop;
  }
  if(frame->ret){
    return 1;
  }else{
    retv=1;
    goto jmp_return;
  }
}

mt_list* mf_load_data(unsigned char* data){
  long n = *(uint32_t*)(data+4);
  int i;
  data+=8;
  uint32_t size;
  mt_list* a = mf_raw_list(0);
  mt_string* s;
  mt_object x;
  x.type=mv_string;
  for(i=0; i<n; i++){
    if(*data==MT_STRING){
      data++;
      size=*(uint32_t*)data;
      data+=4;
      s = mf_str_new_u32(size,(uint32_t*)data);
      x.value.p=(mt_basic*)s;
      mf_list_push(a,&x);
      data+=size*4;
    }else{
      printf("Error in mf_load_data.");
      exit(1);
    }
  }
  return a;
}

void mf_vm_set_program(mt_module* module){
  stack_pointer=0;
  base_pointer=vm_stack;
  global_function->gtab=mv_gvtab;
  function_self=global_function;
  module->refcount++;
  function_self->module=module;
  mv_exception.type=mv_null;
  if(module->data){
    string_pool=mf_load_data(module->data);
    global_function->data=string_pool->a;
  }
}

static
void print_line_col(unsigned char* a, mt_module* m){
  long line = *(uint16_t*)(a+1);
  long col = *(uint8_t*)(a+3);
  if(m && m->id){
    printf("Line %li, col %li (%s)\n",line,col,m->id);
  }else{
    printf("Line %li, col %li\n",line,col);
  }
}

int mf_vm_eval_global(mt_module* module, long ip){
  exception_address=NULL;
  int e = mf_vm_eval(module->program,ip);
  if(e){
    error:
    mf_dec_refcounts(stack_pointer,vm_stack);
    print_error:
    if(mv_traceback_list && mv_traceback_list->size>0){
      long i;
      for(i=mv_traceback_list->size-1; i>=0; i--){
        printf("  in ");
        mf_print(mv_traceback_list->a+i);
      }
    }
    if(exception_address){
      print_line_col(exception_address,exception_module);
    }
    if(mf_print(&mv_exception)){
      mf_traceback("command-line exception print");
      goto print_error;
    }
    return 1;
  }else{
    if(stack_pointer>0){
      if((vm_stack+stack_pointer-1)->type!=mv_null){
        if(mf_put_repr(vm_stack+stack_pointer-1)){
          mf_traceback("command-line print");
          goto error;
        }
        printf("\n");
      }
      mf_dec_refcounts(stack_pointer,vm_stack);
    }
  }
  return 0;
}

typedef struct{
  mt_object* stack;
  mt_stack_frame* frame;
  long stack_pointer;
  mt_object* base_pointer;
  mt_function* function_pointer;
  mt_list* string_pool;
} mt_process_context;

typedef struct{
  mt_object* stack;
  mt_stack_frame* frame;
  int stack_pointer;
  mt_object* base_pointer;
} mt_fiber;

static
void mf_save_process_context(mt_process_context* context){
  context->stack = vm_stack;
  context->frame = frame;
  context->stack_pointer = stack_pointer;
  context->base_pointer = base_pointer;
  context->function_pointer = function_self;
  context->string_pool = string_pool;
}

static
void mf_restore_process_context(mt_process_context* context){
  vm_stack = context->stack;
  frame = context->frame;
  stack_pointer = context->stack_pointer;
  base_pointer = context->base_pointer;
  function_self = context->function_pointer;
  string_pool = context->string_pool;
}

int mf_eval_fiber(mt_fiber* fiber, mt_module* module, long ip){
  mt_process_context context;
  mf_save_process_context(&context);

  vm_stack = fiber->stack;
  frame = fiber->frame;
  stack_pointer = fiber->stack_pointer;
  base_pointer = fiber->base_pointer;
  function_self = mf_new_function(NULL);
  function_self->gtab=mv_gvtab;
  module->refcount++;
  function_self->module=module;
  string_pool=mf_load_data(module->data);
  function_self->data=string_pool->a;
  int e = mf_vm_eval(module->program,ip);
  fiber->stack=vm_stack;
  fiber->stack_pointer=stack_pointer;

  mf_restore_process_context(&context);
  return e;
}

void new_fiber(mt_fiber* fiber){
  fiber->stack = mf_malloc(sizeof(mt_object)*2000);
  fiber->frame = mf_malloc(sizeof(mt_stack_frame)*2000);
  fiber->base_pointer=fiber->stack;
  fiber->stack_pointer=0;
}

mt_table* mf_eval_module(mt_module* module, long ip){
  mt_fiber fiber;
  new_fiber(&fiber);
  mt_map* m = mv_gvtab;
  mv_gvtab = mf_empty_map();
  int e = mf_eval_fiber(&fiber,module,ip);
  // todo: memory leak
  free(fiber.stack);
  free(fiber.frame);
  if(e){
    // todo: delete gvtab
    return NULL;
  }
  mt_table* t = mf_table(NULL);
  t->m=mv_gvtab;
  mv_gvtab = m;
  return t;
}

int mf_eval_bytecode(mt_object* x, mt_module* module){
  mt_fiber fiber;
  new_fiber(&fiber);
  int e = mf_eval_fiber(&fiber,module,0);
  int sp = fiber.stack_pointer;
  if(sp>0){
    mf_copy(x,fiber.stack+sp-1);
    int i;
    for(i=0; i<sp-1; i++){
      mf_dec_refcount(fiber.stack+i);
    }
  }else{
    x->type=mv_null;
  }
  // todo: memory leak
  free(fiber.stack);
  free(fiber.frame);
  return e;
}

