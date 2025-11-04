BCD Face
========

A binary clock for Pebble, updated for firmware 4.9.

![Screenshot]


Overview
--------

This is a binary (actually, [binary coded decimal] or BCD) face for the
Pebble, based on the SDK version 4.9.  In addition to showing the current
time, this watch face can vibrate and display a visual notification if the
watch loses its connection with your phone.

It was created circa 2013 for the OG Pebble watch, but has been updated to
build with and run on the new SDK and firmware for the Pebble Core 2 Duo.


Buliding and compile-time options
---------------------------------

To build this watch face you will need the Pebble SDK 2.0-BETA4, available
here:

https://developer.getpebble.com/2/getting-started/

After following the instructions to download and install the Pebble SDK and
ARM toolchain, run the command

    $ pebble build
    
in the pebble-bcdface directory to build the project.

The watch face's behavior can be modified at compile time by defining or
undefining the following preprocessor macros (as seen at the top of
bcdface.c):

- `SHOW_SECONDS`: Show time in seconds

- `NOTIFY_DISCONNECT`: If defined, cause the watch to vibrate and display a
  "no phone" icon in the upper left corner of the screen if connection is
  lost with your phone

If you don't want to install the Pebble SDK on your computer, you can
directly import this project from GitHub into [CloudPebble] and perform
your compilation there.  This can be especially useful if you're running
Windows, where the Pebble SDK is not natively supported.


See also
--------

- [BinaryPebble], another binary watch face for Pebble firmware 2.0


[Screenshot]: screenshot.png
[binary coded decimal]: http://en.wikipedia.org/wiki/Binary-coded_decimal
[CloudPebble]: https://cloudpebble.net/
[BinaryPebble]: https://github.com/noahsmartin/BinaryPebble
