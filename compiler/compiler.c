
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <moss.h>
#include <vm.h>
#include <modules/system.h>
#include <modules/bs.h>
#include <compiler.h>

// TODO:
// operators <<, >>

// #define MV_PRINT
// ^shows syntax trees and bytecode dump
// (debug mode)

#define MV_LOCAL 0
#define MV_GLOBAL 1
#define MV_ARGUMENT 2
#define MV_CONTEXT 3
#define MV_FN_NAME 4
#define HAS_ELSE_CASE 1
#define CATCH_ID 0
#define CATCH_ID_EXP 1
#define POP_VALUE 0
#define RETURN_VALUE 1
#define SELF 1

typedef struct{
  char* s;
  int size;
  int ownership;
} mt_var_info;

typedef struct mt_var_tab mt_var_tab;
struct mt_var_tab{
  mt_var_tab* context;
  mt_vec v,c,g,a;
  int argc;
  unsigned char* fn_name;
  int fn_name_size;
};

typedef struct{
  int size;
  char* s;
  char type;
  char value;
} mt_keyword;

typedef struct{
  const char* a;
  int size;
  long index;
} mt_label;

typedef struct ast_node ast_node;
struct ast_node{
  int line;
  int col;
  char type;
  char value;
  int info;
  int ownership;
  char* s;
  long size;
  int n;
  ast_node* a[];
};

typedef struct{
  mt_token* a;
  int size;
  int index;
} token_iterator;

static ast_node* expression(token_iterator* i);
static ast_node* comma_expression(token_iterator* i);
static int comma_found;
static ast_node* factor(token_iterator* i);
static ast_node* argument_list(token_iterator* i);
static ast_node* function_literal(token_iterator* i);
static ast_node* statement_list(token_iterator *);
static void ast_print(ast_node* t, int indent);

enum{
  Tid, Tbool, Tint, Tfloat, Timag, Tstring, Tbstring, Tnull,
  Tkeyword, Top, Tsep, Tbleft, Tbright,
  Tapp, Tblock, Tstatement
};

enum{
  Top_add, Top_sub, Top_mpy, Top_div, Top_idiv, Top_neg, Top_mod,
  Top_in, Top_notin, Top_is, Top_and, Top_or, Top_not, Top_pow,
  Top_range, Top_list, Top_map, Top_lt, Top_gt, Top_le, Top_ge,
  Top_eq, Top_ne, Top_assignment, Top_dot, Top_index, Top_if,
  Top_aadd, Top_asub, Top_ampy, Top_adiv, Top_amod, Top_aidiv,
  Top_inc, Top_dec, Top_step, Top_band, Top_bor, Top_bxor, Top_isin,
  Top_qm, Top_tilde, Top_tuple
};

enum{
  Tbegin, Tbreak, Tcatch, Tcontinue, Tdo, Telif, Telse, Tend,
  Tfor, Tglobal, Tgoto, Tif, Tlabel, Tof, Traise, Treturn,
  Tsub, Ttable, Tthen, Ttry, Twhile, Tyield, Timport
};

mt_compiler_context compiler_context={
  (char)0, "stdin"
};

static mt_vec data;
static mt_vec bv_deposit;
static mt_vec stab; // string table
#define JMP_STACK_SIZE 100
static uint32_t jmp_stack[JMP_STACK_SIZE];
static mt_vec out_stack[JMP_STACK_SIZE];
static int jmp_sp;
static mt_vec* const_fn_indices;
static mt_var_tab* context;
static int nesting_level;
static int scope;
static int for_level;
int input_nesting;
static char bstack[200];
static int bstack_sp;
static mt_vec* label_tab;
static mt_vec* goto_tab;

static
void push_token(mt_vec* v, int line, int col, char type, char value){
  mt_token t;
  t.line=line;
  t.col=col;
  t.type=type;
  t.value=value;
  t.s=NULL;
  t.size=0;
  mf_vec_push(v,&t);
}

static
void push_token_string(mt_vec* v, int line, int col, char type,
  long size, const char* a
){
  mt_token t;
  t.line=line;
  t.col=col;
  t.type=type;
  t.s=mf_malloc((size_t)(size+1));
  memcpy(t.s,a,(size_t)size);
  t.s[size]=0;
  t.size=size;
  mf_vec_push(v,&t);
}

void mf_vtoken_delete(mt_vec* v){
  int i;
  int n=v->size;
  for(i=0; i<n; i++){
    mt_token* t = mf_vec_get(v,i);
    mf_free(t->s);
  }
  mf_vec_delete(v);
}

static
mt_token* vtoken_get(mt_vec* v, int i){
  return mf_vec_get(v,i);
}

static
void print_string(const char* a, long size){
  int i;
  for(i=0; i<size; i++){
    putchar(a[i]);
  }
}

static
void print_mem(unsigned char* a, long size){
  long i;
  for(i=0; i<size; i++){
    printf("%02x ",a[i]);
  }
  printf("\n");
}

static
void print_operator(char value){
  switch(value){
  case Top_add: printf("+"); break;
  case Top_sub: printf("-"); break;
  case Top_mpy: printf("*"); break;
  case Top_div: printf("/"); break;
  case Top_idiv: printf("//"); break;
  case Top_mod: printf("%%"); break;
  case Top_neg: printf("u-"); break;
  case Top_pow: printf("^"); break;
  case Top_not: printf("not"); break;
  case Top_and: printf("and"); break;
  case Top_or: printf("or"); break;
  case Top_in: printf("in"); break;
  case Top_notin: printf("not in"); break;
  case Top_is: printf("is"); break;
  case Top_isin: printf("is in"); break;
  case Top_range: printf(".."); break;
  case Top_list: printf("list"); break;
  case Top_map: printf("map"); break;
  case Top_tuple: printf("tuple"); break;
  case Top_lt: printf("<"); break;
  case Top_gt: printf(">"); break;
  case Top_le: printf("<="); break;
  case Top_ge: printf(">="); break;
  case Top_eq: printf("=="); break;
  case Top_ne: printf("!="); break;
  case Top_assignment: printf("="); break;
  case Top_dot: printf("."); break;
  case Top_index: printf("index"); break;
  case Top_if: printf("if"); break;
  case Top_aadd: printf("+="); break;
  case Top_asub: printf("-="); break;
  case Top_ampy: printf("*="); break;
  case Top_adiv: printf("/="); break;
  case Top_aidiv: printf("//="); break;
  case Top_amod: printf("%%="); break;
  case Top_inc: printf("++"); break;
  case Top_dec: printf("--"); break;
  case Top_bor: printf("|"); break;
  case Top_band: printf("&"); break;
  case Top_bxor: printf("$"); break;
  case Top_qm: printf("?:"); break;
  case Top_tilde: printf("~"); break;
  default:
    printf("unknown operator");
  }
}

static
void kw_print(char* s){
  printf("%s",s);
}

static
void print_keyword(char value){
  switch(value){
    case Tbegin: kw_print("begin"); break;
    case Tbreak: kw_print("break"); break;
    case Tcatch: kw_print("catch"); break;
    case Tcontinue: kw_print("continue"); break;
    case Tdo: kw_print("do"); break;
    case Telif: kw_print("elif"); break;
    case Telse: kw_print("else"); break;
    case Tend: kw_print("end"); break;
    case Tfor: kw_print("for"); break;
    case Tglobal: kw_print("global"); break;
    case Tgoto: kw_print("goto"); break;
    case Timport: kw_print("use"); break;
    case Tif: kw_print("if"); break;
    case Tlabel: kw_print("label"); break;
    case Tof: kw_print("of"); break;
    case Traise: kw_print("raise"); break;
    case Treturn: kw_print("return"); break;
    case Tsub: kw_print("sub"); break;
    case Ttable: kw_print("table"); break;
    case Tthen: kw_print("then"); break;
    case Ttry: kw_print("try"); break;
    case Twhile: kw_print("while"); break;
    case Tyield: kw_print("yield"); break;
    default:
      printf("unknown keyword");
  };
}

static
void print_token(mt_token* t){
  int type=t->type;
    if(type==Top){
      printf("[");
      print_operator(t->value);
      printf("]");
    }else if(type==Tint){
      printf("[");
      print_string(t->s,t->size);
      printf("]");
    }else if(type==Tfloat){
      printf("[");
      print_string(t->s,t->size);
      printf("]");
    }else if(type==Timag){
      printf("[");
      print_string(t->s,t->size);
      printf("i]");
    }else if(type==Tid){
      printf("[");
      print_string(t->s,t->size);
      printf("]");
    }else if(type==Tstring){
      printf("[\"");
      print_string(t->s,t->size);
      printf("\"]");
    }else if(type==Tbstring){
      printf("[b\"");
      print_string(t->s,t->size);
      printf("\"]");
    }else if(type==Tnull){
      printf("[null]");
    }else if(type==Tkeyword){
      printf("['");
      print_keyword(t->value);
      printf("']");
    }else if(type==Tbool){
      if(t->value==1){
        printf("[true]");
      }else if(t->value==0){
        printf("[false]");
      }else{
        printf("Error: boolean token value is outside of range 0..1.\n");
        exit(1);
      }
    }else if(type==Tbleft){
      printf("[%c]",t->value);
    }else if(type==Tbright){
      printf("[%c]",t->value);
    }else if(type==Tsep){
      printf("[%c]",t->value);
    }else if(type==Tapp){
      printf("[app]");
    }else{
      printf("[token of unkown type]");
    }
}

static
void print_vtoken(mt_vec* v){
  int i;
  int n=v->size;
  mt_token* t;
  int type;
  for(i=0; i<n; i++){
    t=mf_vec_get(v,i);
    print_token(t);
  }
  printf("\n");
}

mt_keyword keywords[]={
  {3, "and", Top, Top_and},
  {5, "begin", Tkeyword, Tbegin},
  {5, "break", Tkeyword, Tbreak},
  {5, "catch", Tkeyword, Tcatch},
  {8, "continue", Tkeyword, Tcontinue},
  {2, "do", Tkeyword, Tdo},
  {4, "elif", Tkeyword, Telif},
  {4, "else", Tkeyword, Telse},
  {3, "end", Tkeyword, Tend},
  {5, "false", Tbool, 0},
  {3, "for",Tkeyword, Tfor},
  {6, "global",Tkeyword, Tglobal},
  {4, "goto", Tkeyword, Tgoto},
  {2, "if", Tkeyword, Tif},
  {2, "in", Top, Top_in},
  {2, "is", Top, Top_is},
  {5, "label", Tkeyword, Tlabel},
  {3, "not", Top, Top_not},
  {4, "null", Tnull, 0},
  {2, "of", Tkeyword, Tof},
  {2, "or", Top, Top_or},
  {5, "raise", Tkeyword, Traise},
  {6, "return",Tkeyword, Treturn},
  {3, "sub", Tkeyword, Tsub},
  {5, "table", Tkeyword, Ttable},
  {4, "then", Tkeyword, Tthen},
  {4, "true", Tbool, 1},
  {3, "try", Tkeyword, Ttry},
  {3, "use", Tkeyword, Timport},
  {5, "while", Tkeyword, Twhile},
  {5, "yield", Tkeyword, Tyield}
};

static
mt_keyword* find_keyword(int size, char* a){
  int i;
  int n = sizeof(keywords)/sizeof(mt_keyword);
  for(i=0; i<n; i++){
    if(size==keywords[i].size){
      if(memcmp(a,keywords[i].s,size)==0){
        return keywords+i;
      }
    }
  }
  return NULL;
}

static
int keyword_compare(mt_bstr* key, mt_keyword *element){
  long size = key->size<element->size? key->size: element->size;
  int c = memcmp(key->a,element->s,size);
  if(c==0){
    if(key->size<element->size) return -1;
    if(key->size>element->size) return 1;
    return 0;
  }else{
    return c;
  }
}

static
mt_keyword* find_keyword_bsearch(int size, const char* a){
  int n=sizeof(keywords)/sizeof(mt_keyword);
  mt_bstr s; s.size=size; s.a=(unsigned char*)a;
  typedef int (*T)(const void*, const void*);
  return bsearch(&s,keywords,n,sizeof(mt_keyword),
    (T)keyword_compare
  );
}

#ifdef MV_PRINT
static
void syntax_error_analyze(int line, int col, const char* s, int LINE){
  printf("Line %i, col %i (COMPILER LINE %i)\nSyntax error: %s\n",line+1,col+1,LINE,s);
  abort();
}
#define syntax_error(line,col,s)\
  syntax_error_analyze(line,col,s,__LINE__)
#else
static
void syntax_error(int line, int col, const char* s){
  printf("Line %i, col %i\nSyntax error: %s\n",line+1,col+1,s);
}
#endif


static
void unexpected_character(int line, int col, char c){
  char a[200];
  snprintf(a,200,"unexpected character: '%c'.",c);
  syntax_error(line,col,a);
}

// section
// lexical analysis

void mf_push_newline(mt_vec* v, int line, int col){
  push_token(v,line,col,Tsep,'n');
}

static
int is_before(mt_vec* v, int type, int value){
  if(v->size==0) return 0;
  mt_token* t = vtoken_get(v,-1);
  return t->type==type && t->value==value;
}

static
int sub_syntax(mt_vec* v){
  if(v->size<2) return 0;
  mt_token* t2=mf_vec_get(v,-2);
  if(t2->type!=Tkeyword || t2->value!=Tsub) return 0;
  mt_token* t1=mf_vec_get(v,-1);
  if(t1->type!=Tid) return 0;
  int line=t2->line;
  int col=t2->col;
  char* id=t1->s;
  long size=t1->size;
  mf_vec_pop(v,2);
  push_token_string(v,line,col,Tid,size,id);
  push_token(v,line,col,Top,Top_assignment);
  push_token(v,line,col,Tkeyword,Tsub);
  push_token_string(v,line,col,Tid,size,id);
  mf_free(id);
  return 1;
}

