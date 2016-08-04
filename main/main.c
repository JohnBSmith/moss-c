
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <moss.h>
#include <modules/bs.h>
#include <modules/tk.h>
#include <modules/vec.h>
#include <modules/system.h>
#include <objects/list.h>
#include <vm.h>
#include <compiler.h>

extern int mode_unsafe;
extern int mode_network;
extern int gv_argc;
extern char** gv_argv;

extern int input_nesting;
extern mt_compiler_context compiler_context;
void mf_vtoken_delete(mt_vec* v);

#define OPTION_i 1
#define OPTION_c 2
static int option;

static
char* mf_strdup(const char* s){
  long size = strlen(s);
  char* s2 = mf_malloc(size+1);
  strcpy(s2,s);
  return s2;
}

static
void module_delete(mt_module* m){
  mf_free(m->id);
  mf_free(m->path);
  mf_free(m->program);
  mf_free(m->data);
  if(m->string_pool){
    mf_list_dec_refcount(m->string_pool);
  }
  mf_free(m);
}

mt_module* mf_new_module(void){
  mt_module* m = mf_malloc(sizeof(mt_module));
  m->refcount=1;
  m->prototype.type=mv_null;
  m->del = (void(*)(void*))module_delete;
  m->id=NULL;
  m->path=NULL;
  m->program=NULL;
  m->data=NULL;
  m->string_pool=NULL;
  return m;
}

void mf_module_dec_refcount(mt_module* m){
  if(m->refcount==1){
    m->del(m);
  }else{
    m->refcount--;
  }
}

void mf_get_complete_id(mt_bs* bid, const char* id, int size){
  if(size>0 && id[0]=='/'){
    mf_bs_push_cstr(bid,"/usr/local/lib/moss");
    mf_bs_push(bid,id,size);
  }else{
    mf_bs_push(bid,id,size);
  }
  mf_bs_push(bid,"\0",1);
}

int mf_read_file(mt_bstr* data, const char* id){
  FILE* fp;
  char id2[300];
  if(id[0]=='/'){
    snprintf(id2,300,"/usr/local/lib/moss%s",id);
    fp = fopen(id2,"rb");
    if(fp==NULL) return 1;
  }
  fp = fopen(id,"rb");
  if(fp==NULL){
    char id2[300];
    snprintf(id2,300,"/usr/local/lib/moss/%s",id);
    fp = fopen(id2,"rb");
    if(fp==NULL) return 1;
  }
  mf_file_read(fp,data);
  fclose(fp);
  return 0;
}

mt_table* mf_eval_module(mt_module* module, long ip);
mt_table* mf_load_module(const char* id){
  mt_vec v;
  mf_vec_init(&v,sizeof(mt_token));
  mt_bstr input;
  int e;
  e=mf_read_file(&input,id);
  if(e){
    char buffer[200];
    snprintf(buffer,200,"Error: could not read file '%s'.",id);
    mf_std_exception(buffer);
    return NULL;
  }
  mt_compiler_context context;
  mf_compiler_context_save(&context);
  compiler_context.mode_cmd=0;
  compiler_context.file_name=id;

  e = mf_scan(&v,input.a,input.size,0);
  if(e) goto error;
  mt_module* module = mf_new_module();
  module->id = mf_strdup(id);
  e = mf_compile(&v,module);
  if(e){
    char buffer[200];
    snprintf(buffer,200,"Error: could not compile '%s'.",id);
    mf_std_exception(buffer);
    goto error;
  }
  mt_table* t;
  t = mf_eval_module(module,0);
  mf_vec_delete(&v);
  mf_compiler_context_restore(&context);
  mf_module_dec_refcount(module);
  return t;
  error:
  mf_compiler_context_restore(&context);
  mf_vec_delete(&v);
  mf_module_dec_refcount(module);
  return NULL;
}

int mf_eval_bytecode(mt_object* x, mt_module* module);
int mf_eval_string(mt_object* x, mt_string* s){
  mt_compiler_context context;
  mf_compiler_context_save(&context);
  compiler_context.mode_cmd=1;
  mt_bstr bs;
  mf_encode_utf8(&bs,s->a,s->size);
  int e;
  mt_vec v;
  mf_vec_init(&v,sizeof(mt_token));
  e = mf_scan(&v,bs.a,bs.size,0);
  if(e) goto error;
  mt_module* module = mf_new_module();
  e = mf_compile(&v,module);
  if(e){
    mf_module_dec_refcount(module);
    mf_std_exception("Error: could not compile string.");
    goto error;
  }
  mf_vtoken_delete(&v);
  mt_object y;
  e = mf_eval_bytecode(&y,module);
  mf_compiler_context_restore(&context);
  mf_module_dec_refcount(module);
  if(e) return 1;
  mf_copy(x,&y);
  return 0;
  error:
  mf_vtoken_delete(&v);
  mf_compiler_context_restore(&context);
  return 1;
}

