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

static void print(vector<range> const& r) {
	unsigned pages = 0;
	for(auto const& v : r) {
		pages += (v.to - v.from + page - 1) / page;
		v.print();
	}
	cout << "(" << dec << pages << " pages)" << endl;
}

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

static bool nulled(Elf const &e, range const &r, uintptr_t start, size_t len) {
	Elf::ProgramHeader * ph = reinterpret_cast<Elf::ProgramHeader *>(r.info);
	if (start < ph->virtualAddress) {
		return false;
	}
	uintptr_t base = start - ph->virtualAddress + ph->offset;
	for (uintptr_t p = base; p < base + len; p++) {
		if (p >= e.len || e.data[p] != 0) {
			return false;
		}
		if (p % page == 0) {
			return true;
		}
	}
	return true;
}

static void clear(Elf const &e, uintptr_t from, uintptr_t to, void * info) {
	Elf::ProgramHeader * phe = reinterpret_cast<Elf::ProgramHeader *>(info);
	assert(from >= phe->virtualAddress && to <= phe->virtualAddress + phe->memorySize);
	uintptr_t dataFrom = from - phe->virtualAddress + phe->offset;
	uintptr_t dataLen = to - from;
	if (dataLen == 0)
		return;
	if (dataLen > phe->fileSize)
		dataLen = phe->fileSize;
	assert(phe->offset + dataLen <= e.len);
	if (verbose) {
		printf("Removing %lx - %lx [%lx - %lx]\n", from, to, dataFrom, dataFrom + dataLen);
	}
	memset(e.data + dataFrom, 0, dataLen);
}

static void cut(Elf const &e, vector<range> & storage, uintptr_t from, uintptr_t to,
                bool page_wise = false, bool do_clear = false) {
	auto i = begin(storage);
	while (i != end(storage)) {
		if (to >= i->from && from <= i->to) {
			if (from <= i->from && to >= i->to) {
				if (do_clear)
					clear(e, i->from, i->to, i->info);
				i = storage.erase(i);
				continue;
			} else if (from <= i->from) {
				if (do_clear)
					clear(e, i->from, to, i->info);
				i->from = to;
			} else if (to >= i->to) {
				if (do_clear)
					clear(e, from, i->to, i->info);
				i->to = from;
			} else if (!page_wise || from / page != to / page) {  /* inside only if on different pages*/
				if (do_clear)
					clear(e, from, to, i->info);
				range t(i->from, from, i->info);
				i->from = to;
				i = storage.insert(i, t);
			}
		}
		i++;
	}
}

static void translate_page(uintptr_t *a, size_t *t) {
	*a = (t[(*a) / page] * page) + ((*a) % page);
}

