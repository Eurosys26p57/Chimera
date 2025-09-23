#!/bin/bash
./chimera_insert_count.sh   
./indirectjump.sh   
./spec17-count.sh
mkdir result
mv spec17-count.csv result
mv chimera_insert_count.txt result  
mv indirectjump.txt  result
