
// #include <stdlib.h>
// #include <stdio.h>
// #include <time.h>
#include <unistd.h>
#include <moss.h>

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

mt_table* mf_time_load(){
  mt_table* time = mf_table(NULL);
  time->name = mf_cstr_to_str("module time");
  time->m=mf_empty_map();
  mt_map* m=time->m;
  mf_insert_function(m,1,1,"sleep",time_sleep);
  m->frozen=1;
  return time;
}
