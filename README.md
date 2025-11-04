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

Currently supports:

- Pebble Classic (aplite)
- Pebble Time (basalt)
- Pebble 2 (diorite)


Buliding and compile-time options
---------------------------------

To build this watch face you will need the Pebble SDK 4.9.  Follow the
developer guide here:

https://developer.rebble.io/sdk/

After following the instructions to download and install the Pebble SDK and
ARM toolchain, run the command

    $ pebble build
    
in the pebble-bcdface directory to build the project.


See also
--------

- [BinaryPebble], another binary watch face for Pebble firmware 2.0


[Screenshot]: screenshot.png
[binary coded decimal]: http://en.wikipedia.org/wiki/Binary-coded_decimal
[CloudPebble]: https://cloudpebble.net/
[BinaryPebble]: https://github.com/noahsmartin/BinaryPebble
