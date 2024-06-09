clean: main
	@rm -f main.o process.o util.o
main: main.o process.o util.o
	@gcc -o main main.o process.o util.o
main.o: main.c process.h util.h
	@gcc -c main.c
process.o: process.c process.h
	@gcc -c process.c
util.o: util.c util.h
	@gcc -c util.c