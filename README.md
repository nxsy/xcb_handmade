xcb_handmade
============

This is an implementation of an [XCB](http://xcb.freedesktop.org/) (X protocol
C-language binding) platform layer for [Handmade Hero](http://handmadehero.org/).

Prerequisites
-------------

Most importantly, you’ll need Handmade Hero source code for the
platform-independent game code.  Pre-order the game and get the nightly source
code.

A number of XCB libraries and headers are needed to build and run. Here is a
partial list of Ubuntu packages needed (the dev packages include the run-time
libraries):

* libxcb1-dev - the X 
* libxcb-icccm4-dev - this is for setting the window title primarily
* libxcb-image0-dev - The image extension, needed for creating a graphics
  buffer in memory
* libxcb-keysyms1-dev - converting X keycodes into usable key symbols
* libxcb-shm0-dev - Needed for the image extension
* libxcb-util0-dev - Needed to process window events like window close.

In addition, you will need g++ (tested with g++ 4.6) or other C++ compiler.

Build process
-------------

Symlink handmade.h and handmade.cpp into the src directory.

    ln -s /path/to/handmadehero/handmade.h src/
    ln -s /path/to/handmadehero/handmade.cpp src/

I’m creating a hh/ directory and unzipping the Handmade Hero source (and a
particular day) in there.  My symlinks look like this:

    lrwxrwxrwx 1 nbm nbm 27 Dec 21 10:32 src/handmade.cpp -> ../hh/025/code/handmade.cpp
    lrwxrwxrwx 1 nbm nbm 25 Dec 21 10:32 src/handmade.h -> ../hh/025/code/handmade.h

Build!

    sh build.sh

Run!

    build/xcb_handmade

Implementation progress
-----------------------

I am currently working towards parity with Handmade Hero day 25.

Completed (at least partially) so far:

* Game memory allocation
* Graphics
* Keyboard input
* Mouse input (x/y only)
* Load and calling UpdateAndRender game code (and hot reloading)

Still needed:

* Audio
* Frame timing and locking
* XBox360 controller support
* Debug platform functions (read file, write file, free memory)

Licensing/Copyright/Author
--------------------------

Casey Muratori is the author of Handmade Hero, and this code is built by
observation of his Win32 layer.  That isn’t freely distributable.

The xcb platform layer was written by Neil Blakey-Milner (and in future
others?), and is freely distributable in isolation under the BSD license, as
specified in the header of the relevant files.
