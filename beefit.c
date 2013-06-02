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

  int size;
  void (*fptr)(uint8_t*) = assemble(code, &size);

  printf("compiled %d instructions into %d bytes\n", count, size);

  uint8_t *buf = calloc(30000, 1);
  fptr(buf);
  free(buf);
  munmap(fptr, size);
}
