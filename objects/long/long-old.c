
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <moss.h>
#include <modules/str.h>
#include <modules/vec.h>
#include <objects/string.h>
#include <objects/list.h>

void mf_print_string(long size, uint32_t* a);

typedef struct{
  int sgn;
  long size, capacity;
  unsigned char* a;
} bint;

struct mt_long{
  long refcount;
  bint value;
};

void bint_print(bint* x);

static
void bint_clear(bint* x){
  x->size=0;
}

static
void bint_delete(bint* x){
  if(x->a!=NULL){
    mf_free(x->a);
    x->a=NULL;
  }
}

void mf_long_delete(mt_long* x){
  bint_delete(&x->value);
  mf_free(x);
}

void mf_long_dec_refcount(mt_long* x){
  if(x->refcount==1){
    bint_delete(&x->value);
    mf_free(x);
  }else{
    x->refcount--;
  }
}

void bint_set_value(bint* x, long n){
  if(n<0){
    x->sgn=-1;
    n=-n;
  }else{
    x->sgn=1;
  }
  if(x->a==NULL){
    x->capacity=4;
    x->a=mf_malloc(4);
  }else if(x->capacity<4){
    mf_free(x->a);
    x->capacity=4;
    x->a=mf_malloc(4);
  }
  if(n<0x100){
    x->a[0]=n;
    x->size=1;
  }else if(n<0x10000){
    x->a[0]=n&0xff;
    x->a[1]=(n>>8)&0xff;
    x->size=2;
  }else if(n<0x1000000){
    x->a[0]=n&0xff;
    x->a[1]=(n>>8)&0xff;
    x->a[2]=(n>>16)&0xff;
    x->size=3;
  }else{
    x->a[0]=n&0xff;
    x->a[1]=(n>>8)&0xff;
    x->a[2]=(n>>16)&0xff;
    x->a[3]=(n>>24)&0xff;
    x->size=4;
  }
}

mt_long* long_new(){
  mt_long* x = mf_malloc(sizeof(mt_long));
  x->refcount=1;
  x->value.a=NULL;
  return x;
}

mt_long* mf_int_to_long(long n){
  mt_long* x = long_new();
  bint_set_value(&x->value,n);
  return x;
}

static
void bint_init_raw(bint* x, int size){
  if(x->a==NULL){
    x->a=mf_malloc(size);
    x->capacity=size;
    x->size=size;
  }else if(size>x->capacity){
    mf_free(x->a);
    x->a=mf_malloc(size);
    x->capacity=size;
    x->size=size;
  }else{
    x->size=size;
  }
}

mt_long* mf_string_to_long(mt_string* s);

mt_long* to_long(mt_object* x){
  if(x->type==mv_int){
    long value = x->value.i;
    return mf_int_to_long(value);
  }else if(x->type==mv_long){
    x->value.p->refcount++;
    return (mt_long*)x->value.p;
  }else if(x->type==mv_string){
    return mf_string_to_long((mt_string*)x->value.p);
  }else{
    mf_type_error("Type error in long(x): x is not an integer.");
    return 0;
  }
}

int mf_flong(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"long");
    return 1;
  }
  mt_long* a = to_long(v+1);
  if(a==NULL) return 1;
  x->type=mv_long;
  x->value.p=(mt_basic*)a;
  return 0;
}

static
void bint_copy(bint* x, bint* y){
  int size = x->size;
  bint_init_raw(y,size);
  int i;
  for(i=0; i<size; i++){
    y->a[i]=x->a[i];
  }
  y->sgn=x->sgn;
}

void long_print_hex(mt_long* x){
  if(x->value.sgn==-1){
    printf("-");
  }
  printf("0x");
  if(x->value.size==0){
    printf("0");
  }
  int i;
  int byte;
  for(i=x->value.size-1; i>=0; i--){
    byte = x->value.a[i];
    if(byte<16){
      printf("0");
    }
    printf("%x",byte);
  }
}


// TODO

static
int ord(int x){
  if(isalpha(x)) return x-'0';
  else if('A'<=x && x<='F') return x-'A'+10;
  else if('a'<=x && x<='f') return x-'a'+10;
  else return 0;
}