int mf_scan(mt_vec* v, unsigned char* s, long n, int line){
  int col=0;
  unsigned c;
  int j,i=0;
  int floatsep;
  int imag;
  int hcol,hline;
  while(i<n){
    c=s[i];
    if(isalpha(c) || c=='_'){
      if(c=='b' && i+1<n && s[i+1]=='{'){
        hcol=col; hline=line;
        i+=2; col+=2; j=i;
        while(1){
          if(i>=n){
            syntax_error(line,col,"expected end of byte string literal.");
            goto error;
          }else if(s[i]=='\n'){
            line++; col=0; i++;
          }else if(s[i]=='}'){
            break;
          }else{
            i++; col++;
          }
        }
        push_token_string(v,line,hcol,Tbstring,i-j,(char*)s+j);
        i++; col++;
        continue;
      }
      j=i; hcol=col;
      while(i<n && (isalpha(s[i]) || isdigit(s[i]) || s[i]=='_')){
        i++; col++;
      }
      mt_keyword* keyword = find_keyword_bsearch(i-j,(char*)s+j);
      if(keyword){
        int type = keyword->type;
        int value = keyword->value;
        if(type==Top){
          if(value==Top_in){
            if(is_before(v,Top,Top_not)){
              vtoken_get(v,-1)->value=Top_notin;
            }else if(is_before(v,Top,Top_is)){
              vtoken_get(v,-1)->value=Top_isin;
            }else{
              push_token(v,line,hcol,Top,value);
            }
          }else{
            push_token(v,line,hcol,Top,value);
          }
        }else if(type==Tbool){
          push_token(v,line,hcol,Tbool,value);
        }else if(type==Tkeyword){
          if(value==Tbegin){
            push_token(v,line,hcol,Tkeyword,Tsub);
            push_token(v,line,hcol,Top,Top_step);
          }else{
            push_token(v,line,hcol,Tkeyword,value);
          }
          if(value==Tsub || value==Tbegin || value==Tfor || value==Tif){
            input_nesting++;
          }else if(value==Ttable || value==Ttry || value==Twhile){
            input_nesting++;
          }else if(value==Tend){
            input_nesting--;
          }
        }else if(type==Tnull){
          push_token(v,line,hcol,Tnull,0);
        }else{
          push_token_string(v,line,hcol,type,i-j,(char*)s+j);
        }
      }else{
        push_token_string(v,line,hcol,Tid,i-j,(char*)s+j);
      }
    }else{
      switch(c){
      case '#':
        while(i<n && s[i]!='\n'){
          i++;
        }
        break;
      case ' ': case '\t': case '\n':
        if(c=='\n'){
          if(sub_syntax(v)){
            push_token(v,line,col,Top,Top_step);
          }
          push_token(v,line,col,Tsep,'n');
          i++; col=0; line++;
        }else{
          i++; col++;
        }
        break;
      case '0':
        if(i+1<n && isalpha(s[i+1])){
          if(s[i+1]=='x'){
            j=i; hcol=col;
            col+=2; i+=2;
            while(i<n){
              if(isdigit(s[i])||isxdigit(s[i])){
                i++; col++;
              }else{
                break;
              }
            }
            push_token_string(v,line,hcol,Tint,i-j,(char*)s+j);
            break;
          }else if(s[i+1]=='b'){
            j=i; hcol=col;
            col+=2; i+=2;
            while(i<n){
              if(s[i]=='0' || s[i]=='1'){
                i++; col++;
              }else{
                break;
              }
            }
            push_token_string(v,line,hcol,Tint,i-j,(char*)s+j);
            break;
          }else if(s[i+1]=='o'){
            j=i; hcol=col;
            col+=2; i+=2;
            while(i<n){
              if(isdigit(s[i]) && s[i]!='9' && s[i]!='8'){
                i++; col++;
              }else{
                break;
              }
            }
            push_token_string(v,line,hcol,Tint,i-j,(char*)s+j);
            break;
          }
        }
      case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        j=i; hcol=col;
        floatsep=0; imag=0;
        while(i<n){
          if(s[i]=='.'){
            if(i+1<n && s[i+1]=='.'){
              break;
            }
            if(floatsep){
              unexpected_character(line,col,'.');
              return 1;
            }
            floatsep=1;
            i++; col++;
          }else if(s[i]=='i'){
            imag=1;
            break;
          }else if(s[i]=='e' || s[i]=='E'){
            floatsep=1;
            i++; col++;
            if(i<n && (s[i]=='+' || s[i]=='-')){
              i++; col++;
            }
            if(i>=n || !isdigit(s[i])){
              syntax_error(line,col,"invalid float literal.");
              return 1;
            }
          }else if(isdigit(s[i])){
            i++; col++;
          }else{
            break;
          }
        }
        if(imag){
          push_token_string(v,line,hcol,Timag,i-j,(char*)s+j);
          i++; col++;
        }else if(floatsep){
          push_token_string(v,line,hcol,Tfloat,i-j,(char*)s+j);
        }else{
          push_token_string(v,line,hcol,Tint,i-j,(char*)s+j);
        }
        break;
      case '+':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_aadd);
          i+=2; col+=2;
        }else if(i+1<n && s[i+1]=='+'){
          push_token(v,line,col,Top,Top_aadd);
          push_token_string(v,line,col,Tint,1,"1");
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_add);
          i++; col++;
        }
        break;
      case '-':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_asub);
          i+=2; col+=2;
        }else if(i+1<n && s[i+1]=='-'){
          push_token(v,line,col,Top,Top_asub);
          push_token_string(v,line,col,Tint,1,"1");
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_sub);
          i++; col++;
        }
        break;
      case '*':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_ampy);
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_mpy);
          i++; col++;
        }
        break;
      case '/':
        if(i+1<n && s[i+1]=='/'){
          if(i+2<n && s[i+2]=='='){
            push_token(v,line,col,Top,Top_aidiv);
            i+=3; col+=3;
          }else{
            push_token(v,line,col,Top,Top_idiv);
            i+=2; col+=2;
          }
        }else if(i+1<n && s[i+1]=='*'){
          i+=2;
          while(i+1<n){
            if(s[i]=='\n'){line++; col=0;}
            if(s[i]=='*' && s[i+1]=='/'){
              i+=2; col+=2;
              break;
            }
            i++; col++;
          }
        }else if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_adiv);
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_div);
          i++; col++;
        }
        break;
      case '%':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_amod);
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_mod);
          i++; col++;
        }
        break;
      case '^':
        push_token(v,line,col,Top,Top_pow);
        i++; col++;
        break;
      case '<':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_le);
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_lt);
          i++; col++;
        }
        break;
      case '>':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_ge);
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_gt);
          i++; col++;
        }
        break;
      case '|':
        push_token(v,line,col,Top,Top_bor);
        i++; col++;
        break;
      case '&':
        push_token(v,line,col,Top,Top_band);
        i++; col++;
        break;
      case '$':
        push_token(v,line,col,Top,Top_bxor);
        i++; col++;
        break;
      case '?':
        push_token(v,line,col,Top,Top_qm);
        i++; col++;
        break;
      case '(': case '[': case '{':
        if(sub_syntax(v)){
        }else if(c=='(' && v->size>0){
          mt_token* pt = mf_vec_get(v,-1);
          if(pt->type==Tid || pt->type==Tbright){
            push_token(v,line,col,Tapp,0);
          }else if(pt->line==line && pt->type==Tkeyword && pt->value==Tend){
            push_token(v,line,col,Tapp,0);
          }
        }
        push_token(v,line,col,Tbleft,c);
        i++; col++;
        input_nesting++;
        break;
      case ')': case ']': case '}':
        if(v->size>0){
          mt_token* t = mf_vec_get(v,-1);
          if(t->type==Top && t->value==Top_range){
            push_token(v,line,col,Tnull,0);
          }
        }
        push_token(v,line,col,Tbright,c);
        i++; col++;
        input_nesting--;
        break;
      case ',': case ';':
        push_token(v,line,col,Tsep,c);
        i++; col++;
        break;
      case ':':
        if(v->size>0){
          mt_token* t = mf_vec_get(v,-1);
          if(t->type==Top && t->value==Top_range){
            push_token(v,line,col,Tnull,0);
          }
        }
        push_token(v,line,col,Tsep,c);
        i++; col++;
        break;
      case '.':
        if(i+1<n && s[i+1]=='.'){
          if(v->size>0){
            mt_token* t = mf_vec_get(v,-1);
            if(t->type==Tbleft){
              push_token(v,line,col,Tnull,0);
            }
          }
          push_token(v,line,col,Top,Top_range);
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_dot);
          i++; col++;
        }
        break;
      case '"':
        hcol=col;
        if(i+2<n && s[i+1]=='"' && s[i+2]=='"'){
          i+=3; col+=3; j=i;
          while(1){
            if(i+2>=n){
              syntax_error(line,col,"expected end of multiline string literal.");
              goto error;
            }if(s[i]=='"' && s[i+1]=='"' && s[i+2]=='"'){
              break;
            }else if(s[i]=='\n'){
              line++; col=0;
            }
            i++; col++;
          }
          push_token_string(v,line,hcol,Tstring,i-j,(char*)s+j);
          i+=3; col+=3;
        }else{
          i++; col++; j=i;
          while(1){
            if(i>=n || s[i]=='\n'){
              syntax_error(line,col,"expected end of string literal.");
              goto error;
            }else if(s[i]=='"'){
              break;
            }
            i++; col++;
          }
          push_token_string(v,line,hcol,Tstring,i-j,(char*)s+j);
          i++; col++;
        }
        break;
      case '\'':
        hcol=col;
        i++; col++; j=i;
        while(1){
          if(i>=n || s[i]=='\n'){
            syntax_error(line,col,"expected end of string literal.");
            goto error;
          }else if(s[i]=='\''){
            break;
          }
          i++; col++;
        }
        push_token_string(v,line,hcol,Tstring,i-j,(char*)s+j);
        i++; col++;
        break;
      case '=':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_eq);
          i+=2; col+=2;
        }else{
          push_token(v,line,col,Top,Top_assignment);
          i++; col++;
        }
        break;
      case '!':
        if(i+1<n && s[i+1]=='='){
          push_token(v,line,col,Top,Top_ne);
          i+=2; col+=2;
        }else{
          unexpected_character(line,col,'!');
          goto error;
        }
        break;
      case '~':
        push_token(v,line,col,Top,Top_tilde);
        i++; col++;
        break;
      case '\\':
        i++; col++;
        while(i<n){
          if(s[i]=='\n'){
            i++; col=0; line++;
            break;
          }else if(s[i]==' '){
            i++; col++;
          }else{
            break;
          }
        }
        break;
      case '\r':
        i++;
        break;
      default:
        unexpected_character(line,col,c);
        goto error;
      }
    }
  }
  return 0;
  error:
  return 1;
}

// section
// syntactic analysis

static
void ast_delete(ast_node* t){
  if(t==NULL) return;
  int i;
  for(i=0; i<t->n; i++){
    ast_delete(t->a[i]);
  }
  if(t->ownership){
    mf_free(t->s);
  }
  mf_free(t);
}

static
void ast_buffer_delete(mt_vec* v){
  int size = v->size;
  ast_node** a = (ast_node**)v->a;
  int i;
  for(i=0; i<size; i++){
    ast_delete(a[i]);
  }
  mf_vec_delete(v);
}

static
ast_node* ast_new(long n, int line, int col){
  ast_node* y;
  y = mf_malloc(sizeof(ast_node)+n*sizeof(ast_node*));
  y->n=n;
  y->line=line;
  y->col=col;
  y->ownership=0;
  return y;
}

ast_node* ast_buffer_to_node(long size, ast_node** a, int line, int col){
  ast_node* y = ast_new(size,line,col);
  ast_node** b=y->a;
  long k;
  for(k=0; k<size; k++){
    b[k]=a[k];
  }
  return y;
}

ast_node* ast_construct_list(long size, ast_node** a, int line, int col){
  ast_node* y = ast_new(size,line,col);
  y->type=Top; y->value=Top_list;
  long k;
  for(k=0; k<size; k++){
    y->a[k]=a[k];
  }
  return y;
}

static
ast_node* unary_operator(mt_token* t, ast_node* p){
  ast_node* op;
  op = mf_malloc(sizeof(ast_node)+sizeof(ast_node*));
  op->line=t->line;
  op->col=t->col;
  op->type=Top;
  op->value=t->value;
  op->n=1;
  op->a[0]=p;
  op->ownership=0;
  return op;
}

static
ast_node* binary_operator(mt_token* t, ast_node* p1, ast_node* p2){
  ast_node* op;
  op = mf_malloc(sizeof(ast_node)+2*sizeof(ast_node*));
  op->line=t->line;
  op->col=t->col;
  op->type=Top;
  op->value=t->value;
  op->n=2;
  op->a[0]=p1;
  op->a[1]=p2;
  op->ownership=0;
  return op;
}

static
ast_node* ternary_operator(mt_token* t,
  ast_node* p1, ast_node* p2, ast_node* p3
){
  ast_node* op;
  op = mf_malloc(sizeof(ast_node)+3*sizeof(ast_node*));
  op->line=t->line;
  op->col=t->col;
  op->type=Top;
  op->value=t->value;
  op->n=3;
  op->a[0]=p1;
  op->a[1]=p2;
  op->a[2]=p3;
  op->ownership=0;
  return op;
}

static
ast_node* expression_statement(ast_node* p){
  ast_node* y = ast_new(1,p->line,p->col);
  y->type = Tstatement;
  y->info=POP_VALUE;
  y->a[0]=p;
  return y;
}

static
mt_token* get_any_token(token_iterator* i){
  if(i->index>=i->size){
    syntax_error(i->a[i->index-1].line,i->a[i->index-1].col,"expected a token.");
    return NULL;
  }else{
    return i->a+i->index;
  }
}

static
mt_token* get_token(token_iterator* i){
  while(1){
    if(i->index>=i->size){
      syntax_error(i->a[i->index-1].line,i->a[i->index-1].col,"expected a token.");
      return NULL;
    }else if(bstack_sp>0 && bstack[bstack_sp-1]=='b'){
      mt_token* t = i->a+i->index;
      if(t->type==Tsep && t->value=='n'){
        i->index++;
        continue;
      }
      return t;
    }else{
      return i->a+i->index;
    }
  }
}

static
mt_token* lookup_token(token_iterator* i){
  while(1){
    if(i->index>=i->size){
      return NULL;
    }else if(bstack_sp>0 && bstack[bstack_sp-1]=='b'){
      mt_token* t = i->a+i->index;
      if(t->type==Tsep && t->value=='n'){
        i->index++;
        continue;
      }
      return t;
    }else{
      return i->a+i->index;
    }
  }
}

static
int end_of(token_iterator* i, int value, const char* s){
  if(i->index>=i->size)return 0;
  mt_token* t=i->a+i->index;
  if(t->type!=Tkeyword || t->value!=Tof){
    return 0;
  }
  i->index++;
  if(i->index>=i->size){
    syntax_error(t->line,t->col,s);
    return 1;
  }
  t=i->a+i->index;
  if(t->type!=Tkeyword || t->value!=value){
    syntax_error(t->line,t->col,s);
    return 1;
  }else{
    i->index++;
    return 0;
  }
}

