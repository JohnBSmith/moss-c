#ifndef BS_H
#define BS_H

typedef struct{
  char* a;
  long size;
  long capacity;
} mt_bs;

void mf_bs_static(mt_bs* s, const char* a, long size);
void mf_bs_cstr(mt_bs* s, const char* a);
void mf_bs_init(mt_bs* s);
void mf_bs_delete(mt_bs* s);
void mf_bs_push_raw(mt_bs* s, long size);
void mf_bs_push(mt_bs* s, long size, const char* a);
void mf_bs_push_cstr(mt_bs* s, const char* a);

#endif
