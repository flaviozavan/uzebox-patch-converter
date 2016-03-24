CXXFLAGS=-Wall -Wextra -O2 -std=c++11

all: uzebox-patch-converter

uzebox-patch-converter: input.o generate.o

clean:
	rm -f uzebox-patch-converter input.o generate.o
