#ifndef STR_H
#define STR_H
#include <stdint.h>
#include "vec.h"

typedef struct{
  long size;
  unsigned char* a;
} mt_bstr;

typedef struct{
  long size;
  uint32_t* a;
} mt_str;

// deprecated
typedef struct bstring{
  int size;
  int allocated;
  char* a;
} bstring;

// deprecated
typedef struct str{
  int size;
  int allocated;
  uint32_t* a;
} str;

void mf_bstr(mt_bstr* s, const char* a);
void bstr_delete(bstring* s);
void bstr_slice(bstring* s, char* a, int n);
void bstr_get(bstring* s);
void bstr_put(char* s, int n);
int bstr_eqa(bstring* s, char* a, int n);

void bvec_push_cstring(mt_vec* bv, char* s);
void bvec_push_data(mt_vec* bv, bstring* data);
void bvec_tos(bstring* s, mt_vec* bv);
void str_putc(uint32_t n);

uint32_t str_mbtoc(unsigned char* s);
int str_mblen(unsigned char c);
void mf_put_byte(unsigned char c);

void mf_encode_utf8(mt_bstr* s, uint32_t* a, long size);
void mf_decode_utf8(mt_str* s, unsigned char* a, long size);

#endif