static
mt_module* compile_file(char* id){
  mt_vec v;
  mf_vec_init(&v,sizeof(mt_token));
  mt_bstr input;
  mf_file_load(id,&input);
  int e = mf_scan(&v,input.a,input.size,0);
  if(e) goto error;
  mt_module* module;
  module = mf_new_module();
  module->id = mf_strdup(id);
  e = mf_compile(&v,module);
  if(e){
    mf_module_dec_refcount(module);
    goto error;
  }
  mf_vtoken_delete(&v);
  mf_free(input.a);
  return module;

  error:
  mf_vtoken_delete(&v);
  mf_free(input.a);
  return NULL;
}

static
int eval_file(char* id){
  mt_vec v;
  mf_vec_init(&v,sizeof(mt_token));
  mt_bstr input;
  mf_file_load(id,&input);
  int e = mf_scan(&v,input.a,input.size,0);
  if(e) goto error;
  mt_module* module;
  if(v.size>0){
    module = mf_new_module();
    module->id = mf_strdup(id);
    e = mf_compile(&v,module);
    if(e){
      mf_module_dec_refcount(module);
      goto error;
    }
    mf_vm_set_program(module);
    e = mf_vm_eval_global(module,0);
    mf_module_dec_refcount(module);
  }
  mf_vtoken_delete(&v);
  mf_free(input.a);
  return 0;

  error:
  mf_vtoken_delete(&v);
  mf_free(input.a);
  return 1;
}

static
int slash_cmd(long size, char* a){
  if(size==1){
    char c=a[0];
    if(c=='c'){
      system("clear");
    }else if(c=='q'){
      return 1;
    }
  }else if(size==2){
    if(memcmp(a,"ls",2)==0){
      system("ls");
    }
  }
  return 0;
}

static
void eval_cmd(void){
  char* input;
  long size;
  mt_vec v;
  mf_vec_init(&v,sizeof(mt_token));
  int e;
  mt_module* module;
  while(1){
    input_nesting=0;
    input = mf_getline_hist("> ");
    size = strlen(input);
    if(size>0 && input[0]=='/'){
      if(slash_cmd(size-1,input+1)){
        free(input);
        break;
      }else{
        free(input);
        continue;
      }
    }
    e = mf_scan(&v,(unsigned char*)input,size,0);
    free(input);
    int line_counter=0;
    while(e==0 && input_nesting>0){
      mf_push_newline(&v,line_counter,-1);
      input = mf_getline_hist("| ");
      e = mf_scan(&v,(unsigned char*)input,strlen(input),line_counter);
      free(input);
      line_counter++;
    }
    if(e==0 && v.size>0){
      module = mf_new_module();
      module->id = mf_strdup("command-line");
      e = mf_compile(&v,module);
      if(e==0){
        mf_vm_set_program(module);
        mf_vm_eval_global(module,0);
        mf_vm_unset_program(module);
      }
      mf_module_dec_refcount(module);
    }
    mf_vtoken_delete(&v);
  }
}

static
int is_option(char* s, int size){
  return size>0 && s[0]=='-';
}


int mf_module_save(mt_module* module, const char* id);

int main(int argc, char** argv){
  gv_argv=argv+1;
  gv_argc=argc-1;
  int i;
  char* s;
  int size;
  int error=0;

  mf_vm_init();
  mf_vm_init_gvtab();

  mt_bs file_id;
  mf_bs_init(&file_id);
  for(i=1; i<argc; i++){
    s = argv[i];
    size=strlen(s);
    if(is_option(s,size)){
      if(strcmp(s,"-i")==0){
        option=OPTION_i;
      }else if(strcmp(s,"-net")==0){
        mode_network=1;
      }else if(strcmp(s,"-unsafe")==0){
        mode_unsafe=1;
        mode_network=1;
      }else if(strcmp(s,"-c")==0){
        option=OPTION_c;
      }else{
        printf("Error: unknown option %s.\n",s);
        error=1;
        goto out;
      }
      gv_argv++;
      gv_argc--;
    }else{
      compiler_context.file_name=s;
      mf_get_complete_id(&file_id,s,size);
      break;
    }
  }

  if(option==OPTION_i && file_id.size>0){
    error=eval_file(file_id.a);
    if(error==0){
      compiler_context.mode_cmd=1;
      eval_cmd();
    }
  }else if(option==OPTION_c && file_id.size>0){
    mt_module* m=compile_file(file_id.a);
    if(m==NULL){
      printf("Error: could not compile module.");
      error=1;
    }else{
      if(mf_module_save(m,"out.mb")) error=1;
    }
  }else if(file_id.size>0){
    error=eval_file(file_id.a);
  }else{
    compiler_context.mode_cmd=1;
    eval_cmd();
  }

  out:
  mf_vm_delete();
  mf_bs_delete(&file_id);

  return error;
}


