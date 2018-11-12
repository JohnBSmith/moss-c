
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
// * operators <<, >>
// * nesting_level seems to be too unreliable
// (nesting_level only matters in command-line mode)

// #define MV_PRINT
// ^shows syntax trees and bytecode dump
// (debug mode)

#define MV_LOCAL 0
#define MV_GLOBAL 1
#define MV_ARGUMENT 2
#define MV_CONTEXT 3
#define MV_FN_NAME 4
#define CATCH_ID 0
#define CATCH_ID_EXP 1
#define POP_VALUE 0
#define RETURN_VALUE 1
#define SELF 1
#define NO_SELF 0
#define WITHOUT_ELSE_CASE 0
#define WITH_ELSE_CASE 1

typedef struct{
    char* s;
    int size;
    int ownership;
} mt_var_info;

typedef struct mt_var_tab mt_var_tab;
struct mt_var_tab{
    mt_var_tab* context;
    mt_vec v,c,g,a;
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
    long refcount;
    int line;
    int col;
    char type;
    char value;
    int info;
    int info2;
    int ownership;
    char* s;
    long size;
    int n;
    ast_node* a[];
};

typedef struct token_iterator token_iterator;
struct token_iterator{
    mt_token* a;
    int size;
    int index;
    token_iterator* i;
};
// If a token_iterator is assigned to a new array,
// a copy of the old iterator will be stored in it.
// The information will be needed later by the code generator
// that uses weak references in the abstract syntax tree.
// Therefore deletion must be delayed until compilation is
// completed.

static ast_node* atom(token_iterator *);
static ast_node* expression(token_iterator* i);
static ast_node* comma_expression(token_iterator* i);
static int comma_found;
static ast_node* factor(token_iterator* i);
static ast_node* argument_list(token_iterator* i);
static ast_node* function_literal(token_iterator* i);
static ast_node* statement_list(token_iterator * i);
static ast_node* block_app(ast_node* block, mt_token* t);
static void ast_print(ast_node* t, int indent);

enum{
    Tid, Tbool, Tint, Tfloat, Timag, Tstring, Tbstring, Tnull,
    Tkeyword, Top, Tsep, Tbleft, Tbright,
    Tapp, Tblock, Tinline_block, Tstatement, Tterminal
};

enum{
    Top_add, Top_sub, Top_mpy, Top_div, Top_idiv, Top_neg, Top_mod,
    Top_and, Top_or, Top_not, Top_pow, Top_range, Top_list, Top_map,
    Top_lt, Top_gt, Top_le, Top_ge,
    Top_eq, Top_ne, Top_is, Top_in, Top_notin, Top_isin,
    Top_assignment, Top_dot, Top_index, Top_if,
    Top_aadd, Top_asub, Top_ampy, Top_adiv, Top_amod, Top_aidiv,
    Top_inc, Top_dec, Top_step, Top_band, Top_bor, Top_bxor,
    Top_qm, Top_tilde, Top_tuple
};

