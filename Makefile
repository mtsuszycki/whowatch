
CFLAGS=-O2 -fomit-frame-pointer -Wall 
OBJ=whowatch.o screen.o proctree.o proc.o process.o owner.o

BINDIR=/usr/bin/
MANDIR=/usr/man/man1/

whowatch: $(OBJ)
	gcc -o whowatch $(OBJ) -lncurses 
	strip whowatch
clean: 
	rm -f ./*.o
	rm -f ./core
	rm -f ./whowatch
install:
	install	-m 755 ./whowatch $(BINDIR)
	install -m 644 ./whowatch.1 $(MANDIR)
