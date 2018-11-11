#ifndef COMPILER_H
#define COMPILER_H

typedef struct{
  char mode_cmd;
  const char* file_name;
} mt_compiler_context;

typedef struct{
  int line;
  int col;
  char type;
  char value;
  char* s;
  long size;
} mt_token;

void mf_compiler_context_save(mt_compiler_context* context);
void mf_compiler_context_restore(mt_compiler_context* context);
int mf_scan(mt_vec* v, unsigned char* s, long n, int line);
int mf_compile(mt_vec* v, mt_module* module);
void mf_push_newline(mt_vec* v, int line, int col);

#endif

