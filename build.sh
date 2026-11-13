#!/bin/bash
# Build the 32-bit version.dll proxy. Needs Wine's winegcc with i386 PE support
# (clang + lld are enough; mingw is not required).
set -e
cd "$(dirname "$0")"
mkdir -p dist
winegcc -b i386-windows -shared -O2 -Wall -o dist/version.dll src/inputfix.c src/version.def -lkernel32
echo "built: $(pwd)/dist/version.dll"
