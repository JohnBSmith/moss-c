#ifndef MAP_H
#define MAP_H

mt_map* mf_empty_map();
void mf_map_delete(mt_map* m);
void mf_map_dec_refcount(mt_map* m);
void mf_map_set_hash(mt_map* m, mt_object* key, mt_object* value, uint32_t hash);
void mf_map_set_str(mt_map* m, mt_string* key, mt_object* value);
int mf_map_get_hash(mt_object* x, mt_map* m, mt_object* key, uint32_t hash);
int mf_map_get(mt_object* x, mt_map* m, mt_object* key);
int mf_map_set(mt_map* m, mt_object* key, mt_object* value);
mt_object* mf_map_set_key(mt_map* m, mt_object* key);
mt_map* mf_map(long argc, mt_object* v);
int mf_map_print(mt_map* m);
int mf_map_eq(mt_map* m1, mt_map* m2);
void mf_init_type_dict(mt_table* t);
void mf_map_extend(mt_map* m, mt_map* m2);
void mf_map_update(mt_map* m, mt_map* m2);

#endif