static
int hex_to_byte(int a, int b){
  return 16*ord(a)+ord(b);
}

/*
static
mt_long* hex_to_long(unsigned char* a, int size){
  mt_long* x;
  int i,n,m;
  if(size%2==0){
    n = size/2;
    x = long_raw_new(n);
    for(i=0; i<n; i++){
      x->value.a[i] = hex_to_byte(a[size-2-2*i],a[size-1-2*i]);
    }
    return x;
  }else{
    n = (size+1)/2;
    x = long_raw_new(n);
    m = size/2;
    for(i=0; i<m; i++){
      x->value.a[i] = hex_to_byte(a[size-2-2*i],a[size-1-2*i]);
    }
    x->value.a[n-1] = hex_to_byte(0,a[size-1]);
    return x;
  }
}
*/

static
void print_byte_bin(unsigned char byte){
  char a[8];
  int i;
  for(i=0; i<8; i++){
    a[i]=0;
  }
  i=7;
  while(byte>0){
    if(byte%2==1){
      a[i]=1;
    }else{
      a[i]=0;
    }
    byte=byte/2;
    i--;
  }
  for(i=0; i<8; i++){
    printf("%i",a[i]);
  }
}

void long_print_bin(mt_long* x){
  if(x->value.sgn==-1){
    printf("-");
  }
  printf("0b");
  if(x->value.size==0){
    printf("0");
  }
  int i;
  int byte;
  for(i=x->value.size-1; i>=0; i--){
    byte = x->value.a[i];
    print_byte_bin(byte);
  }
}

static
void normalize(bint* x){
  while(x->size>0 && x->a[x->size-1]==0){
    x->size--;
  }
  if(x->size==0){
    x->sgn=1;
  }
}

static
int ltabs(bint* a, bint* b){
  unsigned char bytea,byteb;
  int i;
  normalize(a);
  normalize(b);
  if(a->size==0){
    return b->size!=0;
  }else if(a->size==b->size){
    i=a->size-1;
    while(i>=0){
      bytea=a->a[i];
      byteb=b->a[i];
      if(bytea<byteb) return 1;
      else if(bytea>byteb) return 0;
      i--;
    }
    return 0;
  }else if(a->size<b->size){
    return 1;
  }else{
    return 0;
  }
}

static
int gtabs(bint* a, bint* b){
  unsigned char bytea,byteb;
  int i;
  normalize(a);
  normalize(b);
  if(b->size==0){
    return a->size!=0;
  }if(a->size==b->size){
    i=a->size-1;
    while(i>=0){
      bytea=a->a[i];
      byteb=b->a[i];
      if(bytea>byteb) return 1;
      else if(bytea<byteb) return 0;
      i--;
    }
    return 0;
  }else if(a->size>b->size){
    return 1;
  }else{
    return 0;
  }
}

int bint_lt(bint* a, bint* b){
  if(a->sgn==b->sgn){
    if(a->sgn==-1){
      return gtabs(a,b);
    }else{
      return ltabs(a,b);
    }
  }else if(a->sgn==-1){
    return 1;
  }else{
    return 0;
  }
}

int bint_gt(bint* a, bint* b){
  int bytea,byteb;
  if(a->sgn==b->sgn){
    if(a->sgn==-1){
      return ltabs(a,b);
    }else{
      return gtabs(a,b);
    }
  }else if(a->sgn==1){
    return 1;
  }else{
    return 0;
  }
}

int bint_le(bint* a, bint* b){
  return !bint_gt(a,b);
}

int bint_ge(bint* a, bint* b){
  return !bint_lt(a,b);
}

int bint_eq(bint* a, bint* b){
  normalize(a);
  normalize(b);
  if(a->sgn!=b->sgn){
    return 0;
  }else if(a->size!=b->size){
    return 0;
  }else if(a->size==0){
    return 1;
  }else{
    int i=a->size-1;
    while(i>=0){
      if(a->a[i]!=b->a[i]) return 0;
      i--;
    }
    return 1;
  }
}

int bint_neq(bint* a, bint* b){
  return !bint_eq(a,b);
}

