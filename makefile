
all: cache-sim

cache-sim: cache-sim.cpp
	g++ -std=c++11 cache-sim.cpp -o cache-sim

clean:
	rm -f *.o cache-sim

