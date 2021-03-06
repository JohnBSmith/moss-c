
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <moss.h>
#include <modules/str.h>
#include <modules/bs.h>
#include <objects/string.h>
#include <objects/list.h>

mt_string* mf_list_to_string(mt_list* list);
mt_string* mf_long_to_string(mt_long* x, int base);
mt_string* mf_map_to_string(mt_map* m);

int mf_object_get_memory(mt_object* x, mt_object* a,
    long size, const char* id);
mt_bstring* mf_buffer_to_bstring(long size, const unsigned char* a);
double mf_float(mt_object* x, int* error);
void mf_type(mt_object* x, mt_object* a);

mt_string* mf_raw_string(long size){
    mt_string* s = mf_malloc(sizeof(mt_string)+size*sizeof(uint32_t));
    s->refcount = 1;
    s->size = size;
    return s;
}

mt_string* mf_str_new(long size, const char* a){
    mt_string* s = mf_raw_string(size);
    long i;
    for(i=0; i<size; i++){
        s->a[i] = a[i];
    }
    return s;
}

mt_string* mf_str_new_u32(long size, const uint32_t* a){
    mt_string* s = mf_raw_string(size);
    long i;
    for(i=0; i<size; i++){
        s->a[i] = a[i];
    }
    return s;
}

void mf_str_dec_refcount(mt_string* s){
    if(s->refcount==1){
        mf_free(s);
    }else{
        s->refcount--;
    }
}

mt_string* mf_str_add(mt_string* a, mt_string* b){
    long asize = a->size;
    long bsize = b->size;
    mt_string* x = mf_raw_string(asize+bsize);
    long i;
    for(i=0; i<asize; i++){
        x->a[i] = a->a[i];
    }
    for(i=0; i<bsize; i++){
        x->a[asize+i] = b->a[i];
    }
    return x;
}

mt_string* mf_str_mpy(mt_string* a, long n){
    long size = a->size;
    mt_string* x = mf_raw_string(n*size);
    long i,j,k;
    for(i=0; i<n; i++){
        k = i*size;
        for(j=0; j<size; j++){
            x->a[k+j] = a->a[j];
        }
    }
    return x;
}

mt_string* mf_cstr_to_str(const char* a){
    long size = strlen(a);
    mt_string* s = mf_raw_string(size);
    long i;
    for(i=0; i<size; i++){
        s->a[i] = a[i];
    }
    return s;
}

char* mf_str_to_cstr(mt_string* s){
    long size = s->size;
    unsigned char* a = mf_malloc(size+1);
    long i;
    for(i=0; i<size; i++){
        a[i] = s->a[i];
    }
    a[size] = 0;
    return (char*)a;
}

mt_string* mf_bu32_move_to_string(mt_bu32* b){
    unsigned long i,size;
    size = b->size;
    uint32_t* ba = mf_bu32_move(b);
    mt_string* s = mf_raw_string(size);
    uint32_t* a = s->a;
    for(i=0; i<size; i++){
        a[i] = ba[i];
    }
    mf_free(ba);
    return s;
}

static
void reverse(int size, char* a){
    int i;
    int m = size/2;
    char h;
    for(i=0; i<m; i++){
        h=a[i]; a[i]=a[size-1-i];
        a[size-1-i]=h;
    }
}

static
void base(char* buffer, long n, long base){
    int i = 0;
    while(n!=0){
        buffer[i] = n%base+'0';
        i++;
        n/=base;
    }
    reverse(i,buffer);
    buffer[i] = 0;
    i++;
}

