#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>

#include "beefit.h"


int main(int argc, char *argv[]) {
  FILE *in = stdin;

  if (argc == 2) {
    in = fopen(argv[1], "r");
    if (!in) {
      perror("unable to open file");
      return 1;
    }
  }

  int limit = 1 << 16;
  int count = 0;
  ins_t *code = malloc(limit * sizeof(ins_t));
  char c;
  while ((c = getc(in)) != EOF) {
    ins_t ins;
    ins.op = OP_NOP;
    switch (c) {
      case '+': ins = (ins_t){OP_ADD, 1, 0};    break;
      case '-': ins = (ins_t){OP_ADD, -1, 0};   break;
      case '>': ins = (ins_t){OP_SHIFT, 0, 1};    break;
      case '<': ins = (ins_t){OP_SHIFT, 0, -1};   break;
      case '[': ins = (ins_t){OP_SKIPZ};      break;
      case ']': ins = (ins_t){OP_LOOPNZ};     break;
      case '.': ins = (ins_t){OP_PRINT};      break;
      case ',': ins = (ins_t){OP_READ};     break;
    }
    if (ins.op != OP_NOP) {
      code[count++] = ins;
      if (count >= limit) {
        limit *= 2;
        code = realloc(code, limit * sizeof(ins_t));
      }
    }
  }
  code[count] = (ins_t){OP_EOF};

  int opt_size = optimize(code);

#ifndef DEBUG
  print_code(code, opt_size);
#endif

  int size;
  bf_ptr fptr = assemble(code, &size);

  printf("ins:%d opt:%d x86:%dB\n", count, opt_size, size);

  uint8_t *buf = calloc(30000, 1);
  fptr(buf);
  free(buf);
  munmap(fptr, size);
}

void print_code(ins_t *code, int count) {
  int indent = 0;
  while (count--) {
    switch (code->op) {
      case OP_SHIFT:
        printf("%*s%s %d\n", indent, "", "shift", code->b);
        break;
      case OP_ADD:
        printf("%*s%d += %d\n", indent, "", code->b, (int8_t)code->a);
        break;
      case OP_SKIPZ:
        printf("%*s[\n", indent, "");
        indent += 2;
        break;
      case OP_LOOPNZ:
        indent -= 2;
        printf("%*s]\n", indent, "");
        break;
      case OP_PRINT:
        printf("%*sprint %d\n", indent, "", code->b);
        break;
      case OP_READ:
        printf("%*s%d = read\n", indent, "", code->b);
        break;
    }
    ++code;
  }
}
