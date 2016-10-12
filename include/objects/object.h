#ifndef OBJECT_H
#define OBJECT_H
#include "../modules/str.h"

#define mv_null 0
#define mv_bool 1
#define mv_int 2
#define mv_float 3
#define mv_complex 4
#define mv_long 5
#define mv_list 6
#define mv_tuple 7
#define mv_string 8
#define mv_bstring 9
#define mv_map 10
#define mv_array 11
#define mv_range 12
#define mv_function 13
#define mv_table 14
#define mv_native 15

typedef struct mt_basic mt_basic;
typedef struct{
  double re,im;
} mt_complex;

typedef struct{
  unsigned char type;
  union{
    int b;
    long i;
    double f;
    mt_complex c;
    mt_basic* p;
  } value;
} mt_object;

struct mt_basic{
  long refcount;
};

typedef struct{
  long refcount;
  long size;
  uint32_t a[];
} mt_string;

typedef struct{
  long refcount;
  long size;
  unsigned char a[];
} mt_bstring;

typedef struct{
  long refcount;
  long size, capacity;
  int frozen;
  mt_object* a;
} mt_list;

typedef struct{
  unsigned int taken;
  uint32_t hash;
  mt_object key;
  mt_object value;
} mt_htab_element;

typedef struct{
  int size, capacity;
  mt_htab_element* a;
} mt_htab;

typedef struct{
  long refcount;
  mt_htab htab;
  int frozen;
} mt_map;

typedef struct{
  long refcount;
  int size;
  mt_object a[];
} mt_tuple;

typedef struct{
  long refcount;
  mt_object prototype;
  void (*del)(void*);
  char* id;
  char* path;
  long program_size;
  long data_size;
  unsigned char* program;
  unsigned char* data;
  mt_list* string_pool;
  int gtab_owner;
  mt_map* gtab;
} mt_module;

typedef struct{
  long refcount;
  mt_object prototype;
  mt_map* m;
  mt_string* name;
  int (*fp)(mt_object* x, int argc, mt_object* v);
  unsigned char* address;
  mt_object* data;
  mt_map* gtab;
  int argc,min,max;
  int varcount;
  mt_tuple* context;
  mt_module* module;
} mt_function;

typedef struct{
  long refcount;
  mt_object prototype;
  mt_map* m;
  mt_string* name;
  mt_function* del;
} mt_table;

typedef struct{
  long refcount;
  mt_object a;
  mt_object b;
  mt_object step;
} mt_range;

typedef struct{
  long refcount;
  mt_object prototype;
  void (*del)(void*);
} mt_native;

typedef struct mt_long mt_long;
typedef int (*mt_plain_fn)(mt_object* x, int argc, mt_object* v);

#endif