static
mt_string* number_to_string(mt_object* x, mt_string* s, long n){
    long size = s->size;
    uint32_t* a = s->a;
    int i;
    int sign_space;
    char c;
    const char* format;
    if(size>0 && a[0]=='+'){
        sign_space = 2;
        i = 1;
    }else if(size>0 && a[0]=='-'){
        sign_space = 1;
        i = 1;
    }else{
        sign_space = 0;
        i = 0;
    }
    if(i<size){
        c = (char)a[i];
    }else{
        c = 'f';
    }
    char buffer[100];
    if(c=='b'){
        if(x->type!=mv_int){
            mf_type_error1("in str(x,'b',base): x is not an integer.",x);
            return NULL;
        }
        if(n<2){
            mf_value_error("Value error in str(x,'b',base): base<2.");
            return NULL;
        }
        base(buffer,x->value.i,n);
        puts(buffer);
        return mf_cstr_to_str(buffer);
    }
    int error=0;
    double t = mf_float(x,&error);
    if(error){
        mf_type_error1("in str(x,format,n): cannot convert x (type: %s) to float.",x);
        return NULL;
    }
    if(c=='f'){
        format = sign_space>1? "%+.*f": sign_space? "% .*f": "%.*f";
    }else if(c=='E'){
        format = sign_space>1? "%+.*E": sign_space? "% .*E": "%.*E";
    }else if(c=='e'){
        format = sign_space>1? "%+.*e": sign_space? "% .*e": "%.*e";
    }else if(c=='g'){
        format = sign_space>1? "%+.*g": sign_space? "% .*g": "%.*g";
    }else{
        format = sign_space>1? "%+.*g": sign_space? "% .*g": "%.*g";
    }
    if(n<0){n=0;}else if(n>20){n=20;}
    snprintf(buffer,100,format,(int)n,t);
    return mf_cstr_to_str(buffer);
}

static
void print_float(char* a, double x){
    int n = snprintf(a,100,"%.14g",x);
    if(isfinite(x) && strchr(a,'.')==NULL && strchr(a,'e')==NULL){
        a+=n;
        snprintf(a,10,".0");
    }
}

static
void print_complex(char* a, double re, double im){
    if(re==0){
        snprintf(a,200,"%.14gi",im);
    }else{
        if(im<0){
            snprintf(a,200,"%.14g-%.14gi",re,-im);
        }else{
            snprintf(a,200,"%.14g+%.14gi",re,im);
        }
    }
}

mt_string* mf_str(mt_object* x){
    mt_string* s;
    char a[200];
    mt_object prototype;
    mt_object f;
    mt_table* t;
    int e;

    switch(x->type){
    case mv_null:
        s = mf_cstr_to_str("null");
        return s;
    case mv_bool:
        s = mf_cstr_to_str(x->value.b? "true": "false");
        return s;
    case mv_int:
        snprintf(a,200,"%li",x->value.i);
        s = mf_cstr_to_str(a);
        return s;
    case mv_float:
        print_float(a,x->value.f);
        s = mf_cstr_to_str(a);
        return s;
    case mv_complex:
        print_complex(a,x->value.c.re,x->value.c.im);
        s = mf_cstr_to_str(a);
        return s;
    case mv_long:
        s = mf_long_to_string((mt_long*)x->value.p,10);
        return s;
    case mv_string:
        s = (mt_string*)x->value.p;
        s->refcount++;
        return s;
    case mv_bstring:
        s = mf_cstr_to_str("bstring");
        return s;
    case mv_list:
        s = mf_list_to_string((mt_list*)x->value.p);
        return s;
    case mv_map:
        s = mf_map_to_string((mt_map*)x->value.p);
        return s;
    case mv_function:
        s = mf_cstr_to_str("function");
        return s;
    case mv_table:
        t = (mt_table*)x->value.p;
        if(t->name){
            s = t->name;
            s->refcount++;
            return s;
        }
        goto Default;
    default: Default:
        mf_type(&prototype,x);
        e = mf_object_get_memory(&f,&prototype,6,"string");
        if(e){
            s = mf_cstr_to_str("object");
            mf_dec_refcount(&prototype);
            return s;
        }
        if(f.type!=mv_function){
            mf_type_error("Type error in str(x): x.string is not a function.");
            goto error;
        }
        mt_object y;
        if(mf_call((mt_function*)f.value.p,&y,0,x)){
            goto error;
        }
        if(y.type!=mv_string){
            mf_type_error("Type error in str(x): return value of x.string() is not a string.");
            mf_dec_refcount(&y);
            goto error;
        }
        mf_dec_refcount(&prototype);
        return (mt_string*)y.value.p;

        error:
        mf_dec_refcount(&prototype);
        return NULL;
    }
}

