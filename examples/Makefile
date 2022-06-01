
BINARIES = example kill

CXXFLAGS = -I.. -std=c++11 -g -ggdb -Wall -Wextra
LDFLAGS = $(subst MINGW,-luuid -lucrt,$(findstring MINGW,$(shell uname)))

all: $(BINARIES)

example: example.cpp ../portable-file-dialogs.h
	$(CXX) $(CXXFLAGS) $(filter %.cpp, $^) $(LDFLAGS) -o $@

kill: kill.cpp ../portable-file-dialogs.h
	$(CXX) $(CXXFLAGS) $(filter %.cpp, $^) $(LDFLAGS) -o $@

clean:
	rm -f $(BINARIES)

