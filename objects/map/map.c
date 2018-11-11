
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <moss.h>
#include <modules/str.h>
#include <modules/bs.h>
#include <objects/list.h>
#include <objects/long.h>
#include <objects/string.h>
int mf_eq(mt_object* x, mt_object* a, mt_object* b);
uint32_t mf_hash(mt_object* key, int* error);

#define INIT_CAPACITY 4
#define LOAD_FACTOR_INV 2
#define GROWTH_FACTOR 4
#define UNDERLOAD_FACTOR_INV 4
#define SHRINKAGE_FACTOR 2

uint32_t mf_hash_string(mt_string* s){
    uint32_t* a = s->a;
    long size = s->size;
    long i;
    uint32_t h = 0;
    for(i=0; i<size; i++){
        h = a[i]+(h<<6)+(h<<16)-h;
    }
    return h;
}

uint32_t mf_hash_list(mt_list* list, int *error){
    list->frozen=1;
    long size = list->size;
    long i;
    uint32_t h = 0;
    for(i=0; i<size; i++){
        h = mf_hash(list->a+i,error)+(h<<6)+(h<<16)-h;
        if(*error) return 0;
    }
    return h;
}

uint32_t mf_hash_map(mt_map* m, int *error){
    m->frozen = 1;
    mt_htab_element* a = m->htab.a;
    long n = m->htab.capacity;
    long i;
    uint32_t h = 0;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
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
    case mv_bool:
        return key->value.b;
    case mv_string:
        return mf_hash_string((mt_string*)key->value.p);
    case mv_int:
        return (uint32_t)key->value.i;
    case mv_list:
        return mf_hash_list((mt_list*)key->value.p,error);
    case mv_map:
        return mf_hash_map((mt_map*)key->value.p,error);
    case mv_long:
        return mf_long_hash((mt_long*)key->value.p);
    case mv_table:
        return (uint32_t)key->value.p;
    default:
         *error = 1;
         return 0;
    }
}

void mf_htab_init(mt_htab* t){
    t->a = mf_malloc(INIT_CAPACITY*sizeof(mt_htab_element));
    int i;
    for(i=0; i<INIT_CAPACITY; i++){
        t->a[i].taken = 0;
    }
    t->capacity = INIT_CAPACITY;
    t->size = 0;
}

void mf_map_delete(mt_map* m){
    mt_htab* t = &m->htab;
    mt_htab_element* a = t->a;
    long i,n = t->capacity;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
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
        if(a[i].taken==2){
            mf_dec_refcount(&a[i].key);
            mf_dec_refcount(&a[i].value);
            a[i].taken = 0;
        }
    }
    t->size = 0;
}