static
ast_node* list_literal(token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  ast_node* p;
  mt_token* t1 = i->a+i->index;
  i->index++;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    if(t->type==Tbright && t->value==']'){
      i->index++;
      goto end_of_loop;
    }
  }
  while(1){
    p = expression(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    if(i->index>=i->size){
      syntax_error(t1->line,t1->col,"expected ']'.");
      goto error;
    }
    mt_token* t = i->a+i->index;
    if(t->type==Tsep && t->value==','){
      i->index++;
    }else if(t->type==Tbright && t->value==']'){
      i->index++;
      break;
    }else{
      syntax_error(t->line,t->col,"expected ']'.");
      goto error;
    }
  }
  ast_node* y;
  end_of_loop:
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Top; y->value=Top_list;
  mf_vec_delete(&v);
  return y;
  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* map_literal(token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  ast_node *p1,*p2;
  mt_token* t;
  mt_token* t1 = i->a+i->index;
  i->index++;
  t = get_token(i);
  if(t==NULL) goto error;
  if(t->type==Tbright && t->value=='}'){
    i->index++;
    goto end_of_loop;
  }
  while(1){
    p1 = expression(i);
    if(p1==NULL) goto error;
    mf_vec_push(&v,&p1);
    t = get_token(i);
    if(t->type!=Tsep || t->value!=':'){
      if(t->type==Top && t->value==Top_assignment){
        if(p1->type==Tid){
          p1->type=Tstring;
        }else{
          syntax_error(t->line,t->col,"expected identifier before '='.");
          goto error;
        }
      }else if((t->type==Tsep && t->value==',')
        || (t->type==Tbright && t->value=='}')
      ){
        p2 = ast_new(0,t->line,t->col);
        p2->type=Tnull;
        mf_vec_push(&v,&p2);
        goto comma;
      }else{
        syntax_error(t->line,t->col,"expected ':'.");
        goto error;
      }
    }
    i->index++;
    p2 = expression(i);
    if(p2==NULL) goto error;
    mf_vec_push(&v,&p2);
    t = get_token(i);
    comma:
    if(t->type==Tbright && t->value=='}'){
      i->index++;
      break;
    }else if(t->type!=Tsep || t->value!=','){
      syntax_error(t->line,t->col,"expected ',' or '}'.");
      goto error;
    }
    i->index++;
  }

  ast_node* y;
  end_of_loop:
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Top; y->value=Top_map;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* object_literal(token_iterator* i){
  mt_token* t1 = i->a+i->index;
  i->index++;
  int prototype=0;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  mt_token* t;
  t = get_token(i);
  if(t){
    if(t->type==Tbleft && t->value=='['){
      prototype=1;
      i->index++;
      ast_node* p = expression(i);
      if(p==NULL) goto error;
      mf_vec_push(&v,&p);
      t = get_token(i);
      if(t==NULL) goto error;
      if(t->type!=Tbright || t->value!=']'){
        syntax_error(t->line,t->col,"expected ']'.");
        goto error;
      }
      i->index++;
    }
  }
  t = get_token(i);
  if(t->type==Tkeyword && t->value==Tend){
    goto keyword_end;
  }
  while(1){
    ast_node* p1 = expression(i);
    if(p1==NULL) goto error;
    mf_vec_push(&v,&p1);
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type!=Tsep || t->value!=':'){
      if(t->type==Top && t->value==Top_assignment){
        if(p1->type==Tid){
          p1->type=Tstring;
        }else{
          syntax_error(t->line,t->col,"expected identifier before '='.");
          goto error;
        }
      }else{
        syntax_error(t->line,t->col,"expected '=' or ':'.");
        goto error;
      }
    }
    i->index++;
    ast_node* p2 = expression(i);
    if(p2==NULL) goto error;
    mf_vec_push(&v,&p2);
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tsep && t->value==','){
      i->index++;
      continue;
    }else if(t->type==Tkeyword && t->value==Tend){
      keyword_end:
      i->index++;
      if(end_of(i,Ttable,"expected 'end of table'.")){
        goto error;
      }
      break;
    }else{
      syntax_error(t->line,t->col,"expected ',' or 'end'.");
      goto error;
    }
  }
  ast_node* y;
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Tkeyword;
  y->value=Ttable;
  y->info=prototype;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* function_application(ast_node* p1, token_iterator* i){
  int self=0;
  mt_token* t1;
  if(i->index<i->size){
    t1=i->a+i->index;
    if(t1->type!=Tbleft || t1->value!='('){
      syntax_error(t1->line,t1->col,"expected '('.");
      return NULL;
    }
  }else{
    if(i->size>0){
      t1=i->a+i->size-1;
      syntax_error(t1->line,t1->col,"expected '('.");
    }else{
      syntax_error(-1,-1,"expected '('.");
    }
    return NULL;
  }
  ast_node* p;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  mf_vec_push(&v,&p1);

  i->index++;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    if(t->type==Tbright && t->value==')'){
      i->index++;
      goto end_of_loop;
    }
  }
  while(1){
    p = expression(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    mt_token* t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tsep && t->value==','){
      i->index++;
    }else if(t->type==Tsep && t->value==';'){
      self=1;
      i->index++;
      t = get_token(i);
      if(t==NULL) goto error;
      if(t->type==Tbright && t->value==')'){
        i->index++;
        break;
      }
    }else if(t->type==Tbright && t->value==')'){
      i->index++;
      break;
    }else{
      syntax_error(t1->line,t1->col,"expected ')'.");
      goto error;
    }
  }
  ast_node* y;
  end_of_loop:
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Tapp;
  y->value=0;
  y->info=self;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* if_expression(token_iterator* i){
  mt_token* t1 = i->a+i->index;
  i->index++;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  ast_node* p;
  mt_token* t;
  while(1){
    p = expression(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type!=Tkeyword || t->value!=Tthen){
      syntax_error(t->line,t->col,"expected keyword 'then'.");
      goto error;
    }
    i->index++;
    p = comma_expression(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tkeyword && t->value==Telif){
      i->index++;
      continue;
    }else if(t->type==Tkeyword && t->value==Telse){
      i->index++;
      break;
    }else{
      syntax_error(t->line,t->col,"expected keyword 'elif' or 'else'.");
      goto error;
    }
  }
  p = comma_expression(i);
  if(p==NULL) goto error;
  mf_vec_push(&v,&p);
  t = get_token(i);
  if(t==NULL) goto error;
  if(t->type!=Tkeyword || t->value!=Tend){
    syntax_error(t->line,t->col,"expected keyword 'end'.");
    goto error;
  }
  i->index++;
  if(end_of(i,Tend,"expected 'end of if'")){
    goto error;
  }
  ast_node* y;
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Top;
  y->value=Top_if;
  y->info=HAS_ELSE_CASE;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* concise_function_literal(token_iterator* i){
  mt_token* t1=i->a+i->index;
  i->index++;
  ast_node* p = argument_list(i);
  if(p==NULL) return NULL;
  ast_node* b = expression(i);
  if(b==NULL){
    ast_delete(p);
    return NULL;
  }
  ast_node* s;
  s = ast_new(1,b->line,b->col);
  s->type=Tstatement; s->value=1;
  s->info=RETURN_VALUE; s->a[0]=b;

  ast_node* y;
  y = ast_new(2,t1->line,t1->col);
  y->type=Tkeyword; y->value=Tsub;
  y->a[0]=p; y->a[1]=s;
  y->s=NULL;
  return y;
}

static
ast_node* atom(token_iterator* i){
  if(i->index>=i->size){
    if(i->size>0){
      mt_token* t = i->a+i->size-1;
      syntax_error(t->line,t->col,"expected operand.");
    }else{
      syntax_error(-1,-1,"expected operand.");
    }
    return NULL;
  }
  ast_node* p;
  mt_token* t = i->a+i->index;
  int type=t->type;
  int value=t->value;
  if(type==Tid || type==Tint || type==Tfloat || type==Timag ||
    type==Tstring || type==Tbstring || type==Tnull
  ){
    p = ast_new(0,t->line,t->col);
    p->type=t->type;
    p->s=t->s;
    p->size=t->size;
    i->index++;
  }else if(type==Tbool){
    p = ast_new(0,t->line,t->col);
    p->type=t->type;
    p->value=t->value;
    i->index++;
  }else if(type==Tbleft && value=='('){
    i->index++;
    bstack[bstack_sp]='b';
    bstack_sp++;
    p = comma_expression(i);
    if(p==NULL){
      bstack_sp--;
      return NULL;
    }
    if(comma_found){
      p->value=Top_tuple;
    }
    mt_token* t2 = get_token(i);
    bstack_sp--;
    if(t2==NULL) return NULL;
    if(t2->type!=Tbright || t2->value!=')'){
      syntax_error(t2->line,t2->col,"expected ')'.");
      ast_delete(p);
      return NULL;
    }else{
      i->index++;
    }
  }else if(type==Tbleft && value=='['){
    bstack[bstack_sp]='b';
    bstack_sp++;
    p = list_literal(i);
    bstack_sp--;
  }else if(type==Tbleft && value=='{'){
    bstack[bstack_sp]='b';
    bstack_sp++;
    p = map_literal(i);
    bstack_sp--;
  }else if(type==Tkeyword && value==Ttable){
    bstack[bstack_sp]='b';
    bstack_sp++;
    p = object_literal(i);
    bstack_sp--;
  }else if(type==Tkeyword && value==Tif){
    bstack[bstack_sp]='b';
    bstack_sp++;
    p = if_expression(i);
    bstack_sp--;
  }else if(type==Tkeyword && value==Tsub){
    p = function_literal(i);
  }else if(type==Top && value==Top_bor){
    p = concise_function_literal(i);
  }else{
    syntax_error(t->line,t->col,"expected operand.");
    return NULL;
  }
  return p;
}

static
ast_node* index_list(mt_token* bleft, ast_node* p1, token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  mf_vec_push(&v,&p1);
  while(1){
    ast_node* p = expression(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    mt_token* t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tbright && t->value==']'){
      i->index++;
      break;
    }else if(t->type==Tsep && t->value==','){
      i->index++;
      continue;
    }else{
      syntax_error(t->line,t->col,"expected ',' or ']'.");
      goto error;
    }
  }
  ast_node* op;
  op = ast_buffer_to_node(v.size,(ast_node**)v.a,bleft->line,bleft->col);
  op->type=Top; op->value=Top_index;
  mf_vec_delete(&v);
  return op;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* atomic_expression(token_iterator* i){
  ast_node* p1 = atom(i);
  if(p1==NULL) return NULL;
  while(i->index<i->size){
    mt_token* t = i->a+i->index;
    if(t->type==Tapp){
      i->index++;
      bstack[bstack_sp]='b';
      bstack_sp++;
      p1=function_application(p1,i);
      bstack_sp--;
      if(p1==NULL) return NULL;
    }else if(t->type==Tbleft && t->value=='['){
      i->index++;
      bstack[bstack_sp]='b';
      bstack_sp++;
      p1=index_list(t,p1,i);
      bstack_sp--;
      if(p1==NULL) return NULL;
    }else if(t->type==Top && t->value==Top_dot){
      i->index++;
      ast_node* p2 = atom(i);
      if(p2==NULL) goto error;
      p1=binary_operator(t,p1,p2);
      if(p1==NULL) return NULL;
    }else{
      break;
    }
  }
  return p1;
  error:
  ast_delete(p1);
  return NULL;
}

static
ast_node* power(token_iterator* i){
  ast_node* p1 = atomic_expression(i);
  if(p1==NULL) return NULL;
  mt_token* t = lookup_token(i);
  if(t){
    if(t->type==Top && t->value==Top_pow){
      i->index++;
      ast_node* p2 = factor(i);
      if(p2==NULL){
        ast_delete(p1);
        return NULL;
      }
      ast_node* op = binary_operator(t,p1,p2);
      return op;
    }
  }
  return p1;
}

static
ast_node* factor(token_iterator* i){
  mt_token* t = get_token(i);
  if(t==NULL) return NULL;
  ast_node *p,*op;
  if(t->type==Top){
    int value=t->value;
    if(value==Top_sub){
      t->value=Top_neg;
      i->index++;
      p = factor(i);
      if(p==NULL) return NULL;
      op = unary_operator(t,p);
      return op;
    }else if(value==Top_add){
      i->index++;
      return factor(i);
    }else if(value==Top_tilde){
      i->index++;
      p = factor(i);
      if(p==NULL) return NULL;
      op = unary_operator(t,p);
      return op;
    }else{
      goto pow;
    }
  }else{
    pow:
    return power(i);
  }
}

static
ast_node* term(token_iterator* i){
  ast_node* p1=factor(i);
  if(p1==NULL) return NULL;
  while(1){
    mt_token* t = lookup_token(i);
    if(t==NULL) break;
    if(t->type!=Top) break;
    int value=t->value;
    if(value!=Top_mpy && value!=Top_div && value!=Top_idiv &&
      value!=Top_mod
    ){
      break;
    }
    i->index++;
    ast_node* p2 = factor(i);
    if(p2==NULL){
      ast_delete(p1);
      return NULL;
    }
    p1 = binary_operator(t,p1,p2);
  }
  return p1;
}

static
ast_node* pm_expression(token_iterator* i){
  ast_node* p1=term(i);
  if(p1==NULL) return NULL;
  while(1){
    mt_token* t = lookup_token(i);
    if(t==NULL) break;
    if(t->type!=Top) break;
    if(t->value!=Top_add && t->value!=Top_sub){
      break;
    }
    i->index++;
    ast_node* p2 = term(i);
    if(p2==NULL){
      ast_delete(p1);
      return NULL;
    }
    p1 = binary_operator(t,p1,p2);
  }
  return p1;
}

static
ast_node* band_expression(token_iterator *i){
  ast_node* p1=pm_expression(i);
  if(p1==NULL) return NULL;
  while(1){
    mt_token* t = lookup_token(i);
    if(t==NULL) break;
    if(t->type!=Top) break;
    if(t->value!=Top_band){
      break;
    }
    i->index++;
    ast_node* p2 = pm_expression(i);
    if(p2==NULL){
      ast_delete(p1);
      return NULL;
    }
    p1 = binary_operator(t,p1,p2);
  }
  return p1;
}

static
ast_node* bor_expression(token_iterator *i){
  ast_node* p1=band_expression(i);
  if(p1==NULL) return NULL;
  while(1){
    mt_token* t = lookup_token(i);
    if(t==NULL) break;
    if(t->type!=Top) break;
    if(t->value!=Top_bor && t->value!=Top_bxor){
      break;
    }
    i->index++;
    ast_node* p2 = band_expression(i);
    if(p2==NULL){
      ast_delete(p1);
      return NULL;
    }
    p1 = binary_operator(t,p1,p2);
  }
  return p1;
}

static
ast_node* range_expression(token_iterator* i){
  ast_node* p1 = bor_expression(i);
  if(p1==NULL) return NULL;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    if(t->type==Top && t->value==Top_range){
      i->index++;
      ast_node* p2 = bor_expression(i);
      if(p2==NULL){
        ast_delete(p1);
        return NULL;
      }
      if(i->index<i->size){
        mt_token* t2 = i->a+i->index;
        if(t2->type==Tsep && t2->value==':'){
          i->index++;
          ast_node* p3 = bor_expression(i);
          if(p3==NULL){
            ast_delete(p1);
            ast_delete(p2);
            return NULL;
          }
          return ternary_operator(t,p1,p2,p3);
        }
      }
      return binary_operator(t,p1,p2);
    }
  }
  return p1;
}

static
ast_node* cmp_expression(token_iterator* i){
  ast_node* p1 = range_expression(i);
  if(p1==NULL) return NULL;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    int value=t->value;
    if(t->type==Top && (value==Top_lt || value==Top_gt ||
      value==Top_le || value==Top_ge)
    ){
      i->index++;
      ast_node* p2 = range_expression(i);
      if(p2==NULL){
        ast_delete(p1);
        return NULL;
      }
      return binary_operator(t,p1,p2);
    }
  }
  return p1;
}

static
ast_node* eq_expression(token_iterator* i){
  ast_node* p1 = cmp_expression(i);
  if(p1==NULL) return NULL;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    int value=t->value;
    if(t->type==Top && (value==Top_eq || value==Top_ne ||
      value==Top_is || value==Top_in || value==Top_notin ||
      value==Top_isin)
    ){
      i->index++;
      ast_node* p2 = cmp_expression(i);
      if(p2==NULL){
        ast_delete(p1);
        return NULL;
      }
      return binary_operator(t,p1,p2);
    }
  }
  return p1;
}

static
ast_node* not_expression(token_iterator* i){
  mt_token* t = get_token(i);
  if(t==NULL) return NULL;
  if(t->type==Top && t->value==Top_not){
    i->index++;
    ast_node* p = eq_expression(i);
    if(p==NULL) return NULL;
    return unary_operator(t,p);
  }else{
    return eq_expression(i);
  }
}

static
ast_node* and_expression(token_iterator* i){
  ast_node* p1=not_expression(i);
  if(p1==NULL) return NULL;
  while(i->index<i->size){
    mt_token* t = i->a+i->index;
    if(t->type!=Top) break;
    if(t->value!=Top_and){
      break;
    }
    i->index++;
    ast_node* p2 = not_expression(i);
    if(p2==NULL){
      ast_delete(p1);
      return NULL;
    }
    p1 = binary_operator(t,p1,p2);
  }
  return p1;
}

static
ast_node* qm_expression(ast_node* p1, token_iterator* i){
  mt_token* t1 = i->a+i->index;
  i->index++;
  ast_node* p2 = expression(i);
  if(p2==NULL) goto error1;
  mt_token* t = lookup_token(i);
  ast_node* y;
  if(t==NULL || t->type!=Tsep || t->value!=':'){
    y=ast_new(2,t1->line,t1->col);
    y->type=Top; y->value=Top_if;
    y->info=0;
    y->a[0]=p1;
    y->a[1]=p2;
    return y;
  }
  i->index++;
  ast_node* p3 = expression(i);
  if(p3==NULL) goto error2;
  y=ast_new(3,t1->line,t1->col);
  y->type=Top; y->value=Top_if;
  y->info=HAS_ELSE_CASE;
  y->a[0]=p1;
  y->a[1]=p2;
  y->a[2]=p3;
  return y;
  error2:
  ast_delete(p2);
  error1:
  ast_delete(p1);
  return NULL;
}

