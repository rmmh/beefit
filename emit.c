// Driver file for DynASM-based JITs
// based on public domain code from haberman's jitdemo

#include <stdio.h>
#include <sys/mman.h>
#include <assert.h>

#include "dynasm/dasm_proto.h"
#include "dynasm/dasm_x86.h"

#include "beefit.h"

void err(char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

// generated from the .dasc file
#include "emit_x86.gen.h"

bf_ptr assemble(ins_t *code, int *size_out) {
  dasm_State *state;
  dasm_init(&state, 1);
  dasm_setup(&state, actionlist);

  emit(&state, code);

  size_t size;
  int dasm_status = dasm_link(&state, &size);
  assert(dasm_status == DASM_S_OK);

  char *mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(mem != MAP_FAILED);

  dasm_encode(&state, mem);
  dasm_free(&state);

  int success = mprotect(mem, size, PROT_EXEC | PROT_READ);
  assert(success == 0);

  #ifndef NDEBUG
  // Write generated machine code to a temporary file.
  // View with:
  //  objdump -D -b binary -mi386 /tmp/jitcode
  // Or:
  //  ndisasm -b 64 /tmp/jitcode
  FILE *f = fopen("/tmp/jitcode", "wb");
  fwrite(mem, size, 1, f);
  fclose(f);
  #endif

  *size_out = size;
  return (bf_ptr)mem;
}
