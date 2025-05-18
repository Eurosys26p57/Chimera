
#!/bin/bash


if [ "$
    echo "Usage: $0 <ELF-file> <SECTION-NAME>"
    exit 1
fi

ELF_FILE=$1
SECTION_NAME=$2








OFFSET=$(readelf -S "$ELF_FILE" | awk -v section_name=".$SECTION_NAME" '
    /^ *\[.*\] +/ {
        if ($2 == section_name) {
            getline
            print $1
        }
    }'
)


if [ -z "$OFFSET" ]; then
    echo "$2 section not found in $ELF_FILE"
else
    echo "$OFFSET"
fi
