CFLAGS = -Wall -g
LPTHREADFLAGS = -D_REENTRANT -lpthread
TARGET = fire_seq.out
SRC = fire_seq.c
INPUT_FILE = entrada_carga_pequena.txt
OUTPUT_FILE = ../relatorio/output.out

all: ${TARGET}

${TARGET}: ${SRC}
	gcc ${CFLAGS} ${SRC} -o ${TARGET} ${LPTHREADFLAGS}

run:
	./${TARGET} ${INPUT_FILE} > ${OUTPUT_FILE}

valgrind: all
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes ./${TARGET} ${INPUT_FILE}

gdb:
	gdb ${TARGET}

clean:
	rm -f ${TARGET}