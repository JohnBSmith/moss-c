
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <moss.h>
#include <modules/vec.h>

void mf_vec_init(mt_vec* v, int bsize){
    v->size = 0;
    v->capacity = 0;
    v->bsize = bsize;
    v->a = NULL;
}

void mf_vec_delete(mt_vec* v){
    mf_free(v->a);
    v->a = NULL;
    v->capacity = 0;
    v->size = 0;
}

void* mf_vec_move(mt_vec* v){
    void* a = v->a;
    v->a = NULL;
    v->size = 0;
    v->capacity = 0;
    return a;
}

void mf_vec_push(mt_vec* v, void* data){
    unsigned char* a;
    if(v->size<v->capacity){
        memcpy(v->a+v->size*v->bsize,data,v->bsize);
        v->size++;
    }else if(v->capacity==0){
        v->a = mf_malloc(4*v->bsize);
        v->capacity = 4;
        memcpy(v->a,data,v->bsize);
        v->size++;
    }else{
        a = mf_malloc(2*v->bsize*v->size);
        memcpy(a,v->a,v->bsize*v->size);
        memcpy(a+v->size*v->bsize,data,v->bsize);
        v->capacity = 2*v->size;
        v->size++;
        mf_free(v->a);
        v->a = a;
    }
}

void mf_vec_pop(mt_vec* v, int n){
    if(n>v->size){
        printf("Error in mf_vec_pop: too few elements in stack.\n");
        exit(1);
    }
    v->size-=n;
    unsigned char* a;
    if(v->capacity>=4 && v->size*2<v->capacity){
        v->capacity = v->capacity/2;
        a = mf_malloc(v->capacity*v->bsize);
        memcpy(a,v->a,v->bsize*v->size);
        mf_free(v->a);
        v->a = a;
    }
}

int mf_vec_size(mt_vec* v){
    return v->size;
}

void* mf_vec_get(mt_vec* v, int index){
    if(index<0) index=v->size+index;
    if(index<0 || index>=v->size){
        printf("Error in mf_vec_get: out of bounds.\n");
        exit(1);
    }
    return v->a+v->bsize*index;
}

void mf_vec_set(mt_vec* v, int index, void* data){
    if(index<0) index=v->size+index;
    if(index<0 || index>=v->size){
        printf("Error in mf_vec_set: out of bounds.\n");
        exit(1);
    }
    memcpy(v->a+v->bsize*index,data,v->bsize);
}

void mf_vec_append(mt_vec* v, mt_vec* v2){
    int n = v2->size;
    int bsize = v2->bsize;
    unsigned char* a = v2->a;
    int i;
    for(i=0; i<n; i++){
        mf_vec_push(v,a+i*bsize);
    }
}
