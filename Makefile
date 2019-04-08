
BINARIES = example

all: $(BINARIES)

example: example.cpp portable-file-dialogs.h
	$(CXX) -std=c++11 -g -ggdb -Wall -Wextra $(filter %.cpp, $^) -o $@

clean:
	rm -f $(BINARIES)

