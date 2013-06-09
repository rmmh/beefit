#pragma once

// from http://stackoverflow.com/a/6766023/3694
#define STATIC_ASSERT( condition, name )\
    typedef char assert_failed_ ## name [ (condition) ? 1 : -1 ];

#include <stdint.h>

typedef enum {
  // optimization passes to edit with new opcodes:
  // -- dce: if the new op uses tmp

  OP_NOP,
  OP_SHIFT,   // ptr += b
  OP_ADD,     // ptr[b] += a
  OP_SET,     // ptr[b] = a
  OP_SETT,    // ptr[b] = tmp
  OP_LOAD,    // tmp = ptr[b] + a
  OP_TADD,    // tmp = tmp*a + ptr[b]
  OP_ADDT,    // ptr[b] += tmp
  OP_SKIPZ,   // while (ptr[0]) {
  OP_LOOPNZ,  // }
  OP_PRINT,   // putchar(ptr[b])
  OP_READ,    // ptr[b] = getchar()
  OP_EOF      // end of instructions
} ins_op_t;

typedef struct {
  ins_op_t op;
  uint8_t a;
  int16_t b;
} ins_t;

STATIC_ASSERT(sizeof(ins_t) == sizeof(uint32_t), packed_opcodes);

typedef void (*bf_ptr)(uint8_t*);

int optimize(ins_t *code);
void print_code(ins_t *code, int count);

bf_ptr assemble(ins_t *code, int *size_out);
