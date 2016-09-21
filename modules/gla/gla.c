
#include <moss.h>

#define SHAPESIZE_MAX 10
typedef struct{
  long refcount;
  mt_object prototype;
  int plain;
  unsigned int n;
  unsigned long shape[SHAPESIZE_MAX];
  long stride[SHAPESIZE_MAX];
  mt_tuple* data;
  mt_object* base;
} mt_garray;

mt_table* mf_gla_load(){
  mt_map* m;
  // mt_map* m = mv_type_array->m;
  // mf_insert_function(m,0,0,"STR",garray_STR);
  // mf_insert_function(m,0,0,"list",garray_list);
  // mf_insert_function(m,1,1,"ADD",garray_ADD);
  // mf_insert_function(m,1,1,"SUB",garray_SUB);
  // mf_insert_function(m,1,1,"MPY",garray_MPY);
  // mf_insert_function(m,1,1,"RMPY",garray_RMPY);
  // mf_insert_function(m,0,0,"NEG",garray_NEG);
  // mf_insert_function(m,1,1,"DIV",garray_DIV);
  // mf_insert_function(m,1,1,"POW",garray_POW);
  // mf_insert_function(m,1,-1,"GET",garray_GET);
  // mf_insert_function(m,0,0,"T",garray_T);
  // mf_insert_function(m,0,0,"plain",garray_plain);
  // mf_insert_function(m,0,0,"copy",garray_copy);

  mt_table* gla = mf_table(NULL);
  gla->name = mf_cstr_to_str("module gla");
  gla->m = mf_empty_map();
  m = gla->m;
  // mf_insert_function(m,1,1,"array",gla_array);
  // mf_insert_function(m,1,1,"vector",gla_vector);
  // mf_insert_function(m,1,1,"matrix",gla_matrix);
  // mf_insert_function(m,1,1,"idm",gla_idm);
  m->frozen=1;
  return gla;
}

