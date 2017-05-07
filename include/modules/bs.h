#ifndef BS_H
#define BS_H

#include <stdint.h>

typedef struct{
  unsigned char* a;
  long size;
  long capacity;
} mt_bs;

// buffer of u32 ADT
typedef struct{
// readonly public:
  unsigned long size;
// private:
  unsigned long capacity;
  uint32_t* a;
} mt_bu32;

void mf_bs_static(mt_bs* s, const char* a, long size);
void mf_bs_cstr(mt_bs* s, const char* a);
void mf_bs_init(mt_bs* s);
void mf_bs_delete(mt_bs* s);
void mf_bs_push_raw(mt_bs* s, long size);
void mf_bs_push(mt_bs* s, long size, const unsigned char* a);
void mf_bs_push_cstr(mt_bs* s, const char* a);

void mf_bu32_init(mt_bu32* b, unsigned long capacity);
void mf_bu32_space(mt_bu32* b, unsigned long size);
void mf_bu32_push(mt_bu32* b, uint32_t x);
void mf_bu32_append(mt_bu32* b, unsigned long size, const uint32_t* a);
void mf_bu32_append_u8(mt_bu32* b, unsigned long size, const unsigned char* a);
void mf_bu32_push_cstr(mt_bu32* b, const char* s);
uint32_t* mf_bu32_move(mt_bu32* b);
void mf_bu32_delete(mt_bu32* b);


#endif