static
ast_node* or_expression(token_iterator* i){
  ast_node* p1=and_expression(i);
  if(p1==NULL) return NULL;
  while(1){
    mt_token* t = lookup_token(i);
    if(t==NULL) break;
    if(t->type!=Top) break;
    if(t->value==Top_qm){
      return qm_expression(p1,i);
    }
    if(t->value!=Top_or) break;
    i->index++;
    ast_node* p2 = and_expression(i);
    if(p2==NULL){
      ast_delete(p1);
      return NULL;
    }
    p1 = binary_operator(t,p1,p2);
  }
  return p1;
}

static
ast_node* expression(token_iterator* i){
  ast_node* p = or_expression(i);
  if(p==NULL) return NULL;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    if(t->type==Top && t->value==Top_assignment){
      return p;
    }
    if(t->type!=Tbright && t->type!=Tsep && t->type!=Tkeyword){
      syntax_error(t->line,t->col,"expected end of expression.");
      return NULL;
    }
  }
  return p;
}

static
ast_node* comma_list(ast_node* p1, token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  mf_vec_push(&v,&p1);
  ast_node* p;
  int type,value;
  while(1){
    p = expression(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    if(i->index<i->size){
      mt_token* t = i->a+i->index;
      type=t->type; value=t->value;
      if(type==Tsep && value==','){
        i->index++;
        continue;
      }else if(type==Tbright || type==Tsep){
        break;
      }else if(type==Top && value==Top_assignment){
        break;
      }else if(type==Tkeyword &&
        (value==Tend || value==Telse || value==Telif || value==Tdo)
      ){
        break;
      }else{
        syntax_error(t->line,t->col,"expected end of comma separated list.");
        goto error;
      }
    }else{
      break;
    }
  }
  // todo
  ast_node* y;
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,-1,-1);
    y->type=Top;
  y->value=Top_list;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* comma_expression(token_iterator* i){
  ast_node* p1 = or_expression(i);
  if(p1==NULL) return NULL;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    if(t->type==Tsep && t->value==','){
      i->index++;
      ast_node* p2 = comma_list(p1,i);
      if(p2==NULL) return NULL;
      comma_found=1;
      return p2;
    }
  }
  comma_found=0;
  return p1;
}

static
ast_node* assignment(token_iterator* i){
  ast_node* p1 = comma_expression(i);
  if(p1==NULL) return NULL;
  if(i->index<i->size){
    mt_token* t = i->a+i->index;
    int value=t->value;
    if(t->type==Top &&
      (value==Top_assignment || value==Top_aadd || value==Top_asub ||
      value==Top_ampy || value==Top_adiv || value==Top_amod ||
      value==Top_aidiv)
    ){
      i->index++;
      ast_node* p2 = comma_expression(i);
      if(p2==NULL) goto error;
      return binary_operator(t,p1,p2);
    }
  }
  return expression_statement(p1);
  error:
  ast_delete(p1);
  return NULL;
}

static
ast_node* while_statement(token_iterator* i){
  mt_token* t1 = i->a+i->index;
  i->index++;
  ast_node* p1 = expression(i);
  if(p1==NULL) return NULL;
  if(i->index>=i->size){
    syntax_error(i->a[i->index-1].line,i->a[i->index-1].col,
      "expected keyword 'do' or newline.");
    goto error1;
  }
  mt_token* t = i->a+i->index;
  if((t->type!=Tsep || t->value!='n') &&
    (t->type!=Tkeyword || t->value!=Tdo)
  ){
    syntax_error(t->line,t->col,"expected keyword 'do' or newline.");
    goto error1;
  }
  i->index++;
  ast_node* p2 = statement_list(i);
  if(p2==NULL) goto error1;
  if(i->index>=i->size){
    syntax_error(i->a[i->index-1].line,i->a[i->index-1].col,
      "expected keyword 'end'.");
    goto error2;
  }
  t = i->a+i->index;
  if(t->type!=Tkeyword || t->value!=Tend){
    syntax_error(t->line,t->col,"exptected keyword 'end'.");
    goto error2;
  }
  i->index++;
  if(end_of(i,Twhile,"expected 'end of while'.")){
    goto error2;
  }
  ast_node* y;
  y = ast_new(2,t1->line,t1->col);
  y->type = Tkeyword;
  y->value = Twhile;
  y->size=0;
  y->a[0]=p1;
  y->a[1]=p2;
  return y;
  error2:
  ast_delete(p2);
  error1:
  ast_delete(p1);
  return NULL;
}

ast_node* identifier(mt_token* t){
  if(t->type!=Tid){
    syntax_error(t->line,t->col,"compiler error: expected identifier.");
    exit(1);
  }
  ast_node* y = ast_new(0,t->line,t->col);
  y->type=Tid; y->s=t->s; y->size=t->size;
  return y;
}

static
ast_node* for_statement(token_iterator* i){
  mt_token* t1 = i->a+i->index;
  i->index++;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  mt_token* t;
  while(1){
    t = get_token(i);
    if(t==NULL) goto error1;
    if(t->type!=Tid){
      syntax_error(t->line,t->col,"expected identifier.");
      return NULL;
    }
    ast_node* id = identifier(t);
    mf_vec_push(&v,&id);
    i->index++;
    t = get_token(i);
    if(t==NULL) goto error1;
    if(t->type==Tsep && t->value==','){
      i->index++;
      continue;
    }else if(t->type==Top && t->value==Top_in){
      i->index++;
      break;
    }else{
      syntax_error(t->line,t->col,"expected ',' or 'in'.");
      goto error1;
    }
  }
  ast_node* p = comma_expression(i);
  if(p==NULL) goto error1;
  t=get_any_token(i);
  if(t==NULL) goto error2;
  if(t->type==Tsep && t->value=='n'){
    i->index++;
  }else if(t->type==Tkeyword && t->value==Tdo){
    i->index++;
  }else{
    syntax_error(t->line,t->col,"expected newline or 'do'.");
    goto error2;
  }
  ast_node* b = statement_list(i);
  if(b==NULL) goto error2;
  t = get_token(i);
  if(t==NULL) goto error3;
  if(t->type!=Tkeyword || t->value!=Tend){
    syntax_error(t->line,t->col,"expected 'end' (of for loop).");
    goto error3;
  }
  i->index++;
  if(end_of(i,Tfor,"expected 'end of for'.")){
    ast_delete(p);
    goto error3;
  }
  ast_node* y;
  y = ast_new(3,t1->line,t1->col);
  y->type=Tkeyword; y->value=Tfor;
  y->a[0] = ast_construct_list(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->a[1] = p;
  y->a[2] = b;
  mf_vec_delete(&v);
  return y;
  error3:
  ast_delete(b);
  error2:
  ast_delete(p);
  error1:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* if_statement(token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  mt_token* t1 = i->a+i->index;
  i->index++;
  int has_else_case=0;
  while(1){
    ast_node* p = expression(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    mt_token* t = get_any_token(i);
    if(t==NULL) goto error;
    if((t->type!=Tsep || t->value!='n') &&
      (t->type!=Tkeyword || t->value!=Tthen)
    ){
      syntax_error(t->line,t->col,"expected 'then' or newline.");
      goto error;
    }
    i->index++;
    p = statement_list(i);
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tkeyword){
      if(t->value==Telif){
        i->index++;
        continue;
      }else if(t->value==Telse){
        has_else_case=1;
        i->index++;
        p = statement_list(i);
        if(p==NULL) goto error;
        mf_vec_push(&v,&p);
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type==Tkeyword && t->value==Tend){
          i->index++;
          if(end_of(i,Tif,"expected 'end of if'.")){
            goto error;
          }
          break;
        }else{
          syntax_error(t->line,t->col,"expected 'end'.");
          goto error;
        }
      }else if(t->value==Tend){
        i->index++;
        if(end_of(i,Tif,"expected 'end of if'.")){
          goto error;
        }
        break;
      }else{
        goto second_else;
      }
    }else{
      second_else:
      syntax_error(t->line,t->col,"expected 'elif', 'else', or 'end'.");
      goto error;
    }
  }
  ast_node* y;
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Tkeyword;
  y->value=Tif;
  y->info=has_else_case;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* try_statement(token_iterator* i){
  mt_token* t1 = i->a+i->index;
  i->index++;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  ast_node* p = statement_list(i);
  if(p==NULL) goto error;
  mf_vec_push(&v,&p);
  mt_token* t = get_token(i);
  if(t==NULL) goto error;
  if(t->type!=Tkeyword || t->value!=Tcatch){
    syntax_error(t->line,t->col,"expected 'catch'.");
    goto error;
  }
  i->index++;
  mt_token* id;
  ast_node* p1;
  ast_node* p2;
  int catch_type;
  while(1){
    id = get_token(i);
    if(id==NULL) goto error;
    if(id->type!=Tid){
      syntax_error(t->line,t->col,"expected identifer.");
      goto error;
    }
    i->index++;
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tsep && t->value==':'){
      i->index++;
      p1 = comma_expression(i);
      t = get_any_token(i);
      if(t==NULL) goto error;
      if(t->type!=Tsep || t->value!='n'){
        syntax_error(t->line,t->col,"expected newline.");
        ast_delete(p1);
        goto error;
      }
      i->index++;
      catch_type=CATCH_ID_EXP;
    }else{
      catch_type=CATCH_ID;
    }
    p2 = statement_list(i);
    if(p2==NULL){
      if(catch_type==CATCH_ID_EXP) ast_delete(p1);
      goto error;
    }
    p = identifier(id);
    ast_node* tuple[3];
    if(catch_type==CATCH_ID_EXP){
      tuple[0]=p; tuple[1]=p1; tuple[2]=p2;
      p = ast_buffer_to_node(3,tuple,id->line,id->col);
      p->type=Top; p->value=Top_list;
    }else{
      tuple[0]=p; tuple[1]=p2;
      p = ast_buffer_to_node(2,tuple,id->line,id->col);
      p->type=Top; p->value=Top_list;
    }
    p->info=catch_type;
    mf_vec_push(&v,&p);
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tkeyword && t->value==Tcatch){
      i->index++;
      continue;
    }else if(t->type==Tkeyword && t->value==Tend){
      i->index++;
      if(end_of(i,Ttry,"expected 'end of try'.")){
        goto error;
      }
      break;
    }
  }
  ast_node* y;
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Tkeyword; y->value=Ttry;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* break_statement(mt_token* t, token_iterator* i){
  i->index++;
  ast_node* y = ast_new(0,t->line,t->col);
  y->type=Tkeyword;
  y->value=Tbreak;
  return y;
}

static
ast_node* continue_statement(mt_token* t, token_iterator* i){
  i->index++;
  ast_node* y = ast_new(0,t->line,t->col);
  y->type=Tkeyword;
  y->value=Tcontinue;
  return y;
}

static
ast_node* return_statement(mt_token* t, token_iterator* i){
  i->index++;
  ast_node* p = comma_expression(i);
  if(p==NULL) return NULL;
  ast_node* y = ast_new(1,t->line,t->col);
  y->type=Tkeyword;
  y->value=Treturn;
  y->a[0]=p;
  return y;
}

static
ast_node* raise_statement(mt_token* t, token_iterator* i){
  i->index++;
  ast_node* p = comma_expression(i);
  if(p==NULL) return NULL;
  ast_node* y = ast_new(1,t->line,t->col);
  y->type=Tkeyword;
  y->value=Traise;
  y->a[0]=p;
  return y;
}

static
ast_node* goto_statement(mt_token* t1, token_iterator* i){
  i->index++;
  mt_token* t = get_token(i);
  if(t->type!=Tid){
    syntax_error(t->line,t->col,"expected identifier.");
    return NULL;
  }
  ast_node* y = ast_new(1,t1->line,t1->col);
  y->type=Tkeyword;
  y->value=Tgoto;
  y->a[0]=identifier(t);
  i->index++;
  return y;
}

static
ast_node* label_statement(mt_token* t1, token_iterator* i){
  i->index++;
  mt_token* t = get_token(i);
  if(t->type!=Tid){
    syntax_error(t->line,t->col,"expected identifier.");
    return NULL;
  }
  ast_node* y = ast_new(1,t1->line,t1->col);
  y->type=Tkeyword;
  y->value=Tlabel;
  y->a[0]=identifier(t);
  i->index++;
  return y;
}

static
ast_node* global_statement(token_iterator* i){
  mt_token* t1 = i->a+i->index;
  i->index++;
  mt_token* t;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  while(1){
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type!=Tid){
      syntax_error(t->line,t->col,"expected identifier.");
      goto error;
    }
    ast_node* id = ast_new(0,t->line,t->col);
    id->type=Tid; id->s=t->s; id->size=t->size;
    mf_vec_push(&v,&id);
    i->index++;
    if(i->index>=i->size) break;
    t = i->a+i->index;
    if(t->type==Tsep && t->value==','){
      i->index++;
      continue;
    }else{
      break;
    }
  }
  ast_node* y;
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Tkeyword; y->value=Tglobal;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* null_literal(mt_token *t){
  ast_node* p = ast_new(0,t->line,t->col);
  p->type=Tnull;
  return p;
}

static
ast_node* id_to_ast(char* id, mt_token* t){
  ast_node* y = ast_new(0,t->line,t->col);
  y->type=Tid; y->s=id; y->size=strlen(id);
  return y;
}

static
ast_node* construct_app(
  char* id, ast_node* arg1, ast_node* arg2, mt_token* t
){
  int n = arg2? 3: 2;
  ast_node* y = ast_new(n,t->line,t->col);
  y->type=Tapp; y->value=0;
  y->info=0;
  y->a[0]=id_to_ast(id,t);
  y->a[1]=arg1;
  if(arg2){
    y->a[2]=arg2;
  }
  return y;
}

static
ast_node* construct_statement(ast_node* p, mt_token* t){
  ast_node* y = ast_new(1,t->line,t->col);
  y->type=Tstatement;
  y->info=POP_VALUE;
  y->a[0]=p;
  return y;
}

static
ast_node* path(mt_token* t1, token_iterator* i){
  mt_bs bs;
  mt_token* t;
  t = get_token(i);
  if(t==NULL) return NULL;
  if(t->type==Tid){
    mf_bs_init(&bs);
    mf_bs_push(&bs,t->size,t->s);
  }else{
    syntax_error(t->line,t->col,"expected identifier.");
    return NULL;
  }
  i->index++;
  while(1){
    t = lookup_token(i);
    if(t==NULL) break;
    if(t->type!=Top || t->value!=Top_dot){
      break;
    }
    mf_bs_push(&bs,1,"/");
    i->index++;
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type!=Tid){
      syntax_error(t->line,t->col,"expected identifier.");
      goto error;
    }
    mf_bs_push(&bs,t->size,t->s);
    i->index++;
  }
  mf_bs_push(&bs,1,"\0");
  bs.size--;
  ast_node* y;
  y = ast_new(0,t1->line,t1->col);
  y->type=Tstring;
  y->size=bs.size;
  y->s = bs.a;
  y->ownership=1;
  return y;

  error:
  mf_bs_delete(&bs);
  return NULL;
}

static
int qualification;

