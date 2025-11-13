all:
	gcc -static bwtdecode.c bwt-util.c -o bwtdecode -pg -O2
	gcc -static bwtsearch.c bwt-util.c -o bwtsearch -pg -O2

clean:
	rm -f bwtdecode
	rm -f bwtsearch
