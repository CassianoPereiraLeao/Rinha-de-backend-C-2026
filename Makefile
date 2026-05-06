COMPILER = GCC
SOURCE = ./main.c
OUT = ./backend_result.exe
FLAGS = -Wall

compile: build run

build:
	${COMPILER} ${SOURCE} -o ${OUT} ${FLAGS}

run:
	${OUT}