static
ast_node* import_list(mt_token* t1, token_iterator* i){
  ast_node* p;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  qualification=0;
  while(1){
    mt_token* t = get_token(i);
    if(t==NULL) goto error;
    ast_node* id;
    if(t->type==Top && t->value==Top_mpy){
      id = ast_new(0,t->line,t->col);
      id->type=Tstring; id->s="*"; id->size=1;
      i->index++;
    }else if(t->type==Tid){
      // id = ast_new(0,t->line,t->col);
      // id->type=Tstring; id->s=t->s; id->size=t->size;
      // i->index++;
      id = path(t,i);
      if(id==NULL) goto error;
    }else{
      syntax_error(t->line,t->col,"expected identifier.");
      goto error;
    }
    mf_vec_push(&v,&id);
    mt_token* ta = lookup_token(i);
    if(ta==NULL){
      p = null_literal(t);
      mf_vec_push(&v,&p);
      break;
    }
    if(ta->type==Top && ta->value==Top_assignment){
      i->index++;
      t = get_token(i);
      if(t==NULL) goto error;
      if(t->type!=Tid){
        syntax_error(t->line,t->col,"expected identifier.");
        goto error;
      }

      // p = ast_new(0,t->line,t->col);
      // p->type=Tstring; p->s=t->s; p->size=t->size;
      // i->index++;
      p = path(t,i);
      mf_vec_push(&v,&p);

      ta = lookup_token(i);
      if(ta==NULL) break;
    }else{
      p = null_literal(ta);
      mf_vec_push(&v,&p);
    }
    if(ta->type==Tsep && ta->value=='n'){
      break;
    }else if(ta->type==Tsep && ta->value==':'){
      qualification=1;
      break;
    }else if(ta->type==Tbright && ta->value==')'){
      break;
    }else if(ta->type!=Tsep || ta->value!=','){
      syntax_error(ta->line,ta->col,"expected end of line or ','.");
      goto error;
    }
    i->index++;
  }

  ast_node* y;
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
  y->type=Top; y->value=Top_map;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* import_statement(mt_token* t1, token_iterator* i){
  i->index++;
  mt_token* t = get_token(i);
  int parens;
  if(t->type==Tbleft && t->value=='('){
    bstack[bstack_sp]='b';
    bstack_sp++;
    i->index++;
    parens=1;
  }else{
    parens=0;
  }
  ast_node* a = import_list(t1,i);
  if(a==NULL) return NULL;
  ast_node* b;
  if(qualification){
    i->index++;
    b=import_list(t1,i);
    if(b==NULL){
      ast_delete(a);
      return NULL;
    }
  }else{
    b=NULL;
  }
  if(parens){
    t = get_token(i);
    if(t->type!=Tbright || t->value!=')'){
      syntax_error(t->line,t->col,"expected ')'.");
      ast_delete(a);
      ast_delete(b);
      return NULL;
    }
    bstack_sp--;
    i->index++;
  }
  return construct_statement(
    construct_app("__import__",a,b,t1),t1
  );
}

static
ast_node* statement_list(token_iterator* i){
  mt_vec v;
  mf_vec_init(&v,sizeof(token_iterator*));
  ast_node* p;
  mt_token* t;
  int type,value;
  while(i->index<i->size){
    t = i->a+i->index;
    type=t->type;
    value=t->value;
    if(type==Tsep && (value==';' || value=='n')){
      i->index++;
      continue;
    }else if(type==Tkeyword &&
      (value==Tend || value==Telif || value==Telse || value==Tcatch)
    ){
      break;
    }
    if(type==Tkeyword && value==Twhile){
      p = while_statement(i);
    }else if(type==Tkeyword && value==Tfor){
      p = for_statement(i);
    }else if(type==Tkeyword && value==Tif){
      p = if_statement(i);
    }else if(type==Tkeyword && value==Tbreak){
      p = break_statement(t,i);
    }else if(type==Tkeyword && value==Tcontinue){
      p = continue_statement(t,i);
    }else if(type==Tkeyword && value==Treturn){
      p = return_statement(t,i);
    }else if(type==Tkeyword && value==Traise){
      p = raise_statement(t,i);
    }else if(type==Tkeyword && value==Tglobal){
      p = global_statement(i);
    }else if(type==Tkeyword && value==Ttry){
      p = try_statement(i);
    }else if(type==Tkeyword && value==Tgoto){
      p = goto_statement(t,i);
    }else if(type==Tkeyword && value==Tlabel){
      p = label_statement(t,i);
    }else if(type==Tkeyword && value==Timport){
      p = import_statement(t,i);
    }else{
      p = assignment(i);
    }
    if(p==NULL) goto error;
    mf_vec_push(&v,&p);
    if(i->index<i->size){
      t = i->a+i->index;
      if(t->type==Tsep && (t->value==';' || t->value=='n')){
        i->index++;
        continue;
      }else if(t->type==Tkeyword &&
        (t->value==Tend || t->value==Telif || t->value==Telse || t->value==Tcatch)
      ){
        break;
      }else{
        syntax_error(t->line,t->col,"expected end of statement.");
        goto error;
      }
    }
  }
  ast_node* y;
  if(v.size==1){
    y = ((ast_node**)v.a)[0];
    mf_vec_delete(&v);
    return y;
  }
  // todo
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,-1,-1);
  y->type=Tblock;
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* argument_list(token_iterator* i){
  ast_node* y;
  mt_vec v;
  mf_vec_init(&v,sizeof(ast_node*));
  int argc=0;
  mt_token* t = get_token(i);
  if(t==NULL) goto error;
  if((t->type==Top && t->value==Top_bor) ||
    (t->type==Tbright && t->value==')')
  ){
    i->index++;
    y = ast_new(0,-1,-1);
    y->type=Top; y->value=Top_list;
    y->info=0;
    mf_vec_delete(&v);
    return y;
  }
  while(1){
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Top && t->value==Top_mpy){
      argc=-v.size-1;
      i->index++;
    }
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tid){
      ast_node* id = ast_new(0,t->line,t->col);
      id->type=Tid;
      id->s=t->s;
      id->size=t->size;
      mf_vec_push(&v,&id);
    }else{
      syntax_error(t->line,t->col,"expected identifier.");
      goto error;
    }
    i->index++;
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tsep && t->value==','){
      if(argc!=0){
        syntax_error(t->line,t->col,"variadic argument must be the last one.");
        goto error;
      }
      i->index++;
      continue;
    }else if((t->type==Top && t->value==Top_bor) ||
      (t->type==Tbright && t->value==')')
    ){
      i->index++;
      break;
    }else{
      syntax_error(t->line,t->col,"expected ',' or '|'.");
      goto error;
    }
  }
  // todo
  y = ast_buffer_to_node(v.size,(ast_node**)v.a,-1,-1);
  y->type=Top;
  y->value=Top_list;
  if(argc==0){
    y->info=v.size;
  }else{
    y->info=argc;
  }
  mf_vec_delete(&v);
  return y;

  error:
  ast_buffer_delete(&v);
  return NULL;
}

static
ast_node* function_literal(token_iterator* i){
  mt_token* t1=i->a+i->index;
  i->index++;
  mt_token* t = get_token(i);
  if(t==NULL) return NULL;
  mt_token* id;
  if(t->type==Tid){
    id=t; i->index++;
  }else{
    id=NULL;
  }
  t = get_token(i);
  if(t==NULL) return NULL;
  ast_node* p;
  if(t->type==Top && t->value==Top_step){
    i->index++;
    p = ast_new(0,t1->line,t1->col);
    p->type=Top; p->value=Top_list;
    p->info=0;
  }else if((t->type==Top && t->value==Top_bor) ||
    (t->type==Tbleft && t->value=='(')
  ){
    i->index++;
    p = argument_list(i);
    if(p==NULL) return NULL;
  }else{
    syntax_error(t->line,t->col,"expected '|'.");
    return NULL;
  }
  bstack[bstack_sp]='s';
  bstack_sp++;
  ast_node* b = statement_list(i);
  bstack_sp--;
  if(b==NULL){
    ast_delete(p);
    return NULL;
  }
  if(b->type==Tstatement){
    b->info=RETURN_VALUE;
  }else if(b->type==Tblock && b->n>0){
    ast_node* last = b->a[b->n-1];
    if(last->type==Tstatement){
      last->info=RETURN_VALUE;
    }
  }
  t = get_token(i);
  if(t==NULL) goto error;
  if(t->type!=Tkeyword || t->value!=Tend){
    syntax_error(t->line,t->col,"expected end of sub.");
    goto error;
  }
  i->index++;
  if(end_of(i,Tsub,"expected 'end of sub'.")){
    goto error;
  }
  ast_node* y;
  y = ast_new(2,t1->line,t1->col);
  y->type=Tkeyword;
  y->value=Tsub;
  y->a[0]=p;
  y->a[1]=b;
  if(id){
    y->s=id->s;
    y->size=id->size;
  }else{
    y->s=NULL;
  }
  return y;
  error:
  ast_delete(p);
  ast_delete(b);
  return NULL;
}

static
void print_space(int n){
  int i;
  for(i=0; i<n; i++){
    putchar(' ');
  }
}

static
ast_node* ast(mt_vec* v){
  token_iterator i;
  i.a=(mt_token*)v->a;
  i.size=v->size; i.index=0;
  return statement_list(&i);
}

static
void ast_print(ast_node* t, int indent){
  if(t==NULL){
    printf("Error: no AST.\n");
    return;
  }
  int type=t->type;
  int value=t->value;
  if(type==Tid || type==Tint || type==Tfloat){
    print_space(indent);
    print_string(t->s,t->size);
    printf("\n");
  }else if(type==Tbool){
    print_space(indent);
    if(value){
      printf("true\n");
    }else{
      printf("false\n");
    }
  }else if(type==Tnull){
    print_space(indent);
    printf("null\n");
  }else if(type==Tstring){
    print_space(indent);
    printf("\"");
    print_string(t->s,t->size);
    printf("\"\n");
  }else if(type==Tbstring){
    print_space(indent);
    printf("b\"");
    print_string(t->s,t->size);
    printf("\"\n");
  }else if(type==Timag){
    print_space(indent);
    print_string(t->s,t->size);
    printf("i\n");
  }else if(type==Top){
    print_space(indent);
    print_operator(value);
    printf("\n");
  }else if(type==Tapp){
    print_space(indent);
    printf("app\n");
  }else if(type==Tstatement){
    print_space(indent);
    printf("statement\n");
  }else if(type==Tblock){
    print_space(indent);
    printf("block\n");
  }else if(type==Tkeyword && value==Twhile){
    print_space(indent);
    printf("while\n");
  }else if(type==Tkeyword && value==Tif){
    print_space(indent);
    printf("if\n");
  }else if(type==Tkeyword && value==Tbreak){
    print_space(indent);
    printf("break\n");
  }else if(type==Tkeyword && value==Tcontinue){
    print_space(indent);
    printf("continue\n");
  }else if(type==Tkeyword && value==Tsub){
    print_space(indent);
    printf("sub\n");
  }else if(type==Tkeyword && value==Treturn){
    print_space(indent);
    printf("return\n");
  }else if(type==Tkeyword && value==Traise){
    print_space(indent);
    printf("raise\n");
  }else if(type==Tkeyword && value==Tglobal){
    print_space(indent);
    printf("global\n");
  }else if(type==Tkeyword && value==Tfor){
    print_space(indent);
    printf("for\n");
  }else if(type==Tkeyword && value==Ttable){
    print_space(indent);
    printf("table\n");
  }else if(type==Tkeyword && value==Ttry){
    print_space(indent);
    printf("try\n");
  }else if(type==Tkeyword && value==Tlabel){
    print_space(indent);
    printf("label\n");
  }else if(type==Tkeyword && value==Tgoto){
    print_space(indent);
    printf("goto\n");
  }else if(type==Tkeyword && value==Timport){
    print_space(indent);
    printf("import\n");
  }else{
    print_space(indent);
    printf("AST of unknown type\n");
  }
  int i;
  for(i=0; i<t->n; i++){
    ast_print(t->a[i],indent+2);
  }
}

// section
// byte code generator

static
void vtab_delete_type(mt_vec* v){
  unsigned long size = v->size;
  mt_var_info* a = (mt_var_info*)v->a;
  unsigned long i;
  for(i=0; i<size; i++){
    if(a[i].ownership) mf_free(a[i].s);
  }
  mf_vec_delete(v);
}

static
void vtab_delete(mt_var_tab* vtab){
  if(vtab==NULL) return;
  vtab_delete_type(&vtab->v);
  vtab_delete_type(&vtab->a);
  vtab_delete_type(&vtab->c);
  vtab_delete_type(&vtab->g);
}

static
long vtab_index(mt_var_tab* vtab, int type, char* s, long size){
  if(vtab==NULL) return -1;
  long i,n;
  mt_var_info* a;
  if(type==MV_LOCAL){
    n = vtab->v.size;
    a = (mt_var_info*)vtab->v.a;
  }else if(type==MV_GLOBAL){
    n = vtab->g.size;
    a = (mt_var_info*)vtab->g.a;
  }else if(type==MV_ARGUMENT){
    n = vtab->a.size;
    a = (mt_var_info*)vtab->a.a;
  }else if(type==MV_CONTEXT){
    n = vtab->c.size;
    a = (mt_var_info*)vtab->c.a;
  }else{
    puts("Compiler error in vtab_index.");
    exit(1);
  }
  for(i=0; i<n; i++){
    if(size==a[i].size && memcmp(s,a[i].s,size)==0){
      return i;
    }
  }
  return -1;
}

static
int vtab_index_type(int* type, mt_var_tab* vtab, char* s, long size){
  if(vtab==NULL) return -1;
  long index;
  index = vtab_index(vtab,MV_GLOBAL,s,size);
  if(index>=0){
    *type=MV_GLOBAL;
    return index;
  }
  index = vtab_index(vtab,MV_CONTEXT,s,size);
  if(index>=0){
    *type=MV_CONTEXT;
    return index;
  }
  index = vtab_index(vtab,MV_ARGUMENT,s,size);
  if(index>=0){
    *type=MV_ARGUMENT;
    return index;
  }
  index = vtab_index(vtab,MV_LOCAL,s,size);
  if(index>=0){
    *type=MV_LOCAL;
    return index;
  }
  return -1;
}

static
void vtab_push(mt_var_tab* vtab, int type, char* s, long size,
  int ownership
){
  if(vtab==NULL) return;
  mt_var_info info;
  info.s=s;
  info.size=size;
  info.ownership=ownership;
  switch(type){
  case MV_ARGUMENT:
    mf_vec_push(&vtab->a,&info);
    break;
  case MV_LOCAL:
    mf_vec_push(&vtab->v,&info);
    break;
  case MV_CONTEXT:
    mf_vec_push(&vtab->c,&info);
    break;
  case MV_GLOBAL:
    mf_vec_push(&vtab->g,&info);
    break;
  default:
    puts("Compiler error in vtab_push.");
    exit(1);
  }
}

static
void vtab_print_type(mt_var_tab* vtab, int type, int indent){
  int size;
  mt_var_info* a;
  if(type==MV_LOCAL){
    size = vtab->v.size;
    a = (mt_var_info*)vtab->v.a;
  }else if(type==MV_ARGUMENT){
    size = vtab->a.size;
    a = (mt_var_info*)vtab->a.a;
  }else if(type==MV_GLOBAL){
    size = vtab->g.size;
    a = (mt_var_info*)vtab->g.a;
  }else if(type==MV_CONTEXT){
    size = vtab->c.size;
    a = (mt_var_info*)vtab->c.a;
  }else{
    abort();
  }
  int i;
  for(i=0; i<size; i++){
    printf("(%.*s)",a[i].size,a[i].s);
  }
}

static
void vtab_print(mt_var_tab* vtab, int indent){
  if(vtab==NULL){
    print_space(indent);
    printf("vtab == NULL\n");
    return;
  }
  print_space(indent); printf("vtab.local == [");
  vtab_print_type(vtab,MV_LOCAL,indent);
  printf("]\n");
  print_space(indent); printf("vtab.argument == [");
  vtab_print_type(vtab,MV_ARGUMENT,indent);
  printf("]\n");
  print_space(indent); printf("vtab.context == [");
  vtab_print_type(vtab,MV_CONTEXT,indent);
  printf("]\n");
  print_space(indent); printf("vtab.global == [");
  vtab_print_type(vtab,MV_GLOBAL,indent);
  printf("]\n");
  vtab_print(vtab->context,indent+2);
}

