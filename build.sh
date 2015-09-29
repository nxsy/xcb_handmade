#!/bin/sh

set -e
set -u

XCBLIBS="-lrt -lm -ldl -lxcb -lxcb-image -lxcb-icccm -lxcb-keysyms -lxcb-randr -lasound"
CPPFLAGS="-pthread -I../iaca/include/ -DHANDMADE_PROFILE=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"
WARNFLAGS="-Wall -Wno-unused-variable -Wno-unused-but-set-variable\
	 -Wno-write-strings -Wno-unused-function -Wno-strict-aliasing -Wno-switch"
GAMEWARNFLAGS="${WARNFLAGS} -Wno-sign-compare"
DEBUG_FLAGS="-DDEBUG -g -DHANDMADE_SLOW=1 -DHHXCB_SLOW=1"
OPT_FLAGS="-Ofast"


buildType=""
dir=`basename $PWD`
if [ "$dir" == "debug" ]
then
	buildType="debug"
	cd ../..
fi

if [ "$dir" == "opt" ]
then
	buildType="opt"
	cd ../..
fi

dir=`basename $PWD`
if [ "$dir" == "src" ]
then
	cd ..
fi

dir=`basename $PWD`
if [ "$dir" != "xcb_handmade" ]
then
	exit
fi

mkdir -p build

if [ "$buildType" != "opt" ]
then

### Debug build

mkdir -p build/debug
## Asset file builder build
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=0 ${WARNFLAGS} -o build/debug/multiplatform_test_asset_builder src/multiplatform_test_asset_builder.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS} -lX11
## Optimized renderer build
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=1 ${GAMEWARNFLAGS} -c src/handmade_optimized.cpp -o build/debug/handmade_optimized.o ${CPPFLAGS} -Ofast -fPIC
## Shared library build
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=0 ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/debug/libhandmade.so.new src/handmade.cpp build/debug/handmade_optimized.o ${CPPFLAGS} ${DEBUG_FLAGS}
## Overwrite the old shared library with the new one
mv -f build/debug/libhandmade.so.new build/debug/libhandmade.so

## Alternate shared library build
#g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libalternate.so.1 -fPIC -o build/debug/libalternate.so.new src/alternate/alternate.cpp ${CPPFLAGS} ${DEBUG_FLAGS}
#mv -f build/debug/libalternate.so.new build/debug/libalternate.so

## Platform code build
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=2 ${WARNFLAGS} -o build/debug/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}

## Alternate platform code build
#g++ -DGAME_CODE_FILENAME=libalternate.so -std=c++0x ${WARNFLAGS} -o build/debug/xcb_alternate src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}

fi

if [ "$buildType" != "debug" ]
then

### Optimized build

mkdir -p build/opt
## Asset file builder build
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=0 ${WARNFLAGS} -o build/opt/multiplatform_test_asset_builder src/multiplatform_test_asset_builder.cpp ${CPPFLAGS} ${XCBLIBS} -lX11
## Optimized renderer
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=1 ${GAMEWARNFLAGS} -c src/handmade_optimized.cpp -o build/opt/handmade_optimized.o ${CPPFLAGS} -Ofast -fPIC
## Shared library build
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=0 ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/opt/libhandmade.so.new src/handmade.cpp build/debug/handmade_optimized.o ${CPPFLAGS} ${OPT_FLAGS}
## Overwrite the old shared library with the new one
mv -f build/opt/libhandmade.so.new build/opt/libhandmade.so

## Alternate shared library build
#g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libalternate.so.1 -fPIC -o build/opt/libalternate.so.new -I src src/alternate/alternate.cpp ${CPPFLAGS} ${OPT_FLAGS}
#mv -f build/opt/libalternate.so.new build/opt/libalternate.so

## Platform code build
g++ -std=c++0x -DTRANSLATION_UNIT_INDEX=2 ${WARNFLAGS} -o build/opt/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${OPT_FLAGS}

## Alternate platform code build
#g++ -DGAME_CODE_FILENAME=libalternate.so -std=c++0x ${WARNFLAGS} -o build/opt/xcb_alternate src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${OPT_FLAGS}

fi