static void is_used(Elf const &e, const size_t start, const size_t len, const size_t block, bool * array) {
	// Elf structure (debug)
	Elf::Header * header = e.getHeader();
	// null
	for (size_t p = 0; p < (len + block - 1) / block; p++) {
		array[p] = false;
	}

	// Program Header
	uintptr_t phEnd = header->programHeaderOffset + header->programHeaderEntries * header->programHeaderEntrySize;
	for (size_t p = max(start, header->programHeaderOffset); p < min(start + len, phEnd) + block - 1; p += block) {
		array[(p - start) / block] = true;
	}

	// Program Header contents
	for (unsigned entry = 0 ; entry < header->programHeaderEntries; entry++) {
		uintptr_t pheOffset = header->programHeaderOffset + header->programHeaderEntrySize * entry;
		Elf::ProgramHeader * programHeaderEntry = reinterpret_cast<Elf::ProgramHeader*>(e.data + pheOffset);

		uintptr_t pheEnd = programHeaderEntry->offset + programHeaderEntry->fileSize;
		for (size_t p = max(start, programHeaderEntry->offset); p < min(start + len, pheEnd);
		     p++) {
			array[(p - start) / block] = true;
		}
	}

	// Section Header
	uintptr_t shEnd = header->sectionHeaderOffset + header->sectionHeaderEntries * header->sectionHeaderEntrySize;
	for (size_t p = max(start, header->sectionHeaderOffset); p < min(start + len, shEnd) + block - 1; p += block) {
		array[(p - start) / block] = true;
	}

	// Section Header contents
	for (unsigned entry = 0 ; entry < header->sectionHeaderEntries; entry++) {
		uintptr_t sheOffset = header->sectionHeaderOffset + header->sectionHeaderEntrySize * entry;
		Elf::SectionHeader * sectionHeaderEntry = reinterpret_cast<Elf::SectionHeader*>(e.data + sheOffset);

		/* Ignore PROGBITS, since they reference everything */
		/* Ignore NOBITS, since they can point outside elf */
		if (sectionHeaderEntry->type != Elf::SectionHeader::TypeProgamData &&
		    sectionHeaderEntry->type != Elf::SectionHeader::TypeBSS) {
			uintptr_t sheEnd = sectionHeaderEntry->offset + sectionHeaderEntry->size;
			for (size_t p = max(start, sectionHeaderEntry->offset); p < min(start + len, sheEnd) + block - 1; p += block) {
				array[(p - start) / block] = true;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	// defaults
	bool debug = false;
	bool shrink = false;
	bool trim = false;
	bool extended = false;
	bool combine = false;
	bool minimal = false;
	const char * output = "b.out";
	vector<range> remove;

	// Parse args
	int opt;
	while ((opt = getopt(argc, argv, "?hdo:r:mstcxv")) != -1) {
		switch (opt) {
			case 'd':
				debug = true;
				break;
			case 'o':
				output = optarg;
				break;
			case 'r': {
				char * p = strpbrk(optarg, ":-");
				if (p == nullptr) {
					cerr << "Invalid remove argument '" << optarg << "' - ignoring!" << endl;
				} else {
					errno = 0;
					range r;
					char * f = nullptr;
					r.from = strtoull(optarg, &f, 0);
					char * t = nullptr;
					r.to = strtoull(p + 1, &t, 0);
					if (r.from == 0 || r.to == 0 || errno != 0 || f != p || *t != '\0') {
						cerr << "Remove argument '" << optarg << "' not parsable - ignoring!" << endl;
					} else {
						if (*p == ':') {
							r.to += r.from;
						}
						r.info = &optarg;
						if (r.to <= r.from) {
							cerr << "Invalid range '" << optarg << "' (" << reinterpret_cast<void*>(r.from)
							     << " > " << reinterpret_cast<void*>(r.to) << ") - ignoring!" << endl;
						} else {
							remove.push_back(r);
						}
					}
				}
				break;
			}
			case 's':
				shrink = true;
				break;
			case 'c':
				combine = true;
				break;
			case 't':
				trim = true;
				break;
			case 'm':
				minimal = true;
				break;
			case 'x':
				extended = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'h':
			case '?':
				cout << "Usage: " << argv[0]
				     << " [-h] [-d] [-o OUTPUT] [-r RANGE] [-m] [-s [-t] [-c]] [-x] [-v] INPUT" << endl
				     << endl
				     << "   -d         Debug mode (print ELF structre)" << endl
				     << "   -h         Print this help" << endl
				     << "   -o OUTPUT  Use 'OUTPUT' instead of 'b.out' for resulting file" << endl
				     << "   -r RANGE   Remove given RANGE. Accepted formats ar" << endl
					 << "                  'START-END' and 'START:LEN'" << endl
					 << "              with common prefixes for base ('0x' for hex), e.g. '0xdead:0xbeef'" << endl
					 << "   -m         Minimal block size (will increase LOADs)" << endl
					 << "   -s         Shrink ELF (remove empty pages from file)" << endl
					 << "   -t         Try to trim trailing zeros (into BSS) while shrinking" << endl
					 << "   -x         Strip unused blocks (detected by symbol table)" << endl
					 << "   -c         Try to combine consecutive pages while shrinking" << endl
					 << "   -v         Verbose output (print ranges)" << endl << endl
					 << "   INPUT      Input file (currently we only support ELF64)" << endl << endl;
				return EXIT_SUCCESS;
			default:
				cerr << "Invalid parameter '-" << opt << "', abort (use -h for help)." << endl;
				return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		cerr << "No input file (use -h for help)" << endl;
		return EXIT_FAILURE;
	}

	// Load Elf
	Elf elf(argv[optind]);
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

		assert(programHeaderEntry->virtualAddress == programHeaderEntry->physicalAddress);
		assert(programHeaderEntry->fileSize <= programHeaderEntry->memorySize);
		if (programHeaderEntry->type == Elf::ProgramHeader::TypeLoad) {
			assert(programHeaderEntry->alignment >= page);
			assert(programHeaderEntry->alignment % page == 0);

			uintptr_t pheVirtEnd = programHeaderEntry->virtualAddress + programHeaderEntry->memorySize;
			range r(programHeaderEntry->virtualAddress, pheVirtEnd, programHeaderEntry);

			assert(r.from >= last.to);
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
	if (extended) {
		vector<range> extended_remove;
		extended_remove.push_back(range(64, elf.len, nullptr));  // Preserve ELF Header

		uintptr_t phEnd = header->programHeaderOffset + header->programHeaderEntries * header->programHeaderEntrySize;
		cut(elf, extended_remove, header->programHeaderOffset,  phEnd);

		uintptr_t shEnd = header->sectionHeaderOffset + header->sectionHeaderEntries * header->sectionHeaderEntrySize;
		cut(elf, extended_remove, header->sectionHeaderOffset,  shEnd);

		for (unsigned entry = 0 ; entry < header->sectionHeaderEntries; entry++) {
			unsigned sheOffset = header->sectionHeaderOffset + header->sectionHeaderEntrySize * entry;
			assert(sheOffset + header->sectionHeaderEntrySize <= elf.len);
			Elf::SectionHeader * sectionHeaderEntry = reinterpret_cast<Elf::SectionHeader*>(elf.data + sheOffset);
			if (sectionHeaderEntry->type != Elf::SectionHeader::TypeBSS) {
				cut(elf, extended_remove, sectionHeaderEntry->offset, sectionHeaderEntry->offset + sectionHeaderEntry->size);
			}
		}
		if (verbose) {
			for(auto const& s : extended_remove)
				printf("Additional remove 0x%lx-0x%lx\n", s.from, s.to);
		}
		remove.insert(remove.end(), extended_remove.begin(), extended_remove.end());
	}



	if (verbose) {
		cout << "Virtual memory of original file:" << endl;
		print(virt);
		cout << endl;
	}
	if (debug) {
		print(elf);
	}

	// Calculate new mapping
	if (!remove.empty()) {
		for(auto const& s : remove)
			cut(elf, virt, s.from, s.to, !minimal, true);

		if (verbose) {
			cout << "Virtual memory after removal of code:" << endl;
			print(virt);
			cout << endl;
		}

		// combine load statements if possible
		auto v = begin(virt);
		while (v != end(virt) && v + 1 != end(virt)) {
			auto next = v + 1;
			Elf::ProgramHeader * vi = reinterpret_cast<Elf::ProgramHeader *>(v->info);
			Elf::ProgramHeader * ni = reinterpret_cast<Elf::ProgramHeader *>(next->info);
			if (combine && (
			    v->to % page < next->from % page ||
			    (v->from % page > next->to % page && v->from / page == v->to / page && next->to / page == next->from / page))) {
				// combinable
			} else if (next->from / page - v->to / page <= 1 && vi != nullptr && ni != nullptr && vi->flags == ni->flags) {
				v->to = next->to;
				virt.erase(next);
				continue;
			}
			v++;
		}

		// Find space for identity mapped Program Header
		size_t psize = sizeof(Elf::ProgramHeader) * (virt.size() + otherProgramHeaders);
		uintptr_t paddr = 0;
		if (psize <= header->programHeaderEntrySize * header->programHeaderEntries) {
			if (verbose) {
				cout << "Using old program header space" << endl;
			}
			paddr = header->programHeaderOffset;
		} else {
			if (verbose) {
				cout << "Need " << psize << " bytes for program header " << endl;
			}
			// todo: check readable
			for (auto v = begin(virt); v != end(virt); v++) {
				if (v->from % page >= psize && nulled(elf, *v, v->from - psize, psize)) {
					v->from -= psize;
					paddr = v->from;
					break;
				} else if (v + 1 != end(virt)) {
					auto next = v + 1;
					if (next->from - v->to >= psize && nulled(elf, *v, v->to, psize)) {
						paddr = v->to;
						v->to += psize;
						break;
					}
				}
			}
		}

		if (verbose) {
			cout << "Having ProgramHeader mapped at " << hex << paddr << ":" << endl;
			print(virt);
			cout << endl;
		}

		// check phdr
		if (paddr != header->programHeaderOffset) {
			if (verbose) {
				cout << "Modifing Program Header..." << endl;
			}
			// Modify
			if (phdr != nullptr) {
				phdr->physicalAddress = phdr->virtualAddress = phdr->offset = paddr;
				phdr->memorySize = phdr->fileSize = (virt.size() + otherProgramHeaders) * sizeof(Elf::ProgramHeader);
			}

			// write new phdr
			uintptr_t pos = paddr;
			for(auto const& v : phdr_pre) {
				memcpy(elf.data + pos, v, sizeof(Elf::ProgramHeader));
				pos += sizeof(Elf::ProgramHeader);
			}
			for(auto const& v : virt) {
				Elf::ProgramHeader ph = *reinterpret_cast<Elf::ProgramHeader *>(v.info);
				intptr_t delta = v.from - ph.virtualAddress;
				assert(delta >= 0 || (uintptr_t)(delta * (-1)) < ph.offset);
				ph.offset += delta;
				ph.fileSize -= delta;
				ph.virtualAddress += delta;
				ph.physicalAddress += delta;
				// TODO:
				uintptr_t len = v.to - v.from;
				ph.memorySize = len;
				if (ph.fileSize > len || (phdr != nullptr && ph.offset < phdr->offset && phdr->offset < ph.offset + len))
					ph.fileSize = len;
				ph.alignment = page;
				memcpy(elf.data + pos, &ph, sizeof(Elf::ProgramHeader));
				pos += sizeof(Elf::ProgramHeader);
			}
			for(auto const& v : phdr_post) {
				memcpy(elf.data + pos, v, sizeof(Elf::ProgramHeader));
				pos += sizeof(Elf::ProgramHeader);
			}
			assert(pos == paddr + psize);

			// overwrite old phdr
			memset(elf.data + header->programHeaderOffset, 0, header->programHeaderEntrySize * header->programHeaderEntries);
			// reference new phdr
			header->programHeaderEntrySize = sizeof(Elf::ProgramHeader);
			header->programHeaderOffset = paddr;
			header->programHeaderEntries = virt.size() + otherProgramHeaders;
		}
	} else if (verbose) {
		cout << "(No code to remove)" << endl;
	}

	// reduce
	if (shrink) {
		// use BSS if possible
		if (trim) {
			for (unsigned entry = 1 ; entry < header->programHeaderEntries; entry++) {
				Elf::ProgramHeader * programHeader = reinterpret_cast<Elf::ProgramHeader*>(
				                     elf.data + header->programHeaderOffset + header->programHeaderEntrySize * entry);
				while (programHeader->type == Elf::ProgramHeader::TypeLoad && programHeader->fileSize > 0 &&
				       elf.data[programHeader->offset + programHeader->fileSize - 1] == 0) {
					programHeader->fileSize--;
				}
			}
		}


		// Find (un)used pages
		size_t pages = (elf.len + page - 1) / page;
		bool used[pages] = { false };
		is_used(elf, 0, elf.len, page, used);

		if (debug) {
			print(elf, used);
		}

		// move and create translation table
		size_t translate[pages] = { 0 };
		size_t delta = 0;
		bool * usage = nullptr;
		if (combine) {
			usage = new bool[elf.len];
			is_used(elf, 0, elf.len, 1, usage);
		}

		uintptr_t phEnd = header->programHeaderOffset + header->programHeaderEntries * header->programHeaderEntrySize;
		for (size_t p = 0 ; p < pages ; p++) {
			// phdr must be identity mapped - we'll start after
			if (p <= (phEnd + page - 1) / page) {
				translate[p] = p;
			} else {
				if (used[p]) {
					bool combinable = combine;
					if (usage != nullptr) {
						bool b = true;
						for (size_t i = 0; i < page; i++) {
							if (b && (p * page + i >= elf.len || usage[p * page + i])) {
								b = false;
							}
							if (!b && usage[translate[p - 1] * page + i]) {
								combinable = false;
								break;
							}
						}
					}

					if (combinable) {
						for (size_t i = 0; i < page && p * page + i < elf.len; i++) {
							if (usage[p * page + i]) {
								assert(!usage[translate[p - 1] * page + i]);
								elf.data[translate[p - 1] * page + i] = elf.data[p * page + i];
								usage[translate[p - 1] * page + i] = true;
							}
						}
					} else {
						translate[p] = p - delta;
						if (delta > 0) {
							memcpy(elf.data + translate[p] * page,  elf.data + p * page, std::min(page, elf.len - p * page));
							if (usage != nullptr)
								memcpy(usage + translate[p] * page,  usage + p * page, sizeof(bool) * std::min(page, elf.len - p * page));
						}
						continue;
					}
				}

				translate[p] = translate[p - 1];
				delta++;
			}
		}

		if (usage != nullptr) {
			delete [] usage;
		}
		// Special case: unused last page
		if (!used[pages - 1]) {
			elf.len &= ~(page - 1);
			delta--;
		}
		elf.len -= delta * page;

		// update references
		translate_page(&(header->programHeaderOffset), translate);
		for (unsigned entry = 0 ; entry < header->programHeaderEntries; entry++) {
			uintptr_t pheOffset = header->programHeaderOffset + header->programHeaderEntrySize * entry;
			Elf::ProgramHeader * programHeaderEntry = reinterpret_cast<Elf::ProgramHeader*>(elf.data + pheOffset);
			if (programHeaderEntry->offset + programHeaderEntry->fileSize == 0) {
				continue;
			}
			uintptr_t to = programHeaderEntry->offset + programHeaderEntry->fileSize - 1;
			translate_page(&(programHeaderEntry->offset), translate);
			translate_page(&to, translate);
			assert(to + 1 == programHeaderEntry->offset + programHeaderEntry->fileSize);
		}

		translate_page(&(header->sectionHeaderOffset), translate);
		for (unsigned entry = 0 ; entry < header->sectionHeaderEntries; entry++) {
			uintptr_t sheOffset = header->sectionHeaderOffset + header->sectionHeaderEntrySize * entry;
			Elf::SectionHeader * sectionHeaderEntry = reinterpret_cast<Elf::SectionHeader*>(elf.data + sheOffset);
			if (sectionHeaderEntry->offset + sectionHeaderEntry->size == 0) {
				continue;
			}
			uintptr_t to = sectionHeaderEntry->offset + sectionHeaderEntry->size - 1;
			translate_page(&(sectionHeaderEntry->offset), translate);
			if (sectionHeaderEntry->type != Elf::SectionHeader::TypeBSS) {
				translate_page(&to, translate);
				sectionHeaderEntry->size = to - sectionHeaderEntry->offset + 1;
			}
		}
		// TODO: Reorder code by offset - filesize (reduce file size)
	} else if (verbose) {
		cout << "(No shrinking of ELF)" << endl;
	}

	if (debug) {
		cout << "Final ELF:" << endl;
		print(elf);
	}

	if (verbose) {
		cout << "writting to '"  << output << "'..." << endl;
	}
	return elf.write(output) ? EXIT_SUCCESS : EXIT_FAILURE;
}
