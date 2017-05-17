#ifndef MOSS_H
#define MOSS_H
#include "objects/object.h"
#include <stddef.h>
// #define BIG_ENDIAN
// #define MOSS_LEVEL2
// #define MOSS_LEVEL1
// #define MOSS_LEVEL0

// MOSS_LEVEL2: no BLAS, no SDL, no SDL-TTF
// MOSS_LEVEL1: no MOSS_LEVEL2, no GNU readline, no LibGMP
// MOSS_LEVEL0: no MOSS_LEVEL1, no POSIX

#ifdef MOSS_LEVEL0
  #define MOSS_LEVEL1
#endif
#ifdef MOSS_LEVEL1
  #define MOSS_LEVEL2
#endif

void* mf_malloc(size_t size);
#define mf_free free

void mf_inc_refcount(mt_object* x);
void mf_dec_refcount(mt_object* x);
void mf_dec_refcounts(long size, mt_object* a);
void mf_copy(mt_object* x, mt_object* a);
void mf_copy_inc(mt_object* x, mt_object* a);
int mf_call(mt_function* f, mt_object* x, int argc, mt_object* v);
mt_map* mf_empty_map(void);
void mf_insert_object(mt_map* m, const char* id, mt_object* x);
mt_function* mf_insert_function(mt_map* m, int min, int max,
  const char* id, mt_plain_fn pf);
mt_table* mf_table(mt_object* prototype);
mt_table* mf_table_table(mt_table* t);
mt_string* mf_cstr_to_str(const char* a);
int mf_print(mt_object* x);
int mf_isa(mt_object* x, void* prototype);

void mf_type_error(const char* s);
void mf_type_error1(const char* s, mt_object* x);
void mf_type_error2(const char* s, mt_object* x1, mt_object* x2);
void mf_arg_error(int given, const char* s);
void mf_argc_error(int argc, int min, int max, const char* s);
void mf_value_error(const char* s);
void mf_index_error(const char* s);
void mf_index_error2(const char* s, long index, long size);
void mf_std_exception(const char* s);
void mf_traceback(const char* s);

extern mt_function* function_self;
extern mt_table* mv_type_iterable;

#endif

