#include <assert.h>

#include "beefit.h"

// optimization passes
// return nonzero when they change anything
int trivial_dce(ins_t *code);
int fold(ins_t *code);
int condense(ins_t *code);
int unloop(ins_t *code);
int dce(ins_t *code);
int peep(ins_t *code);
void peepfinal(ins_t *code);

int optimize(ins_t *code) {
  int changed;

  // keep optimizing until there's nothing left
  do {
    changed = 0;
    changed |= fold(code);
    changed |= condense(code);
    changed |= trivial_dce(code);
    changed |= unloop(code);
    changed |= dce(code);
    changed |= peep(code);
  } while (changed);
  peepfinal(code);

  int opt_size;
  for (opt_size = 0; code[opt_size].op != OP_EOF; ++opt_size) {}
  return opt_size;
}

int trivial_dce(ins_t *code) {
  // remove trivially dead code (comments)
  int changed = 0;
  for (; code->op != OP_EOF; ++code) {
    if (code->op == OP_SKIPZ) {
      // [ preceded by the beginning of the file or a ]
      if (code[-1].op == OP_EOF || code[-1].op == OP_LOOPNZ) {
        changed = 1;
        int depth = 0;
        do {
          if (code->op == OP_SKIPZ)
            depth++;
          else if (code->op == OP_LOOPNZ)
            depth--;
          code->op = OP_NOP;
          ++code;
        } while (depth);
      }
    }
  }
  return changed;
}

