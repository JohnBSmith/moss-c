
// #include <stdlib.h>
// #include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <moss.h>
#include <objects/function.h>
#include <objects/list.h>

mt_tuple* mf_raw_tuple(long size);

static
int time_sleep(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sleep");
    return 1;
  }
  long n;
  if(v[1].type==mv_int){
    n = 1000000*v[1].value.i;
  }else if(v[1].type==mv_float){
    n = (long)1000000*v[1].value.f;
  }else{
    mf_type_error("Type error in time.sleep(x): x is not an integer and not a float.");
    return 1;
  }
  if(n<0) n=-n;
  usleep(n);
  x->type=mv_null;
  return 0;
}

static
int get_time(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"clock");
    return 1;
  }
  mt_object* a=function_self->context->a;
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC,&tv);
  x->type=mv_float;
  x->value.f = (tv.tv_sec-a[0].value.i)
    +(tv.tv_nsec-a[1].value.i)/1000000000.0;
  return 0;
}

static
int time_clock(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"clock");
    return 1;
  }
  struct timespec tv;
  mt_function* f = mf_new_function(NULL);
  f->argc=0;
  f->fp=get_time;
  f->context = mf_raw_tuple(2);
  f->context->a[0].type=mv_int;
  f->context->a[1].type=mv_int;
  clock_gettime(CLOCK_MONOTONIC,&tv);
  f->context->a[0].value.i=tv.tv_sec;
  f->context->a[1].value.i=tv.tv_nsec;  
  x->type=mv_function;
  x->value.p = (mt_basic*)f;
  return 0;
}

static
int time_time(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"time");
    return 1;
  }
  mt_list* list=mf_raw_list(6);
  mt_object* a=list->a;
  unsigned int i;
  for(i=0; i<6; i++) a[i].type=mv_int;
  time_t rawtime;
  struct tm* t;
  time(&rawtime);
  t = gmtime(&rawtime);
  a[0].value.i=t->tm_year+1900;
  a[1].value.i=t->tm_mon+1;
  a[2].value.i=t->tm_mday;
  a[3].value.i=t->tm_hour;
  a[4].value.i=t->tm_min;
  a[5].value.i=t->tm_sec;
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

mt_table* mf_time_load(){
  mt_table* time = mf_table(NULL);
  time->name = mf_cstr_to_str("module time");
  time->m=mf_empty_map();
  mt_map* m=time->m;
  mf_insert_function(m,1,1,"sleep",time_sleep);
  mf_insert_function(m,0,0,"clock",time_clock);
  mf_insert_function(m,0,0,"time",time_time);
  m->frozen=1;
  return time;
}
