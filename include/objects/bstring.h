#ifndef BSTRING_H
#define BSTRING_H

mt_bstring* mf_buffer_to_bstring(long size, const unsigned char* a);
void mf_print_bstring(long size, const unsigned char* a);
void mf_init_type_bstring(mt_table* type);

#endif

