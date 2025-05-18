
#!/bin/bash
if [ "$
    echo "Usage: $0 <ELF-file> <TRANSLATED-OBJ> <OFFSET> <OUTPUT>"
    exit 1
fi

./change_vaddr.sh $4 $3 $2
mv b.out $4
rm -f b.out
