
// operating system dependent
// program code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <moss.h>
#include <modules/vec.h>
#include <modules/str.h>
#include <sys/stat.h>
#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
#endif

#if defined(__linux__) && !defined(MOSS_LEVEL1)
  #define READLINE
#endif

// install
// libreadline-dev
#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

int mf_put(mt_object* x);
mt_string* mf_str_decode_utf8(long size, const unsigned char* a);

#ifdef _WIN32
const char* mv_path = "C:/local/lib/mossc";
#else
const char* mv_path = "/usr/local/lib/mossc";
#endif

static
unsigned int mv_cp850[] = {
// 0x
  0, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
  0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
// 1x
  0x25ba, 0x25c4, 0x2195, 0x203c, 0xb6, 0xa7, 0x25ac, 0x21a8,
  0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
// 2x
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
// 3x
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
// 4x
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
// 5x
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
// 6x
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
// 7x
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x2302,
// 8x
  0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,
  0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,
// 9x
  0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,
  0xff, 0xd6, 0xdc, 0xf8, 0xa3, 0xd8, 0xd7, 0x192,
// ax
  0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,
  0xbf, 0xae, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,
// bx
  0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0xc1, 0xc2, 0xc0,
  0xa9, 0x2563, 0x2551, 0x2557, 0x255D, 0xa2, 0xa5, 0x2510,
// cx
  0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0xE3, 0xC3,
  0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0xA4,
// dx
  0xF0, 0xD0, 0xCA, 0xCB, 0xC8, 0x131, 0xCD, 0xCE,
  0xCF, 0x2518, 0x250C, 0x2588, 0x2584, 0xA6, 0xCC, 0x2580,
// ex
  0xd3, 0xdf, 0xd4, 0xd2, 0xf5, 0xd5, 0xb5, 0xfe,
  0xde, 0xda, 0xdb, 0xd9, 0xfd, 0xdd, 0xaf, 0xb4,
// fx
  0xad, 0xb1, 0x2017, 0xbe, 0xb6, 0xa7, 0xf7, 0xb8,
  0xb0, 0xa8, 0xb7, 0xb9, 0xb3, 0xb2, 0x25a0, 0xa0
};

static
char* get_utf8(unsigned char* s, int size){
  uint32_t* a = mf_malloc(size*sizeof(uint32_t));
  int i;
  for(i=0; i<size; i++){
    // printf("[%i]",s[i]);
    a[i]=mv_cp850[s[i]];
  }
  mt_bstr bs;
  mf_encode_utf8(&bs,size,a);
  mf_free(a);
  return (char*)bs.a;
}

static
int utf32_to_cp850(uint32_t x){
  if(x<127){
    return x;
  }else{
    int i;
    for(i=0; i<255; i++){
      if(mv_cp850[i]==x){
        return i;
      }
    }
    return '.';
  }
}

static
void mf_encode_cp850(mt_bstr* bs, uint32_t* a, long size){
  bs->a=mf_malloc(size+1);
  bs->size=size+1;
  long i;
  for(i=0; i<size; i++){
    bs->a[i]=utf32_to_cp850(a[i]);
  }
  bs->a[size]=0;
}

void mf_set_io_utf8(){
#ifdef _WIN32
  // SetConsoleCP(CP_UTF8);
  // ^seems not to work
  // SetConsoleOutputCP(CP_UTF8);
#endif
}

#ifdef _WIN32
void mf_print_string_cp850(long size, uint32_t* a){
  long i;
  for(i=0; i<size; i++){
    printf("%c",utf32_to_cp850(a[i]));
  }
}

// See "http://bugs.python.org/issue1602#msg131657".
void mf_print_stringW(long size, uint32_t* a){
  HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
  if(console == INVALID_HANDLE_VALUE){
    mf_print_string_cp850(size,a);
    return;
  }
  uint16_t* b = mf_malloc(size*sizeof(uint16_t));
  long i;
  for(i=0; i<size; i++){
    b[i]=a[i];
  }
  BOOL ok;
  DWORD written=0;
  long chunk;
  uint16_t* text=b;

  while(size!=0){
    if(size>10000){
      chunk = 10000;
    }else{
      chunk = size;
    }
    ok = WriteConsoleW(console, text, chunk, &written, NULL);
    if(!ok) break;
    text += chunk;
    size -= chunk;
  }
  mf_free(b);
}

void mf_print_string(long size, uint32_t* a){
  // mf_print_string_cp850(size,a);
  mf_print_stringW(size,a);
}
#else
void mf_print_string(long size, uint32_t* a){
  mt_bstr s;
  mf_encode_utf8(&s,size,a);
  printf("%s",s.a);
  mf_free(s.a);
}
#endif

