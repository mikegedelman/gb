all: build

build:
	clang *.c -o gb -g -I /usr/local/include -lSDL2

run: build
	./gb