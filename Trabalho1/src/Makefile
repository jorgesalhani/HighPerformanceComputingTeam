CFLAGS = -Wall -g
LPTHREADFLAGS = -D_REENTRANT -O3 -fopenmp -march=native
TARGET = fire_omp fire_seq
INPUT_FILE = entrada_carga_pequena.txt
OUTPUT_FILE = output.out

all: fire_omp fire_seq

fire_omp: fire_omp.c
	gcc $(CFLAGS) fire_omp.c -o fire_omp $(LPTHREADFLAGS)

fire_seq: fire_seq.c
	gcc $(CFLAGS) fire_seq.c -o fire_seq $(LPTHREADFLAGS)

run: all
	./fire_omp ${INPUT_FILE} >> ${OUTPUT_FILE}

valgrind: all
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes ./${TARGET} ${INPUT_FILE}

gdb:
	gdb ${TARGET}

clean:
	rm -f ${TARGET}