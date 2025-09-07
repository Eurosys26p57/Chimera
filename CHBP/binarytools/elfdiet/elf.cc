#include "./elf.h"

#include <fstream>
#include <iostream>

using std::cerr;
using std::endl;

bool Elf::Header::isValid() {
	return identMagic[0] == 0x7f
	    && identMagic[1] == 'E'
	    && identMagic[2] == 'L'
	    && identMagic[3] == 'F';
}

bool Elf::Header::isSupported() {
	if (identClass != classElf64) {
		cerr << "Only 64bit Elf supported" << endl;
	} else if (identData != DataLittleEndian) {
		cerr << "Only little endian supported (yet)" << endl;
	} else if (identVersion != VersionCurrent) {
		cerr << "Elf version " << int(identVersion)  << " not supported (yet), only "
		          << int(VersionCurrent) << endl;
	} else if (machine != MachineX86_64) {
		cerr << "Wrong/unsupported architecture!" << endl;
	} else {
		return true;
	}
	return false;
}

bool Elf::isValid() {
	if (len < sizeof(Header)) {
		cerr << "ELF file too small for header" << endl;
	} else {
		Header * header = getHeader();

		if (!header->isValid()) {
			cerr << "ELF file with invalid magic identification" << endl;
		} else if (!header->isSupported()) {
			cerr << "ELF file unsupported" << endl;
		} else if (len < header->programHeaderOffset + (header->programHeaderEntries * sizeof(ProgramHeader))) {
			cerr << "ELF file too small for program headers" << endl;
		} else {
			return true;
		}
	}
	return false;
}

Elf::Elf(const char * file) : data(nullptr), len(0) {
	// load binary
	std::ifstream bin(file, std::ifstream::binary | std::ifstream::ate);
	if (!bin) {
		cerr << "Could not open input binary" << endl;
	} else if (bin.tellg() == -1) {
		cerr << "Could not determine length" << endl;
	} else {
		// get size
		len = bin.tellg();
		// allocate buffer
		data = new char[len + 1];
		// read into buffer
		bin.seekg(0, std::ios::beg);
		if (!bin.read(data, len)) {
			cerr << "read of input binary failed" << endl;
		}
		bin.close();
	}
}

Elf::Elf(size_t size) : data(new char[size]), len(size) {
}

Elf::~Elf() {
	if (data != nullptr) {
		delete [] data;
	}
}

bool Elf::write(const char * file) {
	std::ofstream bin(file, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
	if (bin) {
		bin.write(data, len);
		bin.close();
		return true;
	}
	return false;
}
