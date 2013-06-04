CFLAGS=-m32 -O2 -g -std=gnu99 -Wall -Wextra -Wswitch-enum -fshort-enums
LDFLAGS=-m32

beefit: beefit.o emit.o optimize.o

emit.o: emit.c emit_x86.gen.h

%.gen.h: %.dasc
	lua dynasm/dynasm.lua $< > $@

clean:
	rm -f beefit *.o *.gen.h
