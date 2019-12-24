decoder: $(wildcard ../../src/*.c) main.c $(wildcard ../../src/*.h) Makefile
	gcc -Og -ggdb -g3 -iquote ../../src -o $@ $(filter %.c, $^) \
		-DMPDEC_ALLOCATOR=malloc -DMPDEC_FREE=free -include stdlib.h
.PHONY: clean
clean:
	rm -f decoder
