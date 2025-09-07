#!/bin/bash
if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <ELF-file> <TRANSLATED-OBJ> <OFFSET> <OUTPUT>"
    exit 1
fi
#python3 sectioncopy/addtest.py $1 $2 $4
./change_vaddr.sh $4 $3 $2
mv b.out $4
rm -f b.out
