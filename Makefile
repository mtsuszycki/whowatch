
CFLAGS=-O2 -fomit-frame-pointer -malign-loops=2 -Wall 
OBJ=whowatch.o screen.o proctree.o proc.o


whowatch: $(OBJ)
	gcc -o whowatch $(OBJ) -lncurses
	strip whowatch
clean: 
	rm -f ./*.o
	rm -f ./core
	rm -f ./whowatch