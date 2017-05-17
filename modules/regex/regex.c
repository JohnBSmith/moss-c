
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <moss.h>
#include <objects/string.h>
#include <objects/list.h>
#include <modules/vec.h>

enum{CHAR,REGEX,OR,STAR,PLUS,QM,DOT,QUANTIFIER,CLASS,CCLASS,SCLASS};
enum{ALPHA,DIGIT,XDIGIT,NALPHA,NDIGIT,RANGE};

typedef struct{
  char type;
  unsigned long a,b;
} char_class_item;

typedef struct{
  char inversion;
  size_t size;
  char_class_item a[];
} char_class;

typedef struct regex_ast_node regex_ast_node;
struct regex_ast_node{
  unsigned char type;
  union{
    unsigned long c;
    struct{long min,max;} r;
    char_class* class;
  } value;
  size_t n;
  regex_ast_node* a[];
};

typedef struct{
  long refcount;
  mt_object prototype;
  void(*del)(void*);
  regex_ast_node* regex;
} mt_regex;

typedef struct{
  long line, col, index;
  unsigned long c;
} regex_token;

typedef struct{
  long index;
  long size;
  regex_token* a;
  mt_string* s;
} token_iterator;

typedef struct{
  long index;
  long size;
  uint32_t* a;
} string_iterator;

static
mt_table* regex_type;

static
int mf_uisxdigit(unsigned long c){
  return c<256 && isxdigit(c);
}

static
mt_string* mf_str_strip(mt_string* s){
  long k,n,i;
  n=0;
  for(k=0; k<s->size; k++){
    if(!mf_uisspace(s->a[k])) n++;
  }
  mt_string* y=mf_raw_string(n);
  i=0;
  for(k=0; k<s->size; k++){
    if(!mf_uisspace(s->a[k])){
      y->a[i] = s->a[k];
      i++;
    }
  }
  return y;
}

static
token_iterator* regex_scan(mt_string* s){
  mt_vec v;
  mf_vec_init(&v,sizeof(regex_token));
  long k;
  regex_token x;
  x.line=0;
  x.col=0;
  uint32_t* a = s->a;
  for(k=0; k<s->size; k++){
    if(mf_uisspace(a[k])){
      if(a[k]=='\n'){
        x.line++;
        x.col = 0;
      }else{
        x.col++;
      }
    }else{
      x.c = a[k];
      x.index = k;
      mf_vec_push(&v,&x);
      x.col++;
    }
  }
  token_iterator* i = mf_malloc(sizeof(token_iterator));
  i->index=0;
  i->size = v.size;
  i->a = (regex_token*)v.a;
  s->refcount++;
  i->s=s;
  return i;
}

static
regex_ast_node* new_node(size_t n, unsigned char type){
  regex_ast_node* y = mf_malloc(
    sizeof(regex_ast_node)+n*sizeof(regex_ast_node*)
  );
  y->n = n;
  y->type = type;
  return y;
}

static
void regex_ast_delete(regex_ast_node* p){
  size_t i;
  for(i=0; i<p->n; i++){
    regex_ast_delete(p->a[i]);
  }
  mf_free(p);
}

static
void ast_buffer_delete(mt_vec* v){
  regex_ast_node** a = (regex_ast_node**)v->a;
  long k;
  for(k=0; k<v->size; k++){
    regex_ast_delete(a[k]);
  }
  mf_vec_delete(v);
}

static
int empty_iterator(token_iterator* i){
  return i->index>=i->size;
}

static
int empty(string_iterator* i){
  return i->index>=i->size;
}

static
void syntax_error(token_iterator* i, const char* s){
  long line, col;
  if(i->size==0){
    line = 0; col = 0;
  }else{
    long index = i->index<i->size? i->index: i->size-1;
    regex_token* x = &i->a[index];
    line = x->line; col = x->col;
  }
  char buffer[200];
  snprintf(buffer,200,
    "Error in re.compile(s): syntax error in %li:%li: %s",
    line+1,col+1,s);
  mf_std_exception(buffer);
}

static regex_ast_node* or_operation(token_iterator* i);