mt_string* mf_repr(mt_object* x){
    return mf_str(x);
}

int mf_fstr(mt_object* x, int argc, mt_object* v){
    mt_string* s;
    if(argc==3){
        if(v[2].type!=mv_string){
            mf_type_error1("in str(x,format,n): format (type: %s) is not a string.",&v[2]);
            return 1;
        }
        if(v[3].type!=mv_int){
            mf_type_error1("in str(x,format,n): n (type: %s) is not an integer.",&v[3]);
            return 1;
        }
        s = number_to_string(
            &v[1], (mt_string*)v[2].value.p, v[3].value.i
        );
    }else if(argc==1){
        s = mf_str(&v[1]);
    }else{
        mf_argc_error(argc,1,1,"str");
        return 1;
    }
    if(s==NULL) return 1;
    x->type = mv_string;
    x->value.p = (mt_basic*)s;
    return 0;
}

int mf_str_eq(mt_string* s1, mt_string* s2){
    if(s1->size!=s2->size) return 0;
    long size = s1->size;
    uint32_t* a = s1->a;
    uint32_t* b = s2->a;
    long i;
    for(i=0; i<size; i++){
        if(a[i]!=b[i]) return 0;
    }
    return 1;
}

int mf_str_cmpmem(mt_string* s, long size2, const char* a){
    long size1 = s->size;
    long size = size1<size2? size1: size2;
    long i;
    for(i=0; i<size; i++){
        if((uint32_t)a[i]<s->a[i]) return -1;
        if((uint32_t)a[i]>s->a[i]) return 1;
    }
    if(size1<size2) return -1;
    if(size1>size2) return 1;
    return 0;
}

int mf_str_cmp(mt_string* s1, mt_string* s2){
    long size1 = s1->size;
    long size2 = s2->size;
    long size = size1<size2? size1: size2;
    uint32_t* a = s1->a;
    uint32_t* b = s2->a;
    long i;
    for(i=0; i<size; i++){
        if(a[i]<b[i]) return -1;
        if(a[i]>b[i]) return 1;
    }
    if(size1<size2) return -1;
    if(size1>size2) return 1;
    return 0;
}

int mf_uisalpha(unsigned long n){
    if(n>127){
        if(192<=n && n<=214) return 1;
        if(216<=n && n<=246) return 1;
        if(248<=n && n<=687) return 1;
        return 0;
    }else{
        return isalpha((int)n);
    }
}

int mf_uisdigit(unsigned long n){
    return 48<=n && n<=57;
}

int mf_uisspace(unsigned long n){
    if(n<128){
        return isspace((int)n);
    }else{
        return 0;
    }
}

static
int uislower(unsigned long n){
    if(n<128){
        return islower((int)n);
    }else{
        return 0;
    }
}

static
int uisupper(unsigned long n){
    if(n<128){
        return isupper((int)n);
    }else{
        return 0;
    }
}

