#ifndef LIST_H
#define LIST_H

mt_list* mf_raw_list(long size);
mt_list* mf_list_noinc(int argc, mt_object* v);
int mf_new_list(mt_object* x, int argc, mt_object* v);
void mf_list_delete(mt_list* a);
void mf_list_dec_refcount(mt_list* a);
void mf_list_push(mt_list* a, mt_object* x);
mt_list* mf_list_add(mt_list* a, mt_list* b);
mt_list* mf_list_mpy(mt_list* a, long n);
mt_list* mf_list_pow(mt_list* a, long n);
int mf_list_eq(mt_list* La, mt_list* Lb);

mt_list* mf_list(mt_object* a);
int mf_flist(mt_object* x, int argc, mt_object* v);
int mf_fmap(mt_object* x, int argc, mt_object* v);
int mf_fzip(mt_object* x, int argc, mt_object* v);
int mf_funzip(mt_object* x, int argc, mt_object* v);

int mf_join(mt_vec* buffer, mt_list* list, mt_string* sep);
int mf_list_slice(mt_object* x, mt_list* a, mt_range* r);
void mf_init_type_list(mt_table* type);


#endif

