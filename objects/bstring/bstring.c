
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <moss.h>
#include <objects/list.h>
#include <objects/function.h>
#include <objects/string.h>
#include <modules/bs.h>


mt_bstring* mf_raw_bstring(long size){
    mt_bstring* s = mf_malloc(sizeof(mt_bstring)+size*sizeof(unsigned char));
    s->refcount = 1;
    s->size = size;
    return s;
}

mt_bstring* mf_buffer_to_bstring(long size, const unsigned char* a){
    mt_bstring* s = mf_raw_bstring(size);
    unsigned char* b = s->a;
    long i;
    for(i=0; i<size; i++){
        b[i] = a[i];
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
        b = a[i];
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
    mt_string* encoding = (mt_string*)v[1].value.p;
    if(mf_str_cmpmem(encoding,5,"UTF-8")==0){
        mt_bstring* bs = (mt_bstring*)v[0].value.p;
        mt_string* s = mf_str_decode_utf8(bs->size,bs->a);
        x->type = mv_string;
        x->value.p = (mt_basic*)s;
        return 0;
    }else{
        mf_std_exception("Error in bs.decode(endcoding): unknown encoding.");
        return 1;
    }
}

mt_list* mf_bstring_to_list(mt_bstring* bs){
    mt_list* list = mf_raw_list(bs->size);
    long i;
    mt_object* a = list->a;
    const unsigned char* b = bs->a;
    for(i=0; i<bs->size; i++){
        a[i].type = mv_int;
        a[i].value.i = b[i];
    }
    return list;
}

int mf_bstring(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"bstr");
        return 1;
    }
    mt_function* i = mf_iter(v+1);
    mt_object argv[1];
    argv[0].type = mv_null;
    mt_object y;
    mt_bs buffer;
    mf_bs_init(&buffer);
    unsigned char c;
    while(1){
        if(mf_call(i,&y,0,argv)){
            if(mf_empty()) break;
            goto error;
        }
        if(y.type!=mv_int){
            mf_type_error1("in bstr(a): x (type: %s) in iter(a) is not an integer.",&y);
            mf_dec_refcount(&y);
            goto error;
        }
        if(y.value.i<0 || y.value.i>255){
            mf_value_error("Value error in bstr(a): x in iter(a) is out of range 0..255.");
            goto error;
        }
        c = (unsigned char)y.value.i;
        mf_bs_push(&buffer,1,&c);
    }
    
    mt_bstring* bs = mf_buffer_to_bstring(buffer.size,buffer.a);
    mf_function_dec_refcount(i);
    mf_bs_delete(&buffer);
    x->type = mv_bstring;
    x->value.p = (mt_basic*)bs;
    return 0;
    
    error:
    mf_function_dec_refcount(i);
    mf_bs_delete(&buffer);
    return 1;
}

void mf_init_type_bstring(mt_table* type){
    type->name = mf_cstr_to_str("bstr");
    type->m = mf_empty_map();
    mt_map* m = type->m;
    mf_insert_function(m,1,1,"decode",bstring_decode);
}
