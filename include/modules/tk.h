#ifndef TK_H
#define TK_H
#include "str.h"

void mf_file_read(FILE* f, mt_bstr* data);
void mf_file_load(const char* id, mt_bstr* data);

extern char mv_file_save_error[200];
int mf_file_save(const char* id, mt_bstr* data);

#endif