enum{
    Tbegin, Tbreak, Tcatch, Tcontinue, Tdo, Telif, Telse, Tend,
    Tfor, Tfn, Tfunction, Tglobal, Tgoto, Tif, Tlabel, Tof, Traise,
    Treturn, Ttable, Tthen, Ttry, Twhile, Tyield, Timport, Tassert
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
static int syntax_nesting;
static char bstack[200];
static int bstack_sp;
static mt_vec* label_tab;
static mt_vec* goto_tab;
static int line_counter;
static int colon = 1;

static
void push_token(mt_vec* v, int line, int col, char type, char value){
    mt_token t;
    t.line = line;
    t.col = col;
    t.type = type;
    t.value = value;
    t.s = NULL;
    t.size = 0;
    mf_vec_push(v,&t);
}

static
void push_token_string(mt_vec* v, int line, int col, char type,
    long size, const char* a
){
    mt_token t;
    t.line = line;
    t.col = col;
    t.type = type;

    t.value = 0;
    // ^Unnecessary because unreachable,
    // but valgrind murmured at conditions
    // if(t->type==A && t->value==B){...}.

    t.s = mf_malloc((size_t)(size+1));
    memcpy(t.s,a,(size_t)size);
    t.s[size] = 0;
    t.size = size;
    mf_vec_push(v,&t);
}

static
void atoken_delete(long size, mt_token* a){
    long i;
    for(i=0; i<size; i++){
        mf_free(a[i].s);
    }
}

static
void token_iterator_delete(token_iterator* i){
    if(i->i!=NULL){
        token_iterator_delete(i->i);
    }
    atoken_delete(i->size,i->a);
    mf_free(i->a);
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
const char* operator_to_string(char value){
    switch(value){
    case Top_add: return "+";
    case Top_sub: return "-";
    case Top_mpy: return "*";
    case Top_div: return "/";
    case Top_idiv: return "//";
    case Top_mod: return "%%";
    case Top_neg: return "u-";
    case Top_pow: return "^";
    case Top_not: return "not";
    case Top_and: return "and";
    case Top_or: return "or";
    case Top_in: return "in";
    case Top_notin: return "not in";
    case Top_is: return "is";
    case Top_isin: return "is in";
    case Top_range: return "..";
    case Top_list: return "list";
    case Top_map: return "map";
    case Top_tuple: return "tuple";
    case Top_lt: return "<";
    case Top_gt: return ">";
    case Top_le: return "<=";
    case Top_ge: return ">=";
    case Top_eq: return "==";
    case Top_ne: return "!=";
    case Top_assignment: return "=";
    case Top_dot: return ".";
    case Top_index: return "index";
    case Top_if: return "if";
    case Top_aadd: return "+=";
    case Top_asub: return "-=";
    case Top_ampy: return "*=";
    case Top_adiv: return "/=";
    case Top_aidiv: return "//=";
    case Top_amod: return "%%=";
    case Top_inc: return "++";
    case Top_dec: return "--";
    case Top_bor: return "|";
    case Top_band: return "&";
    case Top_bxor: return "$";
    case Top_qm: return "?";
    case Top_tilde: return "~";
    default:
        return "unknown operator";
    }
}

static
void kw_print(char* s){
    printf("%s",s);
}

static
const char* keyword_to_string(char value){
    switch(value){
        case Tassert: return "assert";
        case Tbegin: return "begin";
        case Tbreak: return "break";
        case Tcatch: return "catch";
        case Tcontinue: return "continue";
        case Tdo: return "do";
        case Telif: return "elif";
        case Telse: return "else";
        case Tend: return "end";
        case Tfn: return "fn";
        case Tfor: return "for";
        case Tfunction: return "function";
        case Tglobal: return "global";
        case Tgoto: return "goto";
        case Timport: return "use";
        case Tif: return "if";
        case Tlabel: return "label";
        case Tof: return "of";
        case Traise: return "raise";
        case Treturn: return "return";
        case Ttable: return "table";
        case Tthen: return "then";
        case Ttry: return "try";
        case Twhile: return "while";
        case Tyield: return "yield";
        default:
            return "unknown keyword";
    };
}

static
char* token_to_string(mt_token* t){
    int type = t->type;
    char* s;
    if(type==Top){
        s = mf_malloc(10);
        snprintf(s,10,"%s",operator_to_string(t->value));
    }else if(type==Tint || type==Tfloat || type==Tid){
        s = mf_malloc(t->size+10);
        snprintf(s,t->size+10,"%.*s",(unsigned int)t->size,t->s);
    }else if(type==Timag){
        s = mf_malloc(t->size+10);
        snprintf(s,t->size+10,"%.*si",(unsigned int)t->size,t->s);
    }else if(type==Tstring){
        s = mf_malloc(t->size+10);
        snprintf(s,t->size+10,"\"%.*s\"",(unsigned int)t->size,t->s);
    }else if(type==Tbstring){
        s = mf_malloc(t->size+10);
        snprintf(s,t->size+10,"b\"%.*s\"",(unsigned int)t->size,t->s);
    }else if(type==Tnull){
        s = mf_malloc(10);
        snprintf(s,10,"null");
    }else if(type==Tkeyword){
        s = mf_malloc(40);
        snprintf(s,40,"'%s'",keyword_to_string(t->value));
    }else if(type==Tbool){
        s = mf_malloc(10);
        if(t->value==1){
            snprintf(s,10,"true");
        }else if(t->value==0){
            snprintf(s,10,"false");
        }else{
            printf("Error: boolean token value is outside of range 0..1.\n");
            abort();
        }
    }else if(type==Tbleft || type==Tbright){
        s = mf_malloc(10);
        snprintf(s,10,"%c",t->value);
    }else if(type==Tsep){
        s = mf_malloc(10);
        if(t->value=='n'){
            snprintf(s,10,"\\n");
        }else{
            snprintf(s,10,"%c",t->value);
        }
    }else if(type==Tapp){
        s = mf_malloc(10);
        snprintf(s,10,"app");
    }else if(type==Tterminal){
        s = mf_malloc(10);
        snprintf(s,10,"\\0");
    }else{
        s = mf_malloc(100);
        snprintf(s,100,"token of unkown type");
    }
    return s;
}

static
void print_token(mt_token* t){
    char* s = token_to_string(t);
    printf("[%s]\n",s);
    mf_free(s);
}

static
void print_vtoken(mt_vec* v){
    int i;
    int n = v->size;
    mt_token* t;
    char* s;
    for(i=0; i<n; i++){
        t=mf_vec_get(v,i);
        s = token_to_string(t);
        printf("[%s]",s);
        mf_free(s);
    }
    printf("\n");
}

mt_keyword keywords[] = {
    {3, "and", Top, Top_and},
    {6, "assert", Tkeyword, Tassert},
    {5, "begin", Tkeyword, Tbegin},
    {5, "break", Tkeyword, Tbreak},
    {5, "catch", Tkeyword, Tcatch},
    {8, "continue", Tkeyword, Tcontinue},
    {2, "do", Tkeyword, Tdo},
    {4, "elif", Tkeyword, Telif},
    {4, "else", Tkeyword, Telse},
    {3, "end", Tkeyword, Tend},
    {5, "false", Tbool, 0},
    {2, "fn", Tkeyword, Tfn},
    {3, "for", Tkeyword, Tfor},
    {8, "function", Tkeyword, Tfunction},
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
    {6, "public", Tkeyword, Tglobal},
    {5, "raise", Tkeyword, Traise},
    {6, "return",Tkeyword, Treturn},
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
    int n = sizeof(keywords)/sizeof(mt_keyword);
    mt_bstr s;
    s.size = size;
    s.a = (unsigned char*)a;
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
    if(line<0){
        printf("Syntax error: %s\n",s);
    }else{
        printf("Line %i, col %i\nSyntax error: %s\n",line+1,col+1,s);
    }
}
#endif


static
void unexpected_character(int line, int col, char c){
    char a[200];
    snprintf(a,200,"unexpected character: '%c'.",c);
    syntax_error(line,col,a);
}

static
void unexpected_token(mt_token* t, int line, int col, const char* s){
    char a[200];
    char* p = token_to_string(t);
    snprintf(a,200,"%s, but got %s.",s,p);
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
    mt_token* t2 = mf_vec_get(v,-2);
    if(t2->type!=Tkeyword || t2->value!=Tfunction) return 0;
    mt_token* t1 = mf_vec_get(v,-1);
    if(t1->type!=Tid) return 0;
    int line = t2->line;
    int col = t2->col;
    char* id = t1->s;
    long size = t1->size;
    mf_vec_pop(v,2);
    push_token_string(v,line,col,Tid,size,id);
    push_token(v,line,col,Top,Top_assignment);
    push_token(v,line,col,Tkeyword,Tfn);
    push_token_string(v,line,col,Tid,size,id);
    mf_free(id);
    return 1;
}

int mf_scan(mt_vec* v, unsigned char* s, long n, int line){
    int col = 0;
    unsigned c;
    int j,i = 0;
    int floatsep;
    int imag;
    int hcol,hline;
    while(i<n){
        c=s[i];
        if(isalpha(c) || c=='_'){
            if(c=='b' && i+1<n && s[i+1]=='{'){
                hcol = col; hline = line;
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
                push_token_string(v,hline,hcol,Tbstring,i-j,(char*)s+j);
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
                        push_token(v,line,hcol,Tkeyword,Tbegin);
                        push_token(v,line,hcol,Tkeyword,Tfn);
                        push_token(v,line,hcol,Top,Top_step);
                    }else{
                        push_token(v,line,hcol,Tkeyword,value);
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
                (void)sub_syntax(v);
                push_token(v,line,col,Tbleft,c);
                i++; col++;
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
                break;
            case ',': case ';':
                push_token(v,line,col,Tsep,c);
                i++; col++;
                break;
            case ':':
                if(i+1<n && s[i+1]=='='){
                    push_token(v,line,col,Top,Top_assignment);
                    i+=2; col+=2;
                }else{
                    if(v->size>0){
                        mt_token* t = mf_vec_get(v,-1);
                        if(t->type==Top && t->value==Top_range){
                            push_token(v,line,col,Tnull,0);
                        }
                    }
                    push_token(v,line,col,Tsep,c);
                    i++; col++;
                }
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
    if(v->size>0){
        push_token(v,line,col,Tterminal,0);
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
    if(t->refcount!=1){
        t->refcount--;
        return;
    }
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
    y->refcount=1;
    y->n=n;
    y->line=line;
    y->col=col;
    y->ownership=0;
    return y;
}

ast_node* ast_buffer_to_node(long size, ast_node** a, int line, int col){
    ast_node* y = ast_new(size,line,col);
    ast_node** b = y->a;
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
    op->refcount = 1;
    op->line = t->line;
    op->col = t->col;
    op->type = Top;
    op->value = t->value;
    op->n = 1;
    op->a[0] = p;
    op->ownership = 0;
    return op;
}

static
ast_node* binary_operator(mt_token* t, ast_node* p1, ast_node* p2){
    ast_node* op;
    op = mf_malloc(sizeof(ast_node)+2*sizeof(ast_node*));
    op->refcount = 1;
    op->line = t->line;
    op->col = t->col;
    op->type = Top;
    op->value = t->value;
    op->n = 2;
    op->a[0] = p1;
    op->a[1] = p2;
    op->ownership = 0;
    return op;
}

static
ast_node* ternary_operator(mt_token* t,
    ast_node* p1, ast_node* p2, ast_node* p3
){
    ast_node* op;
    op = mf_malloc(sizeof(ast_node)+3*sizeof(ast_node*));
    op->refcount = 1;
    op->line = t->line;
    op->col = t->col;
    op->type = Top;
    op->value = t->value;
    op->n = 3;
    op->a[0] = p1;
    op->a[1] = p2;
    op->a[2] = p3;
    op->ownership = 0;
    return op;
}

static
ast_node* expression_statement(ast_node* p){
    ast_node* y = ast_new(1,p->line,p->col);
    y->type = Tstatement;
    y->info = POP_VALUE;
    y->a[0] = p;
    return y;
}

static
mt_token* get_any_token(token_iterator* i){
    if(i->index>=i->size) abort();
    return i->a+i->index;
}

static
int scan_line(mt_token* t, token_iterator* i){
    mt_vec v;
    mf_vec_init(&v,sizeof(mt_token));
    mf_push_newline(&v,line_counter,t->col);
    line_counter++;
    char* input = mf_getline_hist("| ");
    int e = mf_scan(&v,(unsigned char*)input,strlen(input),line_counter);
    free(input);
    if(e){
        mf_vtoken_delete(&v);
        return 1;
    }
    #ifdef MV_PRINT
        print_vtoken(&v);
    #endif
    token_iterator* j = mf_malloc(sizeof(token_iterator));
    j->a = i->a;
    j->size = i->size;
    j->index = i->index;
    j->i = NULL;
    i->i = j;

    i->index = 0;
    i->size = v.size;
    i->a = mf_vec_move(&v);
    return 0;
}

static
mt_token* lookup_token(token_iterator* i){
    while(1){
        if(i->index>=i->size){
            printf("Compiler error: unexpected end of stream.\n");
            abort();
        }
        mt_token* t = i->a+i->index;
        if(compiler_context.mode_cmd && syntax_nesting>0 && t->type==Tterminal){
            if(scan_line(t,i)) return NULL;
            continue;
        }
        if(bstack_sp>0 && bstack[bstack_sp-1]=='b'){
            if(t->type==Tsep && t->value=='n'){
                i->index++;
                continue;
            }else if(t->type==Tterminal && compiler_context.mode_cmd){
                if(scan_line(t,i)) return NULL;
                continue;
            }
            return t;
        }else{
            return t;
        }
    }
}

static
mt_token* get_token(token_iterator* i){
    while(1){
        if(i->index>=i->size){
            printf("Compiler error: unexpected end of stream.\n");
            abort();
        }
        mt_token* t = i->a+i->index;
        if(t->type==Tterminal && compiler_context.mode_cmd){
            if(scan_line(t,i)) return NULL;
            continue;
        }
        if(bstack_sp>0 && bstack[bstack_sp-1]=='b'){
            if(t->type==Tsep && t->value=='n'){
                i->index++;
                continue;
            }
            return t;
        }else{
            return t;
        }
    }
}

static
mt_token* get_visible_token(token_iterator* i){
    while(1){
        if(i->index>=i->size){
            printf("Compiler error: unexpected end of stream.\n");
            abort();
        }
        mt_token* t = i->a+i->index;
        if(t->type==Tterminal && compiler_context.mode_cmd){
            if(scan_line(t,i)) return NULL;
            continue;
        }
        if(t->type==Tsep && t->value=='n'){
            i->index++;
            continue;
        }
        return t;
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
    t = i->a+i->index;
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
            t = get_token(i);
            if(t==NULL) goto error;
            if(t->type==Tbright && t->value==']'){
                i->index++;
                break;
            }
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
    int hcolon;
    while(1){
        hcolon=colon; colon=0;
        p1 = expression(i);
        colon=hcolon;
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
            }else if((t->type==Tsep && t->value==',')
                || (t->type==Tbright && t->value=='}')
            ){
                p2 = NULL;
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
        if(t==NULL) goto error;
        comma:
        if(t->type==Tbright && t->value=='}'){
            i->index++;
            break;
        }else if(t->type!=Tsep || t->value!=','){
            syntax_error(t->line,t->col,"expected ',' or '}'.");
            goto error;
        }
        i->index++;
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type==Tbright && t->value=='}'){
            i->index++;
            break;
        }
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
    int prototype = 0;
    mt_vec v;
    mf_vec_init(&v,sizeof(ast_node*));
    mt_token* t;
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type!=Tbleft || t->value!='{'){
        prototype = 1;
        ast_node* p = atom(i);
        if(p==NULL) goto error;
        mf_vec_push(&v,&p);
    }
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tbleft && t->value=='{'){
        i->index++;
    }else{
        syntax_error(t->line,t->col,"expected '{'.");
        goto error;
    }

    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tbright && t->value=='}'){
        goto table_end;
    }
    int hcolon;
    while(1){
        hcolon=colon; colon=0;
        ast_node* p1 = expression(i);
        colon=hcolon;
        if(p1==NULL) goto error;
        mf_vec_push(&v,&p1);
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type!=Tsep || t->value!=':'){
            if(t->type==Top && t->value==Top_assignment){
                if(p1->type==Tid){
                    p1->type = Tstring;
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
        }else if(t->type==Tbright && t->value=='}'){
            table_end:
            i->index++;
            break;
        }else{
            syntax_error(t->line,t->col,"expected ',' or 'end'.");
            goto error;
        }
    }
    ast_node* y;
    y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
    y->type = Tkeyword;
    y->value = Ttable;
    y->info = prototype;
    mf_vec_delete(&v);
    return y;

    error:
    ast_buffer_delete(&v);
    return NULL;
}

static
ast_node* function_application(mt_token* t1, ast_node* p1, token_iterator* i){
    int self = 0;
    ast_node* p;
    mt_vec v;
    mf_vec_init(&v,sizeof(ast_node*));
    mf_vec_push(&v,&p1);
    mt_vec* vmap = NULL;

    mt_token* t = get_token(i);
    if(t==NULL) goto error;
    if(t->type==Tbright && t->value==')'){
        i->index++;
        goto end_of_loop;
    }
    while(1){
        p = expression(i);
        if(p==NULL) goto error;
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type==Top && t->value==Top_assignment){
            if(p->type==Tid){
                p->type=Tstring;
            }else{
                syntax_error(t->line,t->col,"expected identifier before '='.");
            }
        
            i->index++;
            ast_node* p2 = expression(i);
            if(vmap==NULL){
                vmap=mf_malloc(sizeof(mt_vec));
                mf_vec_init(vmap,sizeof(ast_node*));
            }
            mf_vec_push(vmap,&p);
            mf_vec_push(vmap,&p2);
            
            t = get_token(i);
            if(t==NULL) goto error;
        }else{
            mf_vec_push(&v,&p);
        }
        if(t->type==Tsep && t->value==','){
            i->index++;
        }else if(t->type==Tsep && t->value==';'){
            self = 1;
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
    if(vmap){
        ast_node* map = ast_buffer_to_node(vmap->size,(ast_node**)vmap->a,t1->line,t1->col);
        map->type=Top; map->value=Top_map;
        mf_vec_push(&v,&map);
        mf_vec_delete(vmap);
        mf_free(vmap);
    }

    y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
    y->type = Tapp;
    y->value = 0;
    y->info = self;
    mf_vec_delete(&v);
    return y;

    error:
    if(vmap){
        ast_buffer_delete(vmap);
        mf_free(vmap);
    }
    ast_buffer_delete(&v);
    return NULL;
}

static
ast_node* concise_function_literal(token_iterator* i){
    mt_token* t1 = i->a+i->index;
    i->index++;
    bstack[bstack_sp] = 'b';
    bstack_sp++;
    ast_node* p = argument_list(i);
    bstack_sp--;
    if(p==NULL) return NULL;
    ast_node* b = expression(i);
    if(b==NULL){
        ast_delete(p);
        return NULL;
    }
    ast_node* s;
    s = ast_new(1,b->line,b->col);
    s->type = Tstatement;
    s->value = 1;
    s->info = RETURN_VALUE;
    s->a[0] = b;

    ast_node* y;
    y = ast_new(2,t1->line,t1->col);
    y->type = Tkeyword;
    y->value = Tfn;
    y->a[0] = p;
    y->a[1] = s;
    y->s = NULL;
    return y;
}

static
ast_node* inline_statements(mt_token* t, ast_node* p, token_iterator* i){
    i->index++;
    ast_node* b = statement_list(i);
    if(b==NULL){
        ast_delete(p);
        return NULL;
    }
    if(p==NULL){
        p = ast_new(0,t->line,t->col);
        p->type = Tnull;
    }
    ast_node* y = ast_new(2,t->line,t->col);
    y->type = Tinline_block;
    y->a[0] = b;
    y->a[1] = p;
    return y;
}

static
ast_node* atom(token_iterator* i){
    mt_token* t = get_visible_token(i);
    if(t==NULL) return NULL;
    ast_node* p;
    int type = t->type;
    int value = t->value;
    if(type==Tid || type==Tint || type==Tfloat || type==Timag ||
        type==Tstring || type==Tbstring || type==Tnull
    ){
        p = ast_new(0,t->line,t->col);
        p->type = type;
        p->s = t->s;
        p->size = t->size;
        i->index++;
    }else if(type==Tbool){
        p = ast_new(0,t->line,t->col);
        p->type = t->type;
        p->value = t->value;
        i->index++;
    }else if(type==Tbleft && value=='('){
        i->index++;
        bstack[bstack_sp] = 'b';
        bstack_sp++;
        mt_token* t2 = get_token(i);
        if(t2==NULL) return NULL;
        if(t2->type==Tsep && t2->value==';'){
            p = inline_statements(t2,NULL,i);
            bstack_sp--;
            t2 = get_token(i);
            if(t2==NULL) goto error;
            goto bracket_right;
        }
        p = comma_expression(i);
        if(p==NULL){
            bstack_sp--;
            return NULL;
        }
        if(comma_found){
            p->value = Top_tuple;
        }
        t2 = get_token(i);
        bstack_sp--;
        if(t2==NULL) goto error;
        if(t2->type==Tsep && t2->value==';'){
            bstack_sp++;
            p = inline_statements(t2,p,i);
            bstack_sp--;
            if(p==NULL) return NULL;
            t2 = get_token(i);
            if(t2==NULL) goto error;
        }
        bracket_right:
        if(t2->type!=Tbright || t2->value!=')'){
            syntax_error(t2->line,t2->col,"expected ')'.");
            goto error;
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
    }else if(type==Tkeyword && value==Tfn){
        p = function_literal(i);
    }else if(type==Tkeyword && value==Tbegin){
        t = get_token(i);
        if(t==NULL) return NULL;
        i->index++;
        p = function_literal(i);
        p = block_app(p,t);
    }else if(type==Top && value==Top_bor){
        p = concise_function_literal(i);
    }else{
        syntax_error(t->line,t->col,"expected operand.");
        // unexpected_token(t,t->line,t->col,"expected operand");
        return NULL;
    }
    return p;
    error:
    ast_delete(p);
    return NULL;
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
    op->type = Top;
    op->value = Top_index;
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
        mt_token* t = lookup_token(i);
        if(t->type==Tbleft && t->value=='('){
            i->index++;
            bstack[bstack_sp]='b';
            bstack_sp++;
            p1 = function_application(t,p1,i);
            bstack_sp--;
            if(p1==NULL) return NULL;
        }else if(t->type==Tbleft && t->value=='['){
            i->index++;
            bstack[bstack_sp]='b';
            bstack_sp++;
            p1 = index_list(t,p1,i);
            bstack_sp--;
            if(p1==NULL) return NULL;
        }else if(t->type==Top && t->value==Top_dot){
            i->index++;
            ast_node* p2 = atom(i);
            if(p2==NULL) goto error;
            p1 = binary_operator(t,p1,p2);
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
    if(t==NULL) goto error;
    if(t->type==Top && t->value==Top_pow){
        i->index++;
        ast_node* p2 = factor(i);
        if(p2==NULL) goto error;
        ast_node* op = binary_operator(t,p1,p2);
        return op;
    }
    return p1;
    error:
    ast_delete(p1);
    return NULL;
}

static
ast_node* factor(token_iterator* i){
    mt_token* t = get_visible_token(i);
    if(t==NULL) return NULL;
    ast_node *p,*op;
    if(t->type==Top){
        int value=t->value;
        if(value==Top_sub){
            t->value = Top_neg;
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
        if(t==NULL) goto error;
        if(t->type!=Top) break;
        int value=t->value;
        if(value!=Top_mpy && value!=Top_div && value!=Top_idiv &&
            value!=Top_mod
        ){
            break;
        }
        i->index++;
        ast_node* p2 = factor(i);
        if(p2==NULL) goto error;
        p1 = binary_operator(t,p1,p2);
    }
    return p1;
    error:
    ast_delete(p1);
    return NULL;
}

static
ast_node* pm_expression(token_iterator* i){
    ast_node* p1=term(i);
    if(p1==NULL) return NULL;
    while(1){
        mt_token* t = lookup_token(i);
        if(t==NULL) goto error;
        if(t->type!=Top) break;
        if(t->value!=Top_add && t->value!=Top_sub){
            break;
        }
        i->index++;
        ast_node* p2 = term(i);
        if(p2==NULL) goto error;
        p1 = binary_operator(t,p1,p2);
    }
    return p1;
    error:
    ast_delete(p1);
    return NULL;
}

static
ast_node* band_expression(token_iterator *i){
    ast_node* p1 = pm_expression(i);
    if(p1==NULL) return NULL;
    while(1){
        mt_token* t = lookup_token(i);
        if(t==NULL) goto error;
        if(t->type!=Top) break;
        if(t->value!=Top_band){
            break;
        }
        i->index++;
        ast_node* p2 = pm_expression(i);
        if(p2==NULL) goto error;
        p1 = binary_operator(t,p1,p2);
    }
    return p1;
    error:
    ast_delete(p1);
    return NULL;
}

static
ast_node* bor_expression(token_iterator *i){
    ast_node* p1 = band_expression(i);
    if(p1==NULL) return NULL;
    while(1){
        mt_token* t = lookup_token(i);
        if(t==NULL) goto error;
        if(t->type!=Top) break;
        if(t->value!=Top_bor && t->value!=Top_bxor){
            break;
        }
        i->index++;
        ast_node* p2 = band_expression(i);
        if(p2==NULL) goto error;
        p1 = binary_operator(t,p1,p2);
    }
    return p1;
    error:
    ast_delete(p1);
    return NULL;
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
        int value = t->value;
        if(t->type==Top && Top_lt<=value && value<=Top_ge){
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
        int value = t->value;
        if((t->type==Top && Top_eq<=value && value<=Top_isin) ||
             (t->type==Tsep && value==':' && colon)
        ){
            if(t->type==Tsep){
                t->type=Top; t->value=Top_isin;
            }
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
    ast_node* p1 = not_expression(i);
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
ast_node* if_expression(ast_node* p1, token_iterator* i){
    mt_token* t1 = i->a+i->index;
    i->index++;
    ast_node* p2 = expression(i);
    if(p2==NULL) goto error1;
    ast_node* y;
    mt_token* t = lookup_token(i);
    if(t==NULL) goto error2;
    if(t->type!=Tkeyword || t->value!=Telse){
        y=ast_new(2,t1->line,t1->col);
        y->type=Top; y->value=Top_if;
        y->info=0;
        y->a[0]=p2;
        y->a[1]=p1;
        return y;
    }
    i->index++;
    ast_node* p3 = expression(i);
    if(p3==NULL) goto error2;
    y=ast_new(3,t1->line,t1->col);
    y->type = Top; y->value = Top_if;
    y->info = WITH_ELSE_CASE;
    y->a[0] = p2;
    y->a[1] = p1;
    y->a[2] = p3;
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
        if(t==NULL) goto error;
        if(t->type==Tkeyword && t->value==Tif){
            return if_expression(p1,i);
        }
        if(t->type!=Top || t->value!=Top_or) break;
        i->index++;
        ast_node* p2 = and_expression(i);
        if(p2==NULL) goto error;
        p1 = binary_operator(t,p1,p2);
    }
    return p1;
    error:
    ast_delete(p1);
    return NULL;
}

static
ast_node* expression(token_iterator* i){
    ast_node* p = or_expression(i);
    if(p==NULL) return NULL;
    if(i->index<i->size){
        mt_token* t = i->a+i->index;
        char type=t->type;
        if(type==Top && t->value==Top_assignment){
            return p;
        }
        if(type!=Tbright && type!=Tsep && type!=Tkeyword
            && type!=Tterminal
        ){
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
        mt_token* t = get_any_token(i);
        type=t->type; value=t->value;
        if(type==Tsep && value==','){
            i->index++;
            continue;
        }else if(type==Tbright || type==Tsep || type==Tterminal){
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
    }
    // todo
    ast_node* y;
    y = ast_buffer_to_node(v.size,(ast_node**)v.a,-1,-1);
    y->type = Top;
    y->value = Top_list;
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
            comma_found = 1;
            return p2;
        }
    }
    comma_found=0;
    return p1;
}

static
ast_node* index_ast(mt_token* t, ast_node* a, ast_node* i){
    ast_node* op;
    op = ast_new(2,t->line,t->col);
    op->type = Top;
    op->value = Top_index;
    op->a[0] = a;
    op->a[1] = i;
    return op;
}

static
ast_node* strict_assignment(mt_token* t, ast_node* x, ast_node* d){
    ast_node* key = ast_new(0,x->line,x->col);
    key->type = Tstring;
    key->s = x->s;
    key->size = x->size;
    ast_node* get = index_ast(t,d,key);
    ast_node* y = ast_new(2,t->line,t->col);
    y->type = Top;
    y->value = Top_assignment;
    y->a[0] = x;
    y->a[1] = get;
    d->refcount++;
    x->refcount++;
    return y;
}

static
ast_node* conditional_assignment(mt_token* t, ast_node* x, ast_node* x0, ast_node* d){
    // x = d["x"] if "x" in d else x0
    ast_node* get = index_ast(t,d,x);
    ast_node* test = binary_operator(t,x,d);
    test->value = Top_in;

    ast_node* p = ast_new(3,t->line,t->col);
    p->type = Top; p->value = Top_if;
    p->info = WITH_ELSE_CASE;
    p->a[0] = test;
    p->a[1] = get;
    p->a[2] = x0;

    ast_node* y = ast_new(2,t->line,t->col);
    y->type = Top;
    y->value = Top_assignment;

    if(x->type==Tid){x->type=Tstring;}
    ast_node* id = ast_new(0,x->line,x->col);
    id->type = Tid;
    id->s=x->s;
    id->size=x->size;

    y->a[0] = id;
    y->a[1] = p;
    x->refcount+=2;
    d->refcount+=2;
    x0->refcount++;
    return y;
}

static
ast_node* unpack_dict(mt_token* t, ast_node* d, ast_node* value){
    long i,n;
    ast_node* id;
    ast_node* y;
    ast_node** p;
    n = d->n/2;
    if(value->type==Tid){
        id = value;
        y = ast_new(n,t->line,t->col);
        p = y->a;
    }else{
        id = ast_new(0,t->line,t->col);
        id->type = Tid;
        id->s = "_d0_";
        id->size = 4;
        ast_node* assignment = ast_new(2,t->line,t->col);
        assignment->type = Top;
        assignment->value = Top_assignment;
        assignment->a[0] = id;
        id->refcount++;
        assignment->a[1] = value;
        y = ast_new(n+1,t->line,t->col);
        y->a[0] = assignment;
        p = y->a+1;
    }
    y->type = Tblock;
    for(i=0; i<n; i++){
        ast_node* x = d->a[2*i];
        ast_node* x0 = d->a[2*i+1];
        if(x0==NULL){
            p[i] = strict_assignment(t,x,id);
        }else{
            p[i] = conditional_assignment(t,x,x0,id);
        }
    }
    ast_delete(id);
    ast_delete(d);
    return y;
}

static
ast_node* assignment(token_iterator* i){
    ast_node* p1 = comma_expression(i);
    if(p1==NULL) return NULL;
    if(i->index<i->size){
        mt_token* t = i->a+i->index;
        int value = t->value;
        if(t->type==Top &&
            (value==Top_assignment || value==Top_aadd || value==Top_asub ||
            value==Top_ampy || value==Top_adiv || value==Top_amod ||
            value==Top_aidiv)
        ){
            i->index++;
            ast_node* p2 = comma_expression(i);
            if(p2==NULL) goto error;
            if(p1->type==Top && p1->value==Top_map){
                return unpack_dict(t,p1,p2);
            }else{
                return binary_operator(t,p1,p2);
            }
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
    mt_token* t = get_token(i);
    if(t==NULL) goto error1;
    if((t->type!=Tsep || t->value!='n') &&
        (t->type!=Tkeyword || t->value!=Tdo)
    ){
        syntax_error(t->line,t->col,"expected keyword 'do' or newline.");
        goto error1;
    }
    i->index++;
    syntax_nesting++;
    ast_node* p2 = statement_list(i);
    syntax_nesting--;
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
    // TODO: ownership initialization forgotten?
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
            goto error1;
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
    t=get_token(i);
    if(t==NULL) goto error2;
    if(t->type==Tsep && t->value=='n'){
        i->index++;
    }else if(t->type==Tkeyword && t->value==Tdo){
        i->index++;
    }else{
        syntax_error(t->line,t->col,"expected newline or 'do'.");
        goto error2;
    }
    syntax_nesting++;
    ast_node* b = statement_list(i);
    syntax_nesting--;
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
    y->type = Tkeyword;
    y->value = Tfor;
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
    int has_else_case = WITHOUT_ELSE_CASE;
    while(1){
        ast_node* p = expression(i);
        if(p==NULL) goto error;
        mf_vec_push(&v,&p);
        mt_token* t = get_token(i);
        if(t==NULL) goto error;
        if((t->type!=Tsep || t->value!='n') &&
            (t->type!=Tkeyword || t->value!=Tthen)
        ){
            syntax_error(t->line,t->col,"expected 'then' or newline.");
            goto error;
        }
        i->index++;
        syntax_nesting++;
        p = statement_list(i);
        syntax_nesting--;
        if(p==NULL) goto error;
        mf_vec_push(&v,&p);
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type==Tkeyword){
            if(t->value==Telif){
                i->index++;
                continue;
            }else if(t->value==Telse){
                has_else_case = WITH_ELSE_CASE;
                i->index++;
                syntax_nesting++;
                p = statement_list(i);
                syntax_nesting--;
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
    y->type = Tkeyword;
    y->value = Tif;
    y->info = has_else_case;
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
    syntax_nesting++;
    ast_node* p = statement_list(i);
    syntax_nesting--;
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
            if(p1==NULL) goto error;
            t = get_any_token(i);
            if(t->type!=Tsep || t->value!='n'){
                syntax_error(t->line,t->col,"expected newline.");
                ast_delete(p1);
                goto error;
            }
            i->index++;
            catch_type = CATCH_ID_EXP;
        }else{
            catch_type = CATCH_ID;
        }
        syntax_nesting++;
        p2 = statement_list(i);
        syntax_nesting--;
        if(p2==NULL){
            if(catch_type==CATCH_ID_EXP) ast_delete(p1);
            goto error;
        }
        p = identifier(id);
        ast_node* tuple[3];
        if(catch_type==CATCH_ID_EXP){
            tuple[0] = p;
            tuple[1] = p1;
            tuple[2] = p2;
            p = ast_buffer_to_node(3,tuple,id->line,id->col);
            p->type = Top;
            p->value = Top_list;
        }else{
            tuple[0] = p;
            tuple[1] = p2;
            p = ast_buffer_to_node(2,tuple,id->line,id->col);
            p->type = Top;
            p->value = Top_list;
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
    y->type = Tkeyword;
    y->value = Tbreak;
    return y;
}

static
ast_node* continue_statement(mt_token* t, token_iterator* i){
    i->index++;
    ast_node* y = ast_new(0,t->line,t->col);
    y->type = Tkeyword;
    y->value = Tcontinue;
    return y;
}

static
ast_node* return_statement(mt_token* t, token_iterator* i){
    i->index++;
    ast_node* p = comma_expression(i);
    if(p==NULL) return NULL;
    ast_node* y = ast_new(1,t->line,t->col);
    y->type = Tkeyword;
    y->value = Treturn;
    y->a[0] = p;
    return y;
}

static
ast_node* raise_statement(mt_token* t, token_iterator* i){
    i->index++;
    ast_node* p = comma_expression(i);
    if(p==NULL) return NULL;
    ast_node* y = ast_new(1,t->line,t->col);
    y->type = Tkeyword;
    y->value = Traise;
    y->a[0] = p;
    return y;
}

static
ast_node* goto_statement(mt_token* t1, token_iterator* i){
    i->index++;
    mt_token* t = get_token(i);
    if(t==NULL) return NULL;
    if(t->type!=Tid){
        syntax_error(t->line,t->col,"expected identifier.");
        return NULL;
    }
    ast_node* y = ast_new(1,t1->line,t1->col);
    y->type = Tkeyword;
    y->value = Tgoto;
    y->a[0] = identifier(t);
    i->index++;
    return y;
}

static
ast_node* label_statement(mt_token* t1, token_iterator* i){
    i->index++;
    mt_token* t = get_token(i);
    if(t==NULL) return NULL;
    if(t->type!=Tid){
        syntax_error(t->line,t->col,"expected identifier.");
        return NULL;
    }
    ast_node* y = ast_new(1,t1->line,t1->col);
    y->type = Tkeyword;
    y->value = Tlabel;
    y->a[0] = identifier(t);
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
        id->type = Tid;
        id->s = t->s;
        id->size = t->size;
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
    y->type = Tkeyword;
    y->value = Tglobal;
    mf_vec_delete(&v);
    return y;

    error:
    ast_buffer_delete(&v);
    return NULL;
}

static
ast_node* null_literal(int line, int col){
    ast_node* p = ast_new(0,line,col);
    p->type = Tnull;
    return p;
}

static
ast_node* id_to_ast(char* id, mt_token* t){
    ast_node* y = ast_new(0,t->line,t->col);
    y->type = Tid;
    y->s = id;
    y->size = strlen(id);
    return y;
}

static
ast_node* block_app(ast_node* block, mt_token* t){
    ast_node* y = ast_new(1,t->line,t->col);
    y->type = Tapp;
    y->value = 0;
    y->info = 0;
    y->a[0] = block;
    return y;
}

static
ast_node* construct_app(
    char* id, ast_node* arg1, ast_node* arg2, mt_token* t
){
    int n = arg1? (arg2? 3: 2): 1;
    ast_node* y = ast_new(n,t->line,t->col);
    y->type = Tapp;
    y->value = 0;
    y->info = NO_SELF;
    y->a[0] = id_to_ast(id,t);
    if(arg1) y->a[1] = arg1;
    if(arg2) y->a[2] = arg2;
    return y;
}

static
ast_node* construct_statement(ast_node* p, mt_token* t){
    ast_node* y = ast_new(1,t->line,t->col);
    y->type = Tstatement;
    y->info = POP_VALUE;
    y->a[0] = p;
    return y;
}

static
ast_node* get_path(mt_token* t1, token_iterator* i, ast_node** id){
    mt_bs bs;
    mt_token* t;
    mt_token* tid;
    tid = get_token(i);
    if(tid==NULL) return NULL;
    if(tid->type==Tid){
        mf_bs_init(&bs);
        mf_bs_push(&bs,tid->size,(unsigned char*)tid->s);
    }else{
        syntax_error(tid->line,tid->col,"expected identifier.");
        return NULL;
    }
    i->index++;
    while(1){
        t = lookup_token(i);
        if(t==NULL) goto error;
        if(t->type!=Top || t->value!=Top_dot){
            break;
        }
        mf_bs_push(&bs,1,(unsigned char*)"/");
        i->index++;
        tid = get_token(i);
        if(tid==NULL) goto error;
        if(tid->type!=Tid){
            syntax_error(t->line,t->col,"expected identifier.");
            goto error;
        }
        mf_bs_push(&bs,tid->size,(unsigned char*)tid->s);
        i->index++;
    }
    mf_bs_push(&bs,1,(unsigned char*)"\0");
    bs.size--;
    ast_node* y;
    y = ast_new(0,t1->line,t1->col);
    y->type = Tstring;
    y->size = bs.size;
    y->s = (char*)bs.a;
    y->ownership = 1;

    if(*id==NULL){
        *id = ast_new(0,tid->line,tid->col);
        (*id)->type = Tid;
        (*id)->s = tid->s;
        (*id)->size = tid->size;
        (*id)->ownership = 0;
    }

    return y;

    error:
    mf_bs_delete(&bs);
    return NULL;
}

static int qualification;

static
ast_node* construct_assignment(mt_token* t, ast_node* id, ast_node* value){
    ast_node* p = ast_new(2,t->line,t->col);
    p->type = Top;
    p->value = Top_assignment;
    p->a[0] = id;
    p->a[1] = value;
    return p;
}

static
ast_node* module_import(mt_token* t, ast_node* id, ast_node* path, ast_node* context){
    return construct_assignment(t,id,construct_app("load",path,NULL,t));
}

static
ast_node* module_qualification(mt_token* t, ast_node* id, ast_node* path, ast_node* context){
    ast_node* dot = ast_new(2,t->line,t->col);
    dot->type = Top;
    dot->value = Top_dot;
    dot->a[0] = context;
    dot->a[1] = path;
    context->refcount++;
    return construct_assignment(t,id,dot);
}

static
ast_node* import_list(mt_vec* v, mt_token* t1, token_iterator* i,
    ast_node* context, ast_node* (*f)(mt_token*, ast_node*, ast_node*, ast_node*)
){
    ast_node* id;
    ast_node* path;
    mt_token* ta;
    qualification = 0;
    while(1){
        mt_token* t = get_token(i);
        if(t==NULL) goto error;
        if(t->type==Top && t->value==Top_mpy){
            if(context){
                context->refcount++;
                mt_token dot;
                dot.line = t1->line;
                dot.col = t1->col;
                dot.value = Top_dot;
                ast_node* op = binary_operator(&dot,
                    construct_app("gtab",NULL,NULL,t1),
                    id_to_ast("update",t1));        
                ast_node* arg = construct_app("record",context,NULL,t1);

                ast_node* app = ast_new(2,t->line,t->col);
                app->type = Tapp;
                app->value = 0;
                app->info = NO_SELF;
                app->a[0] = op;
                app->a[1] = arg;
                
                ast_node* p = construct_statement(app,t1);
                mf_vec_push(v,&p);

                i->index++;
                ta = lookup_token(i);
                if(ta==NULL) goto error;
                goto next;
            }else{
                syntax_error(t->line,t->col,"unexpected token.");
                goto error;
            }
        }else if(t->type==Tid){
            id = NULL;
            path = get_path(t,i,&id);
            if(path==NULL) goto error;
        }else if(t->type==Tbleft && t->value=='['){
            i->index++;
            path = expression(i);
            if(path==NULL) goto error;
            t = get_token(i);
            if(t==NULL) return NULL;
            if(t->type!=Tbright && t->value!=']'){
                syntax_error(t->line,t->col,"expected ']'.");
                ast_delete(path);
                goto error;
            }
            i->index++;
            qualification = 1;
            return path;
        }else{
            syntax_error(t->line,t->col,"expected identifier.");
            goto error;
        }

        ta = lookup_token(i);
        if(ta==NULL) goto error;
        if(ta->type==Top && ta->value==Top_assignment){
            ast_delete(path);
            i->index++;
            t = get_token(i);
            if(t==NULL) goto error;
            if(t->type==Tid){
                path = get_path(t,i,&id);
            }else if(t->type==Tbleft && t->value=='('){
                i->index++;
                path = expression(i);
                if(path==NULL) goto error;
                t = get_token(i);
                if(t==NULL) goto error;
                if(t->type!=Tbright || t->value!=')'){
                    syntax_error(t->line,t->col,"expected ')'.");
                    goto error;
                }
                i->index++;
            }else{
                syntax_error(t->line,t->col,"expected identifier.");
                goto error;
            }

            ta = lookup_token(i);
            if(ta==NULL) goto error;
        }
        ast_node* y = f(t1,id,path,context);
        mf_vec_push(v,&y);
        
        next:
        if(ta->type==Tterminal || (ta->type==Tsep && ta->value=='n')){
            break;
        }else if(ta->type==Tsep && ta->value==':'){
            qualification = 1;
            break;
        }else if(ta->type==Tbright && ta->value==')'){
            break;
        }else if(ta->type!=Tsep || ta->value!=','){
            syntax_error(ta->line,ta->col,"expected end of line or ','.");
            goto error;
        }
        i->index++;
    }
    return id;

    error:
    return NULL;
}

static
ast_node* import_statement(mt_token* t1, token_iterator* i){
    mt_vec v;
    mf_vec_init(&v,sizeof(token_iterator*));
    i->index++;
    mt_token* t = get_token(i);
    if(t==NULL) goto error;
    int parens;
    if(t->type==Tbleft && t->value=='('){
        bstack[bstack_sp] = 'b';
        bstack_sp++;
        i->index++;
        parens = 1;
    }else{
        parens = 0;
    }
    ast_node* context;
    context = import_list(&v,t1,i,NULL,module_import);
    if(context==NULL) goto error;
    if(qualification){
        i->index++;
        if(import_list(&v,t1,i,context,module_qualification)==NULL) goto error;
    }
    if(parens){
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type!=Tbright || t->value!=')'){
            syntax_error(t->line,t->col,"expected ')'.");
            goto error;
        }
        bstack_sp--;
        i->index++;
    }

    ast_node* y;
    y = ast_buffer_to_node(v.size,(ast_node**)v.a,t1->line,t1->col);
    y->type = Tblock;
    mf_vec_delete(&v);
    return y;

    error:
    ast_buffer_delete(&v);
    return NULL;
}

static
ast_node* assert_statement(mt_token* t0, token_iterator* i){
    i->index++;
    ast_node* a = expression(i);
    if(a==NULL) return NULL;
    mt_token* t = get_token(i);
    if(t==NULL) goto error;
    if(t->type!=Tsep || t->value!=','){
        syntax_error(t->line,t->col,"expected ','.");
        goto error;
    }
    i->index++;
    ast_node* b = expression(i);
    if(b==NULL) goto error;
    ast_node* negation = unary_operator(t0,a);
    negation->value = Top_not;
    ast_node* y = ast_new(2,t0->line,t0->col);
    y->type = Tkeyword;
    y->value = Tif;
    y->info = WITHOUT_ELSE_CASE;
    y->a[0] = negation;
    y->a[1] = construct_app("assertion_failed",b,NULL,t0);
    return y;

    error:
    ast_delete(a);
    return NULL;
}

static
ast_node* statement_list(token_iterator* i){
    mt_vec v;
    mf_vec_init(&v,sizeof(token_iterator*));
    ast_node* p;
    mt_token* t;
    int type,value;
    while(1){
        t = lookup_token(i);
        if(t==NULL) goto error;
        type = t->type;
        value = t->value;
        if(type==Tterminal){
            break;
        }else if(type==Tsep && (value==';' || value=='n')){
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
        }else if(type==Tkeyword && value==Tyield){
            p = return_statement(t,i);
            p->value=Tyield;
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
        }else if(type==Tkeyword && value==Tassert){
            p = assert_statement(t,i);
        }else{
            p = assignment(i);
        }
        if(p==NULL) goto error;
        mf_vec_push(&v,&p);
        t = lookup_token(i);
        if(t==NULL) goto error;
        if(t->type==Tsep && (t->value==';' || t->value=='n')){
            i->index++;
            continue;
        }else if(
            (t->type==Tkeyword &&
                (t->value==Tend || t->value==Telif || t->value==Telse || t->value==Tcatch))
            || (t->type==Tbright && t->value==')')
        ){
            break;
        }else if(t->type==Tterminal){
            break;
        }else{
            syntax_error(t->line,t->col,"expected end of statement.");
            goto error;
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
    y->type = Tblock;
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
    ast_node* p = ast_new(0,-1,-1); // todo
    p->type = Tnull;
    mf_vec_push(&v,&p);
    int argc_min=0;
    int argc_max=0;
    int variadic=0;
    int optional=0;
    mt_token* t = get_token(i);
    if(t==NULL) goto error;
    if((t->type==Top && t->value==Top_bor) ||
        (t->type==Tbright && t->value==')')
    ){
        i->index++;
        y = ast_new(0,-1,-1); // todo
        y->type = Top;
        y->value = Top_list;
        y->info = 0;
        y->info2 = 0;
        mf_vec_delete(&v);
        return y;
    }
    while(1){
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type==Top && t->value==Top_mpy){
            argc_min = v.size-1;
            argc_max = -1;
            variadic = 1;
            i->index++;
        }
        t = get_token(i);
        if(t==NULL) goto error;
        if(t->type==Tid){
            ast_node* id = ast_new(0,t->line,t->col);
            id->type = Tid;
            id->s = t->s;
            id->size = t->size;

            i->index++;
            mt_token* t2 = get_token(i);
            if(t2==NULL) goto error;
            if(t2->type==Top && t2->value==Top_assignment){
                if(optional==0) argc_min = v.size-1;
                optional=1;
                i->index++;
                ast_node* x = band_expression(i);
                if(x==NULL){
                    ast_delete(id);
                    goto error;
                }
                ast_node* p = ast_new(2,t2->line,t2->col);
                p->type = Top;
                p->value = Top_assignment;
                p->a[0] = id;
                p->a[1] = x;
                id = p;
            }
            t = get_token(i);
            if(t==NULL){
                ast_delete(id);
                goto error;
            }
            if(t->type==Tsep && t->value==';'){
                ast_node** v0 = mf_vec_get(&v,0);
                ast_delete(*v0);
                *v0 = id;
                i->index++;
                t = get_token(i);
                if(t==NULL) goto error;
                if((t->type==Top && t->value==Top_bor) ||
                    (t->type==Tbright && t->value==')')
                ){
                    i->index++;
                    break;
                }else{
                    continue;
                }
            }else{
                mf_vec_push(&v,&id);
            }
        }else{
            syntax_error(t->line,t->col,"expected identifier.");
            goto error;
        }
        if(t->type==Tsep && (t->value==',' || t->value==';')){
            if(variadic){
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
    y->type = Top;
    y->value = Top_list;
    if(variadic){
        y->info = argc_min;
        y->info2 = argc_max;
    }else if(optional){
        y->info = argc_min;
        y->info2 = v.size-1;
    }else{
        y->info = v.size-1;
        y->info2 = v.size-1;
    }
    // printf("y->info = %i\n",y->info);
    mf_vec_delete(&v);
    return y;

    error:
    ast_buffer_delete(&v);
    return NULL;
}

static
ast_node* function_literal(token_iterator* i){
    mt_token* t1 = i->a+i->index;
    i->index++;
    mt_token* id;
    mt_token* t = get_token(i);
    if(t==NULL) return NULL;
    if(t->type==Tid){
        id = t; i->index++;
    }else{
        id = NULL;
    }
    ast_node* p;
    t = get_token(i);
    if(t==NULL) return NULL;
    if(t->type==Top && t->value==Top_step){
        i->index++;
        p = ast_new(0,t1->line,t1->col);
        p->type = Top;
        p->value = Top_list;
        p->info = 0;
        p->info2 = 0;
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
    syntax_nesting++;
    ast_node* b = statement_list(i);
    syntax_nesting--;
    bstack_sp--;
    if(b==NULL){
        ast_delete(p);
        return NULL;
    }
    if(b->type==Tstatement){
        b->info = RETURN_VALUE;
    }else if(b->type==Tblock && b->n>0){
        ast_node* last = b->a[b->n-1];
        if(last->type==Tstatement){
            last->info = RETURN_VALUE;
        }
    }
    t = get_token(i);
    if(t==NULL) goto error;
    if(t->type!=Tkeyword || t->value!=Tend){
        syntax_error(t->line,t->col,"expected end of fn.");
        goto error;
    }
    i->index++;
    if(end_of(i,Tfn,"expected 'end of fn'.")){
        goto error;
    }
    ast_node* y;
    y = ast_new(2,t1->line,t1->col);
    y->type = Tkeyword;
    y->value = Tfn;
    y->a[0] = p;
    y->a[1] = b;
    if(id){
        y->s = id->s;
        y->size = id->size;
    }else{
        y->s = NULL;
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
ast_node* ast(mt_vec* v, token_iterator* i){
    ast_node* y = statement_list(i);
    if(y==NULL) return NULL;
    mt_token* t = get_any_token(i);
    if(t->type!=Tterminal){
        syntax_error(t->line,t->col,"expected end of stream but found a token.");
        return NULL;
    }
    return y;
}

static
void ast_print(ast_node* t, int indent){
    if(t==NULL){
        printf("Error: no AST.\n");
        return;
    }
    int type = t->type;
    int value = t->value;
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
        printf("%s\n",operator_to_string(value));
    }else if(type==Tapp){
        print_space(indent);
        printf("app\n");
    }else if(type==Tstatement){
        print_space(indent);
        printf("statement\n");
    }else if(type==Tblock){
        print_space(indent);
        printf("block\n");
    }else if(type==Tinline_block){
        print_space(indent);
        printf("inline block\n");
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
    }else if(type==Tkeyword && value==Tfn){
        print_space(indent);
        printf("fn\n");
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
    }else if(type==Tkeyword && value==Tassert){
        print_space(indent);
        printf("assert\n");
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
        *type = MV_GLOBAL;
        return index;
    }
    index = vtab_index(vtab,MV_CONTEXT,s,size);
    if(index>=0){
        *type = MV_CONTEXT;
        return index;
    }
    index = vtab_index(vtab,MV_ARGUMENT,s,size);
    if(index>=0){
        *type = MV_ARGUMENT;
        return index;
    }
    index = vtab_index(vtab,MV_LOCAL,s,size);
    if(index>=0){
        *type = MV_LOCAL;
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
    info.s = s;
    info.size = size;
    info.ownership = ownership;
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
    int size = bv2->size;
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
    c = tolower(c);
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
    uint32_t x = 0;
    int i,d;
    for(i=0; i<size; i++){
        d = mf_hex_digit(a[i]);
        if(d<0){
            printf("Syntax error: expected hexadecimal digit.\n");
            exit(1);
        }
        x = 16*x+d;
    }
    return x;
}

int mf_bhtoi(int size, const unsigned char* a){
    int x = 0;
    int i,d;
    for(i=0; i<size; i++){
        d=mf_hex_digit(a[i]);
        if(d<0){
            printf("Syntax error: expected hexadecimal digit.\n");
            exit(1);
        }
        x = 16*x+d;
    }
    return x;
}

static
int compile_string_literal(mt_vec* data, int line, int col,
    long size, const char* s
){
    int i,n;
    mt_str buffer;
    mf_decode_utf8(&buffer,size,(const unsigned char*)s);
    push_u8(data,MT_STRING);
    int index = data->size;
    push_u32(data,0); // placeholder
    uint32_t* a = buffer.a;
    i=0; n=0;
    while(i<buffer.size){
        if(a[i]=='\\'){
            if(i+1>=buffer.size){
                syntax_error(line,col,"invalid escape sequenze.");
                return 1;
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
                    syntax_error(line,col,"invalid escape sequence.");
                    return 1;
                }
                i++;
                push_u32(data,mf_htoi(a+i,2));
                i++;
            }else if(c=='u'){
                if(i+4>=buffer.size){
                    syntax_error(line,col,"invalid unicode sequence.");
                    return 1;
                }
                i++;
                push_u32(data,mf_htoi(a+i,4));
                i+=3;
            }else if(c=='U'){
                if(i+8>=buffer.size){
                    syntax_error(line,col,"invalid unicode sequence.");
                    return 1;
                }
                i++;
                push_u32(data,mf_htoi(a+i,8));
                i+=7;
            }else if(c==' ' || c=='\n' || c=='\t'){
                while(i<buffer.size && (a[i]==' ' || a[i]=='\n' || a[i]=='\t')){
                    i++;
                }
                continue;
            }else if(c=='r'){
                push_u32(data,'\r');
            }else if(c=='e'){
                push_u32(data,'\x1b');
            }else if(c=='0'){
                push_u32(data,'\x00');
            }else{
                syntax_error(line,col,"invalid escape sequenze.");
                mf_free(buffer.a);
                return 1;
            }
            i++; n++;
        }else{
            push_u32(data,a[i]);
            i++; n++;
        }
    }
    *(uint32_t*)(data->a+index)=n;
    mf_free(buffer.a);
    return 0;
}

static
int compile_bstring_literal(mt_vec* data, int line, int col,
    long size, const unsigned char* a
){
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
            syntax_error(line,col,"invalid byte string literal.");
            return 1;
        }
    }
    *(uint32_t*)(data->a+index)=n;
    return 0;
}

typedef struct{
    char type;
    unsigned char* a;
    long size;
} mt_stab_element;

static
void stab_delete(mt_vec* v){
    int size = v->size;
    mt_stab_element* a = (mt_stab_element*)v->a;
    int i;
    for(i=0; i<size; i++){
        mf_free(a[i].a);
    }
    mf_vec_delete(v);
}

static
int stab_find(mt_vec* v, int type, const char* s, long size){
    int n = v->size;
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
int push_string(mt_vec* bv, mt_vec* stab,
    int line, int col, long size, const char* a
){
    int index = stab_find(stab,MT_STRING,a,size);
    if(index>=0){
        push_u32(bv,index);
    }else{
        if(compile_string_literal(&data,line,col,size,a)) return 1;
        push_u32(bv,stab->size);
        mt_stab_element e;
        e.type = MT_STRING;
        e.a = mf_malloc(size+1);
        memcpy(e.a,a,size+1);
        e.size = size;
        mf_vec_push(stab,&e);
    }
    // stab_print(stab);
    return 0;
}

static
int push_bstring(mt_vec* bv, mt_vec* stab,
    int line, int col, long size, const char* a
){
    int index = stab_find(stab,MT_STRING,a,size);
    if(index>=0){
        push_u32(bv,index);
    }else{
        if(compile_bstring_literal(&data,line,col,size,(unsigned char*)a)){
            return 1;
        }
        push_u32(bv,stab->size);
        mt_stab_element e;
        e.type = MT_BSTRING;
        e.a = mf_malloc(size+1);
        memcpy(e.a,a,size+1);
        e.size = size;
        mf_vec_push(stab,&e);
    }
    return 0;
}

static
int compile_string(mt_vec* bv, ast_node* t){
    push_bc(bv,CONST_STR,t);
    return push_string(bv,&stab,t->line,t->col,t->size,t->s);
}

static
int compile_bstring(mt_vec* bv, ast_node* t){
    push_bc(bv,CONST_STR,t);
    return push_bstring(bv,&stab,t->line,t->col,t->size,t->s);
}

static int ast_compile(mt_vec* bv, ast_node* t);
static
int compile_index(mt_vec* bv, ast_node* t){
    if(ast_compile(bv,t->a[0])) return 1;
    if(ast_compile(bv,t->a[1])) return 1;
    push_bc(bv,GETREF,t);
    return 0;
}

static
int compile_dot(mt_vec* bv, ast_node* t){
    if(ast_compile(bv,t->a[0])) return 1;
    if(compile_string(bv,t->a[1])) return 1;
    push_bc(bv,OBGETREF,t);
    return 0;
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
            *type = MV_CONTEXT;
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
        push_u32(bv,index);      
    }else if(type==MV_LOCAL){
        push_bc(bv,LOAD_LOCAL,t);
        push_u32(bv,index);
    }else if(type==MV_CONTEXT){
        push_bc(bv,LOAD_CONTEXT,t);
        push_u32(bv,index);
    }else if(type==MV_GLOBAL){
        global:
        push_bc(bv,LOAD,t);
        push_string(bv,&stab,t->line,t->col,t->size,t->s);
    }else{
        puts("Compiler error in compile_variable.");
        exit(1);
    }
}

static
int ast_load_ref_ownership;

static
int ast_load_ref(mt_vec* bv, ast_node* t){
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
                    push_u32(bv,index);
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
            push_string(bv,&stab,t->line,t->col,t->size,t->s);
        }
    }else if(t->type==Top && t->value==Top_index){
        if(compile_index(bv,t)) return 1;
    }else if(t->type==Top && t->value==Top_dot){
        if(compile_dot(bv,t)) return 1;
    }else{
        puts("Error in ast_load_ref.");
        abort();
    }
    return 0;
}

static
void closure(mt_vec* bv, mt_var_tab* vtab){
    mt_var_info* a = (mt_var_info*)vtab->c.a;
    int size = vtab->c.size;
    int index,type;
    int i;
    for(i=0; i<size; i++){
        index = vtab_index_type(&type,vtab->context,a[i].s,a[i].size);
        if(index<0){
            puts("Compiler error in closure.");
            exit(1);
        }
        if(type==MV_LOCAL){
            push_bc(bv,LOAD_LOCAL,NULL);
            push_u32(bv,index);
        }else if(type==MV_ARGUMENT){
            push_bc(bv,LOAD_ARG,NULL);
            push_u32(bv,index);
        }else if(type==MV_CONTEXT){
            push_bc(bv,LOAD_CONTEXT,NULL);
            push_u32(bv,index);
        }else if(type==MV_GLOBAL){
            push_bc(bv,LOAD,NULL);
            push_string(bv,&stab,-1,-1,a[i].size,a[i].s);
        }else{
            puts("Compiler error in closure, unknown type.");
            exit(1);
        }
    }
    push_bc(bv,TUPLE,NULL);
    push_u32(bv,size);
}

static
int compile_index_assignment(mt_vec* bv, ast_node* t){
    if(ast_compile(bv,t->a[0])) return 1;
    if(ast_compile(bv,t->a[1])) return 1;
    push_bc(bv,STORE_INDEX,t);
    push_u32(bv,1);
    return 0;
}

static
int compile_unpacking(mt_vec* bv, ast_node* t){
    int i;
    for(i=0; i<t->n; i++){
        push_bc(bv,LIST_POP,t);
        push_u32(bv,i);
        if(t->a[i]->type==Top && t->a[i]->value==Top_index){
            if(compile_index_assignment(bv,t->a[i])) return 1;
        }else{
            if(ast_load_ref(bv,t->a[i])) return 1;
            push_bc(bv,STORE,t);
        }
    }
    push_bc(bv,POP,t);
    return 0;
}

static
int ast_compile_app(mt_vec* bv, ast_node* t){
    long i;
    if(t->info==SELF){
        if(ast_compile(bv,t->a[0])) return 1;
        for(i=1; i<t->n; i++){
            if(ast_compile(bv,t->a[i])) return 1;
        }
        push_bc(bv,CALL,t);
        push_u32(bv,t->n-2);
        return 0;
    }
    if(t->a[0]->type==Top && t->a[0]->value==Top_dot){
        if(ast_compile(bv,t->a[0]->a[0])) return 1;
        push_bc(bv,DUP,t);
        if(compile_string(bv,t->a[0]->a[1])) return 1;
        push_bc(bv,OBGET,t);
        push_bc(bv,SWAP,t);
    }else{
        if(ast_compile(bv,t->a[0])) return 1;
        push_bc(bv,CONST_NULL,t);
    }
    for(i=1; i<t->n; i++){
        if(ast_compile(bv,t->a[i])) return 1;
    }
    push_bc(bv,CALL,t);
    push_u32(bv,t->n-1);
    return 0;
}

static
int ast_compile_ifop2(mt_vec* bv, ast_node* t){
    // if c then a else b end
    // c JPZ[1] a JMP[2] (1) b (2)
    if(ast_compile(bv,t->a[0])) return 1;
    push_bc(bv,JPZ,t);
    int index1 = bv->size;
    push_u32(bv,0); // placeholder
    if(ast_compile(bv,t->a[1])) return 1;
    push_bc(bv,JMP,t);
    int index2 = bv->size;
    push_u32(bv,0); // placeholder
    *(uint32_t*)(bv->a+index1)=BC+bv->size-index1;
    if(ast_compile(bv,t->a[2])) return 1;
    *(uint32_t*)(bv->a+index2)=BC+bv->size-index2;
    return 0;
}

static
int ast_compile_ifop(mt_vec* bv, ast_node* t){
    uint32_t a[200];
    int m = t->n/2;
    int i,index;
    for(i=0; i<m; i++){
        if(ast_compile(bv,t->a[2*i])) return 1;
        push_bc(bv,JPZ,t);
        index = bv->size;
        push_u32(bv,0xcafe);
        if(ast_compile(bv,t->a[2*i+1])) return 1;
        push_bc(bv,JMP,t);
        a[i] = bv->size;
        push_u32(bv,0xcafe);
        *(uint32_t*)(bv->a+index)=BC+bv->size-index;
    }
    if(t->info==WITH_ELSE_CASE){
        if(ast_compile(bv,t->a[t->n-1])) return 1;
    }else{
        push_bc(bv,CONST_NULL,t);
    }
    for(i=0; i<m; i++){
        *(uint32_t*)(bv->a+a[i]) = BC+bv->size-a[i]; 
    }
    return 0;
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
int ast_compile_if(mt_vec* bv, ast_node* t){
    int has_else_case = t->info;
    uint32_t a[200];
    int m = t->n/2;
    int i;
    int index;
    nesting_level++;
    for(i=0; i<m; i++){
        if(ast_compile(bv,t->a[2*i])) return 1;
        push_bc(bv,JPZ,t);
        index = bv->size;
        push_u32(bv,0xcafe);
        if(ast_compile(bv,t->a[2*i+1])) return 1;
        push_bc(bv,JMP,t);
        a[i] = bv->size;
        push_u32(bv,0xcafe);
        *(uint32_t*)(bv->a+index) = BC+bv->size-index;
    }
    if(has_else_case){
        if(ast_compile(bv,t->a[t->n-1])) return 1;
    }
    for(i=0; i<m; i++){
        *(uint32_t*)(bv->a+a[i]) = BC+bv->size-a[i]; 
    }
    nesting_level--;
    return 0;
}

static
int ast_compile_while(mt_vec* bv, ast_node* t){
    // while c do b end
    // (1) c JPZ[2] b JMP[1] (2)
    int index1 = bv->size;
    if(jmp_sp==JMP_STACK_SIZE){
        puts("Compiler error: reached JMP_STACK_SIZE.");
        exit(1);
    }
    jmp_stack[jmp_sp] = index1;
    mf_vec_init(out_stack+jmp_sp,sizeof(uint32_t));
    jmp_sp++;
    if(ast_compile(bv,t->a[0])) return 1;
    push_bc(bv,JPZ,t);
    int index2 = bv->size;
    push_u32(bv,0xcafe);
    nesting_level++;
    if(ast_compile(bv,t->a[1])) return 1;
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
    return 0;
}

static
void load_global(mt_vec* bv, const char* a, long size, ast_node* t){
    push_bc(bv,LOAD,t);
    push_string(bv,&stab,-1,-1,size,a);
}

static
void generate_call(mt_vec* bv, int argc, ast_node* t){
    push_bc(bv,CALL,t);
    push_u32(bv,argc);
}

static
int ast_compile_for(mt_vec* bv, ast_node* t){
    ast_node* list = t->a[0];
    load_global(bv,"iter",4,t);
    push_bc(bv,CONST_NULL,t);
    if(ast_compile(bv,t->a[1])) return 1;
    generate_call(bv,1,t);
    ast_node id;
    char* sbuffer = mf_malloc(10);
    snprintf(sbuffer,10,"__it%i",for_level);
    id.type = Tid;
    id.line = -1;
    id.col = -1;
    id.s = sbuffer;
    id.size = strlen(sbuffer);
    id.n = 0;
    ast_load_ref_ownership = 1;
    ast_load_ref(bv,&id);
    ast_load_ref_ownership = 0;
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
        if(ast_load_ref(bv,list->a[0])) return 1;
        push_bc(bv,STORE,t);
    }else{
        if(compile_unpacking(bv,list)) return 1;
    }
    nesting_level++; for_level++;
    if(ast_compile(bv,t->a[2])) return 1;
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
    return 0;
}

static
int ast_compile_augmented(mt_vec* bv, ast_node* t){
    int value = t->value;
    if(ast_compile(bv,t->a[1])) return 1;
    if(ast_load_ref(bv,t->a[0])) return 1;
    push_bc(bv,LOAD_POINTER,t);
    push_bc(bv,SWAP,t);
    if(value==Top_aadd){
        push_bc(bv,ADD,t);
    }else if(value==Top_asub){
        push_bc(bv,SUB,t);
    }else if(value==Top_ampy){
        push_bc(bv,MUL,t);
    }else if(value==Top_adiv){
        push_bc(bv,DIV,t);
    }else if(value==Top_aidiv){
        push_bc(bv,IDIV,t);
    }else if(value==Top_amod){
        push_bc(bv,MOD,t);
    }else{
        puts("Error in ast_compile_augmented.");
        abort();
    }
    push_bc(bv,STORE,t);
    return 0;
}

static
int ast_argument_list(mt_var_tab* vtab, ast_node* t){
    int i;
    ast_node* id;
    // mt_var_info info;
    for(i=0; i<t->n; i++){
        id = t->a[i];
        if(id->type==Tnull){
            vtab_push(vtab,MV_ARGUMENT,"self",4,0);
            continue;
        }else if(id->type==Top && id->value==Top_assignment){
            id = id->a[0];
        }
        if(vtab_index(vtab,MV_LOCAL,id->s,id->size)>=0){
            syntax_error(t->line,t->col,"identifier occurs multiple times.");
            return 1;
        }
        vtab_push(vtab,MV_ARGUMENT,id->s,id->size,0);
    }
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
    t.a = a;
    t.size = size;
    t.index = index;
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
ast_node* if_null(ast_node* t){
    ast_node* y = ast_new(2,t->line,t->col);
    y->type = Tkeyword;
    y->value = Tif;
    y->info = WITHOUT_ELSE_CASE;
    ast_node* condition = ast_new(2,t->line,t->col);
    condition->type = Top;
    condition->value = Top_is;
    t->a[0]->refcount++;
    condition->a[0] = t->a[0];
    condition->a[1] = null_literal(t->line,t->col);
    y->a[0] = condition;
    t->refcount++;
    y->a[1] = t;
    return y;
}

static
int ast_default_args(mt_vec* bv, ast_node* t){
    int i;
    ast_node** a = t->a;
    for(i=0; i<t->n; i++){
        if(a[i]->type==Top && a[i]->value==Top_assignment){
            ast_node* y = if_null(a[i]);
            int e = ast_compile(bv,y);
            ast_delete(y);
            if(e) return 1;
        }
    }
    return 0;
}

static
int ast_compile_function(mt_vec* bv, ast_node* t){
    mt_vec bv2;
    mf_vec_init(&bv2,sizeof(unsigned char));
    mt_var_tab vtab;
    vtab.context = context;
    vtab.fn_name = NULL;
    mf_vec_init(&vtab.v,sizeof(mt_var_info));
    mf_vec_init(&vtab.c,sizeof(mt_var_info));
    mf_vec_init(&vtab.g,sizeof(mt_var_info));
    mf_vec_init(&vtab.a,sizeof(mt_var_info));
    // vtab_print(&vtab,4);
    int e = ast_argument_list(&vtab,t->a[0]);
    if(e) return 1;
    int argc_min = t->a[0]->info;
    int argc_max = t->a[0]->info2;
    vtab.fn_name = (unsigned char*)t->s;
    vtab.fn_name_size = t->size;
    mt_vec indices;
    mf_vec_init(&indices,sizeof(int));
    mt_vec* tindices = const_fn_indices;
    const_fn_indices = &indices;

    mt_vec* hlabel_tab = label_tab;
    mt_vec* hgoto_tab = goto_tab;
    label_tab = label_tab_new();
    goto_tab = label_tab_new();

    context = &vtab;
    // vtab_print(context,4);
    nesting_level++; scope++;
    e = ast_default_args(&bv2,t->a[0]);
    e = ast_compile(&bv2,t->a[1]);
    nesting_level--; scope--;
    context = vtab.context;

    // printf("[%i,%i,%i]\n", bv2.size, bv_deposit.size, bv->size);
    offsets(&bv2,-bv_deposit.size,&indices);
    gotos(&bv2);
    label_tab_delete(label_tab);
    label_tab_delete(goto_tab);
    label_tab = hlabel_tab;
    goto_tab = hgoto_tab;


    const_fn_indices = tindices;
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
        push_string(bv,&stab,t->line,t->col,strlen(a),a);
    }
    push_bc(bv,CONST_FN,t);
    int index = bv->size;
    mf_vec_push(const_fn_indices,&index);
    push_u32(bv,bv_deposit.size);
    push_u32(bv,argc_min);
    push_u32(bv,argc_max);
    push_u32(bv,vtab.v.size); // var_count

    bv_append(&bv_deposit,&bv2);
    mf_vec_delete(&bv2);
    vtab_delete(&vtab);

    return e;
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
int ast_compile_try(mt_vec* bv, ast_node* t){
    push_bc(bv,TRY,t);
    int index = bv->size;
    push_u32(bv,0xcafe);
    if(ast_compile(bv,t->a[0])) return 1;
    push_bc(bv,TRYEND,t);
    push_bc(bv,JMP,t);
    int index2 = bv->size;
    push_u32(bv,0xcafe);
    *(uint32_t*)(bv->a+index) = bv->size+4-index;
    ast_node* p = t->a[1];
    if(p->info==CATCH_ID){
        push_bc(bv,TRYEND,t);
        push_bc(bv,EXCEPTION,t);
        if(ast_load_ref(bv,p->a[0])) return 1;
        push_bc(bv,STORE,t);
        if(ast_compile(bv,p->a[1])) return 1;
    }else if(p->info==CATCH_ID_EXP){
        push_bc(bv,TRYEND,t);
        push_bc(bv,EXCEPTION,t);
        if(ast_compile(bv,p->a[1])) return 1;
        push_bc(bv,ISIN,t);
        push_bc(bv,JPZ,t);
        int index3 = bv->size;
        push_u32(bv,0xcafe);
        push_bc(bv,EXCEPTION,t);
        if(ast_load_ref(bv,p->a[0])) return 1;
        push_bc(bv,STORE,t);
        if(ast_compile(bv,p->a[2])) return 1;
        push_bc(bv,JMP,t);
        int index4 = bv->size;
        push_u32(bv,0xcafe);
        *(uint32_t*)(bv->a+index3) = BC+bv->size-index3;
        push_bc(bv,EXCEPTION,t);
        push_bc(bv,RAISE,t);
        *(uint32_t*)(bv->a+index4) = BC+bv->size-index4;
    }
    *(uint32_t*)(bv->a+index2) = BC+bv->size-index2;
    return 0;
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
        }else{
            abort();
        }
    }else{
        return strtol(s,NULL,10);
    }
}

static
int id_add_set(ast_node* t){
    if(t->type==Tid){
        int size = t->size+4;
        char* s = mf_malloc(size+1);
        strcpy(s,"set_");
        s = strncat(s,t->s,size);
        if(t->ownership){
            mf_free(t->s);
        }
        t->ownership = 1;
        t->size = size;
        t->s = s;
        return 0;
    }else if(t->type==Top && t->value==Top_dot){
        return id_add_set(t->a[1]);
    }else{
        return 1;
    }
}

static
ast_node* app_append(ast_node* t, ast_node* a){
    ast_node* y = ast_new(t->n+1,t->line,t->col);
    y->type = Tapp;
    y->value = 0;
    y->info = t->info;
    int i;
    for(i=0; i<t->n; i++){
        y->a[i] = t->a[i];
    }
    y->a[t->n] = a;
    return y;
}

static
int ast_compile(mt_vec* bv, ast_node* t){
    int value;
    switch(t->type){
    case Tstatement:
        if(ast_compile(bv,t->a[0])) return 1;
        if(t->info==RETURN_VALUE){
            push_bc(bv,RET,t);
        }else if(compiler_context.mode_cmd){
            if(nesting_level>0){
                push_bc(bv,POP,t);
            }
        }else{
            push_bc(bv,POP,t);
        }
        break;
    case Tbool:
        push_bc(bv,CONST_BOOL,t);
        push_u32(bv,t->value);
        break;
    case Tint:{
        errno=0;
        long value = string_to_long(t->size,t->s);
        if(errno){
            compile_string(bv,t);
            push_bc(bv,LONG,t);
        }else{
            push_bc(bv,CONST_INT,t);
            push_u32(bv,value);
        }
    } break;
    case Tfloat:{
        double value = atof(t->s);
        push_bc(bv,CONST_FLOAT,t);
        push_double(bv,value);
    } break;
    case Timag:{
        double value = atof(t->s);
        push_bc(bv,CONST_IMAG,t);
        push_double(bv,value);
    } break;
    case Tnull:
        push_bc(bv,CONST_NULL,t);
        break;
    case Tstring:
        if(compile_string(bv,t)) return 1;
        break;
    case Tbstring:
        if(compile_bstring(bv,t)) return 1;
        break;
    case Tid:
        compile_variable(bv,t);
        break;
    case Top:
        switch(t->value){
        case Top_add:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,ADD,t);
            break;
        case Top_sub:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,SUB,t);
            break;
        case Top_mpy:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,MUL,t);
            break;
        case Top_div:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,DIV,t);
            break;
        case Top_idiv:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,IDIV,t);
            break;
        case Top_mod:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,MOD,t);
            break;
        case Top_pow:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,POW,t);
            break;
        case Top_neg:
            if(ast_compile(bv,t->a[0])) return 1;
            push_bc(bv,NEG,t);
            break;
        case Top_tilde:
            if(ast_compile(bv,t->a[0])) return 1;
            push_bc(bv,TILDE,t);
            break;
        case Top_not:
            if(ast_compile(bv,t->a[0])) return 1;
            push_bc(bv,NOT,t);
            break;
        case Top_lt:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,LT,t);
            break;
        case Top_gt:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,GT,t);
            break;
        case Top_le:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,LE,t);
            break;
        case Top_ge:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,GE,t);
            break;
        case Top_eq:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,EQ,t);
            break;
        case Top_ne:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,NE,t);
            break;
        case Top_is:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,IS,t);
            break;
        case Top_in:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,IN,t);
            break;
        case Top_notin:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,NOTIN,t);
            break;
        case Top_isin:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,ISIN,t);
            break;
        case Top_bor:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,BIT_OR,t);
            break;
        case Top_bxor:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,BIT_XOR,t);
            break;
        case Top_band:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            push_bc(bv,BIT_AND,t);
            break;
        case Top_range:
            if(ast_compile(bv,t->a[0])) return 1;
            if(ast_compile(bv,t->a[1])) return 1;
            if(t->n==3){
                if(ast_compile(bv,t->a[2])) return 1;
            }else{
                push_bc(bv,CONST_INT,t);
                push_u32(bv,1);
            }
            push_bc(bv,RANGE,t);
            break;
        case Top_index:{
            int i;
            for(i=0; i<t->n; i++){
                if(ast_compile(bv,t->a[i])) return 1;
            }
            push_bc(bv,GET,t);
            push_u32(bv,t->n-1);
        } break;
        case Top_dot:
            if(ast_compile(bv,t->a[0])) return 1;
            if(t->a[1]->type!=Tid && t->a[1]->type!=Tstring){
                syntax_error(t->line,t->col,"expected an identifier behind a dot operator.");
                return 1;
            }
            if(compile_string(bv,t->a[1])) return 1;
            push_bc(bv,OBGET,t);
            break;
        case Top_list:{
            long i;
            for(i=0; i<t->n; i++){
                if(ast_compile(bv,t->a[i])) return 1;
            }
            push_bc(bv,LIST,t);
            push_u32(bv,t->n);
        } break;
        case Top_map:{
            long i;
            for(i=0; i<t->n; i++){
                if(t->a[i]==NULL){
                    push_bc(bv,CONST_NULL,t);
                }else{
                    if(ast_compile(bv,t->a[i])) return 1;
                }
            }
            push_bc(bv,MAP,t);
            push_u32(bv,t->n);
        } break;
        case Top_tuple:{
            long i;
            for(i=0; i<t->n; i++){
                if(ast_compile(bv,t->a[i])) return 1;
            }
            push_bc(bv,TUPLE,t);
            push_u32(bv,t->n);
        } break;
        case Top_assignment:{
            ast_node* x=t->a[0];
            if(x->type==Tapp){
                (void)id_add_set(x->a[0]);
                ast_node* p = app_append(x,t->a[1]);
                int e = ast_compile(bv,p);
                mf_free(p);
                if(e) return 1;
                push_bc(bv,POP,t);
                break;
            }
            if(ast_compile(bv,t->a[1])) return 1;
            if(x->type==Top && x->value==Top_list){
                if(compile_unpacking(bv,x)) return 1;
            }else if(x->type==Top && x->value==Top_index){
                if(compile_index_assignment(bv,x)) return 1;
            }else{
                if(ast_load_ref(bv,x)) return 1;
                push_bc(bv,STORE,t);
            }
        } break;
        case Top_aadd: case Top_asub: case Top_ampy:
        case Top_adiv: case Top_amod: case Top_aidiv:
            if(ast_compile_augmented(bv,t)) return 1;
            break;
        case Top_and:{
            // Use a AND[1] b (1) instead of
            // a JPZ[1] b JMP[2] (1) CONST_BOOL false (2).
            if(ast_compile(bv,t->a[0])) return 1;
            push_bc(bv,AND,t);
            int index = bv->size;
            push_u32(bv,0xcafe);
            if(ast_compile(bv,t->a[1])) return 1;
            *(uint32_t*)(bv->a+index)=BC+bv->size-index;
        } break;
        case Top_or:{
            // Use a OR[1] b (1) instead of
            // a JPZ[1] CONST_BOOL true JMP[2] (1) b (2).
            if(ast_compile(bv,t->a[0])) return 1;
            push_bc(bv,OR,t);
            int index = bv->size;
            push_u32(bv,0xcafe);
            if(ast_compile(bv,t->a[1])) return 1;
            *(uint32_t*)(bv->a+index)=BC+bv->size-index;
        } break;
        case Top_if:
            if(ast_compile_ifop(bv,t)) return 1;
            break;
        default:
            printf("Error in ast_compile: unknown operator, value: %i.\n",t->value);
            abort();
        }
        break;
    case Tapp:
        ast_compile_app(bv,t);
        break;
    case Tblock:{
        int i;
        for(i=0; i<t->n; i++){
            if(ast_compile(bv,t->a[i])) return 1;
        }
    } break;
    case Tinline_block:{
        int i;
        nesting_level++;
        for(i=0; i<t->n; i++){
            if(ast_compile(bv,t->a[i])){nesting_level--; return 1;}
        }
        nesting_level--;
    } break;
    case Tkeyword:
        value=t->value;
        if(value==Twhile){
            if(ast_compile_while(bv,t)) return 1;
        }else if(value==Tfor){
            if(ast_compile_for(bv,t)) return 1;
        }else if(value==Tif){
            if(ast_compile_if(bv,t)) return 1;
        }else if(value==Tbreak){
            push_bc(bv,JMP,t);
            uint32_t index = bv->size;
            mf_vec_push(out_stack+jmp_sp-1,&index);
            push_u32(bv,0xcafe);
        }else if(value==Tcontinue){
            push_bc(bv,JMP,t);
            push_u32(bv,BC+jmp_stack[jmp_sp-1]-bv->size);
        }else if(value==Treturn || value==Tyield){
            if(t->n==1){
                if(ast_compile(bv,t->a[0])) return 1;
            }else{
                push_bc(bv,CONST_NULL,t);
            }
            push_bc(bv,value==Treturn? RET: YIELD,t);
        }else if(value==Traise){
            if(ast_compile(bv,t->a[0])) return 1;
            push_bc(bv,RAISE,t);
        }else if(value==Tfn){
            if(ast_compile_function(bv,t)) return 1;
        }else if(value==Tglobal){
            ast_global(t);
        }else if(value==Ttable){
            if(t->info==0){ // todo
                push_bc(bv,CONST_NULL,t);
            }
            int i;
            for(i=0; i<t->n; i++){
                if(ast_compile(bv,t->a[i])) return 1;
            }
            push_bc(bv,MAP,t);
            push_u32(bv,t->n-t->info);
            push_bc(bv,TABLE,t);
        }else if(value==Ttry){
            if(ast_compile_try(bv,t)) return 1;
        }else if(value==Tlabel){
            ast_node* id = t->a[0];
            label_tab_push(label_tab,id->size,id->s,bv->size);
        }else if(value==Tgoto){
            ast_node* id = t->a[0];
            push_bc(bv,JMP,t);
            label_tab_push(goto_tab,id->size,id->s,bv->size);
            push_u32(bv,0xcafe);
        }else{
            printf("Compiler in ast_compile: unknown keyword value: %i.\n",t->value);
            abort();
        }
        break;
    default:
        printf("Error in ast_compile: unknown type value: %i.\n",t->type);
        abort();
    }
    return 0;
}

