#ifndef STRING_H
#define STRING_H
#include <modules/bs.h>

mt_string* mf_raw_string(long size);
mt_string* mf_str_new(long size, const char* a);
mt_string* mf_str_new_u32(long size, const uint32_t* a);
void mf_str_dec_refcount(mt_string* s);
int mf_str_eq(mt_string* a, mt_string* b);
mt_string* mf_str_add(mt_string* a, mt_string* b);
mt_string* mf_str_mpy(mt_string* a, long n);
mt_string* mf_cstr_to_str(const char* a);
mt_string* mf_bu32_move_to_string(mt_bu32* b);
mt_string* mf_str(mt_object* x);
mt_string* mf_repr(mt_object* x);
int mf_fstr(mt_object* x, int argc, mt_object* v);
int mf_str_cmpmem(mt_string* s, long size, const char* a);
int mf_str_cmp(mt_string* s1, mt_string* s2);
void mf_init_type_string(mt_table* type);
int mf_str_to_int(mt_object* x, mt_string* s);
mt_string* mf_str_decode_utf8(long size, const unsigned char* a);
int mf_str_get(mt_object* x, mt_string* s, mt_object* key);
int mf_str_in_str(mt_string* a, mt_string* b);
int mf_str_in_range(mt_string* s, mt_string* a, mt_string* b);

int mf_uisspace(unsigned long n);
int mf_uisdigit(unsigned long n);
int mf_uisalpha(unsigned long n);

#endif

