#!/bin/sh

set -e
set -u

XCBLIBS="-lrt -lm -ldl -lxcb -lxcb-image -lxcb-icccm -lxcb-keysyms -lxcb-randr -lasound"
CPPFLAGS="-pthread -I../iaca/include/ -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"
WARNFLAGS="-Wall -Wno-unused-variable -Wno-unused-but-set-variable\
	 -Wno-write-strings -Wno-unused-function -Wno-strict-aliasing -Wno-switch"
GAMEWARNFLAGS="${WARNFLAGS} -Wno-sign-compare"
DEBUG_FLAGS="-DDEBUG -g -DHANDMADE_SLOW=1 -DHHXCB_SLOW=1"
OPT_FLAGS="-Ofast"

mkdir -p build

mkdir -p build/debug
## Asset file builder build
g++ -std=c++0x ${WARNFLAGS} -o build/debug/test_asset_builder src/test_asset_builder.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}
## Optimized renderer
g++ -std=c++0x ${GAMEWARNFLAGS} -c src/handmade_optimized.cpp -o build/debug/handmade_optimized.o ${CPPFLAGS} -Ofast
g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/debug/libhandmade.so.new src/handmade.cpp build/debug/handmade_optimized.o ${CPPFLAGS} ${DEBUG_FLAGS}

mv -f build/debug/libhandmade.so.new build/debug/libhandmade.so
g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libalternate.so.1 -fPIC -o build/debug/libalternate.so.new src/alternate/alternate.cpp ${CPPFLAGS} ${DEBUG_FLAGS}
mv -f build/debug/libalternate.so.new build/debug/libalternate.so
g++ -std=c++0x ${WARNFLAGS} -o build/debug/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}
g++ -DGAME_CODE_FILENAME=libalternate.so -std=c++0x ${WARNFLAGS} -o build/debug/xcb_alternate src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}

mkdir -p build/opt
## Asset file builder build
g++ -std=c++0x ${WARNFLAGS} -o build/opt/test_asset_builder src/test_asset_builder.cpp ${CPPFLAGS} ${XCBLIBS}
## Optimized renderer
g++ -std=c++0x ${GAMEWARNFLAGS} -c src/handmade_optimized.cpp -o build/opt/handmade_optimized.o ${CPPFLAGS} -Ofast
g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/opt/libhandmade.so.new src/handmade.cpp build/debug/handmade_optimized.o ${CPPFLAGS} ${OPT_FLAGS}

mv -f build/opt/libhandmade.so.new build/opt/libhandmade.so
g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libalternate.so.1 -fPIC -o build/opt/libalternate.so.new -I src src/alternate/alternate.cpp ${CPPFLAGS} ${OPT_FLAGS}
mv -f build/opt/libalternate.so.new build/opt/libalternate.so
g++ -std=c++0x ${WARNFLAGS} -o build/opt/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${OPT_FLAGS}
g++ -DGAME_CODE_FILENAME=libalternate.so -std=c++0x ${WARNFLAGS} -o build/opt/xcb_alternate src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${OPT_FLAGS}
