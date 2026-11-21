CC = mpicc
CFLAGS = -Wall -g
OPTFLAGS = -D_REENTRANT -O3 -fopenmp -march=native

TARGET = fire_mpi_omp
SRC = fire_mpi_omp.c

HOSTFILE = hosts.txt
NPRC = 4
INPUT_FILE = entrada_carga_pequena.txt
OUTPUT_FILE = output.out

.PHONY: all run valgrind clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(OPTFLAGS) $(SRC) -o $(TARGET)

run: all
	mpirun --hostfile $(HOSTFILE) -np $(NPRC) ./$(TARGET) $(INPUT_FILE) >> $(OUTPUT_FILE)

valgrind: all
	mpirun --hostfile $(HOSTFILE) -np $(NPRC) valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind_rank_%p.log ./$(TARGET) $(INPUT_FILE)

clean:
	rm -f $(TARGET) valgrind_rank_*.log