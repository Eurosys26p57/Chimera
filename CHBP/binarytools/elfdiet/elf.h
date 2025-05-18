

#pragma once

#include <cstdint>
#include <cstddef>



class Elf {
 public:
	
	char * data;
	
	size_t len;

 public:
	
	struct Header {
		
		char identMagic[4];

		
		
		enum : uint8_t {
			classElf32 = 1,
			classElf64 = 2,
		} identClass;

		
		enum : uint8_t {
			DataLittleEndian = 1,
			DataBigEndian    = 2,
		} identData;

		
		enum : uint8_t {
			VersionNone = 0,
			VersionCurrent = 1,
		} identVersion;

		
		uint8_t abi;

		
		uint64_t padding;

		
		
		enum : uint16_t {
			TypeRelocatable = 1,
			TypeExecutable  = 2,
			TypeShared      = 3,
			TypeCore        = 4,
		} type;

		
		enum : uint16_t {
			MachineUnknown = 0,
			MachineSparc   = 2,
			MachineX86     = 3,
			MachineM68k    = 4,
			MachineM88k    = 5,
			MachineMIPS    = 8,
			MachinePowerPC = 0x14,
			MachineIA64    = 0x32,
			MachineX86_64  = 0x3e,
			MachineArm64   = 0xb7,
		} machine;

		
		uint32_t version;

		

		
		uintptr_t entry;

		
		uintptr_t programHeaderOffset;
		
		uintptr_t sectionHeaderOffset;

		
		uint32_t archFlags;

		
		uint16_t elfHeaderSize;
		
		uint16_t programHeaderEntrySize;
		
		uint16_t programHeaderEntries;

		
		uint16_t sectionHeaderEntrySize;
		
		uint16_t sectionHeaderEntries;
		
		uint16_t sectionHeaderNamesIndex;

		
		bool isValid();

		
		bool isSupported();
	} __attribute__((packed));

	

	static_assert(sizeof(Header) == 64, "64bit Elf Header size wrong");

	
	struct ProgramHeader {
		
		enum : uint32_t {
			TypeUnused    = 0,
			TypeLoad      = 1,
			TypeDynamic   = 2,
			TypeInterpret = 3,
			TypeNote      = 4,
			TypeShLib     = 5,
			TypePhdr      = 6,
		} type;

		
		enum : uint32_t {
			FlagExecutable   = 1,
			FlagWriteable    = 2,
			FlagReadable     = 4,
		} flags;

		
		uintptr_t offset;

		
		uintptr_t virtualAddress;

		
		uintptr_t physicalAddress;

		
		uint64_t fileSize;

		
		uint64_t memorySize;

		
		uint64_t alignment;
	} __attribute__((packed));

	
	static_assert(sizeof(ProgramHeader) == 56, "64bit Elf Program Header size wrong");

	struct SectionHeader {
		uint32_t name;
		enum : uint32_t {
			TypeNull    = 0,
			TypeProgamData      = 1,
			TypeSymbolTable   = 2,
			TypeStringTable = 3,
			TypeRelocationEntriesWithAddends      = 4,
			TypeHashTable     = 5,
			TypeDynamic      = 6,
			TypeNote      = 7,
			TypeBSS      = 8,
			TypeRelocationEntries      = 9,
			TypeShLib = 0xa,
			TypeDynamicSymbolTable = 0xb,
			TypeInitArray = 0xe,
			TypeFiniArray = 0xf,
			TypePreInitArray = 0x10,
			TypeGroup = 0x11,
			TypeSectionIndex = 0x12,
			TypeNum = 0x13
		} type;

		enum : uint64_t {
			FlagWriteable    = 1,
			FlagAllocate     = 2,
			FlagExecutable   = 4,
			FlagMerge        = 0x10,
			FlagStrings      = 0x20,
			FlagInfoLink     = 0x40,
			FlagLinkOrder    = 0x80,
			FlagOSNonConform = 0x100,
			FlagGroup        = 0x200,
			FlagTLS          = 0x400,
		} flags;

		
		uintptr_t virtualAddress;

		
		uintptr_t offset;

		uint64_t size;
		uint32_t link;
		uint32_t info;
		uint64_t alignment;
		uint64_t entrySize;
	}  __attribute__((packed));

	
	static_assert(sizeof(SectionHeader) == 64, "64bit Elf Section Header size wrong");

	
	Header * getHeader() const {
		return reinterpret_cast<Header*>(data);
	}


 public:
	
	explicit Elf(const char * file);

	explicit Elf(size_t size);

	~Elf();

	bool write(const char * file);

	
	bool isValid();
};
