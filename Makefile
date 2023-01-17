CC = gcc
CFLAGS = -w -g -c
AR = ar -rc
RANLIB = ranlib
 
all: writeonceFS.o writeonceFS.a
 
writeonceFS.a: writeonceFS.o
	$(AR) libwriteonceFS.a writeonceFS.o
	$(RANLIB) libwriteonceFS.a
 
writeonceFS.o:
	$(CC) $(CFLAGS) writeonceFS.c
 
clean:
	rm -rf test writeonceFS *.o *.a

