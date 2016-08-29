
// operating system
// interface

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <moss.h>
#include <objects/string.h>
#include <objects/list.h>
#include <modules/str.h>
#include <modules/system.h>

extern int mode_unsafe;
extern int mode_network;
extern mt_object mv_exception;
mt_string* mf_str_decode_utf8(long size, const unsigned char* a);
static mt_object not_found;

static
void raise_not_found(const char* a){
  mt_string* s = mf_cstr_to_str(a);
  mt_table* e = mf_table(&not_found);
  e->m=mf_empty_map();
  mt_object value;
  value.type=mv_string;
  value.value.p=(mt_basic*)s;
  mf_insert_object(e->m,"value",&value);
  mf_dec_refcount(&mv_exception);
  mv_exception.type=mv_table;
  mv_exception.value.p=(mt_basic*)e;
}

static
int os_ls(mt_object* x, int argc, mt_object* v){
  mt_string* s;
  DIR* dir;
  mt_bstr id;
  if(argc==0){
    dir = opendir(".");
    mf_bstr(&id,".");
  }else if(argc==1){
    if(v[1].type!=mv_string){
      mf_type_error("Type error in ls(id): id is not a string.");
      return 1;
    }
    mt_string* s = (mt_string*)v[1].value.p;
    mf_encode_utf8(&id,s->size,s->a);
    dir = opendir((char*)id.a);
  }else{
    mf_argc_error(argc,0,1,"ls");
    return 1;
  }

  struct dirent* entry;
  mt_list* list;
  mt_object t;
  t.type=mv_string;
  if(dir){
    list = mf_raw_list(0);
    while(1){
      entry = readdir(dir);
      if(!entry) break;
      if(strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0){
        mt_string* es = mf_str_decode_utf8(strlen(entry->d_name),(unsigned char*)entry->d_name);
        t.value.p=(mt_basic*)es;
        mf_list_push(list,&t);
      }
    }
    closedir(dir);
    mf_free(id.a);
    x->type=mv_list;
    x->value.p=(mt_basic*)list;
    return 0;
  }else{
    char buffer[200];
    snprintf(buffer,200,"Exception: file or directory '%s' not found.",id.a);
    raise_not_found(buffer);
    mf_free(id.a);
    return 1;
  }
}

static
int os_wd(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"wd");
    return 1;
  }
  char wd[1024];
  getcwd(wd,1024);
  mt_string* s = mf_str_decode_utf8(strlen(wd),(unsigned char*)wd);
  x->type=mv_string;
  x->value.p=(mt_basic*)s;
  return 0;
}

static
int os_cd(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"cd");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in cd(id): id is not a string.");
    return 1;
  }
  mt_string* s = (mt_string*)v[1].value.p;
  mt_bstr id;
  mf_encode_utf8(&id,s->size,s->a);
  chdir((char*)id.a);
  mf_free(id.a);
  x->type=mv_null;
  return 0;
}

// This function removes files.
static
int os_rm(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"rm");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in rm(id): id is not a string.");
    return 1;
  }
  if(0==mode_unsafe){
    mf_std_exception("Error in rm(id): permission denied.");
    return 1;
  }
  mt_string* s = (mt_string*)v[1].value.p;
  mt_bstr id;
  mf_encode_utf8(&id,s->size,s->a);
  int error = remove((char*)id.a);
  mf_free(id.a);
  if(error){
    mf_std_exception("Error in rm(id): could not remove id.");
    return 1;
  }
  x->type=mv_null;
  return 0;
}

// This function enables execution of other programs.
static
int os_system(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"system");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in os.system(s): s is not a string.");
    return 1;
  }
  if(0==mode_unsafe){
    mf_std_exception("Error in os.system(s): permission denied.");
    return 1;
  }
  mt_string* s = (mt_string*)v[1].value.p;
  mt_bstr bs;
  mf_encode_utf8(&bs,s->size,s->a);
  int e = system((char*)bs.a);
  mf_free(bs.a);
  x->type=mv_int;
  x->value.i=e;
  return 0;
}

static
int os_isdir(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"isdir");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in os.isdir(id): id is not a string.");
    return 1;
  }
  mt_string* s = (mt_string*)v[1].value.p;
  mt_bstr id;
  mf_encode_utf8(&id,s->size,s->a);
  int y = mf_isdir((char*)id.a);
  mf_free(id.a);
  x->type=mv_bool;
  x->value.b=y;
  return 0;
}

static
int os_isfile(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"isfile");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in os.isfile(id): id is not a string.");
    return 1;
  }
  mt_string* s = (mt_string*)v[1].value.p;
  mt_bstr id;
  mf_encode_utf8(&id,s->size,s->a);
  int y = mf_isfile((char*)id.a);
  mf_free(id.a);
  x->type=mv_bool;
  x->value.b=y;
  return 0;
}

mt_table* mf_os_load(){
  mt_table* os = mf_table(NULL);
  os->name = mf_cstr_to_str("module os");
  os->m = mf_empty_map();
  mt_map* m = os->m;
  not_found.type=mv_table;
  not_found.value.p = (mt_basic*)mf_table(NULL);
  mf_insert_object(m,"not_found",&not_found);
  mf_insert_function(m,0,1,"ls",os_ls);
  mf_insert_function(m,0,0,"wd",os_wd);
  mf_insert_function(m,1,1,"cd",os_cd);
  mf_insert_function(m,1,1,"rm",os_rm);
  mf_insert_function(m,1,1,"isdir",os_isdir);
  mf_insert_function(m,1,1,"isfile",os_isfile);
  mf_insert_function(m,1,1,"system",os_system);
  m->frozen=1;
  return os;
}
