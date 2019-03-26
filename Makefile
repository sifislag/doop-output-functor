# rudimentary
all:	libfunctors.so

libfunctors.so: 	incrOut.cpp
	g++ -O3 -shared -o libfunctors.so -fPIC incrOut.cpp
