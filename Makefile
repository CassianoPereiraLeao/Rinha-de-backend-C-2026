COMPILER = gcc -g
SOURCE = ./src/*.c
OUT = ./backend_result
FLAGS = -Wall -O3 -lsqlite3

compile: build run

build:
	${COMPILER} ${SOURCE} -o ${OUT} ${FLAGS}

run:
	${OUT}
