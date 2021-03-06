#ifndef SYSTEM_H
#define SYSTEM_H
#include "../moss.h"

void mf_set_io_utf8();
char* mf_getline(const char* prompt);
char* mf_getline_hist(const char* prompt);
void mf_getline_clear(void);
void mf_print_string(long size, uint32_t* a);
void mf_print_string_lit(long size, uint32_t* a);
char* mf_realpath(const char* id);

int mf_isdir(const char* id);
int mf_isfile(const char* id);

#endif