int bint_cmp(bint* a, bint* b){
  if(bint_lt(a,b)){
    return -1;
  }else if(bint_eq(a,b)){
    return 0;
  }else{
    return 1;
  }
}

static
void sub_from_greater(bint* a, bint* b, bint* x){
  int size = a->size;
  bint_init_raw(x,size);
  x->sgn = a->sgn;
  int i,s,carry;
  int bytea,byteb;
  carry=0;
  for(i=0; i<size; i++){
    bytea = i<a->size? a->a[i]: 0;
    byteb = i<b->size? b->a[i]: 0;
    s = bytea-byteb-carry;
    if(s<0){
      s+=256;
      carry=1;
    }else{
      carry=0;
    }
    x->a[i]=s;
  }
  while(size>0 && x->a[size-1]==0) size--;
}

void bint_add(bint* a, bint* b, bint* y){
  if(a->sgn!=b->sgn){
    // printf("a: ");
    // long_print_hex(a);
    // printf("\n");
    // printf("b: ");
    // long_print_hex(b);
    // printf("\n");
    // printf("a>b: %i\n",gtabs(a,b));
    if(ltabs(a,b)){
      sub_from_greater(b,a,y);
      return;
    }else if(gtabs(a,b)){
      sub_from_greater(a,b,y);
      return;
    }else{
      bint_set_value(y,0);
      return;
    }
  }
  int size;
  if(a->size>b->size){
    size = a->size;
  }else{
    size = b->size;
  }
  bint_init_raw(y,size+1);
  y->sgn = a->sgn;
  int i,s,carry;
  int bytea,byteb;
  carry=0;
  for(i=0; i<size; i++){
    bytea = i<a->size? a->a[i]: 0;
    byteb = i<b->size? b->a[i]: 0;
    s = bytea+byteb+carry;
    if(s>255){
      carry=1;
      s-=256;
    }else{
      carry=0;
    }
    y->a[i] = s;
  }
  if(carry==1){
    y->a[size]=1;
  }else{
    y->size--;
  }
}

void bint_sub(bint* a, bint* b, bint* y){
  b->sgn=-b->sgn;
  bint_add(a,b,y);
  b->sgn=-b->sgn;
}

void bint_add_int(bint* a, int b, bint* y){
  bint bL; bL.a=NULL;
  bint_set_value(&bL,b);
  bint_add(a,&bL,y);
  bint_delete(&bL);
}

static
void lshiftw(bint* x, long n){
  long i;
  long size=x->size;
  unsigned char* a;
  if(size+n>x->capacity){
    a = mf_malloc((size+n)*2);
    for(i=0; i<size; i++){
      a[i]=x->a[i];
    }
    if(x->a!=NULL){
      mf_free(x->a);
    }
    x->a=a;
    x->capacity=(size+n)*2;
  }else{
    a = x->a;
  }
  for(i=size-1; i>=0; i--){
    a[i+n]=a[i];
  }
  for(i=0; i<n; i++){
    a[i]=0;
  }
  x->size=size+n;
}

static
void rshiftw(bint* x, long n){
  long size=x->size;
  unsigned char* a = x->a;
  long i;
  for(i=n; i<size; i++){
    a[i-n]=a[i];
  }
  x->size = size-n<0? 0: size-n;
}

static
void lshift1(bint* x){
  int i,byte,carry;
  if(x->size==x->capacity){
    unsigned char* a = mf_malloc(x->size*2);
    for(i=0; i<x->size; i++){
      a[i]=x->a[i];
    }
    if(x->a!=NULL){
      mf_free(x->a);
    }
    x->a=a;
    x->capacity=x->size*2;
  }
  carry=0;
  for(i=0; i<x->size; i++){
    byte = x->a[i];
    x->a[i]=(byte<<1)|carry;
    carry = byte>>7;
  }
  if(carry==1){
    x->a[x->size]=1;
    x->size++;
  }
}

static
void lshift(bint* x, int n){
  int i;
  for(i=0; i<n; i++){
    lshift1(x);
  }
}

