
// Memory buffer

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <modules/bs.h>
#include <moss.h>

void mf_bs_set(mt_bs* s, long size, const unsigned char* a){
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
    unsigned char* buffer=mf_malloc(capacity);
    memcpy(buffer,s->a,s->size);
    mf_free(s->a);
    s->a=buffer;
    s->size=n;
    s->capacity=capacity;
  }else{
    s->size=n;
  }
}

void mf_bs_push(mt_bs* s, long size, const unsigned char* a){
  long n = s->size+size;
  long capacity=s->capacity;
  if(n>capacity){
    if(capacity==0) capacity=32;
    while(capacity<n){
      capacity*=2;
    }
    unsigned char* buffer = mf_malloc(capacity);
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
  mf_bs_push(s,size+1,(unsigned char*)a);
  s->size--;
}

void mf_bu32_init(mt_bu32* b, unsigned long capacity){
  b->size=0;
  b->capacity=capacity;
  if(capacity==0){
    b->a=NULL;
  }else{
    b->a=mf_malloc(sizeof(uint32_t)*capacity);
  }
}

void mf_bu32_space(mt_bu32* b, unsigned long size){
  unsigned long n=b->size+size;
  unsigned long capacity=b->capacity;
  if(n>capacity){
    if(capacity==0) capacity=32;
    while(capacity<n){
      capacity*=4;
    }
    uint32_t* buffer=mf_malloc(sizeof(uint32_t)*capacity);
    memcpy(buffer,b->a,b->size*sizeof(uint32_t));
    mf_free(b->a);
    b->a=buffer;
    b->capacity=capacity;
  }
}

void mf_bu32_push(mt_bu32* b, uint32_t x){
  mf_bu32_space(b,1);
  b->a[b->size]=x;
  b->size++;
}

void mf_bu32_append(mt_bu32* b, unsigned long size, const uint32_t* a){
  mf_bu32_space(b,size);
  memcpy(b->a+b->size,a,size*sizeof(uint32_t));
  b->size+=size;
}

void mf_bu32_append_u8(mt_bu32* b, unsigned long size, const unsigned char* a){
  mf_bu32_space(b,size);
  unsigned long k;
  uint32_t* p=b->a+b->size;
  for(k=0; k<size; k++){
    p[k]=(uint32_t)a[k];
  }
  b->size+=size;
}

void mf_bu32_push_cstr(mt_bu32* b, const char* s){
  unsigned long size=strlen(s);
  mf_bu32_append_u8(b,size,(const unsigned char*)s);
}

uint32_t* mf_bu32_move(mt_bu32* b){
  uint32_t* a=b->a;
  // phantom operations:
  // b->a=NULL;
  // b->size=0;
  // b->capacity=0;
  return a;
}

void mf_bu32_delete(mt_bu32* b){
  mf_free(b->a);
}



