
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <modules/str.h>
#include <moss.h>
#include <objects/list.h>
#include <objects/long.h>
int mf_eq(mt_object* x, mt_object* a, mt_object* b);
uint32_t mf_hash(mt_object* key, int* error);

uint32_t mf_hash_string(mt_string* s){
  uint32_t* a = s->a;
  long size = s->size;
  long i;
  uint32_t h=0;
  for(i=0; i<size; i++){
    h = a[i]+(h<<6)+(h<<16)-h;
  }
  return h;
}

uint32_t mf_hash_list(mt_list* list, int *error){
  list->frozen=1;
  long size = list->size;
  long i;
  uint32_t h=0;
  for(i=0; i<size; i++){
    h=mf_hash(list->a+i,error)+(h<<6)+(h<<16)-h;
    if(*error) return 0;
  }
  return h;
}

uint32_t mf_hash_set(mt_set* m, int *error){
  m->frozen=1;
  mt_htab_element* a = m->htab.a;
  long n = m->htab.capacity;
  long i;
  uint32_t h=0;
  for(i=0; i<n; i++){
    if(a[i].taken){
      h+=mf_hash(&a[i].key,error);
      if(*error) return 0;
    }
  }
  return h;
}

uint32_t mf_hash_map(mt_map* m, int *error){
  m->frozen=1;
  mt_htab_element* a = m->htab.a;
  long n = m->htab.capacity;
  long i;
  uint32_t h=0;
  for(i=0; i<n; i++){
    if(a[i].taken){
      h+=mf_hash(&a[i].key,error);
      if(*error) return 0;
      h+=mf_hash(&a[i].value,error);
      if(*error) return 0;
    }
  }
  return h;
}

uint32_t mf_hash(mt_object* key, int* error){
  switch(key->type){
  case mv_null:
    return 0;
  case mv_string:
    return mf_hash_string((mt_string*)key->value.p);
  case mv_int:
    return (uint32_t)key->value.i;
  case mv_list:
    return mf_hash_list((mt_list*)key->value.p,error);
  case mv_set:
    return mf_hash_set((mt_set*)key->value.p,error);
  case mv_map:
    return mf_hash_map((mt_map*)key->value.p,error);
  case mv_long:
    return mf_long_hash((mt_long*)key->value.p);
  case mv_table:
    return (uint32_t)key->value.p;
  default:
     *error=1;
     return 0;
  }
}

#define INIT_CAPACITY 4
void mf_htab_init(mt_htab* t){
  t->a = mf_malloc(INIT_CAPACITY*sizeof(mt_htab_element));
  int i;
  for(i=0; i<INIT_CAPACITY; i++){
    t->a[i].taken=0;
  }
  t->capacity=INIT_CAPACITY;
  t->size=0;
}

void mf_map_delete(mt_map* m){
  mt_htab* t = &m->htab;
  mt_htab_element* a = t->a;
  long i,n = t->capacity;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_dec_refcount(&a[i].key);
      mf_dec_refcount(&a[i].value);
    }
  }
  mf_free(t->a);
  mf_free(m);
}

void mf_map_dec_refcount(mt_map* m){
  if(m->refcount==1){
    mf_map_delete(m);
  }else{
    m->refcount--;
  }
}

void mf_htab_clear(mt_htab* t){
  long n = t->capacity;
  long i;
  mt_htab_element* a = t->a;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_dec_refcount(&a[i].key);
      mf_dec_refcount(&a[i].value);
      a[i].taken=0;
    }
  }
  t->size=0;
}

long mf_htab_index(mt_htab* m, mt_object* key, uint32_t hash){
  if(m->size==0) return -1;
  long index = hash%m->capacity;
  mt_htab_element* p;
  mt_object b;
  int e;
  while(1){
    p = m->a+index;
    if(!p->taken) return -1;
    if(p->key.type==key->type){
      e = mf_eq(&b,&p->key,key);
      if(e){
        puts("Error in mf_htab_index.");
        abort();
      }
      assert(b.type==mv_bool);
      // ^todo: exception system
      if(p->hash==hash && b.value.b){
        return index;
      }
    }
    index++;
    if(index>=m->capacity) index=0;
  }
}

