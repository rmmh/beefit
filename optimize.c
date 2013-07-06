#include <assert.h>

#include "beefit.h"

// optimization passes
// return nonzero when they change anything
int fold(ins_t *code);
int condense(ins_t *code);
int unloop(ins_t *code);
int dce(ins_t *code);
int peep(ins_t *code);
int peepfinal(ins_t *code);


int optimize(ins_t *code) {
  int changed;

  // keep optimizing until there's nothing left
  do {
    changed = 0;
    changed |= fold(code);
    changed |= condense(code);
    changed |= unloop(code);
    changed |= dce(code);
    changed |= peep(code);
  } while (changed);

  peepfinal(code);

  int opt_size;
  for (opt_size = 0; code[opt_size].op != OP_EOF; ++opt_size) {}
  return opt_size;
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

int dce(ins_t *code) {
  // remove unnecessary `tmp = ptr[b]` code
  int changed = 0;
  ins_t *maybe_useless = 0;

  for (; code->op != OP_EOF; ++code) {
    if (maybe_useless) {

      if (code->op == OP_ADDT || code->op == OP_SETT || code->op == OP_TADD) {
        maybe_useless = 0;
      } else if (code->op == OP_LOAD) {
        maybe_useless->op = OP_NOP;
        maybe_useless = 0;
        changed = 1;
      }
    }
    if (code->op == OP_LOAD) {
      maybe_useless = code;
    }
  }

  return changed;
}

ins_t* find_ref(ins_t *code, int dir) {
  int off = code->b;
  for (code += dir; code->op != OP_EOF; code += dir) {
    if (code->op == OP_NOP) {
      ;
    } else if (code->b == off) {
      return code;
    } else if (code->op == OP_SKIPZ || code->op == OP_LOOPNZ || code->op == OP_SHIFT) {
      return 0;
    }
  }
  return 0;
}

int peep(ins_t *code) {
  int changed = 0;
  while (code->op != OP_EOF) {
    if (code->op == OP_ADD) {
      ins_t *prev = find_ref(code, -1);
      if (prev && prev->op == OP_LOOPNZ) {
        // ]
        // *0 += A
        // ->
        // ]
        // *0 = A
        code->op = OP_SET;
      }
    } else if (code->op == OP_SETT) {
      if (code[-1].op == OP_LOAD && code[-1].b == code->b && code[-1].a == 0) {
        // tmp = *A
        // *A = tmp
        // ->
        // tmp = *A
        code->op = OP_NOP;
      }
    } else if (code->op == OP_LOAD) {
      ins_t *prev = find_ref(code, -1);
      ins_t *next = find_ref(code, 1);
      if (prev && prev->op == OP_ADDT && (prev->a == 1 || prev->a == -1)
          && next && next->op == OP_SET) {
        // *A += tmp  /  *A -= tmp
        // tmp = *A
        // *A = B
        // ->
        // tmp += *A
        // *A = B
        prev->op = OP_NOP;
        code->op = OP_TADD;
        code->a = prev->a == 1 ? 0 : 0x80;
        changed = 1;
      } else if (prev && prev->op == OP_ADD &&
                 next && (next->op == OP_SET || next->op == OP_SETT)) {
        // *A += C
        // tmp = *A + D
        // *A = B   /  *A = tmp
        // ->
        // tmp = *A + C + D
        // *A = B   /  *A = tmp
        prev->op = OP_NOP;
        code->a += prev->a;
        changed = 1;
      }
    } else if (code->op == OP_SET) {
      ins_t *next = find_ref(code, 1);
      ins_t *prev = find_ref(code, -1);
      if (next && next->op == OP_ADD) {
        // *A = B
        // *A += C
        // ->
        // *A = B + C
        changed = 1;
        code->a += next->a;
        next->op = OP_NOP;
      } else if (next && next->op == OP_ADDT &&
                 code->a == 0 && next->a == 1) {
        // *A = 0
        // *A += tmp
        // ->
        // *A = tmp
        changed = 1;
        code->op = OP_NOP;
        next->op = OP_SETT;
      } else if (code->a == 0 && prev && prev->op == OP_LOOPNZ) {
        // ]
        // *0 = 0
        // ->
        // ]
        code->op = OP_NOP;
      }
    } else if (code->op == OP_TADD) {
      ins_t *prev = find_ref(code, -1);
      ins_t *next = find_ref(code, 1);
      if (prev && prev->op == OP_LOOPNZ) {
        // ]
        // tmp += *0
        // ->
        // ]
        code->op = OP_NOP;
      } else if ((code->a & 0x7f) == 0
          && prev && prev->op == OP_ADD
          && next && next->op == OP_SET
          && prev->a <= 63 && prev->a >= -64)  {
        // *A += X
        // tmp = *A + tmp * (a>>8)
        // *A = Y
        // ->
        // tmp = *A + tmp + (a>>8) + X
        // *A = Y
        changed = 1;
        prev->op = OP_NOP;
        // 1bit: negate tmp, 7bit: offset
        code->a = (code->a & 0x80) | (prev->a & 0x7f);
      }
    }
    ++code;
  }
  return changed;
}

int peepfinal(ins_t *code) {
  while (code->op != OP_EOF) {
    if (code->op == OP_SHIFT) {
      //  *A += X
      //  shift A
      //->
      //  shift A
      //  *0 += X
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
      if (prev && prev->op == OP_SET && prev->a) {
        // *0 = X
        // [
        // ->
        // *0 = X
        // {
        code->a = 1;
      }
    } else if (code->op == OP_LOOPNZ) {
      ins_t *prev = find_ref(code, -1);
      if (prev && prev->op == OP_LOOPNZ) {
        //   ]
        // ]
        // ->
        //   ]
        // }
        code->a = 1;
      } else if (prev && prev->op == OP_SET && prev->a == 0) {
        //   *0 = 0
        // ]
        // ->
        //   *0 = 0
        // }
        code->a = 1;
      } else if (code[-1].op == OP_SHIFT && code[-2].op == OP_SKIPZ &&
                 code[-2].a == 0 && code[-3].op == OP_SHIFT && code[-3].b == code[-1].b) {
        // shift x
        // [
        //    shift x
        // ]
        // ->
        // {
        //    shift x
        // ]
        code[-3].op = OP_NOP;
        code[-2].a = 1;
      }
    }
    ++code;
  }
}
