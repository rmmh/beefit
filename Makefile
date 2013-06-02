CFLAGS=-m32 -O2
LDFLAGS=-m32

beefit: beefit.o emit.o

emit.o: emit.c emit_x86.gen.h

%.gen.h: %.dasc
	lua dynasm/dynasm.lua $< > $@

clean:
	rm -f beefit *.o *.gen.h
