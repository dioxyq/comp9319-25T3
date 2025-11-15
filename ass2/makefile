all:
	musl-gcc -static bwtdecode.c bwt-util.c -o bwtdecode -O3
	musl-gcc -static bwtsearch.c -o bwtsearch -O3

clean:
	rm -f bwtdecode
	rm -f bwtsearch
