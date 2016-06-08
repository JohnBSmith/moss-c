
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <modules/str.h>
#include <moss.h>

void mf_bstr(mt_bstr* s, const char* a){
  int size=strlen(a);
  s->a = mf_malloc(size+1);
  memcpy(s->a,a,size+1);
  s->size=size;
}

void bstr_delete(bstring* s){
  if(s->allocated){
    free(s->a);
    s->allocated=0;
  }
  s->size=0;
}

void bstr_slice(bstring* s, char* a, int n){
  s->a = malloc(n+1);
  memcpy(s->a,a,n);
  s->a[n]=0;
  s->size=n;
  s->allocated=1;
}

void bstr_get(bstring* s){
  char buffer[1000];
  fgets(buffer,1000,stdin);
  int n=strlen(buffer)-1;
  s->size=n;
  s->a=malloc(n+1);
  s->allocated=1;
  memcpy(s->a,buffer,n);
  s->a[n]=0;
}

void bstr_put(char* s, int n){
  int i;
  for(i=0; i<n; i++){
    putchar(s[i]);
  }
}

int bstr_eqa(bstring* s, char* a, int n){
  if(s->size!=n) return 0;
  return memcmp(s->a,a,n)==0;
}

void bvec_push_cstring(mt_vec* bv, char* s){
  while(*s!=0){
    mf_vec_push(bv,s);
    s++;
  }
}

void bvec_push_data(mt_vec* bv, bstring* data){
  int size = data->size;
  char* a = data->a;
  int i;
  for(i=0; i<size; i++){
    mf_vec_push(bv,a+i);
  }
}

void bvec_tos(bstring* s, mt_vec* bv){
  int n=mf_vec_size(bv);
  s->a = malloc(n+1);
  memcpy(s->a,bv->a,n);
  s->a[n]=0;
  s->size=n;
  s->allocated=1;
}

int str_mblen(unsigned char c){
  if((c&0x80)==0){
    return 1;
  }else if((c&0x40)==0){
    printf("Error in str_mblen: byte is leading byte.\n");
    exit(1);
  }else if((c&0x20)==0){
    return 2;
  }else if((c&0x10)==0){
    return 3;
  }else if((c&0x08)==0){
    return 4;
  }else{
    printf("Error in str_mblen: multibyte sequence is longer than 4 bytes.\n");
    exit(1);
  }
}

uint32_t str_mbtoc(unsigned char* s){
  int n = str_mblen(s[0]);
  uint32_t a1,a2,a3;
  if(n==1){
    return s[0];
  }else if(n==2){
    a1=s[0]&0x1f; a2=s[1]&0x3f;
    return (a1<<6)|a2;
  }else if(n==3){
    a1=s[0]&0xf; a2=s[1]&0x3f; a3=s[2]&0x3f;
    return (a1<<12)|(a2<<6)|a3;
  }else{
    printf("Error in str_mbtoc: multibyte sequence is too long.\n");
    exit(1);
  }
}

void mf_put_byte(unsigned char c){
  if(c>31 && c<127){
    putchar(c);
  }else{
    printf("\\x%x%x",(c>>8)&0xf,c&0xf);
  }
}

void mf_decode_utf8(mt_str* s, unsigned char* a, long size){
  mt_vec buffer;
  mf_vec_init(&buffer,4);
  int i,n,m;
  i=0;
  while(i<size){
    m=str_mblen(a[i]);
    if(m>1){
      if(i+m-1<size){
        n=str_mbtoc(a+i);
      }else{
        n='.';
      }
      i+=m;
    }else{
      n=a[i];
      i++;
    }
    mf_vec_push(&buffer,&n);
  }
  s->a=(uint32_t*)buffer.a;
  s->size=buffer.size;
}

static
int u32_to_multibyte(uint32_t c, unsigned char* a){
  char a0,a1,a2;
  if(c<0x80){
    a[0]=c;
    return 1;
  }else if(c<0x800){
    a1=0xc0; a0=0x80;
    a0|=c&0x3f;
    a1|=(c>>6)&0x1f;
    a[0]=a1;
    a[1]=a0;
    return 2;
  }else if(c<0x10000){
    a2=0xe0; a1=0x80; a0=0x80;
    a0|=c&0x3f;
    a1|=(c>>6)&0x3f;
    a2|=(c>>12)&0xf;
    a[0]=a2;
    a[1]=a1;
    a[2]=a0;
    return 3;
  }else{
    printf("Error in char_to_multibyte.");
    exit(1);
  }
}

// Todo: argument order
void mf_encode_utf8(mt_bstr* s, uint32_t* a, long size){
  mt_vec buffer;
  mf_vec_init(&buffer,1);
  long i;
  unsigned char mb[8];
  int j,count;
  for(i=0; i<size; i++){
    count = u32_to_multibyte(a[i],mb);
    for(j=0; j<count; j++){
      mf_vec_push(&buffer,mb+j);
    }
  }
  s->size=buffer.size;
  mb[0]=0;
  mf_vec_push(&buffer,mb);
  // ^Append a null byte, in case the string is used
  // as a null terminated string.
  s->a=buffer.a;
}

