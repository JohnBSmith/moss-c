#ifndef VM_H
#define VM_H
#include <stdint.h>
#include <modules/str.h>
#include <objects/object.h>

enum{
  ADD, SUB, MPY, DIV, MOD, NEG, NOT,
  EQ, NE, LT, GT, LE, GE, POW, IDIV,
  CONST_INT, CONST_FLOAT, CONST_NULL,
  CONST_BOOL, CONST_STR, CONST_BSTR, CONST_IMAG,
  HALT, DATA,
  LOAD, LOAD_REF, STORE, STN,
  PRINT, PUT, JMP, JPZ, LIST, MAP, CALL,
  GET, GETREF, RSTORE, STORE_LOCAL, POP,
  RET, CONST_FN, LOAD_LOCAL, LOAD_REF_LOCAL,
  OBGET, OBGETREF, DUP, SWAP,
  LOAD_ARG, LOAD_ARG_REF, LOAD_CONTEXT,
  TUPLE, LOAD_REF_CONTEXT, RANGE, IS, IN, NOTIN,
  FNSELF, TRY, RAISE, TRYEND, YIELD, LONG,
  ITER_NEXT, ITER_VALUE, LOAD_POINTER,
  BIT_AND, BIT_OR, BIT_XOR, TILDE,
  AND, OR, LIST_POP, TABLE,
  EXCEPTION, ISIN
};

#define MT_BSTRING 0
#define MT_STRING 1
#define BC 4
#define BCp4 8
#define BCp8 12
#define BCp12 16

int mf_vm_eval(unsigned char* data, long ip);
int mf_vm_eval_global(mt_module* module, long ip);
void mf_vm_init_gvtab();
void mf_vm_init();
void mf_vm_set_program(mt_module* module);

#endif

