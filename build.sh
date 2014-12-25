#!/bin/sh

set -e
set -u

XCBLIBS="-lrt -lm -ldl -lxcb -lxcb-image -lxcb-icccm -lxcb-keysyms"
CPPFLAGS="-DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"
WARNFLAGS="-Wall -Wno-unused-variable"
GAMEWARNFLAGS="${WARNFLAGS} -Wno-sign-compare"
DEBUG_FLAGS="-DDEBUG -g"

mkdir -p build

mkdir -p build/debug
g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/debug/libhandmade.so.new src/handmade.cpp ${CPPFLAGS} ${DEBUG_FLAGS}
mv -f build/debug/libhandmade.so.new build/debug/libhandmade.so
g++ -std=c++0x ${WARNFLAGS} -o build/debug/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}

mkdir -p build/opt
g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/opt/libhandmade.so.new src/handmade.cpp ${CPPFLAGS}
mv -f build/opt/libhandmade.so.new build/opt/libhandmade.so
g++ -std=c++0x ${WARNFLAGS} -o build/opt/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS}
