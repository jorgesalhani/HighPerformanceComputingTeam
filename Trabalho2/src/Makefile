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

OMPI_SUPP := $(firstword $(wildcard \
	/usr/share/openmpi/openmpi-valgrind.supp \
	/usr/lib/x86_64-linux-gnu/openmpi/share/openmpi/openmpi-valgrind.supp \
	/usr/local/share/openmpi/openmpi-valgrind.supp))

SUPP_FLAG := $(if $(OMPI_SUPP),--suppressions=$(OMPI_SUPP),)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(OPTFLAGS) $(SRC) -o $(TARGET)

run: all
	mpirun --hostfile $(HOSTFILE) -np $(NPRC) ./$(TARGET) $(INPUT_FILE) >> $(OUTPUT_FILE)

valgrind: all
	mpirun 	--hostfile $(HOSTFILE) -np $(NPRC) valgrind -s --tool=memcheck \
			--leak-check=full --show-leak-kinds=all --track-origins=yes \
			$(SUPP_FLAG) \
			--log-file=valgrind_rank_%q{OMPI_COMM_WORLD_RANK}.out \
			./$(TARGET) $(INPUT_FILE)

clean:
	rm -f $(TARGET) valgrind_rank_*.out