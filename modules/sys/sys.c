
// interface to the
// runtime system

#include <moss.h>
#include <objects/string.h>
#include <objects/list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <compiler.h>

mt_list* sys_argv;
extern int gv_argc;
extern char** gv_argv;
extern mt_list* mv_path_list;
extern mt_module* mv_module;
extern mt_exception mv_Exception;
void mf_insert_table(mt_map* m, const char* id, mt_table* t);

static
int sys_exit(mt_object* x, int argc, mt_object* v){
  if(argc==0){
    exit(0);
  }else if(argc==1){
    if(v[0].type!=mv_int){
      mf_type_error("Type error in exit(n): n is not an integer.");
      return 1;
    }
    exit((int)v[0].value.i);
    x->type=mv_null;
    return 0;
  }else{
    mf_argc_error(argc,0,1,"exit");
    return 1;
  }
}

static
int sys_main(mt_object* x, int argc, mt_object* v){
  x->type = mv_bool;
  x->value.b = mv_module->main;
  return 0;
}

mt_table* mf_sys_load(){
  sys_argv = mf_raw_list(0);
  int i;
  mt_object s;
  s.type=mv_string;
  for(i=0; i<gv_argc; i++){
    s.value.p=(mt_basic*)mf_cstr_to_str(gv_argv[i]);
    mf_list_push(sys_argv,&s);
  }
  sys_argv->frozen=1;
  mt_table* sys = mf_table(NULL);
  sys->name = mf_cstr_to_str("module sys");
  sys->m = mf_empty_map();
  mt_map* m = sys->m;
  mt_object t;
  t.type=mv_list;
  t.value.p=(mt_basic*)sys_argv;
  mf_insert_object(m,"argv",&t);
  if(mv_path_list){
    t.value.p=(mt_basic*)mv_path_list;
    mf_insert_object(m,"path",&t);
    // Todo: inc refcount?
  }
  mf_insert_function(m,1,1,"exit",sys_exit);
  mf_insert_function(m,0,0,"main",sys_main);
  mf_insert_table(m,"TypeError",mv_Exception.type);
  mf_insert_table(m,"ValueError",mv_Exception.value);
  mf_insert_table(m,"IndexError",mv_Exception.index);
  m->frozen=1;
  return sys;
}
