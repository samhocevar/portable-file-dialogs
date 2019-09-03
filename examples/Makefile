
BINARIES = example

CXXFLAGS = -I.. -std=c++11 -g -ggdb -Wall -Wextra

all: $(BINARIES)

example: example.cpp ../portable-file-dialogs.h
	$(CXX) $(CXXFLAGS) $(filter %.cpp, $^) -o $@

clean:
	rm -f $(BINARIES)