int fold(ins_t *code) {
  // combine runs of instructions together
  // e.g. >>>> => ptr += 4
  //    ++++ => *ptr += 4
  int changed = 0;
  while(code->op != OP_EOF) {
    ins_t *begin = code; // where the run started
    if (begin->op == OP_SHIFT) {
      for (++code; begin->op == code->op; ++code) {
        begin->b += code->b;
        code->op = OP_NOP;
        changed = 1;
      }
    } else if (begin->op == OP_ADD || begin->op == OP_SET) {
      for (++code; code->op == OP_ADD && begin->b == code->b; ++code) {
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
      case OP_TADD:
      case OP_ADD:
      case OP_LOAD:
      case OP_ADDT:
      case OP_SET:
      case OP_SETT:
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
        break;
      case OP_NOP:
        // remove NOPs from instruction stream
        break;
      case OP_EOF:
      default:
        assert("unreachable");
        break;
    }
  }
  *dst = *src;

  return dst != src;
}

int unloop(ins_t *code) {
  // "unloop" inner loops with only adds
  //  and no net shifts (no shifts after condense)
  // [-] => ptr[0] = 0
  // [->+<] => ptr[1] += ptr[0]; ptr[0] = 0
  //
  // note that this can cause out-of-bound
  // reads/writes, which are handled by padding
  // the memory buffer.

  int loop_good = 0;

  ins_t *loop_start = code;

  for (; code->op != OP_EOF; ++code) {
    if (code->op == OP_SKIPZ) {
      loop_good = 1;
      loop_start = code;
    } else if (code->op == OP_LOOPNZ) {
      if (loop_good) {
        // check that ptr[0] is decremented once
        loop_good = 0;
        for (ins_t *ins = loop_start; ins < code; ++ins) {
          if (ins->op == OP_ADD && ins->a == -1 && ins->b == 0) {
            loop_good = 1;
            *ins = (ins_t){OP_SET, 0, 0};
          }
        }
        if (!loop_good) {
          continue;
        }

        *loop_start = (ins_t){OP_LOAD, 0, 0};
        *code = (ins_t){OP_NOP, 0, 0};

        for (ins_t *ins = loop_start + 1; ins < code; ++ins) {
          if (ins->op == OP_ADD) {
            *ins = (ins_t){OP_ADDT, ins->a, ins->b};
          }
        }
      }
    } else if (code->op != OP_ADD) {
      loop_good = 0;
    }
  }
  return 0;
}

ins_t* find_ref(ins_t *code, int dir) {
  int off = code->b;
  for (code += dir; code->op != OP_EOF; code += dir) {
    ins_op_t op = code->op;
    if (op == OP_NOP) {
      ;
    } else if (code->b == off) {
      return code;
    } else if (op == OP_SKIPZ || op == OP_LOOPNZ || op == OP_SHIFT) {
      return 0;
    }
  }
  return 0;
}

enum {
  READ_MEM =  1 << 0,
  WRITE_MEM = 1 << 1,
  READ_TMP = 1 << 2,
  WRITE_TMP = 1 << 3,
  ASSERT_MEM_ZERO = 1 << 4,
  DOES_IO = 1 << 5,
};

int op_effect[] = {
  [OP_NOP] =   0,
  [OP_SHIFT] = 0,
  [OP_ADD] =    READ_MEM | WRITE_MEM,
  [OP_SET] =               WRITE_MEM,
  [OP_SETT] =              WRITE_MEM | READ_TMP,
  [OP_ADDT] =   READ_MEM | WRITE_MEM | READ_TMP,
  [OP_LOAD] =   READ_MEM                        | WRITE_TMP,
  [OP_TADD] =   READ_MEM             | READ_TMP | WRITE_TMP,
  [OP_SKIPZ] =  READ_MEM,
  [OP_LOOPNZ] = READ_MEM | ASSERT_MEM_ZERO,
  [OP_PRINT] =  READ_MEM                        | WRITE_TMP | DOES_IO,
  [OP_READ] =              WRITE_MEM            | WRITE_TMP | DOES_IO,
  [OP_EOF] =   0,
};

ins_t* find_tref(ins_t *code, int dir, int type) {
  for (code += dir; code->op != OP_EOF; code += dir) {
    ins_op_t op = code->op;
    if (op_effect[op] & type) {
      return code;
    } else if (op == OP_SKIPZ || op == OP_LOOPNZ) {
      return 0;
    }
  }
  return 0;
}

#define CHANGE(ins, to_op) { (ins)->op = to_op; changed = 1; }
#define CHANGEIF(ins, is_op, to_op) \
  if ((ins)->op == (is_op)) CHANGE((ins), (to_op))

int dce(ins_t *code) {
  int changed = 0;

  for (; code->op != OP_EOF; ++code) {
    ins_t *next = find_ref(code, 1);
    int eff_code = op_effect[code->op];
    int eff_next = next ? op_effect[next->op] : 0;

    if (code->op == OP_SET && code->a == 0) {
      eff_code |= ASSERT_MEM_ZERO;
    }

    if (eff_code & WRITE_MEM && !(eff_code & DOES_IO)) {
      if (eff_next && !(eff_next & READ_MEM)) {
        // eliminate useless writes to cells
        CHANGE(code, OP_NOP);
        continue;
      }
    }
    if (eff_code & WRITE_TMP && !(eff_code & DOES_IO)) {
      ins_t *next_tref = find_tref(code, 1, WRITE_TMP | READ_TMP);
      if (!next_tref || !(op_effect[next_tref->op] & READ_TMP)) {
        // eliminate useless writes to tmp
        CHANGE(code, OP_NOP);
      }
    } else if (eff_code & ASSERT_MEM_ZERO && next) {
      //   ] *0 += B / tmp * 1
      //=> ] *0 = B / tmp * 1
      CHANGEIF(next, OP_ADD, OP_SET);
      if (next->a == 1) CHANGEIF(next, OP_ADDT, OP_SETT);
      //   ] tmp += *0
      //=> ]
      CHANGEIF(next, OP_TADD, OP_NOP);
      //   ] *0 = 0
      //=> ]
      if (next->a == 0) CHANGEIF(next, OP_SET, OP_NOP);
    }
  }
  return changed;
}

int peep(ins_t *code) {
  int changed = 0;
  for (;code->op != OP_EOF; ++code) {
    if (code->op == OP_LOAD) {
      ins_t *prev = find_ref(code, -1);
      ins_t *next = find_ref(code, 1);
      if (prev && ((prev->op == OP_ADDT && (prev->a == 1 || prev->a == -1)) || prev->op == OP_SETT)
          && code->a == 0 && next && next->op == OP_SET) {
        // *A += tmp  /  *A -= tmp / *A = tmp
        // tmp = *A
        // *A = B
        // ->
        // tmp += *A  / tmp -= *A  / nop
        // *A = B

        // make sure there are no further writes to tmp
        // between *A += tmp and tmp = *A
        ins_t *prev_write = find_tref(code, -1, WRITE_TMP);
        if (prev_write && prev_write > prev) {
          continue;
        }

        if (prev->op == OP_SETT) {
          code->op = OP_NOP;
        } else {
          code->op = OP_TADD;
          code->a = prev->a == 1 ? 0 : 0x80;
        }
        CHANGE(prev, OP_NOP);
      } else if (prev && prev->op == OP_ADD &&
                 next && (next->op == OP_SET || next->op == OP_SETT)) {
        // *A += C
        // tmp = *A + D
        // *A = B   /  *A = tmp
        // ->
        // tmp = *A + C + D
        // *A = B   /  *A = tmp
        code->a += prev->a;
        CHANGE(prev, OP_NOP);
      }
    } else if (code->op == OP_SET) {
      ins_t *next = find_ref(code, 1);
      if (next && next->op == OP_ADD) {
        //   *A = B; *A += C
        //=> *A = B + C
        code->a += next->a;
        CHANGE(next, OP_NOP);
      }
    } else if (code->op == OP_TADD) {
      ins_t *prev = find_ref(code, -1);
      ins_t *next = find_ref(code, 1);
      if ((code->a & 0x7f) == 0
          && prev && prev->op == OP_ADD
          && next && next->op == OP_SET
          && prev->a <= 63 && prev->a >= -64)  {
        // *A += X
        // tmp = *A + tmp * (a>>8)
        // *A = Y
        // ->
        // tmp = *A + tmp + (a>>8) + X
        // *A = Y

        // 1bit: negate tmp, 7bit: offset
        code->a = (code->a & 0x80) | (prev->a & 0x7f);
        CHANGE(prev, OP_NOP);
      }
    }
  }
  return changed;
}

void peepfinal(ins_t *code) {
  while (code->op != OP_EOF) {
    if (code->op == OP_SHIFT) {
      //   *A += X; shift A
      //=> shift A; *0 += X
      int off = code->b;
      ins_t* prev = find_ref(code, -1);
      if (prev) {
        for (ins_t *dst = code; dst != prev; --dst) {
          *dst = *(dst - 1);
          dst->b -= off;
        }
        *prev = (ins_t){OP_SHIFT, 0, off};
      }
    } if (code->op == OP_SKIPZ) {
      ins_t *prev = find_ref(code, -1);
      if (prev && prev->op == OP_SKIPZ) {
        //   [ [
        //=> [ {
        code->a = 1;
      }
      if (prev && prev->op == OP_SET && prev->a) {
        //   *0 = X (nonzero) [
        //=> *0 = X {
        code->a = 1;
      }
    } else if (code->op == OP_LOOPNZ) {
      ins_t *prev = find_ref(code, -1);
      if (prev && prev->op == OP_LOOPNZ) {
        //   ] ]
        //=> ] }
        code->a = 1;
      } else if (prev && prev->op == OP_SET && prev->a == 0) {
        //   *0 = 0 ]
        //=> *0 = 0 }
        code->a = 1;
      } else if (code[-1].op == OP_SHIFT && code[-2].op == OP_SKIPZ &&
                 code[-2].a == 0 && code[-3].op == OP_SHIFT && code[-3].b == code[-1].b) {
        //   shift x [ shift x ]
        //=>         { shift x ]
        code[-3].op = OP_NOP;
        code[-2].a = 1;
      }
    }
    ++code;
  }
}