static
void push_char(mt_vec* v, unsigned char c){
  mf_vec_push(v,&c);
}

void mf_print_string_lit(long size, uint32_t* s){
  mt_bstr bs;
#ifdef _WIN32
  mf_encode_cp850(&bs,s,size);
#else
  mf_encode_utf8(&bs,size,s);
#endif
  long n=bs.size;
  unsigned char* a=bs.a;
  mt_vec buffer;
  mf_vec_init(&buffer,sizeof(unsigned char));
  int i;
  for(i=0; i<n; i++){
    if(a[i]=='\n'){
      push_char(&buffer,'\\');
      push_char(&buffer,'n');
    }else if(a[i]=='\t'){
      push_char(&buffer,'\\');
      push_char(&buffer,'t');
    }else if(a[i]=='\\'){
      push_char(&buffer,'\\');
      push_char(&buffer,'b');
    }else if(a[i]=='\"'){
      push_char(&buffer,'\\');
      push_char(&buffer,'d');
    }else{
      push_char(&buffer,a[i]);
    }
  }
  push_char(&buffer,0);
  printf("%s",buffer.a);
  mf_vec_delete(&buffer);
  mf_free(bs.a);
}

char* mf_getline(const char* prompt){
  printf("%s",prompt);
  char* a = mf_malloc(10000);
  char* p=fgets(a,10000,stdin);
  if(p==NULL){
    mf_free(a);
    return NULL;
  }
  int size = strlen(a);
  char* s = mf_malloc(size);
  memcpy(s,a,size-1);
  s[size-1]=0;
  mf_free(a);
#ifdef _WIN32
  a=get_utf8(s,size-1);
  mf_free(s);
  return a;
#endif
  return s;
}

#ifdef READLINE
char* last_input;
char* mf_getline_hist(const char* prompt){
  char* p = readline(prompt);
  if(p==NULL) return NULL;
  int size=strlen(p);
  if(size>0 && (last_input==0 || strcmp(p,last_input)!=0)){
    if(last_input!=0) mf_free(last_input);
    last_input=mf_malloc(size+1);
    strncpy(last_input,p,size+1);
    add_history(p);
    stifle_history(20);
  }
  return p;
}
#else
char* mf_getline_hist(const char* prompt){
  return mf_getline(prompt);
}
#endif

void mf_getline_clear(void){
#ifdef READLINE
  rl_clear_history();
/*
  long i;
  HIST_ENTRY **list = history_list();
  for(i=0; list[i]; i++){
    free(list[i]->line);
    free(list[i]->timestamp);
    free(list[i]->data);
    free(list[i]);
  }
  free(list);
*/
#endif
}

int mf_finput(mt_object* x, int argc, mt_object* v){
  char* p;
  if(argc==0){
    p = mf_getline_hist("");
  }else if(argc==1){
    if(v[1].type!=mv_string){
      mf_type_error1("in input(prompt): prompt (type: %s) is not a string.",&v[1]);
      return 1;
    }
    mt_string* s=(mt_string*)v[1].value.p;
    mt_bstr bs;
    mf_encode_utf8(&bs,s->size,s->a);
    p = mf_getline_hist((char*)bs.a);
    mf_free(bs.a);
  }else{
    mf_argc_error(argc,0,1,"input");
    return 1;  
  }
  if(p==NULL){
    mf_std_exception("Error in input(): input error.");
    return 1;
  }else{
    mt_string* s = mf_str_decode_utf8(strlen(p),(unsigned char*)p);
    x->type=mv_string;
    x->value.p=(mt_basic*)s;
    return 0;
  }
}

#ifdef _WIN32
char* mf_realpath(const char* id){
  return _fullpath(NULL,id,400);
}
#else
char* mf_realpath(const char* id){
  return realpath(id,NULL);
}
#endif

#ifdef _WIN32
int mf_isdir(const char* id){
  if(_access(id,0)==0){
    struct stat m;
    stat(id,&m);
    return (m.st_mode & S_IFDIR)!=0;
  }
  return 0;
}

int mf_isfile(const char* id){
  if(_access(id,0)==0){
    struct stat m;
    stat(id,&m);
    return (m.st_mode & S_IFREG)!=0;
  }
  return 0;
}

#else
int mf_isdir(const char* id){
  struct stat m;
  if(stat(id,&m)) return 0;
  return S_ISDIR(m.st_mode);
}

int mf_isfile(const char* id){
  struct stat m;
  if(stat(id,&m)) return 0;
  return S_ISREG(m.st_mode);
}
#endif


