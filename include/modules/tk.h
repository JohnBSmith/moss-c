#ifndef TK_H
#define TK_H
#include "str.h"

void mf_file_read(FILE* f, mt_bstr* data);
void mf_file_load(const char* id, mt_bstr* data);

// deprecated
void file_read(FILE* f, bstring* data);
void file_load(const char* id, bstring* data);
void file_write(FILE* f, bstring* data);
void file_save(const char* id, bstring* data);

#endif