static
void push_u8(mt_vec* bv, unsigned char b){
  mf_vec_push(bv,&b);
}

static
void push_u16(mt_vec* bv, uint32_t n){
#ifdef MV_BIG_ENDIAN
  push_u8(bv,(n>>8)&0xff);
  push_u8(bv,n&0xff);
#else
  push_u8(bv,n&0xff);
  push_u8(bv,(n>>8)&0xff);
#endif
}

static
void push_u32(mt_vec* bv, uint32_t n){
#ifdef MV_BIG_ENDIAN
  push_u8(bv,(n>>24)&0xff);
  push_u8(bv,(n>>16)&0xff);
  push_u8(bv,(n>>8)&0xff);
  push_u8(bv,n&0xff);
#else
  push_u8(bv,n&0xff);
  push_u8(bv,(n>>8)&0xff);
  push_u8(bv,(n>>16)&0xff);
  push_u8(bv,(n>>24)&0xff);
#endif
}

static
void push_double(mt_vec* bv, double x){
  uint64_t* p=(uint64_t*)&x;
  uint64_t n=*p;
#ifdef MV_BIG_ENDIAN
  push_u8(bv,(n>>56)&0xff);
  push_u8(bv,(n>>48)&0xff);
  push_u8(bv,(n>>40)&0xff);
  push_u8(bv,(n>>32)&0xff);
  push_u8(bv,(n>>24)&0xff);
  push_u8(bv,(n>>16)&0xff);
  push_u8(bv,(n>>8)&0xff);
  push_u8(bv,n&0xff);
#else
  push_u8(bv,n&0xff);
  push_u8(bv,(n>>8)&0xff);
  push_u8(bv,(n>>16)&0xff);
  push_u8(bv,(n>>24)&0xff);
  push_u8(bv,(n>>32)&0xff);
  push_u8(bv,(n>>40)&0xff);
  push_u8(bv,(n>>48)&0xff);
  push_u8(bv,(n>>56)&0xff);
#endif
}

static
void push_bc(mt_vec* bv, unsigned char b, ast_node* t){
  push_u8(bv,b);
  #if BC==4
  if(t){
    push_u16(bv,t->line+1);
    push_u8(bv,t->col+1);
  }else{
    push_u16(bv,0);
    push_u8(bv,0);
  }
  #endif
}

static
void bv_append(mt_vec* bv, mt_vec* bv2){
  unsigned char* a = (unsigned char*)bv2->a;
  int size=bv2->size;
  int i;
  for(i=0; i<size; i++){
    push_u8(bv,a[i]);
  }
}

static
void init_data(mt_vec* data){
  mf_vec_init(data,sizeof(unsigned char));
  push_u32(data,0); // size of data
  push_u32(data,0); // number of strings
  // ^push placeholder for data_size
  // todo: safe total size of the data block to be memory safe
}

static
void clear_data(mt_vec* data){
  mf_vec_delete(data);
  init_data(data);
}

int mf_hex_digit(int c){
  c=tolower(c);
  if((c>='0' && c<='9') || (c>='a' && c<='f')){
    if(c<'a'){
      return c-'0';
    }else{
      return c-'a'+10;
    }
  }else{
    return -1;
  }
}

uint32_t mf_htoi(uint32_t* a, int size){
  uint32_t x=0;
  int i,d;
  for(i=0; i<size; i++){
    d=mf_hex_digit(a[i]);
    if(d<0){
      printf("Syntax error: expected hexadecimal digit.\n");
      exit(1);
    }
    x=16*x+d;
  }
  return x;
}

int mf_bhtoi(int size, const unsigned char* a){
  int x=0;
  int i,d;
  for(i=0; i<size; i++){
    d=mf_hex_digit(a[i]);
    if(d<0){
      printf("Syntax error: expected hexadecimal digit.\n");
      exit(1);
    }
    x=16*x+d;
  }
  return x;
}

static
void compile_string_literal(mt_vec* data, long size, const char* s){
  int i,n;
  mt_str buffer;
  mf_decode_utf8(&buffer,size,(const unsigned char*)s);
  push_u8(data,MT_STRING);
  int index = data->size;
  push_u32(data,0); // placeholder
  uint32_t* a=buffer.a;
  i=0; n=0;
  while(i<buffer.size){
    if(a[i]=='\\'){
      if(i+1>=buffer.size){
        syntax_error(-1,-1,"invalid escape sequenze.");
        exit(1);
      }
      i++;
      int c = a[i];
      if(c=='n'){
        push_u32(data,'\n');
      }else if(c=='t'){
        push_u32(data,'\t');
      }else if(c=='s'){
        push_u32(data,' ');
      }else if(c=='b'){
        push_u32(data,'\\');
      }else if(c=='q'){
        push_u32(data,'\'');
      }else if(c=='d'){
        push_u32(data,'"');
      }else if(c=='x'){
        if(i+2>=buffer.size){
          syntax_error(-1,-1,"invalid escape sequence.");
          exit(1);
        }
        i++;
        push_u32(data,mf_htoi(a+i,2));
        i++;
      }else if(c=='u'){
        if(i+4>=buffer.size){
          syntax_error(-1,-1,"invalid unicode sequence.");
          exit(1);
        }
        i++;
        push_u32(data,mf_htoi(a+i,4));
        i+=3;
      }else if(c=='U'){
        if(i+8>=buffer.size){
          syntax_error(-1,-1,"invalid unicode sequence.");
          exit(1);
        }
        i++;
        push_u32(data,mf_htoi(a+i,8));
        i+=7;
      }else if(c==' ' || c=='\n' || c=='\t'){
        while(i<buffer.size && (a[i]==' ' || a[i]=='\n' || a[i]=='\t')){
          i++;
        }
        continue;
      }else{
        syntax_error(-1,-1,"invalid escape sequenze.");
        exit(1);
      }
      i++; n++;
    }else{
      push_u32(data,a[i]);
      i++; n++;
    }
  }
  *(uint32_t*)(data->a+index)=n;
  mf_free(buffer.a);  
}

static
void compile_bstring_literal(mt_vec* data, long size, const unsigned char* a){
  int i,n;
  push_u8(data,MT_BSTRING);
  int index = data->size;
  push_u32(data,0xcafe);
  i=0; n=0;
  while(i<size){
    if(i+1<size && isxdigit(a[i]) && isxdigit(a[i+1])){
      push_u8(data,mf_bhtoi(2,a+i));
      i+=2; n++;
    }else if(isspace(a[i]) || a[i]==','){
      i++;
    }else{
      syntax_error(-1,-1,"invalid byte string literal.");
      exit(1);
    }
  }
  *(uint32_t*)(data->a+index)=n;
}

typedef struct{
  char type;
  unsigned char* a;
  long size;
} mt_stab_element;

static
void stab_delete(mt_vec* v){
  int size=v->size;
  mt_stab_element* a=(mt_stab_element*)v->a;
  int i;
  for(i=0; i<size; i++){
    mf_free(a[i].a);
  }
  mf_vec_delete(v);
}

static
int stab_find(mt_vec* v, int type, const char* s, long size){
  int n=v->size;
  mt_stab_element* a = (mt_stab_element*)v->a;
  mt_stab_element* e;
  int i;
  for(i=0; i<n; i++){
    e = a+i;
    if(e->type==type){
      if(e->size==size && memcmp(e->a,s,size)==0) return i;
    }
  }
  return -1;
}

static
void stab_print(mt_vec* stab){
  printf("STRING TABLE=[");
  int i;
  mt_stab_element* e;
  for(i=0; i<stab->size; i++){
    e=(mt_stab_element*)mf_vec_get(stab,i);
    if(e->type==MT_STRING){
      printf("MT_STRING");
      printf("[%s] ",e->a);
    }
  }
  printf("]\n");
}

void dump_program(unsigned char* bv, int n);

static
void push_string(mt_vec* bv, mt_vec* stab,
  int line, long size, const char* a
){
  int index = stab_find(stab,MT_STRING,a,size);
  if(index>=0){
    push_u32(bv,index);
  }else{
    compile_string_literal(&data,size,a);
    push_u32(bv,stab->size);
    mt_stab_element e;
    e.type=MT_STRING;
    e.a=mf_malloc(size+1);
    memcpy(e.a,a,size+1);
    e.size=size;
    mf_vec_push(stab,&e);
  }
  // stab_print(stab);
}

static
void push_bstring(mt_vec* bv, mt_vec* stab,
  int line, long size, const char* a
){
  int index = stab_find(stab,MT_STRING,a,size);
  if(index>=0){
    push_u32(bv,index);
  }else{
    compile_bstring_literal(&data,size,(unsigned char*)a);
    push_u32(bv,stab->size);
    mt_stab_element e;
    e.type=MT_BSTRING;
    e.a=mf_malloc(size+1);
    memcpy(e.a,a,size+1);
    e.size=size;
    mf_vec_push(stab,&e);
  }
}

static
void compile_string(mt_vec* bv, ast_node* t){
  push_bc(bv,CONST_STR,t);
  push_string(bv,&stab,t->line,t->size,t->s);
}

static
void compile_bstring(mt_vec* bv, ast_node* t){
  push_bc(bv,CONST_STR,t);
  push_bstring(bv,&stab,t->line,t->size,t->s);
}

static void ast_compile(mt_vec* bv, ast_node* t);
static
void compile_index(mt_vec* bv, ast_node* t){
  ast_compile(bv,t->a[0]);
  ast_compile(bv,t->a[1]);
  push_bc(bv,GETREF,t);
}

static
void compile_dot(mt_vec* bv, ast_node* t){
  ast_compile(bv,t->a[0]);
  compile_string(bv,t->a[1]);
  push_bc(bv,OBGETREF,t);
}

static
int variable_index(int* type, mt_var_tab* vtab, ast_node* t){
  if(vtab==NULL) return -1;
  int index;
  index = vtab_index_type(type,vtab,t->s,t->size);
  if(index>=0){
    return index;
  }else{
    index = variable_index(type,vtab->context,t);
    if(index>=0){
      vtab_push(vtab,MV_CONTEXT,t->s,t->size,0);
      *type=MV_CONTEXT;
      return vtab->c.size-1;
    }else{
      return -1;
    }
  }
}

static
void compile_variable(mt_vec* bv, ast_node* t){
  int index,type;
  if(t->size==4 && memcmp(t->s,"self",4)==0){
    if(scope>0){
      push_bc(bv,LOAD_ARG,t);
      push_u32(bv,0);
    }else{
      syntax_error(t->line,t->col,"self argument only occurs in function scope.");
      exit(1);
    }
    return;
  }
  if(context==NULL){
    goto global;
  }
  if(context->fn_name){
    if(context->fn_name_size==t->size &&
      memcmp(context->fn_name,t->s,t->size)==0
    ){
      push_bc(bv,FNSELF,t);
      return;
    }
  }
  index = variable_index(&type,context,t);
  // vtab_print(context,4);
  if(index<0){
    goto global;
  }
  if(type==MV_ARGUMENT){
    push_bc(bv,LOAD_ARG,t);
    push_u32(bv,index+1);      
  }else if(type==MV_LOCAL){
    push_bc(bv,LOAD_LOCAL,t);
    push_u32(bv,index);
  }else if(type==MV_CONTEXT){
    push_bc(bv,LOAD_CONTEXT,t);
    push_u32(bv,index);
  }else if(type==MV_GLOBAL){
    global:
    push_bc(bv,LOAD,t);
    push_string(bv,&stab,t->line,t->size,t->s);
  }else{
    puts("Compiler error in compile_variable.");
    exit(1);
  }
}

static
int ast_load_ref_ownership;

static
void ast_load_ref(mt_vec* bv, ast_node* t){
  if(t->type==Tid){
    if(scope>0){
      int index,type;
      // index = vtab_index_type(&type,context,t->s,t->size);
      index = variable_index(&type,context,t);
      if(index<0){
        push_bc(bv,LOAD_REF_LOCAL,t);
        push_u32(bv,context->v.size);
        vtab_push(context,MV_LOCAL,t->s,t->size,ast_load_ref_ownership);
      }else{
        if(type==MV_ARGUMENT){
          push_bc(bv,LOAD_ARG_REF,t);
          push_u32(bv,index+1);
        }else if(type==MV_GLOBAL){
          goto global;
        }else if(type==MV_CONTEXT){
          push_bc(bv,LOAD_REF_CONTEXT,t);
          push_u32(bv,index);
        }else if(type==MV_LOCAL){
          push_bc(bv,LOAD_REF_LOCAL,t);
          push_u32(bv,index);
        }else{
          puts("Compiler error in ast_load_ref.");
          exit(1);
        }
      }
    }else{
      global:
      push_bc(bv,LOAD_REF,t);
      push_string(bv,&stab,t->line,t->size,t->s);
    }
  }else if(t->type==Top && t->value==Top_index){
    compile_index(bv,t);
  }else if(t->type==Top && t->value==Top_dot){
    compile_dot(bv,t);
  }else{
    puts("Error in ast_load_ref.");
    exit(1);
  }
}

static
void closure(mt_vec* bv, mt_var_tab* vtab){
  mt_var_info* a = (mt_var_info*)vtab->c.a;
  int size = vtab->c.size;
  int index,type;
  int i;
  for(i=0; i<size; i++){
    index=vtab_index_type(&type,vtab->context,a[i].s,a[i].size);
    if(index<0){
      puts("Compiler error in closure.");
      exit(1);
    }
    if(type==MV_LOCAL){
      push_bc(bv,LOAD_LOCAL,NULL);
      push_u32(bv,index);
    }else if(type==MV_ARGUMENT){
      push_bc(bv,LOAD_ARG,NULL);
      push_u32(bv,index+1);
    }else if(type==MV_CONTEXT){
      push_bc(bv,LOAD_CONTEXT,NULL);
      push_u32(bv,index);
    }else{
      puts("Compiler error in closure, type==MV_GLOBAL.");
      exit(1);
    }
  }
  push_bc(bv,TUPLE,NULL);
  push_u32(bv,size);
}

static
void compile_index_assignment(mt_vec* bv, ast_node* t){
  ast_compile(bv,t->a[0]);
  ast_compile(bv,t->a[1]);
  push_bc(bv,STORE_INDEX,t);
  push_u32(bv,1);
}

static
void compile_unpacking(mt_vec* bv, ast_node* t){
  int i;
  for(i=0; i<t->n; i++){
    push_bc(bv,LIST_POP,t);
    push_u32(bv,i);
    if(t->a[i]->type==Top && t->a[i]->value==Top_index){
      compile_index_assignment(bv,t->a[i]);
    }else{
      ast_load_ref(bv,t->a[i]);
      push_bc(bv,STORE,t);
    }
  }
  push_bc(bv,POP,t);
}

static
void ast_compile_app(mt_vec* bv, ast_node* t){
  long i;
  if(t->info==SELF){
    ast_compile(bv,t->a[0]);
    for(i=1; i<t->n; i++){
      ast_compile(bv,t->a[i]);
    }
    push_bc(bv,CALL,t);
    push_u32(bv,t->n-2);
    return;
  }
  if(t->a[0]->type==Top && t->a[0]->value==Top_dot){
    ast_compile(bv,t->a[0]->a[0]);
    push_bc(bv,DUP,t);
    compile_string(bv,t->a[0]->a[1]);
    push_bc(bv,OBGET,t);
    push_bc(bv,SWAP,t);
  }else{
    ast_compile(bv,t->a[0]);
    push_bc(bv,CONST_NULL,t);
  }
  for(i=1; i<t->n; i++){
    ast_compile(bv,t->a[i]);
  }
  push_bc(bv,CALL,t);
  push_u32(bv,t->n-1);
}

