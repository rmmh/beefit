#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#include "beefit.h"


void usage(char *name) {
  fprintf(stderr, "usage: %s [-d]/[-t]/[-s] [filename]\n", name);
  exit(1);
}

int main(int argc, char *argv[]) {
  FILE *in = stdin;

  if (argc > 3) {
    usage(argv[0]);
  }

  int stats = 0;

  int opt;
  while ((opt = getopt(argc, argv, "dths")) != -1) {
    switch (opt) {
      case 'd':
        debug = 1;
        break;
      case 't':
        debug = 1;
        trace = 1;
        break;
      case 's':
        stats = 1;
        break;
      case 'h':
        usage(argv[0]);
        break;
    }
  }
  if (optind < argc) {
    in = fopen(argv[optind], "r");
    if (!in) {
      perror("unable to open file");
      return 1;
    }
  }

  int limit = 1 << 16;
  int count = 0;
  int loop_depth = 0;
  int loop_count = 0;
  ins_t *code = malloc(limit * sizeof(ins_t));
  code[0] = (ins_t){OP_EOF, 0, 0};
  code++;
  char c;
  while ((c = getc(in)) != EOF) {
    ins_t ins;
    ins.op = OP_NOP;
    switch (c) {
      case '+': ins = (ins_t){OP_ADD, 1, 0};    break;
      case '-': ins = (ins_t){OP_ADD, -1, 0};   break;
      case '>': ins = (ins_t){OP_SHIFT, 0, 1};  break;
      case '<': ins = (ins_t){OP_SHIFT, 0, -1}; break;
      case '[': ins = (ins_t){OP_SKIPZ, 0, 0};
        loop_depth++;
        loop_count++;
        break;
      case ']': ins = (ins_t){OP_LOOPNZ, 0, 0};
        if (--loop_depth < 0) {
          fprintf(stderr, "error: unmatched ]\n");
          exit(1);
        }
        break;
      case '.': ins = (ins_t){OP_PRINT, 0, 0};  break;
      case ',': ins = (ins_t){OP_READ, 0, 0};   break;
    }
    if (ins.op != OP_NOP) {
      code[count++] = ins;
      if (count >= limit) {
        limit *= 2;
        code = realloc(code, limit * sizeof(ins_t));
      }
    }
  }
  code[count] = (ins_t){OP_EOF, 0, 0};

  int opt_size = optimize(code);

  if (trace) {
    trace_counts = calloc(loop_count, sizeof(uint32_t));
  } else if (debug) {
    print_code(code, opt_size);
  }

  int size;
  bf_ptr fptr = assemble(code, &size);

  if (debug || stats) {
    printf("ins:%d opt:%d x86:%dB\n", count, opt_size, size);
  }

  // TODO: calculate padding precisely
  uint8_t *buf = calloc(1 << 16, 1);
  fptr(buf + 1000);

  if (trace) {
    print_code(code, opt_size);
    free(trace_counts);
  }

  free(buf);
  free(code - 1);
  munmap(fptr, size);

  return 0;
}

void print_code(ins_t *code, int count) {
  int indent = 0;
  int loop_count = 0;
  while (count--) {
    switch (code->op) {
      case OP_SHIFT:
        printf("%*s%s %d\n", indent, "", "shift", code->b);
        break;
      case OP_ADD:
        printf("%*s*%d += %d\n", indent, "", code->b, code->a);
        break;
      case OP_ADDT:
        printf("%*s*%d ", indent, "", code->b);
        if (code->a == 1) {
          printf("+= tmp\n");
        } else if (code->a == -1) {
          printf("-= tmp\n");
        } else {
          printf("+= tmp*%d\n", code->a);
        }
        break;
      case OP_SET:
        printf("%*s*%d = %d\n", indent, "", code->b, code->a);
        break;
      case OP_SETT:
        printf("%*s*%d = tmp\n", indent, "", code->b);
        break;
      case OP_TADD: {
         int off = (int8_t)((code->a & 0x7f) | ((code->a & 0x40) << 1));
          if (code->a & 0x80) {
            if (off)
              printf("%*stmp = *%d - tmp %+d\n", indent, "", code->b, off);
            else
              printf("%*stmp = *%d - tmp\n", indent, "", code->b);
          } else {
            if (off)
              printf("%*stmp += *%d %+d\n", indent, "", code->b, off);
            else
              printf("%*stmp += *%d\n", indent, "", code->b);
          }
        }
        break;
      case OP_LOAD:
        if (code->a) {
          printf("%*stmp = *%d + %d\n", indent, "", code->b, code->a);
        } else {
          printf("%*stmp = *%d\n", indent, "", code->b);
        }
        break;
      case OP_SKIPZ:
        printf("%*s%c\n", indent, "", code->a ? '{' : '[');
        indent += 2;
        if (trace) {
          printf("%*sLC: %d #%d\n", indent, "", trace_counts[loop_count], loop_count);
          loop_count++;
        }
        break;
      case OP_LOOPNZ:
        indent -= 2;
        printf("%*s%c\n", indent, "", code->a ? '}' : ']');
        break;
      case OP_PRINT:
        printf("%*sprint *%d\n", indent, "", code->b);
        break;
      case OP_READ:
        printf("%*s*%d = read\n", indent, "", code->b);
        break;
      case OP_NOP:
      case OP_EOF:
        break;
    }
    ++code;
  }
}
