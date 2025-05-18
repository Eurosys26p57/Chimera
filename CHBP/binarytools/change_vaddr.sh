#!/bin/bash


if [ "$
    echo "Usage: $0 <ELF-file> AIMADDR <SECTION-NAME>"
    exit 1
fi

ELF_FILE=$1



SECTION_NAME="$3"


TEXTCOPY_OFFSET=$(readelf -S "$ELF_FILE" | awk -v section_name=".$SECTION_NAME" '
    /^ *\[.*\] +/ {
        if ($2 == section_name) {
            print "0x"$5
        }
    }'
)

if [ -z "$TEXTCOPY_OFFSET" ]; then
    echo "$SECTION_NAME section not found in $ELF_FILE"
    exit 1
fi

echo "The file offset for .textcopy section is: $TEXTCOPY_OFFSET"


VADDR=$(readelf -l "$ELF_FILE" | awk -v offset="$TEXTCOPY_OFFSET" '
    /LOAD/ {
        if (strtonum(offset) == strtonum($2)) {
            print $3
            exit
        }
    }
')


if [ -z "$VADDR" ]; then
    echo "No virtual address found for .textcopy section with offset $TEXTCOPY_OFFSET"
else
    echo "The virtual address for .textcopy section is: $VADDR"
fi
elfdiet/testphad $ELF_FILE $VADDR $2