static
void rshift1(bint* x){
  int i,byte,carry;
  carry=0;
  for(i=x->size-1; i>=0; i--){
    byte = x->a[i];
    x->a[i] = (byte>>1)|carry;
    carry = (byte&1)<<7;
  }
  if(x->size>0 && x->a[x->size-1]==0){
    x->size--;
  }
}

static
void rshift(bint* x, int n){
  int i;
  for(i=0; i<n; i++){
    rshift1(x);
  }
}

/*
Object* fshift(Object** v, int n){
  if(n!=2){
    arg_error("Error: lshift(x,n) takes exactly two arguments.");
    return 0;
  }
  if(v[1]->type!=id_long || v[2]->type!=id_int){
    type_error("Type error in lshift(x,n).");
    return 0;
  }
  mt_long* x = (mt_long*)v[1];
  Int* k =  (Int*)v[2];
  lshift(x,k->value);
  null->refcount++;
  return null;
}
*/

static
int odd(bint* x){
  if(x->size==0) return 0;
  return x->a[0]&1;
}

static
int even(bint* x){
  return !odd(x);
}

static
int is_zero(bint* x){
  if(x->size==0) return 1;
  return x->size==1 && x->a[0]==0;
}

static
void resize(bint* x, int size){
  int i;
  unsigned char* a;
  if(size>x->size){
    if(size>x->capacity){
      a = mf_malloc(size);
      for(i=0; i<x->size; i++){
        a[i]=x->a[i];
      }
      for(i=x->size; i<size; i++){
        a[i]=0;
      }
      if(x->a!=NULL) mf_free(x->a);
      x->a=a;
      x->size=size;
    }else{
      for(i=x->size; i<size; i++){
        x->a[i]=0;
        x->size=size;
      }
    }
  }else{
    x->size=size;
  }
}

static
unsigned char* bint_mpy_by_n(bint* a, int n, unsigned char* result){
  unsigned long carry=0;
  unsigned long tmp;
  long size=a->size;
  long i;
  for(i=0; i<size; i++){
    tmp = carry + a->a[i]*n + *result;
    carry = tmp/256;
    *result++ = tmp%256;
  }
  if(carry){
    *result++ = carry;
  }
  return result;
}

static
void bint_mpy_standard(bint* a, bint* b, bint* y){
  normalize(a);
  if(a->size==0){
    y->size=0;
    if(y->a==NULL) y->capacity=0;
    y->sgn=1;
    return;
  }
  normalize(b);
  long size = b->size;
  long max = a->size + size;
  bint_init_raw(y,max);
  memset(y->a,0,max);
  unsigned char* end=y->a;
  long i;
  for(i=0; i<size; i++){
    end = bint_mpy_by_n(a, b->a[i], y->a+i);
  }
  y->size = end-y->a;
  y->sgn=a->sgn*b->sgn;
}

static
void bint_mpy2(bint* a, bint* b, bint* y){
  bint a1; a1.a=NULL;
  bint b1; b1.a=NULL;
  bint x; x.a=NULL;
  bint_copy(a,&a1);
  bint_copy(b,&b1);
  bint_set_value(y,0);
  while(!is_zero(&b1)){
    if(odd(&b1)){
      bint_copy(y,&x);
      bint_add(&x,&a1,y);
    }
    lshift1(&a1);
    rshift1(&b1);
  }
  bint_delete(&a1);
  bint_delete(&b1);
  bint_delete(&x);
}

#define KARATSUBA

#ifdef KARATSUBA
  #define bint_mpy bint_mpy_karatsuba
#else
  #define bint_mpy bint_mpy_standard
#endif