int mf_htab_get(mt_object* x, mt_htab* m, mt_object* key, uint32_t hash){
  long index = mf_htab_index(m,key,hash);
  if(index<0){
    return 1;
  }else{
    mf_copy(x,&m->a[index].value);
    return 0;
  }
}

static
long insert(mt_htab_element* a, int n,
  mt_object* key, mt_object* value, uint32_t hash
){
  long index = hash%n;
  while(a[index].taken){
    index++;
    if(index>=n) index=0;
  }
  a[index].taken=1;
  mf_copy(&a[index].key,key);
  mf_copy(&a[index].value,value);
  a[index].hash=hash;
  return index;
}

long mf_htab_insert(mt_htab* m,
  mt_object* key, mt_object* value, uint32_t hash
){
  long i,n,index;
  long capacity = m->capacity;
  mt_htab_element *a,*ma;
  // assert
  if(mf_htab_index(m,key,hash)>=0){
    puts("Error in mf_htab_insert: expected unique key.");
    abort();
  }
  if((m->size+1)*2>capacity){
    n = 2*capacity;
    m->capacity = n;
    a = mf_malloc(n*sizeof(mt_htab_element));
    for(i=0; i<n; i++){
      a[i].taken=0;
    }
    ma = m->a;
    for(i=0; i<capacity; i++){
      if(ma[i].taken){
        insert(a,n,&ma[i].key,&ma[i].value,ma[i].hash);
      }
    }
    mf_free(ma);
    m->a = a;
  }
  index = insert(m->a,m->capacity,key,value,hash);
  m->size++;
  return index;
}

static
void mf_htab_remove_index(mt_htab* m, int index){
  int h,i,capacity;
  mt_htab_element *a,*ma;
  if(index<0){
    return;
  }
  m->a[index].taken=0;
  m->size--;
  capacity = m->capacity;
  if(capacity>=4 && m->size<capacity*4){
    h=capacity/2;
    m->capacity=h;
    a = mf_malloc(h*sizeof(mt_htab_element));
    for(i=0; i<h; i++){
      a[i].taken=0;
    }
    ma = m->a;
    for(i=0; i<capacity; i++){
      if(ma[i].taken){
        insert(a,h,&ma[i].key,&ma[i].value,ma[i].hash);
      }
    }
    mf_free(ma);
    m->a=a;
  }
}

void mf_htab_remove(mt_htab* m, mt_object* key, uint32_t hash){
  int index = mf_htab_index(m,key,hash);
  if(index>=0){
    mf_dec_refcount(&m->a[index].value);
    mf_htab_remove_index(m,index);
  }
}

mt_htab_element* mf_map_set_key_hash(mt_map* m, mt_object* key, uint32_t hash){
  mt_htab* t = &m->htab;
  int index = mf_htab_index(t,key,hash);
  if(index<0){
    mf_inc_refcount(key);
    mt_object x;
    x.type=mv_null;
    index = mf_htab_insert(t,key,&x,hash);
    return &t->a[index];
  }else{
    return &t->a[index];
  }
}

mt_object* mf_map_set_key(mt_map* m, mt_object* key){
  int error=0;
  uint32_t hash = mf_hash(key,&error);
  if(error){
    puts("Error in get_gv_slot.");
    abort();
  }
  mt_object* px = &mf_map_set_key_hash(m,key,hash)->value;
  return px;
}

void mf_map_set_hash(mt_map* m, mt_object* key, mt_object* value, uint32_t hash){
  mt_htab_element* p = mf_map_set_key_hash(m,key,hash);
  if(p->taken){
    mf_dec_refcount(&p->value);
  }
  mf_copy_inc(&p->value,value);
}

