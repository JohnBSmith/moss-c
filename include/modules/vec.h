#ifndef VEC_H
#define VEC_H

typedef struct{
  int size,capacity;
  int bsize;
  unsigned char* a;
} mt_vec;

void mf_vec_init(mt_vec* v, int bsize);
void mf_vec_delete(mt_vec* v);
void* mf_vec_move(mt_vec* v);
void mf_vec_push(mt_vec* v, void* data);
int mf_vec_size(mt_vec* v);
void mf_vec_pop(mt_vec* v, int n);
void* mf_vec_get(mt_vec* v, int index);
void mf_vec_set(mt_vec* v, int index, void* data);
void mf_vec_append(mt_vec* v, mt_vec* v2);

#endif