static
void bint_mpy_karatsuba(bint* a, bint* b, bint* p){
  if(a->size<600 || b->size<600){
    bint_mpy_standard(a,b,p);
    return;
  }
  bint XH; XH.a=NULL; bint XL; XL.a=NULL;
  bint YH; YH.a=NULL; bint YL; YL.a=NULL;
  bint_copy(a,&XH); bint_copy(a,&XL);
  bint_copy(b,&YH); bint_copy(b,&YL);
  
  int m = a->size;
  if(b->size>m) m=b->size;
  m=m/2;
  // int n=8*m;
  resize(&XL,m);
  resize(&YL,m);
  rshiftw(&XH,m);
  rshiftw(&YH,m);
  bint PH; PH.a=NULL;
  bint_mpy_karatsuba(&XH,&YH,&PH);
  bint PL; PL.a=NULL;
  bint_mpy_karatsuba(&XL,&YL,&PL);
  // printf("n: %i",n);
  // printf("\nXH: "); bint_print(&XH);
  // printf("\nXL: "); bint_print(&XL);
  // printf("\nYH: "); bint_print(&YH);
  // printf("\nYL: "); bint_print(&YL);
  // printf("\nPH: "); bint_print(&PH);
  // printf("\nPL: "); bint_print(&PL);

  bint t; t.a=NULL;
  bint_copy(&XH,&t);
  bint_add(&t,&XL,&XH);
  bint_copy(&YH,&t);
  bint_add(&t,&YL,&YH);
  bint PHL; PHL.a=NULL;
  bint_mpy_karatsuba(&XH,&YH,&PHL);
  // printf("\nPHL: "); bint_print(&PHL);

  bint_add(&PH,&PL,&t);
  bint_sub(&PHL,&t,&XH);
  // printf("\nD: "); bint_print(&XH);
  lshiftw(&XH,m);
  // printf("\nD*2^n: "); bint_print(&XH);
  lshiftw(&PH,2*m);
  // printf("\nPH*2^(2n): "); bint_print(&PL);
  bint_add(&PL,&XH,&t);
  bint_add(&t,&PH,p);
  // printf("\n");

  bint_delete(&XH); bint_delete(&XL);
  bint_delete(&XH); bint_delete(&YL);
  bint_delete(&PH); bint_delete(&PL);
  bint_delete(&PHL); bint_delete(&t);
}

void bint_mpy_int(bint* a, int b, bint* y){
  bint bL; bL.a=NULL;
  bint_set_value(&bL,b);
  bint_mpy(a,&bL,y);
  bint_delete(&bL);
}

static
int get_bit(bint* x, int i){
  int byte,bit;
  byte=i/8; bit=i%8;
  if(byte>=x->size)return 0;
  return (x->a[byte]>>bit)&1;
}

static
void set_bit(bint* x, int i, int a){
  int byte, bit, k;
  byte=i/8; bit=i%8;
  if(byte>=x->size) resize(x,byte+1);
  if(a==0){
    x->a[byte] &= ~(1<<bit);
  }else{
    x->a[byte] |= 1<<bit;
  }
  while(x->size>0 && x->a[x->size-1]==0){
    x->size--;
  }
}

static
int last_bit(bint* x){
  if(is_zero(x)) return -1;
  int i = 8*x->size;
  while(i>0 && get_bit(x,i)==0){
    i--;
  }
  return i;
}

int division_by_zero;
void bint_divmod(bint* N, bint* D, bint* Q, bint* R){
  if(is_zero(D)){
    division_by_zero=1;
    return;
  }
  bint_set_value(Q,0);
  bint_set_value(R,0);
  bint t; t.a=NULL;
  int i;
  i=last_bit(N);
  while(i>=0){
    lshift1(R);
    set_bit(R,0,get_bit(N,i));
    if(bint_ge(R,D)){
      bint_copy(R,&t);
      bint_sub(&t,D,R);
      set_bit(Q,i,1);
    }
    i--;
  }
  bint_delete(&t);
}

void bint_div(bint* a, bint* b, bint* y){
  bint R; R.a=NULL;
  bint_divmod(a,b,y,&R);
  bint_delete(&R);
}

void bint_mod(bint* a, bint* b, bint* y){
  bint D; D.a=NULL;
  bint_divmod(a,b,&D,y);
  bint_delete(&D);
}

void bint_pow(bint* a, bint* b, bint* y){
  bint x; x.a=NULL;
  bint n; n.a=NULL;
  bint_copy(a,&x);
  bint_copy(b,&n);
  bint_set_value(y,1);
  bint t; t.a=NULL;
  bint one; one.a=NULL;
  bint_set_value(&one,1);
  while(!is_zero(&n)){
    if(even(&n)){
      rshift1(&n);
      bint_copy(&x,&t);
      bint_mpy(&t,&t,&x);
    }else{
      bint_copy(&n,&t);
      bint_sub(&t,&one,&n);
      bint_copy(y,&t);
      bint_mpy(&t,&x,y);
    }
  }
  bint_delete(&x);
  bint_delete(&n);
  bint_delete(&one);
  bint_delete(&t);
}

