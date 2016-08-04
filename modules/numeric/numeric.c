
#include <stdlib.h>
#include <cblas.h>
#include <moss.h>
#include <modules/numeric.h>

double mf_float(mt_object* x, int* e);
static mt_table* mv_array_type;

static
void array_delete(mt_array* a){
  if(a->data->refcount==1){
    mf_free(a->data);
  }else{
    a->data->refcount--;
  }
  mf_free(a);
}

static
mt_array* mf_new_array(long size, int type){
  mt_array* a = mf_malloc(sizeof(mt_array));
  if(type=='d'){
    a->data = mf_malloc(sizeof(mt_array_buffer)+size*sizeof(double));
    a->itemsize = sizeof(double);
  }else{
    abort();
  }
  a->data->refcount=1;
  a->data->type=type;
  a->refcount=1;
  mv_array_type->refcount++;
  a->prototype.type=mv_table;
  a->prototype.value.p=(mt_basic*)mv_array_type;
  a->size=size;
  a->del=(void(*)(void*))array_delete;
  return a;
}

int numeric_array(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"array");
    return 1;
  }
  if(v[1].type!=mv_list){
    mf_type_error1("in array(a): a (type: %s) is not a list.",v+1);
    return 1;
  }
  mt_list* list=(mt_list*)v[1].value.p;
  int size=list->size;
  mt_array* a = mf_new_array(size,'d');
  a->shape_size=1;
  a->shape[0]=size;
  long i;
  double* b = (double*)a->data->a;
  int e=0;
  double t;
  for(i=0; i<size; i++){
    b[i] = mf_float(list->a+i,&e);
  }
  x->type=mv_native;
  x->value.p=(mt_basic*)a;
  return 1;
}

static
int array_str(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"array.type.str");
    return 1;
  }
}

mt_table* mf_numeric_load(){
  mv_array_type = mf_table(NULL);
  mt_map* m = mv_array_type->m;
  mt_table* numeric = mf_table(NULL);
  numeric->name = mf_cstr_to_str("module numeric");
  numeric->m = mf_empty_map();
  m = numeric->m;
  mf_insert_function(m,1,1,"array",numeric_array);
  m->frozen=1;
  return numeric;
}
