all: build

build:
	clang main.c -o gb -g

run: build
	./gb