static
regex_ast_node* class_node(token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(char_class_item));
  char_class_item item;
  char inversion=0;
  while(1){
    if(empty_iterator(i)) goto empty;
    unsigned long c = i->a[i->index].c;
    i->index++;
    if(c==']') break;
    if(c=='^'){
      inversion=1;
      continue;
    }
    if(empty_iterator(i)) goto empty;
    unsigned long op = i->a[i->index].c;
    if(op=='-'){
      i->index++;
      if(empty_iterator(i)) goto empty;
      unsigned long d = i->a[i->index].c;
      i->index++;
      item.type=RANGE;
      item.a=c; item.b=d;
    }else if(c=='{'){
      if(empty_iterator(i)) goto empty;
      unsigned long u = i->a[i->index].c;
      i->index++;
      if(empty_iterator(i)) goto empty;
      i->index++;
      item.type=RANGE;
      if(u=='s') u=' ';
      else if(u=='n') u='\n';
      else if(u=='t') u='\t';
      else if(u=='L') u='{';
      else if(u=='R') u='}';
      else if(u=='.') u='.';
      item.a=u; item.b=u;
    }else{
      item.type=RANGE;
      item.a=c; item.b=c;
    }
    mf_vec_push(&v,&item);
  }
  char_class* p = mf_malloc(
    sizeof(char_class)+v.size*sizeof(char_class_item)
  );
  p->inversion=inversion;
  p->size=v.size;
  long k;
  char_class_item* a = (char_class_item*)v.a;
  for(k=0; k<v.size; k++){
    p->a[k]=a[k];
  }
  regex_ast_node* y = new_node(0,CLASS);
  y->value.class=p;
  return y;
  
  empty:
  syntax_error(i,"expected a character.");
  mf_vec_delete(&v);
  return NULL;
}

static
regex_ast_node* atom(token_iterator* i){
  if(empty_iterator(i)){
    syntax_error(i,"expected a character.");
    return NULL;
  }
  unsigned long c = i->a[i->index].c;
  regex_ast_node* y;
  if(c=='('){
    i->index++;
    return or_operation(i);
  }else if(c=='.'){
    i->index++;
    return new_node(0,DOT);
  }else if(c=='['){
    i->index++;
    return class_node(i);
  }else if(c=='{'){
    i->index++;
    if(empty_iterator(i)){
      syntax_error(i,"invalid escape sequence.");
      return NULL;
    }
    c = i->a[i->index].c;
    if(c=='d'){
      i->index++;
      y = new_node(0,SCLASS);
      y->value.c=DIGIT;
    }else if(c=='x'){
      i->index++;
      y = new_node(0,SCLASS);
      y->value.c=XDIGIT;
    }else if(c=='a'){
      i->index++;
      y = new_node(0,SCLASS);
      y->value.c=ALPHA;
    }else if(c=='s'){
      i->index++;
      y = new_node(0,CHAR);
      y->value.c=' ';
    }else if(c=='n'){
      i->index++;
      y = new_node(0,CHAR);
      y->value.c='\n';
    }else if(c=='t'){
      i->index++;
      y = new_node(0,CHAR);
      y->value.c='\t';
    }else if(c=='r'){
      i->index++;
      y = new_node(0,CHAR);
      y->value.c='\r';
    }else{
      i->index++;
      y = new_node(0,CHAR);
      y->value.c=c;
    }
    if(empty_iterator(i) || i->a[i->index].c!='}'){
      regex_ast_delete(y);
      syntax_error(i,"expected '}'.");
      return NULL;
    }
    i->index++;
    return y;
  }else{
    i->index++;
    y = new_node(0,CHAR);
    y->value.c=c;
    return y;
  }
}

static
regex_ast_node* postfix_operation(token_iterator* i){
  regex_ast_node* p = atom(i);
  if(p==NULL) return NULL;
  if(empty_iterator(i)) return p;
  regex_ast_node* op;
  unsigned long c = i->a[i->index].c;
  if(c=='+'){
    i->index++;
    op = new_node(1,PLUS);
    op->a[0]=p;
    return op;
  }else if(c=='*'){
    i->index++;
    op = new_node(1,STAR);
    op->a[0]=p;
    return op;
  }else if(c=='?'){
    i->index++;
    op = new_node(1,QM);
    op->a[0]=p;
    return op;
  }else{
    return p;
  }
}