static
void ast_compile_ifop2(mt_vec* bv, ast_node* t){
  // if c then a else b end
  // c JPZ[1] a JMP[2] (1) b (2)
  ast_compile(bv,t->a[0]);
  push_bc(bv,JPZ,t);
  int index1 = bv->size;
  push_u32(bv,0); // placeholder
  ast_compile(bv,t->a[1]);
  push_bc(bv,JMP,t);
  int index2 = bv->size;
  push_u32(bv,0); // placeholder
  *(uint32_t*)(bv->a+index1)=BC+bv->size-index1;
  ast_compile(bv,t->a[2]);
  *(uint32_t*)(bv->a+index2)=BC+bv->size-index2;
}

static
void ast_compile_ifop(mt_vec* bv, ast_node* t){
  uint32_t a[200];
  int m = t->n/2;
  int i,index;
  for(i=0; i<m; i++){
    ast_compile(bv,t->a[2*i]);
    push_bc(bv,JPZ,t);
    index=bv->size;
    push_u32(bv,0xcafe);
    ast_compile(bv,t->a[2*i+1]);
    push_bc(bv,JMP,t);
    a[i]=bv->size;
    push_u32(bv,0xcafe);
    *(uint32_t*)(bv->a+index)=BC+bv->size-index;
  }
  if(t->info==HAS_ELSE_CASE){
    ast_compile(bv,t->a[t->n-1]);
  }else{
    push_bc(bv,CONST_NULL,t);
  }
  for(i=0; i<m; i++){
    *(uint32_t*)(bv->a+a[i]) = BC+bv->size-a[i]; 
  }
}

// if c1 then b1
// elif c2 then b2
// elif c3 then b3
// else b4 end
// c1 JPZ[1] b1 JMP[end] (1)
// c2 JPZ[2] b2 JMP[end] (2)
// c3 JPZ[3] b2 JMP[end] (3)
// b4 (end)
static
void ast_compile_if(mt_vec* bv, ast_node* t){
  int has_else_case = t->info;
  uint32_t a[200];
  int m=t->n/2;
  int i;
  int index;
  nesting_level++;
  for(i=0; i<m; i++){
    ast_compile(bv,t->a[2*i]);
    push_bc(bv,JPZ,t);
    index=bv->size;
    push_u32(bv,0xcafe);
    ast_compile(bv,t->a[2*i+1]);
    push_bc(bv,JMP,t);
    a[i]=bv->size;
    push_u32(bv,0xcafe);
    *(uint32_t*)(bv->a+index) = BC+bv->size-index;
  }
  if(has_else_case){
    ast_compile(bv,t->a[t->n-1]);
  }
  for(i=0; i<m; i++){
    *(uint32_t*)(bv->a+a[i]) = BC+bv->size-a[i]; 
  }
  nesting_level--;
}

static
void ast_compile_while(mt_vec* bv, ast_node* t){
  // while c do b end
  // (2) c JPZ[1] b JMP[2] (1)
  int index1 = bv->size;
  if(jmp_sp==JMP_STACK_SIZE){
    puts("Compiler error: reached JMP_STACK_SIZE.");
    exit(1);
  }
  jmp_stack[jmp_sp]=index1;
  mf_vec_init(out_stack+jmp_sp,sizeof(uint32_t));
  jmp_sp++;
  ast_compile(bv,t->a[0]);
  push_bc(bv,JPZ,t);
  int index2 = bv->size;
  push_u32(bv,0xcafe);
  nesting_level++;
  ast_compile(bv,t->a[1]);
  nesting_level--;
  push_bc(bv,JMP,t);
  push_u32(bv,BC+index1-bv->size);
  *(uint32_t*)(bv->a+index2) = BC+bv->size-index2;

  uint32_t* a = (uint32_t*)out_stack[jmp_sp-1].a;
  int size = out_stack[jmp_sp-1].size;
  int i;
  for(i=0; i<size; i++){
    *(uint32_t*)(bv->a+a[i]) = BC+bv->size-a[i];
  }
  jmp_sp--;
  mf_vec_delete(out_stack+jmp_sp);
}

static
void load_global(mt_vec* bv, const char* a, long size, ast_node* t){
  push_bc(bv,LOAD,t);
  push_string(bv,&stab,-1,size,a);
}

static
void generate_call(mt_vec* bv, int argc, ast_node* t){
  push_bc(bv,CALL,t);
  push_u32(bv,argc);
}

static
void ast_compile_for(mt_vec* bv, ast_node* t){
  ast_node* list = t->a[0];
  load_global(bv,"iter",4,t);
  push_bc(bv,CONST_NULL,t);
  ast_compile(bv,t->a[1]);
  generate_call(bv,1,t);
  ast_node id;
  char* sbuffer = mf_malloc(10);
  snprintf(sbuffer,10,"__it%i",for_level);
  id.type=Tid; id.line=-1; id.col=-1;
  id.s = sbuffer; id.size=strlen(sbuffer); id.n=0;
  ast_load_ref_ownership=1;
  ast_load_ref(bv,&id);
  ast_load_ref_ownership=0;
  push_bc(bv,STORE,t);
  int index = bv->size;

  if(jmp_sp==JMP_STACK_SIZE){
    puts("Compiler error: reached JMP_STACK_SIZE.");
    exit(1);
  }
  jmp_stack[jmp_sp]=index;
  mf_vec_init(out_stack+jmp_sp,sizeof(uint32_t));
  jmp_sp++;

  compile_variable(bv,&id);
  push_bc(bv,ITER_NEXT,t);
  push_bc(bv,JPZ,t);
  int index2 = bv->size;
  push_u32(bv,0xcafe);
  push_bc(bv,ITER_VALUE,t);
  if(list->n==1){
    ast_load_ref(bv,list->a[0]);
    push_bc(bv,STORE,t);
  }else{
    compile_unpacking(bv,list);
  }
  nesting_level++; for_level++;
  ast_compile(bv,t->a[2]);
  nesting_level--; for_level--;
  push_bc(bv,JMP,t);
  push_u32(bv,BC+index-bv->size);

  *(uint32_t*)(bv->a+index2) = BC+bv->size-index2;
  uint32_t* a = (uint32_t*)out_stack[jmp_sp-1].a;
  int size = out_stack[jmp_sp-1].size;
  int i;
  for(i=0; i<size; i++){
    *(uint32_t*)(bv->a+a[i]) = BC+bv->size-a[i];
  }
  jmp_sp--;
  mf_vec_delete(out_stack+jmp_sp);
}

static
void ast_compile_augmented(mt_vec* bv, ast_node* t){
  int value=t->value;
  ast_compile(bv,t->a[1]);
  ast_load_ref(bv,t->a[0]);
  push_bc(bv,LOAD_POINTER,t);
  push_bc(bv,SWAP,t);
  if(value==Top_aadd){
    push_bc(bv,ADD,t);
  }else if(value==Top_asub){
    push_bc(bv,SUB,t);
  }else if(value==Top_ampy){
    push_bc(bv,MPY,t);
  }else if(value==Top_adiv){
    push_bc(bv,DIV,t);
  }else if(value==Top_aidiv){
    push_bc(bv,IDIV,t);
  }else if(value==Top_amod){
    push_bc(bv,MOD,t);
  }else{
    puts("Error in ast_compile_augmented.");
    exit(1);
  }
  push_bc(bv,STORE,t);
}

static
int ast_argument_list(mt_var_tab* vtab, ast_node* t){
  int i;
  ast_node* id;
  // mt_var_info info;
  for(i=0; i<t->n; i++){
    id = t->a[i];
    if(vtab_index(vtab,MV_LOCAL,id->s,id->size)>=0){
      syntax_error(t->line,t->col,"identifier occurs multiple times.");
      return 1;
    }
    vtab_push(vtab,MV_ARGUMENT,id->s,id->size,0);
  }
  vtab->argc=t->info;
  return 0;
}

static
void offsets(mt_vec* bv, int offset, mt_vec* indices){
  int i;
  int* a = (int*)indices->a;
  int size = indices->size;
  for(i=0; i<size; i++){
    *(uint32_t*)(bv->a+a[i])+=BC+offset-a[i];
  }
}

static
mt_vec* label_tab_new(){
  mt_vec* v = mf_malloc(sizeof(mt_vec));
  mf_vec_init(v,sizeof(mt_label));
  return v;
}

static
void label_tab_delete(mt_vec* tab){
  mf_vec_delete(tab);
  mf_free(tab);
}

static
void label_tab_push(mt_vec* tab, long size, const char* a, int index){
  mt_label t;
  t.a=a; t.size=size; t.index=index;
  mf_vec_push(tab,&t);
}

static
mt_label* label_tab_find(mt_vec* tab, const char* a, int size){
  int i;
  for(i=0; i<tab->size; i++){
    mt_label* p = mf_vec_get(tab,i);
    if(size==p->size && memcmp(a,p->a,size)==0){
      return p;
    }
  }
  return NULL;
}

static
int gotos(mt_vec* bv){
  int i;
  mt_label *g,*L;
  for(i=0; i<goto_tab->size; i++){
    g = mf_vec_get(goto_tab,i);
    L = label_tab_find(label_tab,g->a,g->size);
    if(L==NULL){
      printf("Error: goto label '%.*s' not found.\n",g->size,g->a);
      exit(1);
      return -1;
    }
    *(uint32_t*)(bv->a+g->index)=BC+L->index-g->index;
  }
  return 0;
}

static
int ast_compile_function(mt_vec* bv, ast_node* t){
  mt_vec bv2;
  mf_vec_init(&bv2,sizeof(unsigned char));
  mt_var_tab vtab;
  vtab.context=context;
  vtab.fn_name=NULL;
  mf_vec_init(&vtab.v,sizeof(mt_var_info));
  mf_vec_init(&vtab.c,sizeof(mt_var_info));
  mf_vec_init(&vtab.g,sizeof(mt_var_info));
  mf_vec_init(&vtab.a,sizeof(mt_var_info));
  // vtab_print(&vtab,4);
  int e=ast_argument_list(&vtab,t->a[0]);
  if(e) return 1;
  int argc=t->a[0]->info;
  vtab.fn_name=(unsigned char*)t->s;
  vtab.fn_name_size=t->size;
  mt_vec indices;
  mf_vec_init(&indices,sizeof(int));
  mt_vec* tindices = const_fn_indices;
  const_fn_indices = &indices;

  mt_vec* hlabel_tab=label_tab;
  mt_vec* hgoto_tab=goto_tab;
  label_tab=label_tab_new();
  goto_tab=label_tab_new();

  context=&vtab;
  // vtab_print(context,4);
  nesting_level++; scope++;
  ast_compile(&bv2,t->a[1]);
  nesting_level--; scope--;
  context=vtab.context;

  // printf("[%i,%i,%i]\n", bv2.size, bv_deposit.size, bv->size);
  offsets(&bv2,-bv_deposit.size,&indices);
  gotos(&bv2);
  label_tab_delete(label_tab);
  label_tab_delete(goto_tab);
  label_tab=hlabel_tab;
  goto_tab=hgoto_tab;


  const_fn_indices=tindices;
  mf_vec_delete(&indices);
  push_bc(&bv2,CONST_NULL,t);
  push_bc(&bv2,RET,t);

  if(vtab.c.size==0){
    push_bc(bv,CONST_NULL,t);
  }else{
    closure(bv,&vtab);
  }
  if(t->s){
    compile_string(bv,t);
  }else if(compiler_context.mode_cmd){
    push_bc(bv,CONST_NULL,t);
  }else{
    push_bc(bv,CONST_STR,t);
    char a[200];
    snprintf(a,200,"function (%s:%i:%i)",
      compiler_context.file_name,t->line+1,t->col+1);
    push_string(bv,&stab,t->line,strlen(a),a);
  }
  push_bc(bv,CONST_FN,t);
  int index = bv->size;
  mf_vec_push(const_fn_indices,&index);
  push_u32(bv,bv_deposit.size);
  push_u32(bv,argc); // argc
  push_u32(bv,vtab.v.size); // var_count

  bv_append(&bv_deposit,&bv2);
  mf_vec_delete(&bv2);
  vtab_delete(&vtab);

  return 0;
}

static
void ast_global(ast_node* t){
  int i;
  for(i=0; i<t->n; i++){
    ast_node* id = t->a[i];
    int index = vtab_index(context,MV_LOCAL,id->s,id->size);
    if(index>=0){
      syntax_error(id->line,id->col,"redeclaration of idenitifer.");
      exit(1);
    }
    vtab_push(context,MV_GLOBAL,id->s,id->size,0);
  }
}

static
void ast_compile_try(mt_vec* bv, ast_node* t){
  push_bc(bv,TRY,t);
  int index=bv->size;
  push_u32(bv,0xcafe);
  ast_compile(bv,t->a[0]);
  push_bc(bv,TRYEND,t);
  push_bc(bv,JMP,t);
  int index2=bv->size;
  push_u32(bv,0xcafe);
  *(uint32_t*)(bv->a+index) = bv->size+4-index;
  ast_node* p = t->a[1];
  if(p->info==CATCH_ID){
    push_bc(bv,TRYEND,t);
    push_bc(bv,EXCEPTION,t);
    ast_load_ref(bv,p->a[0]);
    push_bc(bv,STORE,t);
    ast_compile(bv,p->a[1]);
  }else if(p->info==CATCH_ID_EXP){
    push_bc(bv,TRYEND,t);
    push_bc(bv,EXCEPTION,t);
    ast_compile(bv,p->a[1]);
    push_bc(bv,ISIN,t);
    push_bc(bv,JPZ,t);
    int index3=bv->size;
    push_u32(bv,0xcafe);
    push_bc(bv,EXCEPTION,t);
    ast_load_ref(bv,p->a[0]);
    push_bc(bv,STORE,t);
    ast_compile(bv,p->a[2]);
    push_bc(bv,JMP,t);
    int index4=bv->size;
    push_u32(bv,0xcafe);
    *(uint32_t*)(bv->a+index3) = BC+bv->size-index3;
    push_bc(bv,EXCEPTION,t);
    push_bc(bv,RAISE,t);
    *(uint32_t*)(bv->a+index4) = BC+bv->size-index4;
  }
  *(uint32_t*)(bv->a+index2) = BC+bv->size-index2;
}

static
long string_to_long(long size, const char* s){
  if(size>2 && isalpha(s[1])){
    if(s[1]=='x'){
      return strtol(s+2,NULL,16);
    }else if(s[1]=='b'){
      return strtol(s+2,NULL,2);
    }else if(s[1]=='o'){
      return strtol(s+2,NULL,8);
    }
  }else{
    return strtol(s,NULL,10);
  }
}

