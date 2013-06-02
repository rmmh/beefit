#pragma once

#include <stdint.h>

enum {
  OP_NOP,
  OP_SHIFT,
  OP_ADD,
  OP_SKIPZ,
  OP_LOOPNZ,
  OP_PRINT,
  OP_READ,
  OP_EOF
};

typedef struct {
  uint8_t op;
  uint8_t a;
  int16_t b;
} ins_t;

typedef void (*bf_ptr)(uint8_t*);

bf_ptr assemble(ins_t *code, int *size_out);
