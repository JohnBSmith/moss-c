
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <moss.h>
#include <modules/tk.h>

mt_module* mf_new_module(void);
void mf_push_u8(mt_vec* bv, unsigned char b){
  mf_vec_push(bv,&b);
}

void mf_push_u32(mt_vec* bv, uint32_t n){
#ifdef MV_BIG_ENDIAN
  mf_push_u8(bv,(n>>24)&0xff);
  mf_push_u8(bv,(n>>16)&0xff);
  mf_push_u8(bv,(n>>8)&0xff);
  mf_push_u8(bv,n&0xff);
#else
  mf_push_u8(bv,n&0xff);
  mf_push_u8(bv,(n>>8)&0xff);
  mf_push_u8(bv,(n>>16)&0xff);
  mf_push_u8(bv,(n>>24)&0xff);
#endif
}

void mf_push_mem(mt_vec* bv, unsigned long size, const unsigned char* a){
  unsigned long k;
  for(k=0; k<size; k++){
    mf_push_u8(bv,a[k]);
  }
}

#define MAGIC_NUMBER_SIZE 5
static char* magic_number
= "\0moss.........";

/* == File format ==
packed struct{
  uint8_t magic_number[MAGIC_NUMBER_SIZE];
  uint32_t data_index;
  uint32_t data_size;
  uint32_t program_index;
  uint32_t program_size;
  uint8_t program[];
  uint8_t data[];
}
*/

void mf_module_serialize(mt_bstr* bs, mt_module* module){
  unsigned long program_index, data_index;
  mt_vec bv;
  mf_vec_init(&bv,sizeof(unsigned char));
  mf_push_mem(&bv,MAGIC_NUMBER_SIZE,(unsigned char*)magic_number);
  mf_push_u32(&bv,0xcafe); // program index
  mf_push_u32(&bv,0xcafe); // program size
  mf_push_u32(&bv,0xcafe); // data index
  mf_push_u32(&bv,0xcafe); // data size
  program_index=bv.size;
  mf_push_mem(&bv,module->program_size,module->program);
  data_index=bv.size;
  mf_push_mem(&bv,module->data_size,module->data);
  uint32_t* a = (uint32_t*)(bv.a+MAGIC_NUMBER_SIZE);
  a[0]=program_index;
  a[1]=module->program_size;
  a[2]=data_index;
  a[3]=module->data_size;
  bs->size=bv.size;
  bs->a=bv.a;
}

int mf_module_save(mt_module* module, const char* id){
  mt_bstr bs;
  mf_module_serialize(&bs,module);
  if(mf_file_save(id,&bs)){
    mf_free(bs.a);
    printf("Error: Unable to save module: %s",mv_file_save_error);
    return 1;
  }
  mf_free(bs.a);
  return 0;
}

mt_module* mf_module_unserialize(unsigned long size, const unsigned char* a){
  if(memcmp(a,magic_number,MAGIC_NUMBER_SIZE)!=0){
    return NULL;
  }
  mt_module* m = mf_new_module();
  uint32_t* info = (uint32_t*)(a+MAGIC_NUMBER_SIZE);
  unsigned long program_index, data_index;
  program_index=info[0];
  m->program_size=info[1];
  data_index=info[1];
  m->data_size=info[3];
  
  m->program = mf_malloc(m->program_size);
  m->data = mf_malloc(m->data_size);
  memcpy(m->program,a+program_index,m->program_size);
  memcpy(m->data,a+data_index,m->data_size);
}

mt_module* mf_module_load(const char* id){
  FILE* file = fopen(id,"rb");
  if(file==NULL) return NULL;
  mt_bstr data;
  mf_file_read(file,&data);
  fclose(file);

  mt_module* m;
  m=mf_module_unserialize(data.size,data.a);
  return m;
}

