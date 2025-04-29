perf_poke: perf_poke.c perf_poke.skel.h
	gcc -g -O2 -lbpf perf_poke.c -o perf_poke

perf_poke.bpf.o: perf_poke.bpf.c
	clang -g -O2 -target bpf -c perf_poke.bpf.c -o perf_poke.bpf.o

perf_poke.skel.h: perf_poke.bpf.o
	bpftool gen skeleton perf_poke.bpf.o > perf_poke.skel.h

.PHONY: clean

clean:
	rm -f perf_poke perf_poke.bpf.o perf_poke.skel.h
