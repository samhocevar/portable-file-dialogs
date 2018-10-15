
BINARIES = example

all: $(BINARIES)

example: example.cpp portable-file-dialogs.h
	$(CXX) -Wall -Wextra $(filter %.cpp, $^) -o $@

clean:
	rm -f $(BINARIES)

