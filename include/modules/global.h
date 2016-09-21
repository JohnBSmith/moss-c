#ifndef GLOBAL_H
#define GLOBAL_H

int mf_add_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_sub_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_mpy_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_div_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_idiv_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_mod_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_neg_dec(mt_object* x, mt_object* a);
int mf_tilde_dec(mt_object* x, mt_object* a);
int mf_not_dec(mt_object* x, mt_object* a);
int mf_pow_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_lt_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_gt_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_le_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_ge_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_eq_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_ne_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_is_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_in_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_notin_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_isin_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_bit_and_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_bit_or_dec(mt_object* x, mt_object* a, mt_object* b);
int mf_bit_xor_dec(mt_object* x, mt_object* a, mt_object* b);

int mf_get(mt_object* x, int argc, mt_object* v);

int mf_fcopy(mt_object* x, int argc, mt_object* v);
int mf_fsize(mt_object* x, int argc, mt_object* v);
int mf_frecord(mt_object* x, int argc, mt_object* v);
int mf_fobject(mt_object* x, int argc, mt_object* v);
int mf_ftype(mt_object* x, int argc, mt_object* v);
int mf_fint(mt_object* x, int argc, mt_object* v);
int mf_fload(mt_object* x, int argc, mt_object* v);
int mf_fimport(mt_object* x, int argc, mt_object* v);
int mf_feval(mt_object* x, int argc, mt_object* v);
int mf_fgtab(mt_object* x, int argc, mt_object* v);
int mf_fmax(mt_object* x, int argc, mt_object* v);
int mf_fmin(mt_object* x, int argc, mt_object* v);
int mf_ffloat(mt_object* x, int argc, mt_object* v);
int mf_fchr(mt_object* x, int argc, mt_object* v);
int mf_ford(mt_object* x, int argc, mt_object* v);
int mf_fconst(mt_object* x, int argc, mt_object* v);
int mf_fextend(mt_object* x, int argc, mt_object* v);
int mf_fpow(mt_object* x, int argc, mt_object* v);
int mf_fbool(mt_object* x, int argc, mt_object* v);
int mf_fcomplex(mt_object* x, int argc, mt_object* v);
int mf_fread(mt_object* x, int argc, mt_object* v);
int mf_fassert(mt_object* x, int argc, mt_object* v);
int mf_fhex(mt_object* x, int argc, mt_object* v);

int mf_print(mt_object* x);
int mf_put_repr(mt_object* x);
int mf_fput(mt_object* x, int argc, mt_object* v);
int mf_fprint(mt_object* x, int argc, mt_object* v);

mt_table* mf_table(mt_object* prototype);
int mf_table_literal(mt_object* x, mt_object* prototype, mt_object* m);

mt_range* mf_range(mt_object* a, mt_object* b, mt_object* step);
mt_tuple* mf_tuple_raw(long n);
mt_function* new_function(unsigned char* address);
mt_tuple* mf_tuple_noinc(int argc, mt_object* v);
void mf_tuple_delete(mt_tuple* t);
int mf_object_get_memory(mt_object* x, mt_object* a,
  long size, const char* id);

#endif