int mf_map_set(mt_map* m, mt_object* key, mt_object* value){
  int e=0;
  uint32_t hash = mf_hash(key,&e);
  if(e){
    // todo
    puts("Error in mf_map_set.");
    abort();
  }
  mt_htab_element* p = mf_map_set_key_hash(m,key,hash);
  if(p->taken){
    mf_dec_refcount(&p->value);
  }
  mf_copy_inc(&p->value,value);
  return 0;
}

void mf_map_set_str(mt_map* m, mt_string* key, mt_object* value){
  uint32_t hash = mf_hash_string(key);
  mt_object x;
  x.type=mv_string;
  x.value.p=(mt_basic*)key;
  mf_map_set_hash(m,&x,value,hash);
}

mt_map* mf_empty_map(){
  mt_map* m = mf_malloc(sizeof(mt_map));
  mf_htab_init(&m->htab);
  m->refcount=1;
  m->frozen=0;
  return m;
}

int mf_map_get_hash(mt_object* x, mt_map* m, mt_object* key, uint32_t hash){
  int e = mf_htab_get(x,&m->htab,key,hash);
  if(e) return 1;
  mf_inc_refcount(x);
  // ^TODO: increment refcount outside of mf_map_get
  return 0;
}

int mf_map_get(mt_object* x, mt_map* m, mt_object* key){
  assert(m);
  int e=0;
  uint32_t hash = mf_hash(key,&e);
  if(e){
    mf_type_error1("in hash function: key (type: %s) is not hashable.",key);
    return 2;
  }
  return mf_htab_get(x,&m->htab,key,hash);
}

mt_map* mf_map(long argc, mt_object* v){
  assert(argc%2==0);
  mt_map* m=mf_empty_map();
  long size=argc/2;
  long i;
  mt_object* key;
  mt_object* value;
  for(i=0; i<size; i++){
    key=v+2*i; value=key+1;
    if(mf_map_set(m,key,value)){
      mf_traceback("a dictionary literal");
      return NULL;
    }
  }
  return m;
}

int mf_put_repr(mt_object* x);
int mf_map_print(mt_map* m){
  mt_htab* t = &m->htab;
  mt_htab_element* a = t->a;
  long n=t->capacity;
  long i;
  int first=1;
  printf("{");
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(first) first=0;
      else printf(", ");
      if(mf_put_repr(&a[i].key)) goto error;
      if(a[i].value.type!=mv_null){
        printf(": ");
        if(mf_put_repr(&a[i].value)) goto error;
      }
    }
  }
  printf("}");
  return 0;
  error:
  printf("\n");
  return 1;
}

int mf_map_eq(mt_map* m1, mt_map* m2){
  if(m1->htab.size!=m2->htab.size) return 0;
  mt_htab_element* a = m1->htab.a;
  long n = m1->htab.capacity;
  long i;
  mt_object x;
  mt_object t;
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(mf_htab_get(&x,&m2->htab,&a[i].key,a[i].hash)){
        return 0;
      }else{
        if(mf_eq(&t,&x,&a[i].value)){
          mf_traceback("{...}=={...}");
          return -1;
        }
        if(t.type!=mv_bool){
          mf_type_error("Type error in {...}=={...}: comparison "
            "of values returned a non-boolean");
          return -1;
        }
        if(!t.value.b) return 0;
      }
    }
  }
  return 1;
}

static
int map_keys(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"keys");
    return 1;
  }
  if(v[0].type!=mv_map){
    mf_type_error("Type error in d.keys(): d is not a dictionary.");
    return 1;
  }
  mt_map* m = (mt_map*)v[0].value.p;
  mt_list* list = mf_raw_list(0);
  mt_htab_element* a=m->htab.a;
  long n=m->htab.capacity;
  long i;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_inc_refcount(&a[i].key);
      mf_list_push(list,&a[i].key);
    }
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

