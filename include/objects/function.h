#ifndef FUNCTION_H
#define FUNCTION_H

mt_function* mf_new_function(unsigned char* address);
void mf_function_delete(mt_function* f);
void mf_function_dec_refcount(mt_function* f);
int mf_fcompose(mt_object* x, int argc, mt_object* v);
mt_function* mf_iter(mt_object* x);
int mf_empty(void);

#endif

