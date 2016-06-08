#ifndef LONG_H
#define LONG_H

mt_long* mf_long_new();
void mf_long_delete(mt_long* x);
void mf_long_dec_refcount(mt_long* x);
mt_long* mf_int_to_long(long n);
mt_long* mf_to_long(mt_object* x);
int mf_flong(mt_object* x, int argc, mt_object* v);
mt_long* mf_string_to_long(mt_string* x);
void mf_long_print(mt_long* x);
mt_long* mf_long_add(mt_long *a, mt_long* b);
mt_long* mf_long_sub(mt_long *a, mt_long* b);
mt_long* mf_long_mpy(mt_long *a, mt_long* b);
mt_long* mf_long_div(mt_long* a, mt_long* b);
mt_long* mf_long_mod(mt_long* a, mt_long* b);
mt_long* mf_long_pow(mt_long* a, mt_long* b);
mt_long* mf_long_neg(mt_long* x);
mt_long* mf_long_powmod(mt_long* x, mt_long* n, mt_long* m);
mt_long* mf_long_abs(mt_long* x);
int mf_long_sgn(mt_long* x);
uint32_t mf_long_hash(mt_long* x);
int mf_long_cmp(mt_long* a, mt_long* b);
int mf_long_cmp_si(mt_long* a, long b);

#endif