static
regex_ast_node* operand(token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(regex_ast_node*));
  while(1){
    regex_ast_node* p = postfix_operation(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    if(empty_iterator(i)) break;
    unsigned long c = i->a[i->index].c;
    if(c==')'){
      i->index++;
      break;
    }else if(c=='|'){
      break;
    }
  }
  regex_ast_node* y = new_node(v.size,REGEX);
  regex_ast_node** a = (regex_ast_node**)v.a;
  long k;
  for(k=0; k<v.size; k++){
    y->a[k]=a[k];
  }
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
regex_ast_node* or_operation(token_iterator* i){
  regex_ast_node* p1=operand(i);
  if(p1==NULL) return NULL;
  while(1){
    if(empty_iterator(i)) break;
    unsigned long c = i->a[i->index].c;
    if(c=='|'){
      i->index++;
      regex_ast_node* p2 = operand(i);
      if(p2==NULL){
        regex_ast_delete(p1);
        return NULL;
      }
      regex_ast_node* y = new_node(2,OR);
      y->a[0]=p1; y->a[1]=p2;
      p1=y;
    }else{
      break;
    }
  }
  return p1;
}

static
void regex_delete(mt_regex* regex){
  mf_dec_refcount(&regex->prototype);
  regex_ast_delete(regex->regex);
  mf_free(regex);
}

static
void token_iterator_delete(token_iterator* i){
  mf_str_dec_refcount(i->s);
  mf_free(i->a);
  mf_free(i);
}

static
mt_regex* string_to_regex(mt_string* s){
  token_iterator* i = regex_scan(s);
  regex_ast_node* p = or_operation(i);
  token_iterator_delete(i);
  if(p==NULL) return NULL;
  mt_regex* regex = mf_malloc(sizeof(mt_regex));
  regex->refcount=1;
  regex_type->refcount++;
  regex->prototype.type=mv_table;
  regex->prototype.value.p=(mt_basic*)regex_type;
  regex->del=(void(*)(void*))regex_delete;
  regex->regex=p;
  return regex;
}

static
int regex_compile(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"re.compile");
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error1("in re.compile(s): s (type: %s) is not a string.",&v[1]);
    return 1;
  }
  mt_string* s = mf_str_strip((mt_string*)v[1].value.p);
  mt_regex* regex = string_to_regex(s);
  mf_str_dec_refcount(s);
  if(regex==NULL) return 1;
  x->type=mv_native;
  x->value.p=(mt_basic*)regex;
  return 0;
}

mt_list* groups;

static
void push_group(long size, uint32_t* a){
  mt_object x;
  x.type=mv_string;
  x.value.p = (mt_basic*)mf_str_new_u32(size,a);
  mf_list_push(groups,&x);
}

static
int match(regex_ast_node* p, string_iterator* i, int level){
  switch(p->type){
  case CHAR:
    if(empty(i)) return 0;
    int y = p->value.c==i->a[i->index];
    i->index++;
    return y;
  case DOT:
    if(empty(i)) return 0;
    i->index++;
    return 1;
  case CLASS:{
    if(empty(i)) return 0;
    unsigned long c = i->a[i->index];
    i->index++;
    size_t k;
    size_t size = p->value.class->size;
    char_class_item* a = p->value.class->a;
    if(p->value.class->inversion){
      for(k=0; k<size; k++){
        if(a[k].a<=c && c<=a[k].b) return 0;
      }
      return 1;
    }else{
      for(k=0; k<size; k++){
        if(a[k].a<=c && c<=a[k].b) return 1;
      }
      return 0;
    }
  }
  case SCLASS:
    switch(p->value.c){
    case DIGIT:
      if(empty(i)) return 0;
      i->index++;
      return mf_uisdigit(i->a[i->index-1]);
    case XDIGIT:
      if(empty(i)) return 0;
      i->index++;
      return mf_uisxdigit(i->a[i->index-1]);
    case ALPHA:
      if(empty(i)) return 0;
      i->index++;
      return mf_uisalpha(i->a[i->index-1]);
    default:
      abort();
    }
  case REGEX:{
    long index=i->index;
    size_t k;
    for(k=0; k<p->n; k++){
      if(match(p->a[k],i,level+1)==0) return 0;
    }
    if(level==1 && groups){
      push_group(i->index-index,i->a+index);
    }
    return 1;
  }
  case OR:{
    long index=i->index;
    if(match(p->a[0],i,level)==1) return 1;
    i->index=index;
    return match(p->a[1],i,level);
  }
  case STAR:{
    while(1){
      long index=i->index;
      if(match(p->a[0],i,level)==0){
        i->index=index;
        break;
      }
    }
    return 1;
  }
  case PLUS:{
    if(match(p->a[0],i,level)==0) return 0;
    while(1){
      long index=i->index;
      if(match(p->a[0],i,level)==0){
        i->index=index;
        break;
      }
    }
    return 1;
  }
  case QM:{
    long index=i->index;
    if(match(p->a[0],i,level)==0){
      i->index=index;
    }
    return 1;
  }
  default:
    abort();
  }
}

