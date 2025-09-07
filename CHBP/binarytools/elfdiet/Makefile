CXX = g++
CXXFLAGS = -O2 -std=c++14 -g -fasynchronous-unwind-tables -fexceptions -fstack-clash-protection -fstack-protector-all -Wall -Wextra -Wl,-z,defs -Wl,-z,now
LIBS =
SOURCE = elf.cc testphvadress.cc
TARGET = testphad

$(TARGET): $(SOURCE)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ testphad

.PHONY: clean
