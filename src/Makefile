ifeq ($(OS),Windows_NT)
include Make_msys2.inc
else
include Make_linux.inc
endif

DEPFLAGS = -MMD -MP

CXXFLAGS = -std=c++17
ifdef DEBUG
CXXFLAGS += -g -O0 -Wall -fbounds-check -pedantic -D_GLIBCXX_DEBUG
CXXFLAGS2 = CXXFLAGS
else
CXXFLAGS2 = ${CXXFLAGS} -O2 -march=native -Wall 
CXXFLAGS += -O3 -march=native -Wall
endif

ALL= ant_simu.exe bench_seq.exe bench_soa.exe bench_openmp.exe bench_mpi.exe


default:	help

all: $(ALL)

clean:
	@rm -fr *.o *.d *.exe *~

.cpp.o:
	$(CXX) $(CXXFLAGS2) $(DEPFLAGS) -c $< -o $@	

ant_simu.exe : ant.o fractal_land.o renderer.o window.o ant_simu.o
	$(CXX) $(CXXFLAGS2) $^ -o $@ $(LIB)	

bench_seq.exe : ant.o fractal_land.o bench_seq.o
	$(CXX) $(CXXFLAGS2) -mconsole $^ -o $@

bench_soa.exe : fractal_land.o bench_soa.o
	$(CXX) $(CXXFLAGS2) -mconsole $^ -o $@

bench_openmp.exe : ant.o fractal_land.o bench_openmp.o
	$(CXX) $(CXXFLAGS2) -mconsole $^ -o $@

bench_mpi.exe : ant.cpp fractal_land.cpp bench_mpi.cpp
	$(CXX) $(CXXFLAGS2) ant.cpp fractal_land.cpp bench_mpi.cpp $(MSMPI_INC) $(MSMPI_LIB) -o bench_mpi.exe

help:
	@echo "Available targets : "
	@echo "    all            : compile all executables"
	@echo "Add DEBUG=yes to compile in debug"
	@echo "Configuration :"
	@echo "    CXX      :    $(CXX)"
	@echo "    CXXFLAGS :    $(CXXFLAGS)"

%.html: %.md
	pandoc -s --toc $< --css=./github-pandoc.css --metadata pagetitle="OS202 - TD1" -o $@

-include *.d
