
ifdef D
   CXXFLAGS=-Wall -std=c++11 -g -pg -DDEBUG
else
   CXXFLAGS=-Wall -std=c++11 -g -O3 
endif



#CXXFLAGS=-Wall -std=c++11 -g -pg

CC=g++

all: test test_logging_restore generate testing_reads testing_writes

test: test.cpp betree.hpp swap_space.o backing_store.o

testing_reads: testing_reads.cpp betree.hpp swap_space.o backing_store.o

testing_writes: testing_writes.cpp betree.hpp swap_space.o backing_store.o

test_logging_restore: test_logging_restore.cpp betree.hpp swap_space.o backing_store.o

generate: generate.cpp

swap_space.o: swap_space.cpp swap_space.hpp backing_store.hpp

backing_store.o: backing_store.hpp backing_store.cpp

clean:
	$(RM) *.o test test_logging_restore generate testing_reads testing_writes
