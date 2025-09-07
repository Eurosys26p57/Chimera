/*! \file
 *  \brief Enthält die Klasse Elf
 */

#pragma once

#include <cstdint>
#include <cstddef>


/*! \brief Die Klasse Elf implementiert einen Parser und Loader für eine
 *         Untermenge des Executable and Linkable Formats.
 *
 *  \note
 *  Es werden nur Executable 32 bit ELF für x86 unterstützt.
 *  Eine Erweiterung auf Relokation ist für unseren Anwendungsfall nicht
 *  notwendig.
 */
class Elf {
 public:
	// Zeiger auf Inhalt der ELF-Datei
	char * data;
	// Länge der ELF-Datei
	size_t len;

 public:
	// Allgemeiner ELF Kopf
	struct Header {
		// Magische Kennung (0x7f "ELF")
		char identMagic[4];

		/* Beschreibung des verwendeten ELF Formats */
		// Breite
		enum : uint8_t {
			classElf32 = 1,
			classElf64 = 2,
		} identClass;

		// Byte Reihenfolge
		enum : uint8_t {
			DataLittleEndian = 1,
			DataBigEndian    = 2,
		} identData;

		// Version
		enum : uint8_t {
			VersionNone = 0,
			VersionCurrent = 1,
		} identVersion;

		// Architekturabhängige Attribute (wird nicht verwendet)
		uint8_t abi;

		// Füllbits
		uint64_t padding;

		/* Eigentlicher Inhalt */
		// Programmtyp (wir unterstützen nur Executable)
		enum : uint16_t {
			TypeRelocatable = 1,
			TypeExecutable  = 2,
			TypeShared      = 3,
			TypeCore        = 4,
		} type;

		// Architektur (wir unterstützen nur x86)
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

		// Version
		uint32_t version;

		/* Folgendes gilt nur für selbe Architektur */

		// Einsprungsadresse
		uintptr_t entry;

		// Abstand bis zum ersten Programmkopf
		uintptr_t programHeaderOffset;
		// Abstand bis zur ersten Sektionskopf
		uintptr_t sectionHeaderOffset;

		// Architekturabhängige Attribute (wird ignoriert)
		uint32_t archFlags;

		// Größe des ELF Kopfes
		uint16_t elfHeaderSize;
		// Größe eines einzelnen Programmkopfeintrags
		uint16_t programHeaderEntrySize;
		// Anzahl der Programmkopfeinträge (liegen praktischerweise hintereinander)
		uint16_t programHeaderEntries;

		// Größe eines einzelnen Sektionkopfeintrags
		uint16_t sectionHeaderEntrySize;
		// Anzahl der Sektionkopfeinträge (liegen hintereinander)
		uint16_t sectionHeaderEntries;
		// Index der Sektionsnamen als Nullterminierte Zeichenketten
		uint16_t sectionHeaderNamesIndex;

		/*! \brief Prüfung der magischen Kennung
		 *
		 *  \return 'true' wenn die magische Kennung korrekt ist
		 */
		bool isValid();

		/*! \brief Prüfung, ob dieses Format von uns unterstützten wird.
		 *
		 *  \return 'true' wenn unterstützt
		 */
		bool isSupported();
	} __attribute__((packed));

	// Statische Prüfung der Größe des ELF Kopfes
// Bei 32 bit: static_assert(sizeof(Header) == 52, "32bit Elf Header size wrong");
	static_assert(sizeof(Header) == 64, "64bit Elf Header size wrong");

	/*! \brief Kopf für ein Programmsegment
	 */
	struct ProgramHeader {
		/* Sektionstyp: Wir unterstützen nur 'TypeLoad'.
		 * 'TypeDynamic' und 'TypeInterpret' werden mit Fehler quittiert,
		 * alle anderen ignoriert.
		 */
		enum : uint32_t {
			TypeUnused    = 0,
			TypeLoad      = 1,
			TypeDynamic   = 2,
			TypeInterpret = 3,
			TypeNote      = 4,
			TypeShLib     = 5,
			TypePhdr      = 6,
		} type;

		// Attribute, Seitenattribute können entsprechend gesetzt werden.
		enum : uint32_t {
			FlagExecutable   = 1,
			FlagWriteable    = 2,
			FlagReadable     = 4,
		} flags;

		// Abstand des eigentlichen Programmsegments (der zu diesem Kopf gehört) vom Start der Datei
		uintptr_t offset;

		// Zieladrese im virtuellen Speicher
		uintptr_t virtualAddress;

		// Sollte in unserem Fall identisch zur virtuellen Adresse sein
		uintptr_t physicalAddress;

		// Größe der Programmsektion in der Datei
		uint64_t fileSize;

		// Zu reservierender Speicher, größer oder gleich fileSize (Rest wird genullt)
		uint64_t memorySize;

		// Ausrichtung (wird von uns ignoriert)
		uint64_t alignment;
	} __attribute__((packed));

	// Statische Prüfung der Größe eines Programmkopfes
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

		// Zieladrese im virtuellen Speicher
		uintptr_t virtualAddress;

		// Zieladrese im virtuellen Speicher
		uintptr_t offset;

		uint64_t size;
		uint32_t link;
		uint32_t info;
		uint64_t alignment;
		uint64_t entrySize;
	}  __attribute__((packed));

	// Statische Prüfung der Größe eines Programmkopfes
	static_assert(sizeof(SectionHeader) == 64, "64bit Elf Section Header size wrong");

	/*! \brief ELF Kopf
	 *  \return Zeiger auf den ELF Kopf
	 */
	Header * getHeader() const {
		return reinterpret_cast<Header*>(data);
	}


 public:
	/*! \brief Konstruktor
	 *
	 *  \param data Puffer mit den gesamten Inhalt der ELF-Datei
	 *  \param len Länge des Puffers
	 */
	explicit Elf(const char * file);

	explicit Elf(size_t size);

	~Elf();

	bool write(const char * file);

	/*! \brief Gültigkeitsprüfung
	 *
	 *  Es werden die magische Kennung am Anfang sowie interne Referenzen
	 *  geprüft.
	 *  \return 'true' wenn der Inhalt ein valides ELF im verarbeitbaren
	 *          Format ist.
	 */
	bool isValid();
};