void bint_powmod(bint* a, bint* b, bint* m, bint* y){
  bint x; x.a=NULL;
  bint n; n.a=NULL;
  bint_mod(a,m,&x);
  bint_copy(b,&n);
  bint_set_value(y,1);
  bint t; t.a=NULL;
  bint one; one.a=NULL;
  bint_set_value(&one,1);
  while(!is_zero(&n)){
    if(even(&n)){
      rshift1(&n);
      bint_copy(&x,&t);
      bint_mpy(&t,&t,&x);
      bint_copy(&x,&t);
      bint_mod(&t,m,&x);
    }else{
      bint_copy(&n,&t);
      bint_sub(&t,&one,&n);
      bint_copy(y,&t);
      bint_mpy(&t,&x,y);
      bint_copy(y,&t);
      bint_mod(&t,m,y);
    }
  }
  bint_delete(&x);
  bint_delete(&n);
  bint_delete(&one);
  bint_delete(&t);
}

static
void bint_set10pow(bint* x, int n){
  bint e; e.a=NULL;
  bint b; b.a=NULL;
  bint_set_value(&e,n);
  bint_set_value(&b,10);
  bint_pow(&b,&e,x);
  bint_delete(&e);
  bint_delete(&b);
}

static
void reverse_byte_buffer(unsigned char* a, int size){
  int i;
  int n=size/2;
  unsigned char t;
  for(i=0; i<n; i++){
    t=a[i];
    a[i]=a[size-i-1];
    a[size-i-1]=t;
  }
}

void bint_print(bint* x);

mt_string* bint_to_string(bint* x){
  if(is_zero(x)){
    return mf_str_new(1,"0");
  }
  mt_vec v;
  mf_vec_init(&v,sizeof(unsigned char));
  unsigned char byte;
  bint a; a.a=NULL;
  bint b; b.a=NULL;
  bint_copy(x,&a);
  bint_set_value(&b,10);
  if(a.sgn<0){
    byte='-';
    mf_vec_push(&v,&byte);
    a.sgn=1;
  }
  bint Q; Q.a=NULL;
  bint R; R.a=NULL;
  while(!is_zero(&a)){
    bint_divmod(&a,&b,&Q,&R);
    if(is_zero(&R)){
      byte=48;
      mf_vec_push(&v,&byte);
    }else{
      byte = 48+R.a[0];
      mf_vec_push(&v,&byte);
    }
    bint_copy(&Q,&a);
  }
  bint_delete(&a);
  bint_delete(&b);
  bint_delete(&Q);
  bint_delete(&R);
  if(x->sgn<0){
    reverse_byte_buffer(v.a+1,v.size-1);
  }else{
    reverse_byte_buffer(v.a,v.size);
  }
  mt_string* s = mf_str_new(v.size,(char*)v.a);
  mf_vec_delete(&v);
  return s;
} 

static
void to_buffer(mt_vec* v, bint* R, int n){
  int i,x;
  x=0;
  // printf("base 256 size of remainder: %i\n",R->size);
  // printf("remainder: "); bint_print(R);
  // printf("\n");
  if(!is_zero(R)){
    for(i=R->size-1; i>=0; i--){
      x=256*x+R->a[i];
    }
  }
  int byte;
  for(i=0; i<n; i++){
    byte = 48+x%10;
    mf_vec_push(v,&byte);
    x=x/10;
  }
}

static
long bint_to_int(bint* x){
  if(is_zero(x)){
    return 0;
  }else{
    long y=0;
    int i;
    for(i=x->size-1; i>=0; i--){
      y=256*y+x->a[i];
    }
    return y;
  }
}

