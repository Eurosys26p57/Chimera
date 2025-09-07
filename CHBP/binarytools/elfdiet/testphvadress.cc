#include <unistd.h>
#include <cstddef>
#include <cstring>
#include <cassert>

#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

#include "./elf.h"

using std::cout;
using std::cerr;
using std::endl;
using std::flush;
using std::dec;
using std::hex;
using std::vector;
using std::string;
using std::min;
using std::max;

const uintptr_t page = 1 << 12;
static bool verbose = false;


struct range {
	uintptr_t from;
	uintptr_t to;
	void * info;
	range() : from(0), to(0), info(nullptr) {}
	range(uintptr_t from, uintptr_t to, void* info) : from(from), to(to), info(info) {}
	void print() const {
		printf("%10lx - %10lx\t(%p)\n", from, to, info);
	}
};

static void print(Elf const &e, const bool * const f = nullptr) {
	// Elf structure (debug)
	Elf::Header * header = e.getHeader();
	size_t slotsize = 64;
	size_t slots = (e.len + slotsize - 1) / slotsize;
	struct Used {
		uint8_t ph : 3;
		uint8_t sh : 3;
		Used() : ph(0), sh(0) {}
	};

	Used * used = new Used[slots];

	// Program Header
	uintptr_t phEnd = header->programHeaderOffset + header->programHeaderEntries * header->programHeaderEntrySize;
	for (size_t s = header->programHeaderOffset / slotsize ; s < (phEnd + slotsize - 1) / slotsize; s++) {
		used[s].ph++;
	}

	// Program Header contents
	for (unsigned entry = 0 ; entry < header->programHeaderEntries; entry++) {
		uintptr_t pheOffset = header->programHeaderOffset + header->programHeaderEntrySize * entry;
		Elf::ProgramHeader * programHeaderEntry = reinterpret_cast<Elf::ProgramHeader*>(e.data + pheOffset);

		uintptr_t pheEnd = programHeaderEntry->offset + programHeaderEntry->fileSize;
		for (size_t s = programHeaderEntry->offset / slotsize; s < (pheEnd + slotsize - 1) / slotsize && s < slots; s++) {
			if (used[s].ph < 5) {
				used[s].ph++;
			}
		}
	}

	// Section Header
	uintptr_t shEnd = header->sectionHeaderOffset + header->sectionHeaderEntries * header->sectionHeaderEntrySize;
	for (size_t s = header->sectionHeaderOffset / slotsize; s < (shEnd + slotsize - 1) / slotsize && s < slots; s++) {
		used[s].sh++;
	}

	// Section Header contents
	for (unsigned entry = 0 ; entry < header->sectionHeaderEntries; entry++) {
		uintptr_t sheOffset = header->sectionHeaderOffset + header->sectionHeaderEntrySize  * entry;
		Elf::SectionHeader * sectionHeaderEntry = reinterpret_cast<Elf::SectionHeader*>(e.data + sheOffset);

		uintptr_t sheEnd = sectionHeaderEntry->offset + sectionHeaderEntry->size;
		for (size_t s = sectionHeaderEntry->offset / slotsize; s < (sheEnd + slotsize - 1) / slotsize && s < slots; s++) {
			if (sectionHeaderEntry->type != Elf::SectionHeader::TypeProgamData &&
			    sectionHeaderEntry->type != Elf::SectionHeader::TypeBSS &&
			    used[s].sh < 5) {
				used[s].sh++;
			}
		}
	}

	cout << "\t\treferenced by "
	     << "\e[38;5;226mProgramHeader\e[0m / "
	     << "\e[38;5;201mSectionHeader\e[0m / "
	     << "\e[38;5;196mboth\e[0m / "
	     << "\e[38;5;6mnone\e[0m"
	     << flush;

	const string block[5] = { "·", "░", "▒", "▓", "█" };
	for (size_t s = 0; s < slots; s++) {
		if (s % slotsize == 0) {
			printf(f != nullptr && !f[s * slotsize / page] ? "\e[0m\n\e[36m%10lx\e[0m\t" : "\e[0m\n%10lx\t", s * slotsize);
		}
		unsigned data = 0;
		for (size_t p = 0; p < e.len - (s * slotsize) && p < slotsize; p++) {
			if (e.data[p + s * slotsize] != 0) {
				data++;
			}
		}
		cout << "\e[38;5;" << (used[s].ph + used[s].sh == 0 ? 6 : (231 - used[s].ph - used[s].sh * 6));
		cout << "m" << block[data == 0 ? 0 : (1 + (data - 1) / 16)] << "\e[0m" << flush;
	}
	printf("\e[0m\n");

	delete [] used;
}

static char* Filename = "hello.modify";

int main(int argc, char *argv[]){
	// Load Elf
	char* aimaddress = argv[2];
	char* trampbase = argv[3];
	Elf elf(argv[1]);
	if (elf.len == 0) {
		return false;
	}

	// Load Elf Header
	Elf::Header * header = elf.getHeader();

	// Read current Program Header
	vector<range> virt;
	unsigned otherProgramHeaders = 0;
	range last;
	vector<Elf::ProgramHeader *> phdr_pre, phdr_post;
	Elf::ProgramHeader * phdr = nullptr;
	for (unsigned entry = 0 ; entry < header->programHeaderEntries; entry++) {
		uintptr_t pheOffset = header->programHeaderOffset + header->programHeaderEntrySize * entry;
		assert(pheOffset + header->programHeaderEntrySize < elf.len);
		Elf::ProgramHeader * programHeaderEntry = (Elf::ProgramHeader*)(elf.data + pheOffset);

		//assert(programHeaderEntry->virtualAddress == programHeaderEntry->physicalAddress);
		cout << programHeaderEntry->memorySize << endl;
		cout << programHeaderEntry->fileSize << endl;
		//assert(programHeaderEntry->fileSize <= programHeaderEntry->memorySize);
		if (programHeaderEntry->type == Elf::ProgramHeader::TypeLoad) {
			assert(programHeaderEntry->alignment >= page);
			assert(programHeaderEntry->alignment % page == 0);

			uintptr_t pheVirtEnd = programHeaderEntry->virtualAddress + programHeaderEntry->memorySize;
			if (programHeaderEntry->virtualAddress == std::stoi(aimaddress, 0, 16)){
				cout << "find aim addr" << endl;
				programHeaderEntry->virtualAddress = std::stoi(trampbase, 0, 16);
				//programHeaderEntry->physicalAddress = std::stoi(trampbase, 0, 16);
			}
			cout << "0x" << std::hex << programHeaderEntry->virtualAddress << endl;
			range r(programHeaderEntry->virtualAddress, pheVirtEnd, programHeaderEntry);

			if (r.from < r.to) {
				virt.push_back(r);
			} else {
				cerr << "Ignoring load entry without size" << endl;
			}
			last = r;
		} else {
			if (programHeaderEntry->type == Elf::ProgramHeader::TypePhdr) {
				phdr = programHeaderEntry;
			}
			if (last.info == nullptr) {
				phdr_pre.push_back(programHeaderEntry);
			} else {
				phdr_post.push_back(programHeaderEntry);
			}
			otherProgramHeaders++;
		}
	}
	elf.write("b.out");
	return 0;
}
