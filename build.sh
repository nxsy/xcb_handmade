#!/bin/sh

set -e
set -u

## was using c++11, but it didn't like variadic macros leaving a trailing
## comma if the "__VA_ARGS__" was empty, so I switched to gnu++11
CPPSTD="-std=gnu++11"
XCBLIBS="-lrt -lm -ldl -lxcb -lxcb-image -lxcb-icccm -lxcb-keysyms -lxcb-randr -lasound -lGL -lX11 -lX11-xcb"
CPPFLAGS="-pthread -I../iaca/include/ -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"
WARNFLAGS="-Wall -Wno-unused-variable -Wno-unused-but-set-variable \
	 -Wno-write-strings -Wno-unused-function -Wno-strict-aliasing \
     -Wno-switch -fpermissive -Wno-sign-compare -Wno-format \
     -Wno-return-type -Wno-int-to-pointer-cast"
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
## Simple preprocessor
#g++ ${CPPSTD} ${WARNFLAGS} -o build/debug/simple_preprocessor src/simple_preprocessor.cpp ${CPPFLAGS} ${DEBUG_FLAGS}
#pushd ../handmade/code
#../../xcb_handmade/build/debug/simple_preprocessor > handmade_generated.h
#popd
## Asset file builder
#g++ ${CPPSTD} ${WARNFLAGS} -o build/debug/multiplatform_test_asset_builder src/multiplatform_test_asset_builder.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS} -lX11
## Shared library
g++ ${CPPSTD} ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/debug/libhandmade.so.new src/handmade.cpp ${CPPFLAGS} ${DEBUG_FLAGS}
## Overwrite the old shared library with the new one
mv -f build/debug/libhandmade.so.new build/debug/libhandmade.so

## Alternate shared library
#g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libalternate.so.1 -fPIC -o build/debug/libalternate.so.new src/alternate/alternate.cpp ${CPPFLAGS} ${DEBUG_FLAGS}
#mv -f build/debug/libalternate.so.new build/debug/libalternate.so

## Platform code
g++ ${CPPSTD} ${WARNFLAGS} -o build/debug/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}

## Alternate platform code
#g++ -DGAME_CODE_FILENAME=libalternate.so -std=c++0x ${WARNFLAGS} -o build/debug/xcb_alternate src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${DEBUG_FLAGS}

fi

if [ "$buildType" != "debug" ]
then

### Optimized build

mkdir -p build/opt
## Simple preprocessor
#g++ ${CPPSTD} ${WARNFLAGS} -o build/opt/simple_preprocessor src/simple_preprocessor.cpp ${CPPFLAGS}
#pushd ../handmade/code
#../../xcb_handmade/build/opt/simple_preprocessor > handmade_generated.h
#popd
## Asset file builder
#g++ ${CPPSTD} ${WARNFLAGS} -o build/opt/multiplatform_test_asset_builder src/multiplatform_test_asset_builder.cpp ${CPPFLAGS} ${XCBLIBS} -lX11
## Shared library
g++ ${CPPSTD} ${GAMEWARNFLAGS} -shared -Wl,-soname,libhandmade.so.1 -fPIC -o build/opt/libhandmade.so.new src/handmade.cpp ${CPPFLAGS} ${OPT_FLAGS}
## Overwrite the old shared library with the new one
mv -f build/opt/libhandmade.so.new build/opt/libhandmade.so

## Alternate shared library
#g++ -std=c++0x ${GAMEWARNFLAGS} -shared -Wl,-soname,libalternate.so.1 -fPIC -o build/opt/libalternate.so.new -I src src/alternate/alternate.cpp ${CPPFLAGS} ${OPT_FLAGS}
#mv -f build/opt/libalternate.so.new build/opt/libalternate.so

## Platform code
g++ ${CPPSTD} ${WARNFLAGS} -o build/opt/xcb_handmade src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${OPT_FLAGS}

## Alternate platform code
#g++ -DGAME_CODE_FILENAME=libalternate.so -std=c++0x ${WARNFLAGS} -o build/opt/xcb_alternate src/xcb_handmade.cpp ${CPPFLAGS} ${XCBLIBS} ${OPT_FLAGS}

fi
