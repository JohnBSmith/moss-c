#ifndef SET_H
#define SET_H
#include "object.h"

mt_set* mf_empty_set();
mt_string* mf_set_to_string(mt_set* m);
void mf_set_delete(mt_set* m);
void mf_set_dec_refcount(mt_set* m);
int mf_set_eq(mt_set* x, mt_set* y);
mt_set* mf_set_union(mt_set* x, mt_set* y);
mt_set* mf_set_intersection(mt_set* x, mt_set* y);
mt_set* mf_set_difference(mt_set* x, mt_set* y);
mt_set* mf_set_symmetric_difference(mt_set* x, mt_set* y);
int mf_fset(mt_object* x, int argc, mt_object* v);
int mf_set_in(mt_object* x, mt_set* m);

#endif