long mf_htab_index(mt_htab* m, mt_object* key, uint32_t hash){
    if(m->size==0) return -1;
    long index = hash%m->capacity;
    mt_htab_element* p;
    mt_object b;
    while(1){
        p = m->a+index;
        if(p->taken<2){
            if(p->taken==0) return -1;
        }else if(p->key.type==key->type){
            if(p->hash==hash){
                if(mf_eq(&b,&p->key,key)){
                    puts("Error in mf_htab_index.");
                    abort();
                }
                assert(b.type==mv_bool);
                // ^todo: exception system
                if(b.value.b){
                    return index;
                }
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
    while(a[index].taken==2){
        index++;
        if(index>=n) index=0;
    }
    a[index].taken = 2;
    mf_copy(&a[index].key,key);
    mf_copy(&a[index].value,value);
    a[index].hash = hash;
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
    if((m->size+1)*LOAD_FACTOR_INV>capacity){
        n = GROWTH_FACTOR*capacity;
        m->capacity = n;
        a = mf_malloc(n*sizeof(mt_htab_element));
        for(i=0; i<n; i++){
            a[i].taken = 0;
        }
        ma = m->a;
        for(i=0; i<capacity; i++){
            if(ma[i].taken==2){
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
    m->a[index].taken = 1;
    m->size--;
    capacity = m->capacity;
    if(capacity>=4 && m->size<capacity*UNDERLOAD_FACTOR_INV){
        h = capacity/SHRINKAGE_FACTOR;
        m->capacity = h;
        a = mf_malloc(h*sizeof(mt_htab_element));
        for(i=0; i<h; i++){
            a[i].taken = 0;
        }
        ma = m->a;
        for(i=0; i<capacity; i++){
            if(ma[i].taken==2){
                insert(a,h,&ma[i].key,&ma[i].value,ma[i].hash);
            }
        }
        mf_free(ma);
        m->a = a;
    }
}

int mf_htab_remove(mt_htab* m, mt_object* key, uint32_t hash){
    int index = mf_htab_index(m,key,hash);
    if(index>=0){
        mf_dec_refcount(&m->a[index].key);
        mf_dec_refcount(&m->a[index].value);
        mf_htab_remove_index(m,index);
        return 0;
    }else{
        return 1;
    }
}

mt_htab_element* mf_map_set_key_hash(mt_map* m, mt_object* key, uint32_t hash){
    mt_htab* t = &m->htab;
    int index = mf_htab_index(t,key,hash);
    if(index<0){
        mf_inc_refcount(key);
        mt_object x;
        x.type = mv_null;
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
    if(p->taken==2){
        mf_dec_refcount(&p->value);
    }
    mf_copy_inc(&p->value,value);
}

int mf_map_set(mt_map* m, mt_object* key, mt_object* value){
    int e = 0;
    uint32_t hash = mf_hash(key,&e);
    if(e){
        // todo
        puts("Error in mf_map_set.");
        abort();
    }
    mt_htab_element* p = mf_map_set_key_hash(m,key,hash);
    if(p->taken==2){
        mf_dec_refcount(&p->value);
    }
    mf_copy_inc(&p->value,value);
    return 0;
}

void mf_map_set_str(mt_map* m, mt_string* key, mt_object* value){
    uint32_t hash = mf_hash_string(key);
    mt_object x;
    x.type = mv_string;
    x.value.p = (mt_basic*)key;
    mf_map_set_hash(m,&x,value,hash);
}

mt_map* mf_empty_map(){
    mt_map* m = mf_malloc(sizeof(mt_map));
    mf_htab_init(&m->htab);
    m->refcount = 1;
    m->frozen = 0;
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
    int e = 0;
    uint32_t hash = mf_hash(key,&e);
    if(e){
        mf_type_error1("in hash function: key (type: %s) is not hashable.",key);
        return 2;
    }
    return mf_htab_get(x,&m->htab,key,hash);
}

mt_map* mf_map(long argc, mt_object* v){
    assert(argc%2==0);
    mt_map* m = mf_empty_map();
    long size = argc/2;
    long i;
    mt_object* key;
    mt_object* value;
    for(i=0; i<size; i++){
        key = v+2*i;
        value = key+1;
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
    long n = t->capacity;
    long i;
    int first = 1;
    printf("{");
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            if(first) first = 0;
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
        if(a[i].taken==2){
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
    mt_htab_element* a = m->htab.a;
    long n = m->htab.capacity;
    long i;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            mf_inc_refcount(&a[i].key);
            mf_list_push(list,&a[i].key);
        }
    }
    x->type = mv_list;
    x->value.p = (mt_basic*)list;
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
    mt_htab_element* a = m->htab.a;
    long n = m->htab.capacity;
    long i;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            mf_inc_refcount(&a[i].value);
            mf_list_push(list,&a[i].value);
        }
    }
    x->type = mv_list;
    x->value.p = (mt_basic*)list;
    return 0;
}

mt_list* mf_map_to_list(mt_map* m){
    mt_list* list = mf_raw_list(0);
    mt_htab_element* a = m->htab.a;
    long n = m->htab.capacity;
    long i;
    mt_object t;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            mf_copy_inc(&t,&a[i].key);
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
        if(a[i].taken==2){
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
        if(a[i].taken==2){
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
    x->type = mv_null;
    return 0;
}

static
int map_add(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"add");
        return 1;
    }
    if(v[0].type!=mv_map){
        mf_type_error1("in d.add(key): d (type: %s) is not a dictionary.",&v[0]);
        return 1;
    }
    mt_map* m = (mt_map*)v[0].value.p;
    mt_object t;
    t.type = mv_null;
    if(mf_map_set(m,&v[1],&t)){
        mf_traceback("add");
        return 1;
    }
    x->type = mv_null;
    return 0;
}

static
int map_remove(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"remove");
        return 1;
    }
    if(v[0].type!=mv_map){
        mf_type_error1("in d.remove(key): d (type: %s) is not a dictionary.",&v[0]);
        return 1;
    }
    mt_map* m = (mt_map*)v[0].value.p;
    int e = 0;
    uint32_t hash = mf_hash(v+1,&e);
    if(e){
        mf_type_error1("in d.remove(key): key (type: %s) is not hashable.",v+1);
        return 1;
    }
    if(mf_htab_remove(&m->htab,v+1,hash)){
        mf_std_exception("Error in d.remove(key): key is not in d.");
        return 1;
    }
    x->type = mv_null;
    return 0;
}

mt_string* mf_map_to_string(mt_map* m){
    mt_bu32 bs;
    mf_bu32_init(&bs,20);
    mf_bu32_append_u8(&bs,1,(const unsigned char*)"{");

    unsigned long i,n;
    mt_htab_element* a = m->htab.a;
    n = m->htab.capacity;
    mt_string* es;
    int first = 1;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            if(first){
                first = 0;
            }else{
                mf_bu32_append_u8(&bs,2,(const unsigned char*)", ");
            }
            es = mf_str(&a[i].key);
            if(es==NULL){
                mf_traceback("dictionary to string");
                goto error;
            }
            mf_bu32_append(&bs,es->size,es->a);
            if(a[i].value.type!=mv_null){
                mf_bu32_append_u8(&bs,2,(const unsigned char*)": ");
                es = mf_str(&a[i].value);
                if(es==NULL){
                    mf_traceback("dictionary to string");
                    goto error;
                }
                mf_bu32_append(&bs,es->size,es->a);
            }
        }
    }
    mf_bu32_append_u8(&bs,1,(const unsigned char*)"}");
    mt_string* s = mf_bu32_move_to_string(&bs);
    return s;
    
    error:
    mf_bu32_delete(&bs);
    return NULL;
}

mt_map* mf_map_union(mt_map* x, mt_map* y){
    mt_map* m = mf_empty_map();
    mt_htab* htab;
    mt_htab_element* a;
    unsigned long i,n;

    htab = &y->htab;
    a = htab->a;
    n = htab->capacity;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            mf_map_set_hash(m,&a[i].key,&a[i].value,a[i].hash);
        }
    }
    htab = &x->htab;
    a = htab->a;
    n = htab->capacity;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            mf_map_set_hash(m,&a[i].key,&a[i].value,a[i].hash);
        }
    }
    return m;
}