static
void int_to_buffer(mt_vec* v, int x, int fill){
  int i=0,byte;
  while(x>0){
    byte = x%10+48;
    mf_vec_push(v,&byte);
    x=x/10;
    i++;
  }
  byte=48;
  while(i<fill){
    mf_vec_push(v,&byte);
    i++;
  }
}

static
void to_buffer_rec(mt_vec* v, bint* x, int fill){
  if(is_zero(x)){
    int_to_buffer(v,0,fill);
    return;
  }
  if(x->size<4){
    long y = bint_to_int(x);
    int_to_buffer(v,y,fill);
    return;
  }
  bint p; p.a=NULL;
  // (2.4*size)/2 is approximately one.
  // So we simply do nothing with size.
  bint_set10pow(&p,x->size);
  bint Q; Q.a=NULL;
  bint R; R.a=NULL;
  bint_divmod(x,&p,&Q,&R);
  to_buffer_rec(v,&R,x->size);
  to_buffer_rec(v,&Q,fill-x->size);
  bint_delete(&p);
  bint_delete(&Q);
  bint_delete(&R);
}

mt_string* bint_to_string_fast(bint* x){
  if(is_zero(x)){
    return mf_str_new(1,"0");
  }
  mt_vec v;
  mf_vec_init(&v,sizeof(unsigned char));
  unsigned char byte;
  bint a; a.a=NULL;
  bint b; b.a=NULL;
  bint_copy(x,&a);
  bint_set_value(&b,1000000000);
  if(a.sgn<0){
    byte='-';
    mf_vec_push(&v,&byte);
    a.sgn=1;
  }
  bint Q; Q.a=NULL;
  bint R; R.a=NULL;
  /*
  while(!is_zero(&a)){
    divmod(&a,&b,&Q,&R);
    to_buffer(&v,&R,9);
    bint_copy(&Q,&a);
  }
  // */
  to_buffer_rec(&v,&a,3*a.size);
  bint_delete(&a);
  bint_delete(&b);
  bint_delete(&Q);
  bint_delete(&R);
  while(v.size>0 && v.a[v.size-1]=='0'){
    v.size--;
  }
  if(x->sgn<0){
    reverse_byte_buffer(v.a+1,v.size-1);
  }else{
    reverse_byte_buffer(v.a,v.size);
  }
  mt_string* s = mf_str_new(v.size,(char*)v.a);
  mf_vec_delete(&v);
  return s;
}

mt_long* mf_string_to_long(mt_string* s){
  int i,d,sgn;
  if(s->size>0 && s->a[0]=='-'){
    i=1; sgn=-1;
  }else{
    i=0; sgn=1;
  }
  if(i+1<s->size && s->a[i]=='0' && s->a[i+1]=='x'){
    /*
    printf("Implementation error: cannot convert hex-literal to long.\n");
    exit(1);
    unsigned char* buffer = mf_malloc(s->size-i-2);
    int j;
    for(j=i+2; j<s->size; j++){
      buffer[j]=s->a[j];
    }
    mt_long* y = hex_to_long(buffer,s->size-i-2);
    mf_free(buffer);
    return y;
    */
  }
  mt_long* x = mf_int_to_long(0);
  mt_long* b = mf_int_to_long(10);
  bint t; t.a=NULL;
  while(i<s->size){
    d = s->a[i]-48;
    bint_mpy(&x->value,&b->value,&t);
    bint_add_int(&t,d,&x->value);
    i++;
  }
  bint_delete(&t);
  mf_long_delete(b);
  x->value.sgn=sgn;
  return x;
}

int mf_long_cmp(mt_long* a, mt_long* b){
  return bint_cmp(&a->value,&b->value);
}

int mf_long_cmp_si(mt_long* a, long b){
  bint bi; bi.a=NULL;
  bint_set_value(&bi,b);
  int t = bint_cmp(&a->value,&bi);
  bint_delete(&bi);
  return t;
}

int mf_long_lt(mt_long* a, mt_long* b){
  return bint_lt(&a->value,&b->value);
}

int mf_long_gt(mt_long* a, mt_long* b){
  return bint_gt(&a->value,&b->value);
}

int mf_long_le(mt_long* a, mt_long* b){
  return bint_le(&a->value,&b->value);
}

