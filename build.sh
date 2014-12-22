#!/bin/sh

set -e
set -u

XCBLIBS="-lrt -lm -ldl -lxcb -lxcb-image -lxcb-icccm -lxcb-keysyms"
CPPFLAGS="-DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"
WARNFLAGS="-Wall" # "-Wno-write-strings" # "-Wall"
DEBUG_FLAGS="-DDEBUG -g"

mkdir -p build
g++ -std=c++0x ${WARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/libhandmade.so.new src/handmade.cpp ${CPPFLAGS} ${DEBUG_FLAGS}
mv -f build/libhandmade.so.new build/libhandmade.so
g++ -std=c++0x ${WARNFLAGS} -o build/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}
