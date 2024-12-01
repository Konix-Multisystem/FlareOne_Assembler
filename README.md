# Flare One Assembler

When I started trying to assemble the various flare one applications, I couldn't find an assembler compatible with the various sources I had access to. They were all designed to be built with the PDS system, which would send the code from a host computer down to the Flare One to execute. An FL1 file is just a capture of the byte
stream that would be sent to the Flare One when a program is downloaded. (With an extension byte added to let the emulator I wrote know it should be in Flare One mode).

The Assembler is crude (it may habe bugs) - the original PDS manual was not available so I had to guess at some directives, and may not support all mnemonics (it should build all currently available Flare One Samples however).

## Building

To build on windows - grab cmake and windows flex and bison [Current](https://sourceforge.net/projects/winflexbison/files/win_flex_bison3-latest.zip/download) and unpack somewhere.

Adjust the paths in the below commands to point at the executables for flex and bison respectively

```
mkdir build
cd build
cmake -DFLEX_EXECUTABLE=..\..\win_flex_bison3-latest\win_flex.exe -DBISON_EXECUTABLE=..\..\win_flex_bison3-latest\win_bison.exe ..
cmake --build .
```

## Usage

The assembler is relatively crude. By default it will assemble a single file, and produce a listing to the screen. If you need to assemble multiple files, just create a new file that includes all the files needed.

It can produce two kinds of outputs, .FL1 which are PDS style upload images, these can be thrown directly at the Slipstream emulator and it will run them. The other form is COM files (CP/M) which was the OS used on the Flare One, to run these, they needed to be put onto a disc image that can then be loaded by the emulator. 