
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <moss.h>
#include <objects/string.h>
#include <modules/bs.h>
double mf_float(mt_object* x, int* error);

int get_placement(uint32_t* buffer, long* pk, long size, const uint32_t* a){
  long k=*pk+1;
  long i=0;
  while(1){
    if(k>=size || i>=100){
      return 1;
    }else if(mf_uisspace(a[k])){
      k++;
    }else if(a[k]=='}'){
      buffer[i]='}';
      k++;
      break;
    }else{
      buffer[i]=a[k];
      k++; i++;
    }
  }
  *pk=k;
  return 0;
}

int get_number(uint32_t* a, int* x){
  int n=0;
  int k=0;
  while(mf_uisdigit(a[k])){
    n=10*n+(a[k]-'0');
    k++;
  }
  *x=n;
  return k;
}

enum{NONE, FLOAT};
enum{LEFT, RIGHT, CENTER};

typedef struct{
  int index;
  int type;
  int subtype;
  int align;
  uint32_t fillchar;
  int precision;
  int sign_space;
  int space;
} mt_format_info;

const char* parse_error;
static
int parse_placement(mt_format_info* info, uint32_t* a){
  info->index=-1;
  info->type=NONE;
  info->space=0;
  if(*a=='}') return 0;
  if(mf_uisdigit(*a)){
    a+=get_number(a,&info->space);
    if(*a=='('){
      a++;
      if(*a=='}'){
        parse_error="Error in s%a: unexpected '}'.";
      }
      info->fillchar=*a;
      a++;
      if(*a==')') a++;
    }else{
      info->fillchar=' ';
    }
  }
  if(*a=='-'){
    info->sign_space=1;
    a++;
  }else{
    info->sign_space=0;
  }
  if(*a=='f'){
    info->type=FLOAT;
    info->subtype='f';
    a++;
  }else if(*a=='e'){
    info->type=FLOAT;
    info->subtype='e';
    a++;
  }else if(*a=='E'){
    info->type=FLOAT;
    info->subtype='E';
    a++;
  }else if(*a=='g'){
    info->type=FLOAT;
    info->subtype='g';
    a++;
  }else if(*a=='G'){
    info->type=FLOAT;
    info->subtype='G';
    a++;
  }
  if(mf_uisdigit(*a)){
    a+=get_number(a,&info->precision);
  }else{
    info->precision=14;
  }
  if(*a=='#'){
    a++;
    if(mf_uisdigit(*a)){
      a+=get_number(a,&info->index);
    }
  }
  if(*a=='}') return 0;
  parse_error="Error in s%a: expected '}'.";
  return 1;
}

static
void format_complex(char* buffer, mt_format_info* info,
  double re, double im
){
  const char* format;
  if(info->subtype=='f'){
    format = info->sign_space? "% .*f%+.*fi": "%.*f%+.*fi";
  }else if(info->subtype=='e'){
    format = info->sign_space? "% .*e%+.*ei": "%.*e%+.*ei";
  }else if(info->subtype=='E'){
    format = info->sign_space? "% .*E%+.*Ei": "%.*E%+.*Ei";
  }else if(info->subtype=='g'){
    format = info->sign_space? "% .*g%+.*gi": "%.*g%+.*gi";
  }else if(info->subtype=='G'){
    format = info->sign_space? "% .*G%+.*Gi": "%.*G%+.*Gi";
  }
  snprintf(buffer,100,format,info->precision,re,info->precision,im);
}

static
mt_string* str_formatted(mt_object* x, mt_format_info* info){
  if(info->type==FLOAT){
    char buffer[100];
    const char* format;
    if(x->type==mv_complex){
      format_complex(buffer,info,x->value.c.re,x->value.c.im);
      return mf_cstr_to_str(buffer);
    }
    int e=0;
    double t=mf_float(x,&e);
    if(e){
      mf_type_error1("in s%%a: cannot convert a[k] (type %s) to float.",x);
      return NULL;
    }
    if(info->subtype=='f'){
      format = info->sign_space? "% .*f": "%.*f";
    }else if(info->subtype=='e'){
      format = info->sign_space? "% .*e": "%.*e";
    }else if(info->subtype=='E'){
      format = info->sign_space? "% .*E": "%.*E";
    }else if(info->subtype=='g'){
      format = info->sign_space? "% .*g": "%.*g";
    }else if(info->subtype=='G'){
      format = info->sign_space? "% .*G": "%.*G";
    }
    snprintf(buffer,100,format,info->precision,t);
    return mf_cstr_to_str(buffer);
  }else{
    return mf_str(x);
  }
}

mt_string* mf_format(mt_string* s, mt_list* list){
  mt_bu32 b;
  mf_bu32_init(&b,256);
  long counter=0;
  long index;
  long k,i;
  uint32_t* a=s->a;
  long size=s->size;
  uint32_t buffer[110];
  mt_format_info info;
  k=0;
  while(k<size){
    if(a[k]=='{'){
      if(get_placement(buffer,&k,size,a)){
        mf_std_exception("Error in s%a: expected '}'.");
        goto error;
      }
      if(parse_placement(&info,buffer)){
        mf_std_exception(parse_error);
        goto error;
      }
      if(info.index<0){
        info.index=counter;
        counter++;
      }
      if(info.index<0 || info.index>=list->size){
        mf_std_exception("Error in s%a: {} out of range.");
        goto error;
      }
      mt_string* p=str_formatted(&list->a[info.index],&info);
      if(p==NULL){
        mf_traceback("template % list");
        goto error;
      }
      for(i=0; i+p->size<info.space; i++){
        mf_bu32_push(&b,info.fillchar);
      }
      mf_bu32_append(&b,p->size,p->a);
      mf_str_dec_refcount(p);
      continue;
    }
    mf_bu32_push(&b,a[k]);
    k++;
  }
  unsigned long data_size=b.size;
  uint32_t* data=mf_bu32_move(&b);
  mt_string* y=mf_str_new_u32((long)data_size,data);
  mf_free(data);
  return y;

  error:
  mf_bu32_delete(&b);
  return NULL;
}