int mf_long_ge(mt_long* a, mt_long* b){
  return bint_ge(&a->value,&b->value);
}

int mf_long_eq(mt_long* a, mt_long* b){
  return bint_eq(&a->value, &b->value);
}

int mf_long_ne(mt_long* a, mt_long* b){
  return bint_neq(&a->value, &b->value);
}

mt_long* mf_long_copy(mt_long* x){
  mt_long* y = long_new();
  bint_copy(&x->value, &y->value);
  return y;
}

mt_long* mf_long_neg(mt_long* x){
  mt_long* y = long_new();
  bint_copy(&x->value, &y->value);
  y->value.sgn=-y->value.sgn;
  return y;
}

mt_long* mf_long_add(mt_long *a, mt_long* b){
  mt_long* y = long_new();
  bint_add(&a->value,&b->value,&y->value);
  return y;
}

mt_long* mf_long_sub(mt_long* a, mt_long* b){
  mt_long* y = long_new();
  bint_sub(&a->value,&b->value,&y->value);
  return y;
}

mt_long* mf_long_mpy(mt_long* a, mt_long* b){
  mt_long* y = long_new();
  bint_mpy(&a->value,&b->value,&y->value);
  return y;
}

mt_long* mf_long_div(mt_long* a, mt_long* b){
  mt_long* y = long_new();
  division_by_zero=0;
  bint_div(&a->value,&b->value,&y->value);
  if(division_by_zero){
    mf_long_delete(y);
    mf_value_error("Value error: division by zero.");
    return 0;
  }else{
    return y;
  }
}

mt_long* mf_long_mod(mt_long* a, mt_long* b){
  mt_long* y = long_new();
  division_by_zero=0;
  bint_mod(&a->value,&b->value,&y->value);
  if(division_by_zero){
    mf_long_delete(y);
    mf_value_error("Value error: division by zero.");
    return 0;
  }else{
    return y;
  }
}

mt_long* mf_long_pow(mt_long* a, mt_long* b){
  mt_long* y = long_new();
  bint_pow(&a->value,&b->value,&y->value);
  return y;
}

void bint_print(bint* x){
  mt_string* s = bint_to_string(x);
  mf_print_string(s->size,s->a);
  mf_str_dec_refcount(s);
}

void mf_long_print(mt_long* x){
  mt_string* s = bint_to_string_fast(&x->value);
  mf_print_string(s->size,s->a);
  mf_str_dec_refcount(s);
}

mt_string* mf_long_to_string(mt_long* x, int base){
  if(base==10){
    return bint_to_string_fast(&x->value);
  }else{
    abort();
  }
}

mt_long* mf_long_powmod(mt_long* x, mt_long* n, mt_long* m){
  mt_long* y = long_new();
  bint_powmod(&x->value,&n->value,&m->value,&y->value);
  return y;
}

mt_long* mf_long_abs(mt_long* x){
  mt_long* y = long_new();
  bint_copy(&x->value,&y->value);
  if(y->value.sgn<0){
    y->value.sgn=1;
  }
  return y;
}

int mf_long_sgn(mt_long* x){
  if(is_zero(&x->value)){
    return 0;
  }else{
    return x->value.sgn;
  }
}

uint32_t mf_long_hash(mt_long* x){
  if(x->value.size==0){
    return 0;
  }else if(x->value.size<2){
    return x->value.a[0];
  }else{
    return ((uint32_t)x->value.a[1])*256+x->value.a[0];
  }
}


mt_list* mf_long_base(mt_long* x, int b){
  bint Q; Q.a=NULL;
  bint R; R.a=NULL;
  bint B; B.a=NULL;
  bint_set_value(&B,b);
  mt_list* list = mf_raw_list(0);
  mt_object r;
  r.type=mv_int;
  while(!is_zero(&x->value)){
    bint_divmod(&x->value,&B,&Q,&R);
    r.value.i = bint_to_int(&R);
    mf_list_push(list,&r);
    bint_copy(&Q,&x->value);
  }
  return list;
}

// todo
int mf_long_isprime(mt_long* x){
  abort();
}

// todo
double mf_long_float(mt_long* x){
  abort();
}