mt_map* mf_map_intersection(mt_map* x, mt_map* y){
    mt_map* m = mf_empty_map();
    mt_htab* htab = &x->htab;
    mt_htab_element* a = htab->a;
    long i;
    long n = htab->capacity;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            if(mf_htab_index(&y->htab,&a[i].key,a[i].hash)>=0){
                mf_map_set_hash(m,&a[i].key,&a[i].value,a[i].hash);
            }
        }
    }
    return m;
}

mt_map* mf_map_difference(mt_map* x, mt_map* y){
    mt_map* m = mf_empty_map();
    mt_htab_element* a = x->htab.a;
    long i;
    long n = x->htab.capacity;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            if(mf_htab_index(&y->htab,&a[i].key,a[i].hash)<0){
                mf_inc_refcount(&a[i].key);
                mf_map_set_hash(m,&a[i].key,&a[i].value,a[i].hash);
            }
        }
    }
    return m;
}

mt_map* mf_map_symmetric_difference(mt_map* x, mt_map* y){
    mt_map* d1 = mf_map_difference(x,y);
    mt_map* d2 = mf_map_difference(y,x);
    mt_map* s = mf_map_union(d1,d2);
    mf_map_delete(d1);
    mf_map_delete(d2);
    return s;
}

int mf_map_subseteq(mt_map* x, mt_map* y){
    if(x->htab.size > y->htab.size) return 0;
    mt_htab_element* a = x->htab.a;
    mt_htab* ytab = &y->htab;
    long i;
    long n = x->htab.capacity;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            if(mf_htab_index(ytab,&a[i].key,a[i].hash)<0){
                return 0;
            }
        }
    }
    return 1;
}

int mf_map_subset(mt_map* x, mt_map* y){
    if(x->htab.size >= y->htab.size) return 0;
    mt_htab_element* a = x->htab.a;
    mt_htab* ytab = &y->htab;
    long i;
    long n = x->htab.capacity;
    for(i=0; i<n; i++){
        if(a[i].taken==2){
            if(mf_htab_index(ytab,&a[i].key,a[i].hash)<0){
                return 0;
            }
        }
    }
    return 1;
}

mt_map* mf_list_to_set(mt_list* list){
    mt_map* m = mf_empty_map();
    int error = 0;
    uint32_t hash;
    long i;
    mt_object null;
    null.type = mv_null;
    for(i=0; i<list->size; i++){
        hash = mf_hash(list->a+i,&error);
        if(error){
            mf_std_exception("Error in set: element is not hashable.");
            mf_map_dec_refcount(m);
            return NULL;
        }
        mf_map_set_hash(m,list->a+i,&null,hash);
    }
    return m;
}

int mf_fset(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,0,1,"set");
        return 1;
    }
    mt_list* list = mf_list(v+1);
    if(list==NULL){
        mf_traceback("set");
        return 1;
    }
    mt_map* m = mf_list_to_set(list);
    mf_list_dec_refcount(list);
    if(m==NULL) return 1;
    x->type = mv_map;
    x->value.p = (mt_basic*)m;
    return 0;
}

int mf_map_items(mt_object* x, int argc, mt_object* v);
int mf_map_values(mt_object* x, int argc, mt_object* v);

void mf_init_type_dict(mt_table* type){
    type->name = mf_cstr_to_str("Map");
    type->m = mf_empty_map();
    mt_map* m = type->m;
    mf_insert_function(m,1,1,"add",map_add);
    mf_insert_function(m,0,0,"list",map_keys);
    mf_insert_function(m,0,0,"values",mf_map_values);
    mf_insert_function(m,0,0,"items",mf_map_items);
    mf_insert_function(m,1,1,"update",map_update);
    mf_insert_function(m,1,1,"extend",map_extend);
    mf_insert_function(m,0,0,"clear",map_clear);
    mf_insert_function(m,1,1,"remove",map_remove);
}