static
int str_upper(mt_object* x, int argc, mt_object* v){
    if(argc!=0){
        mf_argc_error(argc,0,0,"upper");
        return 1;
    }
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.upper(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long size = s->size;
    mt_string* s2 = mf_raw_string(size);
    uint32_t* a = s->a;
    uint32_t* b = s2->a;
    long i;
    unsigned long c;
    for(i=0; i<size; i++){
        c = a[i];
        if(c<256){
            b[i] = toupper(c);
        }else{
            b[i] = c;
        }
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)s2;
    return 0;
}

static
int str_lower(mt_object* x, int argc, mt_object* v){
    if(argc!=0){
        mf_argc_error(argc,0,0,"lower");
        return 1;
    }
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.lower(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long size = s->size;
    mt_string* s2 = mf_raw_string(size);
    uint32_t* a = s->a;
    uint32_t* b = s2->a;
    long i;
    unsigned long c;
    for(i=0; i<size; i++){
        c = a[i];
        if(c<256){
            b[i] = tolower(c);
        }else{
            b[i] = c;
        }
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)s2;
    return 0;
}

static
int str_isalpha(mt_object* x, int argc, mt_object* v){
    if(argc!=0){
        mf_argc_error(argc,0,0,"isalpha");
        return 1;
    }
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.isalpha(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long size = s->size;
    long i;
    for(i=0; i<size; i++){
        if(!mf_uisalpha(s->a[i])){
            x->type = mv_bool;
            x->value.b = 0;
            return 0;
        }
    }
    x->type = mv_bool;
    x->value.b = 1;
    return 0;
}

static
int isdigit_base(unsigned long c, unsigned long b){
    if(b>10){
        if('a'<=c && c<='z'){
            return c<'a'+b-10;
        }else if('A'<=c && c<='Z'){
            return c<'A'+b-10;
        }else{
            return '0'<=c && c<='9';
        }
    }else{
        return '0'<=c && c<'0'+b;
    }
}

static
int str_isdigit(mt_object* x, int argc, mt_object* v){
    unsigned long base;
    if(argc==0){
        base = 10;
    }else if(argc==1){
        if(v[1].type!=mv_int){
            mf_type_error("Type error in s.isdigit(n): no is not an integer.");
            return 1;
        }
        base = v[1].value.i;
    }else{
        mf_argc_error(argc,0,0,"isdigit");
        return 1;
    }
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.isdigit(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long size = s->size;
    long i;
    if(base==10){
        for(i=0; i<size; i++){
            if(!mf_uisdigit(s->a[i])){
                x->type = mv_bool;
                x->value.b = 0;
                return 0;
            }
        }
    }else{
        for(i=0; i<size; i++){
            if(!isdigit_base(s->a[i],base)){
                x->type = mv_bool;
                x->value.b = 0;
                return 0;
            }
        }  
    }
    x->type = mv_bool;
    x->value.b = 1;
    return 0;
}

static
int str_isalnum(mt_object* x, int argc, mt_object* v){
    if(argc!=0){
        mf_argc_error(argc,0,0,"isalnum");
        return 1;
    }
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.isalnum(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long size = s->size;
    long i;
    unsigned long c;
    for(i=0; i<size; i++){
        c = s->a[i];
        if(!mf_uisalpha(c) && !mf_uisdigit(c)){
            x->type = mv_bool;
            x->value.b = 0;
            return 0;
        }
    }
    x->type = mv_bool;
    x->value.b = 1;
    return 0;
}

static
int str_isspace(mt_object* x, int argc, mt_object* v){
    if(argc!=0){
        mf_argc_error(argc,0,0,"isspace");
        return 1;
    }
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.isspace(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long size = s->size;
    long i;
    for(i=0; i<size; i++){
        if(!mf_uisspace(s->a[i])){
            x->type = mv_bool;
            x->value.b = 0;
            return 0;
        }
    }
    x->type = mv_bool;
    x->value.b = 1;
    return 0;
}

int str_isupper(mt_object* x, int argc, mt_object* v){
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.isupper(): s is not a string.");
        return 1;
    }
    if(argc!=0){
        mf_argc_error(argc,0,0,"isupper");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long i;
    for(i=0; i<s->size; i++){
        if(!uisupper(s->a[i])){
            x->type = mv_bool;
            x->value.b = 0;
            return 0;
        }
    }
    x->type = mv_bool;
    x->value.b = 1;
    return 0;
}

int str_islower(mt_object* x, int argc, mt_object* v){
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.islower(): s is not a string.");
        return 1;
    }
    if(argc!=0){
        mf_argc_error(argc,0,0,"islower");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long i;
    for(i=0; i<s->size; i++){
        if(!uislower(s->a[i])){
            x->type = mv_bool;
            x->value.b = 0;
            return 0;
        }
    }
    x->type=mv_bool;
    x->value.b=1;
    return 0;
}

int mf_str_to_int(mt_object* x, mt_string* s){
    uint32_t* a = (uint32_t*)s->a;
    long size = s->size;
    long i=0, y=0, sgn=1;
    while(i<size && mf_uisspace(a[i])) i++;
    if(i<size && a[i]=='-'){
        sgn = -1;
        i++;
    }
    while(i<size){
        if(mf_uisdigit(a[i])){
            y = 10*y+a[i]-'0';
            i++;
        }else if(mf_uisspace(a[i])){
            i++;
        }else{
            mf_type_error("Error in int(x): invalid integer literal.");
            return 1;
        }
    }
    x->type = mv_int;
    x->value.i = sgn*y;
    return 0;
}

mt_string* mf_str_decode_utf8(long size, const unsigned char* a){
    mt_str buffer;
    mf_decode_utf8(&buffer,size,a);
    mt_string* s = mf_str_new_u32(buffer.size,buffer.a);
    mf_free(buffer.a);
    return s;
}

int str_rjust(mt_object* x, int argc, mt_object* v){
    long i,j,n;
    unsigned long fillchar;
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.rjust(n): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    if(argc==2){
        if(v[2].type!=mv_string){
            mf_type_error("Type error in s.rjust(n,fillchar): fillchar is not a string.");
            return 1;
        }
        mt_string* fs = (mt_string*)v[2].value.p;
        if(fs->size==0){
            fillchar=' ';
        }else{
            fillchar=fs->a[0];
        }
    }else if(argc==1){
        fillchar=' ';
    }else{
        mf_argc_error(argc,1,2,"rjust");
        return 0;
    }
    if(v[1].type!=mv_int){
        mf_type_error("Type error in s.rjust(n): n is not an integer.");
        return 1;
    }
    n=v[1].value.i;
    if(n<s->size){
        s->refcount++;
        x->type = mv_string;
        x->value.p = (mt_basic*)s;
        return 0;
    }
    mt_string* s2 = mf_raw_string(n);
    for(i=0; i<n; i++){
        s2->a[i] = fillchar;
    }
    j=1;
    for(i=s->size-1; i>=0; i--){
        if(n-j>=0){
            s2->a[n-j] = s->a[i];
        }
        j++;
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)s2;
    return 0;
}

int str_ljust(mt_object* x, int argc, mt_object* v){
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.ljust(n): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    unsigned long fillchar;
    if(argc==2){
        if(v[2].type!=mv_string){
            mf_type_error("Type error in s.rjust(n,fillchar): fillchar is not a string.");
            return 1;
        }
        mt_string* fs = (mt_string*)v[2].value.p;
        if(fs->size==0){
            fillchar=' ';
        }else{
            fillchar=fs->a[0];
        }
    }else if(argc==1){
        fillchar=' ';
    }else{
        mf_argc_error(argc,1,2,"ljust");
        return 0;
    }
    if(v[1].type!=mv_int){
        mf_type_error("Type error in s.ljust(n): n is not an integer.");
        return 0;
    }
    long n = v[1].value.i;
    if(n<s->size){
        s->refcount++;
        x->type = mv_string;
        x->value.p = (mt_basic*)s;
        return 0;
    }
    mt_string* s2 = mf_raw_string(n);
    long i;
    for(i=0; i<s->size; i++){
        s2->a[i] = s->a[i];
    }
    for(i=s->size; i<n; i++){
        s2->a[i] = fillchar;
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)s2;
    return 0;
}

int str_center(mt_object* x, int argc, mt_object* v){
    long i,n,p1,p2,index;
    unsigned long fillchar;
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.center(n): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    if(argc==2){
        if(v[2].type!=mv_string){
            mf_type_error("Type error in s.center(n,fillchar): fillchar is not a string.");
            return 1;
        }
        mt_string* fs = (mt_string*)v[2].value.p;
        if(fs->size==0){
            fillchar=' ';
        }else{
            fillchar=fs->a[0];
        }
    }else if(argc==1){
        fillchar=' ';
    }else{
        mf_argc_error(argc,1,2,"center");
        return 1;
    }
    if(v[1].type!=mv_int){
        mf_type_error("Type error in s.center(n): n is not an integer.");
        return 1;
    }
    n=v[1].value.i;
    if(n<s->size){
        s->refcount++;
        x->type = mv_string;
        x->value.p = (mt_basic*)s;
        return 0;
    }
    p1 = (n-s->size)/2;
    p2 = n-s->size-p1;
    mt_string* s2 = mf_raw_string(n);
    for(i=0; i<p1; i++){
        if(i>=0 && i<s2->size){
            s2->a[i] = fillchar;
        }
    }
    for(i=0; i<s->size; i++){
        index = p1+i;
        if(index>=0 && index<s2->size){
            s2->a[index] = s->a[i];
        }
    }
    for(i=0; i<p2; i++){
        index = p1+s->size+i;
        if(index>=0 && index<s2->size){
            s2->a[index] = fillchar;
        }
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)s2;
    return 0;
}

static
int char_in(unsigned long c, long size, uint32_t* a){
    long i;
    for(i=0; i<size; i++){
        if(c==a[i]) return 1;
    }
    return 0;
}

static
int str_ltrim(mt_object* x, int argc, mt_object* v){
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.ltrim(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    mt_string* st;
    long i,j;
    long size = s->size;
    if(argc==0){
        for(i=0; i<size; i++){
            if(!mf_uisspace(s->a[i])) break;
        }
    }else if(argc==1){
        if(v[1].type!=mv_string){
            mf_type_error("Type error in s.ltrim(chars): chars is not a string.");
            return 1;
        }
        mt_string* sc = (mt_string*)v[1].value.p;
        for(i=0; i<size; i++){
            if(!char_in(s->a[i],sc->size,sc->a)) break;
        }
    }else{
        mf_argc_error(argc,0,1,"ltrim");
        return 1;
    }
    st = mf_raw_string(size-i);
    for(j=i; j<size; j++){
        st->a[j-i] = s->a[j];
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)st;
    return 0;
}

static
int str_rtrim(mt_object* x, int argc, mt_object* v){
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.rtrim(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    mt_string* st;
    long i,j;
    long size = s->size;
    if(argc==0){
        for(i=size-1; i>=0; i--){
            if(!mf_uisspace(s->a[i])) break;
        }
    }else if(argc==1){
        if(v[1].type!=mv_string){
            mf_type_error("Type error in s.rtrim(chars): chars is not a string.");
            return 1;
        }
        mt_string* sc = (mt_string*)v[1].value.p;
        for(i=size-1; i>=0; i--){
            if(!char_in(s->a[i],sc->size,sc->a)) break;
        }
    }else{
        mf_argc_error(argc,0,1,"rtrim");
        return 1;
    }
    st = mf_raw_string(i+1);
    for(j=0; j<=i; j++){
        st->a[j] = s->a[j];
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)st;
    return 0;
}

static
int str_trim(mt_object* x, int argc, mt_object* v){
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.trim(): s is not a string.");
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    long size = s->size;
    if(size==0) goto empty_string;
    long i,j,k;
    if(argc==0){
        i=0;
        while(i<size){
            if(!mf_uisspace(s->a[i])) break;
            i++;
        }
        if(i==size) goto empty_string;
        j = size-1;
        while(1){
            if(!mf_uisspace(s->a[j])) break;
            j--;
        }
    }else if(argc==1){
        if(v[1].type!=mv_string){
            mf_type_error("Type error in s.trim(chars): chars is not a string.");
            return 1;
        }
        mt_string* sc = (mt_string*)v[1].value.p;
        i=0;
        while(i<size){
            if(!char_in(s->a[i],sc->size,sc->a)) break;
            i++;
        }
        if(i==size) goto empty_string;
        j = size-1;
        while(1){
            if(!char_in(s->a[j],sc->size,sc->a)) break;
            j--;
        }
    }
    long stsize = j-i+1;
    mt_string* st = mf_raw_string(stsize);
    for(k=0; k<stsize; k++){
        st->a[k] = s->a[i+k];
    }
    x->type = mv_string;
    x->value.p = (mt_basic*)st;
    return 0;
empty_string:
    x->type = mv_string;
    x->value.p = (mt_basic*)mf_raw_string(0);
    return 0;
}


mt_string* mf_str_slice(mt_string* s, mt_range* r){
    long a,b,step;
    long size = s->size;
    if(r->step.type==mv_null){
        step = 1;
    }else if(r->step.type==mv_int){
        step = r->step.value.i;
        if(step==0){
            mf_value_error("Value error in s[a..b:step]: step must not be zero.");
            return NULL;
        }
    }else{
        mf_type_error("Type error in s[a..b:step]: step is not an integer.");
        return NULL;
    }
    if(r->a.type==mv_int){
        a = r->a.value.i;
    }else if(r->a.type==mv_null){
        a = step<0?size-1:0;
    }else{
        mf_type_error("Type error in s[a..b]: a is not an integer.");
        return NULL;
    }
    if(r->b.type==mv_int){
        b = r->b.value.i;
    }else if(r->b.type==mv_null){
        b = step<0?0:size-1;
    }else{
        mf_type_error("Type error in s[a..b]: b is not an integer.");
        return NULL;
    }
    if(a<0){
        a+=size;
        if(a<0) a = 0;
    }else if(a>=size){
        a = size;
    }
    if(b<0){
        if(b==-1){
            return mf_raw_string(0);
        }
        b+=size;
        if(b<0) b=-1;
    }else if(b>=size){
        b = size-1;
    }
    long n = 1+(b-a)/step;
    // ^SECURITY: overflow
    mt_string* sr = mf_raw_string(n<0?0:n);
    long i;
    for(i=0; i<n; i++){
        sr->a[i] = s->a[a+i*step];
    }
    return sr;
}

int mf_str_get(mt_object* x, mt_string* s, mt_object* key){
    if(key->type==mv_int){
        long index = key->value.i;
        long size = s->size;
        if(index<0){
            index+=size;
            if(index<0){
                mf_index_error("Index error in s[i]: i is out of lower bound.");
                return 1;
            }
        }else if(index>=size){
            mf_index_error("Index error in s[i]: i is out of upper bound.");
            return 1;
        }
        mt_string* c = mf_raw_string(1);
        c->a[0] = s->a[index];
        x->type = mv_string;
        x->value.p = (mt_basic*)c;
        return 0;
    }else if(key->type==mv_range){
        mt_string* sr = mf_str_slice(s,(mt_range*)key->value.p);
        if(sr==NULL) return 1;
        x->type = mv_string;
        x->value.p = (mt_basic*)sr;
        return 0;
    }else{
        mf_type_error("Type error in s[i]: s is a string but i is not an integer.");
        return 1;
    }
}

int mf_str_in_str(mt_string* s1, mt_string* s2){
    long size = s2->size;
    long n = s1->size;
    uint32_t* a = s1->a;
    uint32_t* b = s2->a;
    long i,j;
    i=0;
    do{
        if(i+n>size) return 0;
        for(j=0; j<n; j++){
            if(a[j]!=b[i+j]) goto next;
        }
        return 1;
        next:
        i++;
    }while(i<size);
    return 0;
}

int mf_str_in_range(mt_string* s, mt_string* a, mt_string* b){
    return mf_str_cmp(a,s)<=0 && mf_str_cmp(s,b)<=0;
}

static
int str_encode(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"encode");
        return 1;
    }
    if(v[0].type!=mv_string){
        mf_type_error("Type error in s.encode(e): s is not a string.");
        return 1;
    }
    if(v[1].type!=mv_string){
        mf_type_error("Type error in s.encode(e): e is not a string.");
        return 1;
    }
    mt_string* encoding = (mt_string*)v[1].value.p;
    if(mf_str_cmpmem(encoding,5,"UTF-8")==0){
        mt_string* s = (mt_string*)v[0].value.p;
        mt_bstr buffer;
        mf_encode_utf8(&buffer,s->size,s->a);
        x->type = mv_bstring;
        x->value.p = (mt_basic*)mf_buffer_to_bstring(buffer.size,buffer.a);
        mf_free(buffer.a);
        return 0;
    }else{
        mf_std_exception("Error in s.encode(encoding): unknown encoding.");
        return 1;
    }
}

static
int str_eqa(uint32_t* a, uint32_t* end, uint32_t* sub, long n){
    long i;
    for(i=0; i<n; i++){
        if(a+i==end || a[i]!=sub[i]) return 0;
    }
    return 1;
}

mt_list* str_split_space(mt_string* s){
    mt_list* list = mf_raw_list(0);
    mt_object x;
    x.type = mv_string;
    int i,j,k,size;
    i=0;
    while(i<s->size && isspace(s->a[i])) i++;
    while(i<s->size){
        j=i;
        while(1){
            if(j>=s->size || isspace(s->a[j])){
                break;
            }
            j++;
        }
        size = j-i;
        mt_string* t = mf_raw_string(size);
        for(k=0; k<size; k++){
            t->a[k] = s->a[i+k];
        }
        x.value.p = (mt_basic*)t;
        mf_list_push(list,&x);
        i=j;
        while(i<s->size && isspace(s->a[i])) i++;
    }
    return list;
}

int str_split(mt_object* x, int argc, mt_object* v){
    if(v[0].type!=mv_string){
        mf_type_error1("in s.split(sep): s (type: %s) is not a string.",&v[0]);
        return 1;
    }
    mt_string* s = (mt_string*)v[0].value.p;
    if(argc==0){
        x->type = mv_list;
        x->value.p = (mt_basic*)str_split_space(s);
        return 0;
    }else if(argc!=1){
        mf_argc_error(argc,0,1,"s.split");
        return 0;
    }
    if(v[1].type!=mv_string){
        mf_type_error1("Type error in s.split(sep): sep (type: %s) is not a string.",&v[1]);
        return 1;
    }
    mt_string*  t;
    mt_string* sep = (mt_string*)v[1].value.p;
    if(sep->size==0){
        mf_value_error("Value error in s.split(sep): sep is an empty string.");
        return 1;
    }
    mt_object u;
    u.type = mv_string;
    mt_list* list = mf_raw_list(0);
    long i,j,k;
    long size = s->size;
    uint32_t* end = s->a+size;
    i=0;
    while(1){
        j=i;
        while(1){
            if(j==size){
                t = mf_raw_string(j-i);
                for(k=0; k<j-i; k++){
                    t->a[k] = s->a[i+k];
                }
                u.value.p = (mt_basic*)t;
                mf_list_push(list,&u);
                goto out;
            }
            if(str_eqa(s->a+j,end,sep->a,sep->size)){
                t = mf_raw_string(j-i);
                for(k=0; k<j-i; k++){
                    t->a[k] = s->a[i+k];
                }
                u.value.p = (mt_basic*)t;
                mf_list_push(list,&u);
                i = j+sep->size;
                break;
            }
            j++;
        }
    }
    out:
    x->type = mv_list;
    x->value.p = (mt_basic*)list;
    return 0;
}

void mf_init_type_string(mt_table* type){
    type->name = mf_cstr_to_str("Str");
    type->m = mf_empty_map();
    mt_map* m = type->m;
    mf_insert_function(m,0,0,"upper",str_upper);
    mf_insert_function(m,0,0,"lower",str_lower);
    mf_insert_function(m,0,0,"isalpha",str_isalpha);
    mf_insert_function(m,0,0,"isalnum",str_isalnum);
    mf_insert_function(m,0,1,"isdigit",str_isdigit);
    mf_insert_function(m,0,0,"isspace",str_isspace);
    mf_insert_function(m,0,0,"islower",str_islower);
    mf_insert_function(m,0,0,"isupper",str_isupper);
    mf_insert_function(m,1,2,"rjust",str_rjust);
    mf_insert_function(m,1,2,"ljust",str_ljust);
    mf_insert_function(m,1,2,"center",str_center);
    mf_insert_function(m,0,1,"ltrim",str_ltrim);
    mf_insert_function(m,0,1,"rtrim",str_rtrim);
    mf_insert_function(m,0,1,"trim",str_trim);
    mf_insert_function(m,0,1,"encode",str_encode);
    mf_insert_function(m,0,1,"split",str_split);
}
