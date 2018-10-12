
all: example

example: example.cpp portable-file-dialogs.h
	$(CXX) -Wall -Wextra $(filter %.cpp, $^) -o $@

