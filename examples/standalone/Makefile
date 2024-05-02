decoder: $(wildcard ../../src/*.c) main.c $(wildcard ../../src/*.h) Makefile
	gcc -Og -ggdb -g3 -iquote ../../src -o $@ $(filter %.c, $^) \
		-D'MPDEC_ALLOCATOR(x)=(calloc((x),1))' -DMPDEC_FREE=free -include stdlib.h \
		-fwrapv -Wall
.PHONY: clean
clean:
	rm -f decoder
