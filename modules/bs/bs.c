
// Memory buffer

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <modules/bs.h>
#include <moss.h>

void mf_bs_set(mt_bs* s, const char* a, long size){
  s->a = mf_malloc(size);
  memcpy(s->a,a,size);
  s->size=size;
  s->capacity=size;
}

void mf_bs_cstr(mt_bs* s, const char* a){
  long size = strlen(a);
  s->a = mf_malloc(size+1);
  memcpy(s->a,a,size+1);
  s->size=size;
  s->capacity=size+1;
}

void mf_bs_init(mt_bs* s){
  s->a=NULL;
  s->size=0;
  s->capacity=0;
}

void mf_bs_delete(mt_bs* s){
  mf_free(s->a);
  s->a=NULL;
  s->size=0;
  s->capacity=0;
}

void mf_bs_push_raw(mt_bs* s, long size){
  long n=s->size+size;
  long capacity=s->capacity;
  if(n>capacity){
    if(capacity==0) capacity=32;
    while(capacity<n){
      capacity*=2;
    }
    char* buffer=mf_malloc(capacity);
    memcpy(buffer,s->a,s->size);
    mf_free(s->a);
    s->a=buffer;
    s->size=n;
    s->capacity=capacity;
  }else{
    s->size=n;
  }
}

void mf_bs_push(mt_bs* s, const char* a, long size){
  long n = s->size+size;
  long capacity=s->capacity;
  if(n>capacity){
    if(capacity==0) capacity=32;
    while(capacity<n){
      capacity*=2;
    }
    char* buffer = mf_malloc(capacity);
    memcpy(buffer,s->a,s->size);
    memcpy(buffer+s->size,a,size);
    mf_free(s->a);
    s->a=buffer;
    s->size=n;
    s->capacity=capacity;
  }else{
    memcpy(s->a+s->size,a,size);
    s->size=n;
  }
}

void mf_bs_push_cstr(mt_bs* s, const char* a){
  long size = strlen(a);
  mf_bs_push(s,a,size+1);
  s->size--;
}

