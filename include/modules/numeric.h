#ifndef NUMERIC_H
#define NUMERIC_H

#define MV_SHAPESIZE_MAX 10
typedef struct{
  long refcount;
  int type;
  unsigned char a[];
} mt_array_buffer;

typedef struct{
  long refcount;
  mt_object prototype;
  void (*del)(void*);
  long size;
  long itemsize;
  mt_array_buffer* data;
  mt_array_buffer* base;
  long shape_size;
  long shape[MV_SHAPESIZE_MAX];
  long strides[MV_SHAPESIZE_MAX];
} mt_array;

#endif

