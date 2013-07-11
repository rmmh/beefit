CFLAGS=-O2 -g -std=gnu99 -Wall -Wextra -Wswitch-enum -fshort-enums

beefit: beefit.o emit.o optimize.o

emit.o: emit.c emit_x64.gen.h

%.gen.h: %.dasc
	lua dynasm/dynasm.lua $< > $@

clean:
	rm -f beefit *.o *.gen.h
