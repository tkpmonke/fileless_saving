CFLAGS = -std=c89 -O2 -Wall -Wextra -fPIE

default: 
	gcc $(CFLAGS) testing/test.c -o testing/test

x64:
	gcc $(CFLAGS) -m64 testing/test.c -o testing/test

x32:
	gcc $(CFLAGS) -m32 testing/test.c -o testing/test