static
int map_values(mt_object* x, int argc, mt_object* v){
  if(argc!=0){
    mf_argc_error(argc,0,0,"values");
    return 1;
  }
  if(v[0].type!=mv_map){
    mf_type_error("Type error in d.values(): d is not a dictionary.");
    return 1;
  }
  mt_map* m = (mt_map*)v[0].value.p;
  mt_list* list = mf_raw_list(0);
  mt_htab_element* a=m->htab.a;
  long n=m->htab.capacity;
  long i;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_inc_refcount(&a[i].value);
      mf_list_push(list,&a[i].value);
    }
  }
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

mt_list* mf_map_to_list(mt_map* m){
  mt_list* list = mf_raw_list(0);
  mt_htab_element* a=m->htab.a;
  long n=m->htab.capacity;
  long i;
  mt_list* p;
  mt_object t;
  t.type=mv_list;
  for(i=0; i<n; i++){
    if(a[i].taken){
      p = mf_raw_list(2);
      mf_copy_inc(p->a,&a[i].key);
      mf_copy_inc(p->a+1,&a[i].value);
      t.value.p=(mt_basic*)p;
      mf_list_push(list,&t);
    }
  }
  return list;
}

void mf_map_extend(mt_map* m, mt_map* m2){
  long i,n;
  mt_htab_element* a = m2->htab.a;
  n = m2->htab.capacity;
  mt_htab* htab = &m->htab;
  for(i=0; i<n; i++){
    if(a[i].taken){
      if(mf_htab_index(htab,&a[i].key,a[i].hash)<0){
        mf_map_set_hash(m,&a[i].key,&a[i].value,a[i].hash);
      }
    }
  }
}

void mf_map_update(mt_map* m, mt_map* m2){
  long i,n;
  mt_htab_element* a = m2->htab.a;
  n = m2->htab.capacity;
  for(i=0; i<n; i++){
    if(a[i].taken){
      mf_map_set_hash(m,&a[i].key,&a[i].value,a[i].hash);
    }
  }
}

static
int map_update(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_map){
    mf_type_error("Type error in d.update(d2): d is not a dictionary.");
    return 1;
  }
  if(argc!=1){
    mf_argc_error(argc,1,1,"update");
    return 1;
  }
  if(v[1].type!=mv_map){
    mf_type_error("Type error in d.update(d2): d2 is not a dictionary.");
    return 1;
  }
  mf_map_update((mt_map*)v[0].value.p,(mt_map*)v[1].value.p);
  x->type=mv_null;
  return 0;
}

static
int map_extend(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_map){
    mf_type_error("Type error in d.extend(d2): d is not a dictionary.");
    return 1;
  }
  if(argc!=1){
    mf_argc_error(argc,1,1,"extend");
    return 1;
  }
  if(v[1].type!=mv_map){
    mf_type_error("Type error in d.extend(d2): d2 is not a dictionary.");
    return 1;
  }
  mf_map_extend((mt_map*)v[0].value.p,(mt_map*)v[1].value.p);
  x->type=mv_null;
  return 0;
}

static
int map_clear(mt_object* x, int argc, mt_object* v){
  if(v[0].type!=mv_map){
    mf_type_error("Type error in d.clear(): d is not a dictionary.");
    return 1;
  }
  if(argc!=0){
    mf_argc_error(argc,0,0,"clear");
    return 1;
  }
  mt_map* m = (mt_map*)v[0].value.p;
  mf_htab_clear(&m->htab);
  x->type=mv_null;
  return 0;
}

void mf_init_type_dict(mt_table* type){
  type->name = mf_cstr_to_str("dict");
  type->m=mf_empty_map();
  mt_map* m=type->m;
  mf_insert_function(m,0,0,"keys",map_keys);
  mf_insert_function(m,0,0,"values",map_values);
  mf_insert_function(m,1,1,"update",map_update);
  mf_insert_function(m,1,1,"extend",map_extend);
  mf_insert_function(m,0,0,"clear",map_clear);
}
