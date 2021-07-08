# touchpad_as_touchscreen

Use your touchpad as a touchscreen!

An “add-on” to [virtual_touchscreen](https://github.com/vi/virtual_touchscreen)

touchpad_as_touchscreen can be used with virtual_touchscreen or on a device with a touchscreen.

## virtual_touchscreen

Simple virtual input device for testing things in Linux. Creates a character device and an input device.

![screenshot](screenshot.png).

# Building

## `virtual_touchpad` module

Building for current kernel:

    make

Building for custom kernel (from a configured kernel directory):

    make modules M=/path/to/virtual_touchscreen/

## `touchpad_as_touchscreen`

Building `touchpad_as_touchscreen`:

```
make touchpad_as_touchscreen
```

This will result in the `tpats` binary

## application

To test `virtual_touchscreen` module,use run `virtual_touchscreen.clj` or just use pre-built `virtual_touchscreen.jar` from Github releases

# Using virtual_touchscreen module

## Some testing

```
# insmod virtual_touchscreen.ko
# dmesg | grep virtual_touchscreen
virtual_touchscreen: Major=250
# cat /dev/virtual_touchscreen
Usage: write the following commands to /dev/virtual_touchscreen:
    x num  - move to (x, ...)
    y num  - move to (..., y)
    d 0    - touch down
    u 0    - touch up
    s slot - select multitouch slot (0 to 9)
    a flag - report if the selected slot is being touched
    e 0   - trigger input_mt_report_pointer_emulation
    X num - report x for the given slot
    Y num - report y for the given slot
    S 0   - sync (should be after every block of commands)
    M 0   - multitouch sync
    T num - tracking ID
    also 0123456789:; - arbitrary ABS_MT_ command (see linux/input.h)
  each command is char and int: sscanf("%c%d",...)
  <s>x and y are from 0 to 1023</s> Probe yourself range of x and y
  Each command is terminated with '\n'. Short writes == dropped commands.
  Read linux Documentation/input/multi-touch-protocol.txt to read about events

1# printf 'x 200\ny 300\nS 0\n' > /dev/virtual_touchscreen
1# printf 'd 0\nS 0\n' > /dev/virtual_touchscreen
1# printf 'u 0\nS 0\n' > /dev/virtual_touchscreen

2# hd /dev/input/event11 # or whatever udev assigns
# printf 'x 200\ny 300\nS 0\n' > /dev/virtual_touchscreen
# printf 'd 0\nS 0\n' > /dev/virtual_touchscreen
# printf 'u 0\nS 0\n' > /dev/virtual_touchscreen
```

And events should flow from the newly created input device:

```
# hd /dev/input/event11 # or whatever udev assigns
00000000  df 32 48 4f a6 10 02 00  03 00 00 00 c8 00 00 00  |.2HO............|
00000010  df 32 48 4f ab 10 02 00  03 00 01 00 2c 01 00 00  |.2HO........,...|
00000020  df 32 48 4f bf 10 02 00  00 00 00 00 00 00 00 00  |.2HO............|
00000030  e3 32 48 4f af af 09 00  01 00 4a 01 01 00 00 00  |.2HO......J.....|
00000040  e3 32 48 4f bc af 09 00  00 00 00 00 00 00 00 00  |.2HO............|
00000050  e7 32 48 4f 3d bb 05 00  01 00 4a 01 00 00 00 00  |.2HO=.....J.....|
00000060  e7 32 48 4f 50 bb 05 00  00 00 00 00 00 00 00 00  |.2HOP...........|
```

## GUI

There is a GUI application that can also provide data for virtual touchscreen: `virtual_touchscreen.clj`. ([pre-built bundled version](https://vi-server.org/pub/virtual_touchscreen.jar); SHA256=917698e287e1b707e09c3040d6347f5f041d7a60fef0a6f5e51c2b93ccd39f3c, also available on Github Releases)

It listens port 9494 and provides virtual_touchscreen input for connected clients.

Example (checked with Clojure 1.3, may need updating):

    hostA$  java -cp clojure.jar clojure.main virtual_touchscreen.clj
    
    hostB#  nc hostA 9494 > /dev/virtual_touchscreen

# Using touchpad as touchscreen

Run 

```
./tpats
```

the program will try to detect any touchpad and touchscreen event files (/dev/input/eventX), if none are detected, the user will be prompted to choose from the list of event files

If you already know the number of *both* event files, you can use 

```
tpats [toucpad_event_number touchscreen_event_number]
```

e.g. `tpats 15 18` if touchpad is `/dev/input/event15` and touchscreen is `/dev/input/event18`

## Licence

Kernel module's licence is GPL. GUI app's license is MIT or Apache 2.0.
