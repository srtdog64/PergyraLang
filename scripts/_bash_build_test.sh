#!/bin/bash
set -e
cd /e/PergyraLang
echo 'int main(void){return 0;}' > ht.c
gcc -c ht.c -o ht.o
echo "compile ok"
ls -la ht.o
gcc ht.o -o ht.exe
echo "link ok"
ls -la ht.exe
rm -f ht.c ht.o ht.exe
echo "done"
