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

int mf_get(mt_object* x, mt_object* a, mt_object* key);

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

mt_table* mf_table(mt_object* prototype);
int mf_table_literal(mt_object* x, mt_object* prototype, mt_object* m);

mt_range* mf_range(mt_object* a, mt_object* b, mt_object* step);
mt_tuple* mf_tuple_raw(long n);
mt_function* new_function(unsigned char* address);
mt_tuple* mf_tuple_noinc(int argc, mt_object* v);
void mf_tuple_delete(mt_tuple* t);


/*
void init_obnull(Object* x);

Object* new_function(unsigned char* address);
Object* new_str(unsigned char* address);

Object* mf_bit_and(Object* x1, Object* x2);
Object* mf_bit_or(Object* x1, Object* x2);
Object* mf_bit_xor(Object* x1, Object* x2);
int floor_mod(int x, int m);
Object* mf_fdiv(int argc, Object** v);
Object* mf_not(Object* x);
Object* mf_fpowmod(int argc, Object** v);
int mf_in(Object* x, Object* a);
int mf_isin(Object* x, Object* t);
Object* mf_fprototype(int argc, Object** v);
Object* mf_finput(int argc, Object** v);
Object* mf_fint(int argc, Object** v);
Object* mf_fsize(int argc, Object** v);
Object* mf_fload(int argc, Object** v);
Object* mf_gtab_load(int argc, Object** v);
Object* gfabs(int argc, Object** v);
Object* mf_fimport(int argc, Object** v);
Object* mf_object_tap(int argc, Object** v);
Object* mf_fextend(int argc, Object** v);
Object* mf_ffloat(int argc, Object** v);
Object* mf_fslot(int argc, Object** v);
Object* mf_root_str(int argc, Object** v);
Object* mf_operator_range(Object* a, Object* b, Object* step);
Object* mf_frange(int argc, Object** v);
Object* mf_fset(int argc, Object** v);
Object* mf_ftable(int argc, Object** v);
Object* mf_fisinstance(int argc, Object** v);
char* type_tos(int type);
Object* mf_ftype(int argc, Object** v);
Object* mf_fparse(int argc, Object** v);
Object* mf_module_use(int argc, Object** v);
Object* mf_feval(int argc, Object** v);
Object* mf_fcopy(int argc, Object** v);
Object* mf_fdeepcopy(int argc, Object** v);
*/

#endif