static
mt_list* findall(regex_ast_node* p, string_iterator* i){
  mt_list* list = mf_raw_list(0);
  mt_object x;
  x.type=mv_string;
  while(i->index<i->size){
    long index=i->index;
    if(match(p,i,0)){
      mt_string* s = mf_str_new_u32(i->index-index,i->a+index);
      x.value.p=(mt_basic*)s;
      mf_list_push(list,&x);
    }else{
      i->index=index+1;
    }
  }
  return list;
}

static
long mf_regex_index(regex_ast_node* p, string_iterator* i){
  while(i->index<i->size){
    long index=i->index;
    if(match(p,i,0)){
      return index;
    }else{
      i->index=index+1;
    }
  }
  return -1;
}

static
int regex_match(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"match");
    return 1;
  }
  if(!mf_isa(&v[0],regex_type)){
    mf_type_error1("in r.match(s): r (type %s) is not a regex.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error1("in r.match(s): s (type: %s) is not a string.",&v[1]);
    return 1;
  }
  mt_regex* regex = (mt_regex*)v[0].value.p;
  mt_string* s = (mt_string*)v[1].value.p;
  string_iterator i;
  i.a=s->a; i.size=s->size; i.index=0;
  x->type=mv_bool;
  int y = match(regex->regex,&i,0);
  x->value.b = i.index==i.size? y: 0;
  return 0;
}

static
int regex_list(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"regex.list");
    return 1;
  }
  if(!mf_isa(&v[0],regex_type)){
    mf_type_error1("in r.list(s): r (type %s) is not a regex.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error1("in r.list(s): s (type: %s) is not a string.",&v[1]);
    return 1;
  }
  mt_regex* regex = (mt_regex*)v[0].value.p;
  mt_string* s = (mt_string*)v[1].value.p;
  string_iterator i;
  i.a=s->a; i.size=s->size; i.index=0;
  mt_list* list = findall(regex->regex,&i);
  x->type=mv_list;
  x->value.p=(mt_basic*)list;
  return 0;
}

static
int regex_index(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"regex.index");
    return 1;
  }
  if(!mf_isa(&v[0],regex_type)){
    mf_type_error1("in r.index(s): r (type %s) is not a regex.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error1("in r.index(s): s (type: %s) is not a string.",&v[1]);
    return 1;
  }
  mt_regex* regex = (mt_regex*)v[0].value.p;
  mt_string* s = (mt_string*)v[1].value.p;
  string_iterator i;
  i.a=s->a; i.size=s->size; i.index=0;
  long index = mf_regex_index(regex->regex,&i);
  x->type=mv_int;
  x->value.i=index;
  return 0;
}

static
int regex_groups(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"groups");
    return 1;
  }
  if(!mf_isa(&v[0],regex_type)){
    mf_type_error1("in r.groups(s): r (type %s) is not a regex.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error1("in r.groups(s): s (type: %s) is not a string.",&v[1]);
    return 1;
  }
  mt_regex* regex = (mt_regex*)v[0].value.p;
  mt_string* s = (mt_string*)v[1].value.p;
  string_iterator i;
  i.a=s->a; i.size=s->size; i.index=0;
  groups = mf_raw_list(0);
  int y = match(regex->regex,&i,0);
  if(y==0){
    mf_list_dec_refcount(groups);
    groups=NULL;
    x->type=mv_null;
    return 0;
  }else{
    x->type=mv_list;
    x->value.p=(mt_basic*)groups;
    groups=NULL;
    return 0;
  }
}

mt_table* mf_regex_load(void){
  mt_table* re = mf_table(NULL);
  regex_type = mf_table(NULL);
  regex_type->m = mf_empty_map();
  mt_map* m = regex_type->m;
  mf_insert_function(m,1,1,"match",regex_match);
  mf_insert_function(m,1,1,"list",regex_list);
  mf_insert_function(m,1,1,"groups",regex_groups);
  mf_insert_function(m,1,1,"index",regex_index);

  re->m=mf_empty_map();
  m=re->m;
  mf_insert_function(m,1,1,"compile",regex_compile);
  m->frozen=1;
  return re;
}
