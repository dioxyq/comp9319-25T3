all:
	musl-gcc -static bwtdecode.c bwt-util.c -o bwtdecode 
	musl-gcc -static bwtsearch.c bwt-util.c -o bwtsearch	

clean:
	rm -f bwtdecode
	rm -f bwtsearch
