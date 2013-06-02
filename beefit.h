#pragma once

#include <stdint.h>

enum {
  OP_NOP,
  OP_SHIFT,   // ptr += b
  OP_ADD,     // ptr[b] += a
  OP_SKIPZ,   // while (ptr[0]) {
  OP_LOOPNZ,  // }
  OP_PRINT,   // putchar(ptr[b])
  OP_READ,    // ptr[b] = getchar()
  OP_EOF      // end of instructions
};

typedef struct {
  uint8_t op;
  uint8_t a;
  int16_t b;
} ins_t;

typedef void (*bf_ptr)(uint8_t*);

int optimize(ins_t *code);
void print_code(ins_t *code, int count);

bf_ptr assemble(ins_t *code, int *size_out);
