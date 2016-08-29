#ifndef LA_H
#define LA_H

#define mv_array_float 0
#define mv_array_complex 1
#define mv_array_object 2
#define MV_SHAPESIZE_MAX 10

typedef struct{
  long refcount;
  int type;
  unsigned char a[];
} mt_array_data;

typedef struct{
  long refcount;
  unsigned long size;
  mt_array_data* data;
  unsigned char* base;
  int plain;
  unsigned long n;
  unsigned long shape[MV_SHAPESIZE_MAX];
  long stride[MV_SHAPESIZE_MAX];
} mt_array;

void mf_array_delete(mt_array* a);
void mf_array_dec_refcount(mt_array* a);
long mf_array_size(mt_array* a);

#endif

