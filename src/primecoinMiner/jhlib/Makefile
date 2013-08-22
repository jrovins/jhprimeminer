CXX = g++
CXXFLAGS = -fPIC -c -mtune=native -O2 -Wall -Wextra
INCLUDEFLAGS = -I. 
OBJ = customBuffer.o fastString.o fastString_eprintf.o hashTable_uint32.o packetBuffer.o simpleList.o

all: libjh

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<

libjh: $(OBJ)
	$(CXX) 

clean:
	-rm -f *.o
	-rm -f libjh.a
