xcb_handmade
============

This is an implementation of an [XCB](http://xcb.freedesktop.org/) (X protocol
C-language binding) platform layer for [Handmade Hero](http://handmadehero.org/).

Prerequisites
-------------

Most importantly, you’ll need Handmade Hero source code for the
platform-independent game code.  Pre-order the game and get the nightly source
code.

You will need g++ (tested with g++ 4.6) or other C++ compiler.

A number of XCB libraries and headers are needed to build and run.  Here is a
partial list of Ubuntu packages needed (the dev packages include the run-time
libraries):

* libxcb1-dev - the X protocol C-language binding library
* libxcb-icccm4-dev - this is for setting the window title primarily
* libxcb-image0-dev - The image extension, needed for creating a graphics
  buffer in memory
* libxcb-keysyms1-dev - converting X keycodes into usable key symbols
* libxcb-shm0-dev - Needed for the image extension
* libxcb-util0-dev - Needed to process window events like window close.

The Advanced Linux Sound Architecture (ALSA) development packages are needed
too:

* libasound2-dev

Build process
-------------

Symlink the platform-independent Handmade Hero code files into the src
directory.

    ln -s /path/to/handmadehero/handmade* src/

I’m creating a hh/ directory and unzipping the Handmade Hero source (and a
particular day) in there.  My symlinks look like this:


lrwxrwxrwx 1 nbm nbm 23 Jan  8 16:41 src/handmade.cpp -> ../hh/code/handmade.cpp
lrwxrwxrwx 1 nbm nbm 21 Jan  8 16:41 src/handmade.h -> ../hh/code/handmade.h
lrwxrwxrwx 1 nbm nbm 32 Jan  8 16:41 src/handmade_intrinsics.h -> ../hh/code/handmade_intrinsics.h
lrwxrwxrwx 1 nbm nbm 30 Jan  8 16:41 src/handmade_platform.h -> ../hh/code/handmade_platform.h
lrwxrwxrwx 1 nbm nbm 28 Jan  8 16:41 src/handmade_random.h -> ../hh/code/handmade_random.h
lrwxrwxrwx 1 nbm nbm 28 Jan  8 16:41 src/handmade_tile.cpp -> ../hh/code/handmade_tile.cpp
lrwxrwxrwx 1 nbm nbm 26 Jan  8 16:41 src/handmade_tile.h -> ../hh/code/handmade_tile.h

Build!

    sh build.sh

Don’t forget to install the assets (separate Handmade Hero download) into the
data/ directory.  Currently that would create data/test and data/test2
directories at the moment.

You can then create symlink's to the data/test* directories from build/debug
and build/opt to be able to run the binaries in place with textures.

	cd build/debug
	ls -s ../../data/test .
	ls -s ../../data/test2 .

	cd build/opt
	ls -s ../../data/test .
	ls -s ../../data/test2 .

Run!

    build/debug/xcb_handmade
or
    build/opt/xcb_handmade


You can use WASD for movement (or the XBox 360 D-pad), and Up Arrow (or the Y
button on the XBox 360 controller) to sprint.

Implementation progress
-----------------------

We currently have parity with Handmade Hero Day 38!

Completed (at least partially) so far:

* Game memory allocation
* Graphics
* Keyboard input
* Mouse input (x/y only)
* Load and calling UpdateAndRender game code (and hot reloading)
* Audio (with ALSA, not in a great state)
* XBox360 controller support
* Frame timing and locking
* Save state, record, and replay.
* Debug platform functions (read file, write file, free memory)

Still needed:

* We’re good!

Alternate build
---------------

Included is alternate.cpp, which is built the same way the Handmade Hero code
is, but with a view to having something people can execute without the
Handmade Hero source code (not yet ready, though).

Licensing/Copyright/Author
--------------------------

Casey Muratori is the author of Handmade Hero, and this code is built by
observation of his Win32 layer.  That isn’t freely distributable.

The files in data/ were created by others.  See data/README.md for copyright
information, credits, and licensing information.

The xcb platform layer and the alternate build in src/alternate/ was written by
Neil Blakey-Milner (and others listed below), and is freely distributable in
isolation under the BSD license, as specified in the header of the relevant
files.

Jonathan Nilsson contributed the platform layer debug read/write file
functions.