static
int compile(mt_vec* bv, ast_node* t){
    mf_vec_init(&bv_deposit,sizeof(unsigned char));
    mt_vec indices;
    mf_vec_init(&indices,sizeof(int));
    const_fn_indices = &indices;
    label_tab = label_tab_new();
    goto_tab = label_tab_new();

    int e = ast_compile(bv,t);
    *(uint32_t*)(data.a) = data.size;
    *(uint32_t*)(data.a+4) = stab.size;
    push_bc(bv,HALT,t);
    offsets(bv,bv->size,const_fn_indices);
    gotos(bv);
    label_tab_delete(label_tab);
    label_tab_delete(goto_tab);

    bv_append(bv,&bv_deposit);

    mf_vec_delete(&bv_deposit);
    mf_vec_delete(&indices);
    const_fn_indices=NULL;
    return e;
}

void dump_data(unsigned char* data){
    printf("DATA=[");
    int n = *(uint32_t*)(data+4);
    int i = 8;
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
                c = *(unsigned char*)(data+i);
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
    int i = 0;
    int32_t a;
    while(i<n){
        c = bv[i];
        if(i<10) printf("0");
        printf("%i ",i);
        prefix_line_col(bv+i);
        switch(c){
        case CONST_INT:
            printf("CONST_INT %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LONG:
            i+=BC; printf("LONG\n");
            break;
        case CONST_STR:
            printf("CONST_STR (%i)\n",*(int32_t*)(bv+i+BC));
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
            printf("CONST_BOOL %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case CONST_FN:
            printf("CONST_FN %i, argc_min=%i, argc_max=%i, varcount=%i\n",
                i+*(int32_t*)(bv+i+BC),*(int32_t*)(bv+i+BC+4),
                *(int32_t*)(bv+i+BC+8),*(int32_t*)(bv+i+BC+12));
            i+=BC+16;
            break;
        case ADD:
            i+=BC; printf("ADD\n");
            break;
        case SUB:
            i+=BC; printf("SUB\n");
            break;
        case MUL:
            i+=BC; printf("MUL\n");
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
            printf("LIST %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case MAP:
            printf("MAP %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case TUPLE:
            printf("TUPLE %i\n",*(int32_t*)(bv+i+BC));
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
            printf("STN %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case STORE_INDEX:
            printf("STORE_INDEX %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD:
            printf("LOAD %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD_REF:
            printf("LOAD_REF %i\n",*(int32_t*)(bv+i+BC));
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
            printf("JMP %i\n",i+*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case JPZ:
            printf("JPZ %i\n",i+*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case AND:
            printf("AND %i\n",i+*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case OR:
            printf("OR %i\n",i+*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LIST_POP:
            printf("LIST_POP %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case POP:
            i+=BC; printf("POP\n");
            break;
        case RET:
            i+=BC; printf("RET\n");
            break;
        case CALL:
            printf("CALL, argc = %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD_LOCAL:
            printf("LOAD_LOCAL %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD_REF_LOCAL:
            printf("LOAD_REF_LOCAL %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD_ARG:
            printf("LOAD_ARG %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD_CONTEXT:
            printf("LOAD_CONTEXT %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD_REF_CONTEXT:
            printf("LOAD_REF_CONTEXT %i\n",*(int32_t*)(bv+i+BC));
            i+=BC+4;
            break;
        case LOAD_ARG_REF:
            printf("LOAD_ARG_REF %i\n",*(int32_t*)(bv+i+BC));
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
            printf("TRY, catch address: %i\n",*(int32_t*)(bv+i+BC));
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
    module->program = NULL;
    module->data = NULL;
    mt_vec bv;
    mf_vec_init(&bv,sizeof(unsigned char));
    init_data(&data);
    mf_vec_init(&stab,sizeof(mt_stab_element));
    #ifdef MV_PRINT
    print_vtoken(v);
    #endif

    line_counter = 0;
    token_iterator i;
    i.size = v->size;
    i.index = 0;
    i.a = (mt_token*) mf_vec_move(v);
    i.i=NULL;

    ast_node* t = ast(v,&i);

    if(t==NULL) goto error;
    #ifdef MV_PRINT
    ast_print(t,2);
    #endif

    if(compile(&bv,t)){
        ast_delete(t);
        goto error;
    }
    #ifdef MV_PRINT
    dump_program(bv.a,bv.size);
    dump_data(data.a);
    #endif
    module->program = bv.a;
    module->program_size = bv.size;
    module->data = data.a;
    module->data_size = data.size;

    data.a = NULL;
    data.capacity = 0;
    data.size = 0;
    token_iterator_delete(&i);
    stab_delete(&stab);
    ast_delete(t);
    return 0;

    error:
    token_iterator_delete(&i);
    mf_vec_delete(&data);
    stab_delete(&stab);
    return 1;
}
