#include <assert.h>

#include "beefit.h"

// optimization passes
// return nonzero when they change anything
int fold(ins_t *code);
int condense(ins_t *code);

int optimize(ins_t *code) {
  int changed;

  // keep optimizing until there's nothing left
  do {
    changed = 0;
    changed |= fold(code);
    changed |= condense(code);
  } while (changed);

  int opt_size;
  for (opt_size = 0; code[opt_size].op != OP_EOF; ++opt_size) {}
  return opt_size;
}

#define SAME(X,Y,field) ((X)->field == (Y)->field)

int fold(ins_t *code) {
  // combine runs of instructions together
  // e.g. >>>> => ptr += 4
  //    ++++ => *ptr += 4
  int changed = 0;
  while(code->op != OP_EOF) {
    ins_t *begin = code; // where the run started
    if (begin->op == OP_SHIFT) {
      for (++code; SAME(begin, code, op); ++code) {
        begin->b += code->b;
        code->op = OP_NOP;
        changed = 1;
      }
    } else if (begin->op == OP_ADD) {
      for (++code; SAME(begin, code, op) && SAME(begin, code, b); ++code) {
        begin->a += code->a;
        code->op = OP_NOP;
        changed = 1;
      }
    } else {
      ++code;
    }
  }
  return changed;
}

int condense(ins_t *src) {
  // remove NOPs from the instruction stream,
  // and move SHIFTs to the end of instructions
  // e.g. +>+<< => ptr[0]++; ptr[1]++; ptr--;

  ins_t *dst;
  int shift_offset = 0;
  for (dst = src; src->op != OP_EOF; ++src) {
    assert(dst <= src);
    switch (src->op) {
      case OP_SHIFT:
        shift_offset += src->b;
        break;
      case OP_ADD:
      case OP_PRINT:
      case OP_READ:
        src->b += shift_offset;
        *dst++ = *src;
        break;
      case OP_SKIPZ:
      case OP_LOOPNZ:
        if (shift_offset) {
          *dst++ = (ins_t){OP_SHIFT, 0, shift_offset};
          shift_offset = 0;
        }
        *dst++ = *src;
    }
  }
  *dst = *src;

  return dst != src;
}
