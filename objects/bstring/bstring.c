
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <moss.h>

mt_string* mf_str_decode_utf8(long size, unsigned char* a);
int mf_str_cmpmem(mt_string* s, long size, const char* a);

mt_bstring* mf_raw_bstring(long size){
  mt_bstring* s = mf_malloc(sizeof(mt_bstring)+size*sizeof(unsigned char));
  s->refcount=1;
  s->size = size;
  return s;
}

mt_bstring* mf_buffer_to_bstring(long size, const unsigned char* a){
  mt_bstring* s = mf_raw_bstring(size);
  unsigned char* b=s->a;
  long i;
  for(i=0; i<size; i++){
    b[i]=a[i];
  }
  return s;
}

void mf_print_bstring(long size, const unsigned char* a){
  unsigned char b;
  printf("b{");
  long i;
  for(i=0; i<size; i++){
    if(i!=0){
      if(i%2==0){
        if(i%8==0){
          printf(", ");
        }else{
          printf(" ");
        }
      }
    }
    b=a[i];
    printf("%x",(b>>4)&0xff);
    printf("%x",b&0xf);
  }
  printf("}");
}

static
int bstring_decode(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"decode");
    return 1;
  }
  if(v[0].type!=mv_bstring){
    mf_type_error("Type error in bs.decode(e): bs is not a byte string.");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error("Type error in bs.decode(e): e is not a string.");
    return 1;
  }
  mt_string* encoding=(mt_string*)v[1].value.p;
  if(mf_str_cmpmem(encoding,5,"UTF-8")==0){
    mt_bstring* bs = (mt_bstring*)v[0].value.p;
    mt_string* s = mf_str_decode_utf8(bs->size,bs->a);
    x->type=mv_string;
    x->value.p=(mt_basic*)s;
    return 0;
  }else{
    mf_std_exception("Error in bs.decode(endcoding): unknown encoding.");
    return 1;
  }
}

void mf_init_type_bstring(mt_table* type){
  type->name = mf_cstr_to_str("bstr");
  type->m=mf_empty_map();
  mt_map* m=type->m;
  mf_insert_function(m,1,1,"decode",bstring_decode);
}
