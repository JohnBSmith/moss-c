
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <moss.h>
mt_function* mf_new_function(unsigned char* address);
mt_tuple* mf_raw_tuple(long size);

#define mv_rand_state 100
static uint32_t kx = 123456789;
static uint32_t ky = 234567891;
static uint32_t kz = 345678912;
static uint32_t kw = 456789123;
static uint32_t kc = 0;

typedef struct{
    long refcount;
    mt_object prototype;
    void (*del)(void*);
    uint32_t x,y,z,w,c;
} mt_rand_state;

// JKISS32
uint32_t mf_rand32(mt_rand_state* s){
    uint32_t t;
    s->y ^= (s->y<<5);
    s->y ^= (s->y>>7);
    s->y ^= (s->y<<22);
    t = s->z+s->w+s->c;
    s->z = s->w;
    s->c = t>0x7fffffff;
    s->w = t&0x7fffffff;
    s->x += 1411392427;
    return s->x+s->y+s->w;
}

static
double mf_rand(mt_rand_state* s){
    return mf_rand32(s)/4294967296.0;
}

static
int mf_random(mt_rand_state* s, long a, long b){
    return mf_rand32(s)%(b-a+1)+a;
}

static
void rand_state_delete(void* p){
    mf_free(p);
}

mt_rand_state* mf_rand_state_new(uint32_t seed){
    mt_rand_state* s = mf_malloc(sizeof(mt_rand_state));
    s->refcount = 1;
    s->prototype.type = mv_null;
    s->del = rand_state_delete;
    s->x = kx*(seed+1);
    s->y = ky*(seed+1);
    s->z = kz;
    s->w = kw; s->c=kc;
    return s;
}

int rand_next(mt_object* x, int argc, mt_object* v){
    mt_object* a=function_self->context->a;
    mt_rand_state* s = (mt_rand_state*)a[0].value.p;
    x->type = mv_float;
    x->value.f = mf_rand(s);
    return 0;
}

int rand_int_next(mt_object* x, int argc, mt_object* v){
    mt_object* a = function_self->context->a;
    mt_rand_state* s = (mt_rand_state*)a[0].value.p;
    x->type = mv_int;
    x->value.i = mf_random(s,a[1].value.i,a[2].value.i);
    return 0;
}

int rand_list_next(mt_object* x, int argc, mt_object* v){
    mt_object* a = function_self->context->a;
    mt_rand_state* s = (mt_rand_state*)a[0].value.p;
    mt_list* list = (mt_list*)a[1].value.p;
    long i = mf_random(s,0,list->size-1);
    mf_copy_inc(x,list->a+i);
    return 0;
}

static
void rng(mt_object* x, uint32_t seed){
    mt_function* f = mf_new_function(NULL);
    f->argc = 0;
    f->context = mf_raw_tuple(1);
    mt_object* a = f->context->a;
    a[0].type = mv_native;
    a[0].value.p = (mt_basic*)mf_rand_state_new(seed);
    f->fp = rand_next;
    x->type = mv_function;
    x->value.p = (mt_basic*)f;
}

static
void rng_int(mt_object* x, long i, long j, uint32_t seed){
    mt_function* f = mf_new_function(NULL);
    f->argc = 0;
    f->context = mf_raw_tuple(3);
    mt_object* a = f->context->a;
    a[0].type = mv_native;
    a[0].value.p = (mt_basic*)mf_rand_state_new(seed);
    a[1].type = mv_int;
    a[1].value.i = i;
    a[2].type = mv_int;
    a[2].value.i = j;
    f->fp = rand_int_next;
    x->type = mv_function;
    x->value.p = (mt_basic*)f;
}

static
void rng_list(mt_object* x, mt_list* list, uint32_t seed){
    mt_function* f = mf_new_function(NULL);
    f->argc = 0;
    f->context = mf_raw_tuple(2);
    mt_object* a = f->context->a;
    a[0].type = mv_native;
    a[0].value.p = (mt_basic*)mf_rand_state_new(seed);
    list->refcount++;
    a[1].type = mv_list;
    a[1].value.p = (mt_basic*)list;
    f->fp = rand_list_next;
    x->type = mv_function;
    x->value.p = (mt_basic*)f;
}

int mf_frand(mt_object* x, int argc, mt_object* v){
    if(argc==0){
        rng(x,0);
        return 0;
    }else if(argc==1){
        if(v[1].type==mv_int){
            rng(x,(uint32_t)v[1].value.i);
            return 0;
        }else if(v[1].type==mv_list){
            rng_list(x,(mt_list*)v[1].value.p,0);
            return 0;
        }else if(v[1].type==mv_range){
            mt_range* r = (mt_range*)v[1].value.p;
            if(r->a.type==mv_int && r->b.type==mv_int){
                rng_int(x,r->a.value.i,r->b.value.i,0);
                return 0;
            }else{
                mf_type_error("Type error in rand(a..b): expected a,b of type integer.");
                return 1;
            }
        }else{
            mf_type_error("Type error in rand(a): a is not a range and not a list.");
            return 1;
        }
    }else if(argc==2){
        if(v[1].type==mv_range){
            mt_range* r = (mt_range*)v[1].value.p;
            if(r->a.type==mv_int && r->b.type==mv_int && v[2].type==mv_int){
                rng_int(x,r->a.value.i,r->b.value.i,(uint32_t)v[2].value.i);
                return 0;
            }else{
                mf_type_error("Type error in rand(a..b,seed): expected a,b,seed of type integer.");
                return 1;
            }
        }else if(v[1].type==mv_list && v[2].type==mv_int){
            rng_list(x,(mt_list*)v[1].value.p,(uint32_t)v[2].value.i);
            return 0;
        }else{
            mf_type_error("Type error in rand(a,seed): a is not a range and not a list.");
            return 1;
        }
    }else{
        mf_argc_error(argc,0,2,"rand");
        return 1;
    }
}

