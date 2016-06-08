
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <moss.h>
#include <modules/str.h>
#include <modules/vec.h>
#include <objects/map.h>
#include <objects/list.h>
#include <objects/string.h>

uint32_t mf_hash(mt_object* x, int* error);
void mf_htab_init(mt_htab* m);
int mf_htab_index(mt_htab* m, mt_object* key, uint32_t hash);
void mf_htab_insert(mt_htab* m, mt_object* key, mt_object* value, uint32_t hash);

// Function* mf_iter(Object* x);
// extern Object* exception;
// extern Object* empty;

mt_set* mf_empty_set(){
  mt_set* m = mf_malloc(sizeof(mt_set));
  mf_htab_init(&m->htab);
  m->refcount=1;
  m->frozen=0;
  return m;
}

void mf_set_delete(mt_set* m){
  long i,n;
  mt_htab* htab = &m->htab;
  n = htab->capacity;
  mt_htab_element* a = htab->a;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_dec_refcount(&a[i].key);
    }
  }
  free(htab->a);
  free(m);
}

void mf_set_dec_refcount(mt_set* m){
  if(m->refcount==1){
    mf_set_delete(m);
  }else{
    m->refcount--;
  }
}

mt_string* mf_set_to_string(mt_set* m){
  mt_htab* htab = &m->htab;
  long i,n;
  n = htab->capacity;
  mt_htab_element* a = htab->a;
  mt_string* s;
  mt_list* list = mf_raw_list(0);
  mt_vec buffer;
  mf_vec_init(&buffer,sizeof(uint32_t));
  uint32_t c;
  c='{';
  mf_vec_push(&buffer,&c);

  mt_object t;
  t.type=mv_string;
  for(i=0; i<n; i++){
    if(a[i].taken){
      s=mf_repr(&a[i].key);
      if(s==NULL){
        mf_list_dec_refcount(list);
        mf_vec_delete(&buffer);
        return NULL;
      }
      t.value.p=(mt_basic*)s;
      mf_list_push(list,&t);
    }
  }
  mt_string* sep = mf_cstr_to_str(", ");
  if(mf_join(&buffer,list,sep)){
    mf_str_dec_refcount(sep);
    mf_list_dec_refcount(list);
    mf_vec_delete(&buffer);
    return NULL;
  }
  c='}';
  mf_vec_push(&buffer,&c);
  s = mf_raw_string(buffer.size);
  memcpy(s->a,buffer.a,buffer.size*4);
  mf_str_dec_refcount(sep);
  mf_list_dec_refcount(list);
  return s;
}

void mf_set_insert(mt_set* m, mt_object* x, uint32_t hash){
  mt_htab* htab = &m->htab;
  long index = mf_htab_index(htab,x,hash);
  mt_object null;
  null.type=mv_null;
  if(index<0){
    mf_htab_insert(htab,x,&null,hash);
  }else{
    mf_dec_refcount(x);
  }
}

int mf_set_in(mt_object* x, mt_set* m){
  int error=0;
  uint32_t hash = mf_hash(x,&error);
  if(error) return -1;
  long index = mf_htab_index(&m->htab,x,hash);
  return index>=0;
}

static
mt_set* list_to_set(mt_list* list){
  mt_set* m = mf_empty_set();
  int error=0;
  uint32_t hash;
  long i;
  for(i=0; i<list->size; i++){
    hash = mf_hash(list->a+i,&error);
    if(error){
      mf_std_exception("Error in set: element is not hashable.");
      mf_set_dec_refcount(m);
      return NULL;
    }
    mf_inc_refcount(list->a+i);
    mf_set_insert(m,list->a+i,hash);
  }
  return m;
}

int mf_fset(mt_object* x, int argc, mt_object* v){
  if(argc==0){
    x->type=mv_set;
    x->value.p=(mt_basic*)mf_empty_set();
    return 0;
  }else if(argc!=1){
    mf_argc_error(argc,0,1,"set");
    return 1;
  }
  mt_list* list = mf_list(v+1);
  if(list==NULL){
    mf_traceback("set");
    return 1;
  }
  mt_set* m = list_to_set(list);
  mf_list_dec_refcount(list);
  if(m==NULL) return 1;
  x->type=mv_set;
  x->value.p=(mt_basic*)m;
  return 0;
}

mt_list* mf_set_to_list(mt_set* m){
  mt_htab* htab = &m->htab;
  long i,n;
  n = htab->capacity;
  mt_htab_element* a = htab->a;
  mt_list* list = mf_raw_list(0);
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_inc_refcount(&a[i].key);
      mf_list_push(list,&a[i].key);
    }
  }
  return list;
}

int mf_set_eq(mt_set* x, mt_set* y){
  if(x->htab.size!=y->htab.size){
    return 0;
  }
  mt_htab_element* a = x->htab.a;
  long n = x->htab.capacity;
  long i;
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(!mf_set_in(&a[i].key,y)){
        return 0;
      }
    }
  }
  return 1;
}

mt_set* mf_set_union(mt_set* x, mt_set* y){
  mt_set* m = mf_empty_set();
  mt_htab* htab = &x->htab;
  mt_htab_element* a = htab->a;
  long i,n=htab->capacity;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_inc_refcount(&a[i].key);
      mf_set_insert(m,&a[i].key,a[i].hash);
    }
  }
  htab = &y->htab;
  a=htab->a;
  n=htab->capacity;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_inc_refcount(&a[i].key);
      mf_set_insert(m,&a[i].key,a[i].hash);
    }
  }
  return m;
}

mt_set* mf_set_intersection(mt_set* x, mt_set* y){
  mt_set* m = mf_empty_set();
  mt_htab* htab = &x->htab;
  mt_htab_element* a = htab->a;
  long i,n=htab->capacity;
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(mf_htab_index(&y->htab,&a[i].key,a[i].hash)>=0){
        mf_inc_refcount(&a[i].key);
        mf_set_insert(m,&a[i].key,a[i].hash);
      }
    }
  }
  return m;
}

mt_set* mf_set_difference(mt_set* x, mt_set* y){
  mt_set* m = mf_empty_set();
  mt_htab_element* a = x->htab.a;
  long i,n=x->htab.capacity;
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(mf_htab_index(&y->htab,&a[i].key,a[i].hash)<0){
        mf_inc_refcount(&a[i].key);
        mf_set_insert(m,&a[i].key,a[i].hash);
      }
    }
  }
  return m;
}

mt_set* mf_set_symmetric_difference(mt_set* x, mt_set* y){
  mt_set* d1 = mf_set_difference(x,y);
  mt_set* d2 = mf_set_difference(y,x);
  mt_set* s = mf_set_union(d1,d2);
  mf_set_delete(d1);
  mf_set_delete(d2);
  return s;
}

