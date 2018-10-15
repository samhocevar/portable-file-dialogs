
BINARIES = example

all: $(BINARIES)

example: example.cpp portable-file-dialogs.h
	$(CXX) -g -ggdb -Wall -Wextra $(filter %.cpp, $^) -o $@

clean:
	rm -f $(BINARIES)

