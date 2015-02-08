CXX = mpicxx
CXXFLAGS = --std=c++11 -fno-PIE
LDFLAGS = -fno-PIE

all : example.x

example.x : example.o async.o
	$(CXX) $(LDFLAGS) $? -o $@

.PHONY : clean
clean:
	rm -f async.o example.o