static
void ast_compile(mt_vec* bv, ast_node* t){
  int type=t->type;
  if(type==Tstatement){
    ast_compile(bv,t->a[0]);
    if(t->info==RETURN_VALUE){
      push_bc(bv,RET,t);
    }else if(compiler_context.mode_cmd){
      if(nesting_level>0){
        push_bc(bv,POP,t);
      }
    }else{
      push_bc(bv,POP,t);
    }
  }else if(type==Tbool){
    push_bc(bv,CONST_BOOL,t);
    push_u32(bv,t->value);
  }else if(type==Tint){
    errno=0;
    long value = string_to_long(t->size,t->s);
    if(errno){
      compile_string(bv,t);
      push_bc(bv,LONG,t);
    }else{
      push_bc(bv,CONST_INT,t);
      push_u32(bv,value);
    }
  }else if(type==Tfloat){
    double value = atof(t->s);
    push_bc(bv,CONST_FLOAT,t);
    push_double(bv,value);
  }else if(type==Timag){
    double value = atof(t->s);
    push_bc(bv,CONST_IMAG,t);
    push_double(bv,value);
  }else if(type==Tnull){
    push_bc(bv,CONST_NULL,t);
  }else if(type==Tstring){
    compile_string(bv,t);
  }else if(type==Tbstring){
    compile_bstring(bv,t);
  }else if(type==Tid){
    compile_variable(bv,t);
  }else if(type==Top){
    int value=t->value;
    if(value==Top_add){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,ADD,t);
    }else if(value==Top_sub){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,SUB,t);
    }else if(value==Top_mpy){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,MPY,t);
    }else if(value==Top_div){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,DIV,t);
    }else if(value==Top_idiv){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,IDIV,t);
    }else if(value==Top_mod){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,MOD,t);
    }else if(value==Top_pow){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,POW,t);
    }else if(value==Top_neg){
      ast_compile(bv,t->a[0]);
      push_bc(bv,NEG,t);
    }else if(value==Top_tilde){
      ast_compile(bv,t->a[0]);
      push_bc(bv,TILDE,t);
    }else if(value==Top_not){
      ast_compile(bv,t->a[0]);
      push_bc(bv,NOT,t);
    }else if(value==Top_lt){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,LT,t);
    }else if(value==Top_gt){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,GT,t);
    }else if(value==Top_le){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,LE,t);
    }else if(value==Top_ge){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,GE,t);
    }else if(value==Top_eq){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,EQ,t);
    }else if(value==Top_ne){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,NE,t);
    }else if(value==Top_is){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,IS,t);
    }else if(value==Top_in){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,IN,t);
    }else if(value==Top_notin){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,NOTIN,t);
    }else if(value==Top_isin){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,ISIN,t);
    }else if(value==Top_bor){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,BIT_OR,t);
    }else if(value==Top_bxor){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,BIT_XOR,t);
    }else if(value==Top_band){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      push_bc(bv,BIT_AND,t);
    }else if(value==Top_range){
      ast_compile(bv,t->a[0]);
      ast_compile(bv,t->a[1]);
      if(t->n==3){
        ast_compile(bv,t->a[2]);
      }else{
        push_bc(bv,CONST_INT,t);
        push_u32(bv,1);
      }
      push_bc(bv,RANGE,t);
    }else if(value==Top_index){
      int i;
      for(i=0; i<t->n; i++){
        ast_compile(bv,t->a[i]);
      }
      push_bc(bv,GET,t);
      push_u32(bv,t->n-1);
    }else if(value==Top_dot){
      ast_compile(bv,t->a[0]);
      compile_string(bv,t->a[1]);
      push_bc(bv,OBGET,t);
    }else if(value==Top_list){
      long i;
      for(i=0; i<t->n; i++){
        ast_compile(bv,t->a[i]);
      }
      push_bc(bv,LIST,t);
      push_u32(bv,t->n);
    }else if(value==Top_map){
      long i;
      for(i=0; i<t->n; i++){
        ast_compile(bv,t->a[i]);
      }
      push_bc(bv,MAP,t);
      push_u32(bv,t->n);
    }else if(value==Top_tuple){
      long i;
      for(i=0; i<t->n; i++){
        ast_compile(bv,t->a[i]);
      }
      push_bc(bv,TUPLE,t);
      push_u32(bv,t->n);
    }else if(value==Top_assignment){
      ast_compile(bv,t->a[1]);
      ast_node* x=t->a[0];
      if(x->type==Top && x->value==Top_list){
        compile_unpacking(bv,x);
      }else if(x->type==Top && x->value==Top_index){
        compile_index_assignment(bv,x);
      }else{
        ast_load_ref(bv,x);
        push_bc(bv,STORE,t);
      }
    }else if(value==Top_aadd || value==Top_asub ||
      value==Top_ampy || value==Top_adiv || value==Top_amod ||
      value==Top_aidiv
    ){
      ast_compile_augmented(bv,t);
    }else if(value==Top_and){
      // Use a AND[1] b (1) instead of
      // a JPZ[1] b JMP[2] (1) CONST_BOOL false (2).
      ast_compile(bv,t->a[0]);
      push_bc(bv,AND,t);
      int index=bv->size;
      push_u32(bv,0xcafe);
      ast_compile(bv,t->a[1]);
      *(uint32_t*)(bv->a+index)=BC+bv->size-index;
    }else if(value==Top_or){
      // Use a OR[1] b (1) instead of
      // a JPZ[1] CONST_BOOL true JMP[2] (1) b (2).
      ast_compile(bv,t->a[0]);
      push_bc(bv,OR,t);
      int index=bv->size;
      push_u32(bv,0xcafe);
      ast_compile(bv,t->a[1]);
      *(uint32_t*)(bv->a+index)=BC+bv->size-index;
    }else if(value==Top_if){
      ast_compile_ifop(bv,t);
    }else{
      printf("Error in ast_compile: unknown operator, value: %i.\n",t->value);
      exit(1);
    }
  }else if(type==Tapp){
    ast_compile_app(bv,t);
  }else if(type==Tblock){
    int i;
    for(i=0; i<t->n; i++){
      ast_compile(bv,t->a[i]);
    }
  }else if(type==Tkeyword){
    int value=t->value;
    if(value==Twhile){
      ast_compile_while(bv,t);
    }else if(value==Tfor){
      ast_compile_for(bv,t);
    }else if(value==Tif){
      ast_compile_if(bv,t);
    }else if(value==Tbreak){
      push_bc(bv,JMP,t);
      uint32_t index = bv->size;
      mf_vec_push(out_stack+jmp_sp-1,&index);
      push_u32(bv,0xcafe);
    }else if(value==Tcontinue){
      push_bc(bv,JMP,t);
      push_u32(bv,BC+jmp_stack[jmp_sp-1]-bv->size);
    }else if(value==Treturn){
      if(t->n==1){
        ast_compile(bv,t->a[0]);
      }else{
        push_bc(bv,CONST_NULL,t);
      }
      push_bc(bv,RET,t);
    }else if(value==Traise){
      ast_compile(bv,t->a[0]);
      push_bc(bv,RAISE,t);
    }else if(value==Tsub){
      ast_compile_function(bv,t);
    }else if(value==Tglobal){
      ast_global(t);
    }else if(value==Ttable){
      if(t->info==0){
        push_bc(bv,CONST_NULL,t);
      }
      int i;
      for(i=0; i<t->n; i++){
        ast_compile(bv,t->a[i]);
      }
      push_bc(bv,MAP,t);
      push_u32(bv,t->n-t->info);
      push_bc(bv,TABLE,t);
    }else if(value==Ttry){
      ast_compile_try(bv,t);
    }else if(value==Tlabel){
      ast_node* id=t->a[0];
      label_tab_push(label_tab,id->size,id->s,bv->size);
    }else if(value==Tgoto){
      ast_node* id=t->a[0];
      push_bc(bv,JMP,t);
      label_tab_push(goto_tab,id->size,id->s,bv->size);
      push_u32(bv,0xcafe);
    }else{
      printf("Compiler in ast_compile: unknown keyword value: %i.\n",t->value);
      exit(1);
    }
  }else{
    printf("Error in ast_compile: unknown type value: %i.\n",type);
    exit(1);
  }
}

static
void compile(mt_vec* bv, ast_node* t){
  mf_vec_init(&bv_deposit,sizeof(unsigned char));
  mt_vec indices;
  mf_vec_init(&indices,sizeof(int));
  const_fn_indices=&indices;
  label_tab=label_tab_new();
  goto_tab=label_tab_new();

  ast_compile(bv,t);
  *(uint32_t*)(data.a)=data.size;
  *(uint32_t*)(data.a+4)=stab.size;
  push_bc(bv,HALT,t);
  offsets(bv,bv->size,const_fn_indices);
  gotos(bv);
  label_tab_delete(label_tab);
  label_tab_delete(goto_tab);

  bv_append(bv,&bv_deposit);

  mf_vec_delete(&bv_deposit);
  mf_vec_delete(&indices);
  const_fn_indices=NULL;
}

void dump_data(unsigned char* data){
  printf("DATA=[");
  int n = *(uint32_t*)(data+4);
  int i=8;
  int j,k,size;
  unsigned char c;
  int type;
  for(j=0; j<n; j++){
    type = *(unsigned char*)(data+i);
    i++;
    size = *(uint32_t*)(data+i);
    i+=4;
    if(type==MT_STRING){
      printf("[");
      // for(k=0; k<size; k++){
      //   c=*(unsigned char*)(data+i);
      //   mf_put_byte(c);
      //   i+=4;
      // }
      mf_print_string(size,(uint32_t*)(data+i));
      i+=size*4;
      printf("]");
    }else if(type==MT_BSTRING){
      printf("[");
      for(k=0; k<size; k++){
        c=*(unsigned char*)(data+i);
        mf_put_byte(c);
        i++;
      }
      printf("]");
    }
  }
  printf("]\n");
}

static
void prefix_line_col(unsigned char* a){
  int line = *(uint16_t*)(a+1);
  int col = *(uint8_t*)(a+3);
  printf(" |%i:%i| ",line,col);
}

void dump_program(unsigned char* bv, int n){
  char c;
  int i=0;
  while(i<n){
    c = bv[i];
    if(i<10) printf("0");
    printf("%i ",i);
    prefix_line_col(bv+i);
    switch(c){
    case CONST_INT:
      printf("CONST_INT %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LONG:
      i+=BC; printf("LONG\n");
      break;
    case CONST_STR:
      printf("CONST_STR (%i)\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case CONST_FLOAT:
      i+=BC+8; printf("CONST_FLOAT\n");
      break;
    case CONST_IMAG:
      i+=BC+8; printf("CONST_IMAG\n");
      break;
    case CONST_NULL:
      i+=BC; printf("CONST_NULL\n");
      break;
    case CONST_BOOL:
      printf("CONST_BOOL %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case CONST_FN:
      printf("CONST_FN %i, argc=%i, varcount=%i\n",
        *(int*)(bv+i+BC),*(int*)(bv+i+BC+4),*(int*)(bv+i+BC+8));
      i+=BC+12;
      break;
    case ADD:
      i+=BC; printf("ADD\n");
      break;
    case SUB:
      i+=BC; printf("SUB\n");
      break;
    case MPY:
      i+=BC; printf("MPY\n");
      break;
    case DIV:
      i+=BC; printf("DIV\n");
      break;
    case IDIV:
      i+=BC; printf("IDIV\n");
      break;
    case POW:
      i+=BC; printf("POW\n");
      break;
    case MOD:
      i+=BC; printf("MOD\n");
      break;
    case NEG:
      i+=BC; printf("NEG\n");
      break;
    case TILDE:
      i+=BC; printf("TILDE\n");
      break;
    case NOT:
      i+=BC; printf("NOT\n");
      break;
    case EQ:
      i+=BC; printf("EQ\n");
      break;
    case NE:
      i+=BC; printf("NE\n");
      break;
    case LT:
      i+=BC; printf("LT\n");
      break;
    case GT:
      i+=BC; printf("GT\n");
      break;
    case LE:
      i+=BC; printf("LE\n");
      break;
    case GE:
      i+=BC; printf("GE\n");
      break;
    case IS:
      i+=BC; printf("IS\n");
      break;
    case IN:
      i+=BC; printf("IN\n");
      break;
    case ISIN:
      i+=BC; printf("ISIN\n");
      break;
    case NOTIN:
      i+=BC; printf("NOTIN\n");
      break;
    case RANGE:
      i+=BC; printf("RANGE\n");
      break;
    case PRINT:
      printf("PRINT\n");
      i+=BC+4;
      break;
    case PUT:
      printf("PUT\n");
      i+=BC+4;
      break;
    case LIST:
      printf("LIST %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case MAP:
      printf("MAP %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case TUPLE:
      printf("TUPLE %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case TABLE:
      i+=BC; printf("TABLE\n");
      break;
    case HALT:
      i+=BC; printf("HALT\n");
      break;
    case STORE:
      i+=BC; printf("STORE\n");
      break;
    case RSTORE:
      i+=BC; printf("RSTORE\n");
      break;
    case STORE_LOCAL:
      i+=BC; printf("STORE_LOCAL\n");
      break;
    case STN:
      printf("STN %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case STORE_INDEX:
      printf("STORE_INDEX %i\n",*(int32_t*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD:
      printf("LOAD %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_REF:
      printf("LOAD_REF %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case GET:
      printf("GET %i\n",*(int32_t*)(bv+i+BC));
      i+=BC+4;
      break;
    case GETREF:
      i+=BC; printf("GETREF\n");
      break;
    case JMP:
      printf("JMP %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case JPZ:
      printf("JPZ %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case AND:
      printf("AND %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case OR:
      printf("OR %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LIST_POP:
      printf("LIST_POP %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case POP:
      i+=BC; printf("POP\n");
      break;
    case RET:
      i+=BC; printf("RET\n");
      break;
    case CALL:
      printf("CALL, argc = %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_LOCAL:
      printf("LOAD_LOCAL %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_REF_LOCAL:
      printf("LOAD_REF_LOCAL %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_ARG:
      printf("LOAD_ARG %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_CONTEXT:
      printf("LOAD_CONTEXT %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_REF_CONTEXT:
      printf("LOAD_REF_CONTEXT %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_ARG_REF:
      printf("LOAD_ARG_REF %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case LOAD_POINTER:
      i+=BC; printf("LOAD_POINTER\n");
      break;
    case FNSELF:
      i+=BC; printf("FNSELF\n");
      break;
    case OBGET:
      i+=BC; printf("OBGET\n");
      break;
    case OBGETREF:
      i+=BC; printf("OBGETREF\n");
      break;
    case DUP:
      i+=BC; printf("DUP\n");
      break;
    case SWAP:
      i+=BC; printf("SWAP\n");
      break;
    case ITER_NEXT:
      i+=BC; printf("ITER_NEXT\n");
      break;
    case ITER_VALUE:
      i+=BC; printf("ITER_VALUE\n");
      break;
    case TRY:
      printf("TRY, catch address: %i\n",*(int*)(bv+i+BC));
      i+=BC+4;
      break;
    case TRYEND:
      i+=BC; printf("TRYEND\n");
      break;
    case RAISE:
      i+=BC; printf("RAISE\n");
      break;
    case EXCEPTION:
      i+=BC; printf("EXCEPTION\n");
      break;
    default:
      printf("Error in dump_program: unknown byte code, value=%i.\n",c);
      exit(1);
    }
  }
}

void mf_compiler_context_save(mt_compiler_context* context){
  context->mode_cmd = compiler_context.mode_cmd;
  context->file_name = compiler_context.file_name;
}

void mf_compiler_context_restore(mt_compiler_context* context){
  compiler_context.mode_cmd = context->mode_cmd;
  compiler_context.file_name = context->file_name;
}

int mf_compile(mt_vec* v, mt_module* module){
  module->program=NULL;
  module->data=NULL;
  mt_vec bv;
  mf_vec_init(&bv,sizeof(unsigned char));
  init_data(&data);
  mf_vec_init(&stab,sizeof(mt_stab_element));
  #ifdef MV_PRINT
  print_vtoken(v);
  #endif
  ast_node* t = ast(v);
  if(t==NULL) goto error;
  #ifdef MV_PRINT
  ast_print(t,2);
  #endif

  compile(&bv,t);
  #ifdef MV_PRINT
  dump_program(bv.a,bv.size);
  dump_data(data.a);
  #endif
  module->program=bv.a;
  module->program_size=bv.size;
  module->data=data.a;
  module->data_size=data.size;

  data.a=NULL;
  data.capacity=0;
  data.size=0;
  stab_delete(&stab);
  ast_delete(t);
  return 0;
  error:
  mf_vec_delete(&data);
  stab_delete(&stab);
  return 1;
}
