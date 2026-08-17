# Fujirkle

Fujirkle is a FujiNet-enabled push-your-luck dice game built with the MekkoGX retro build system. It runs on the Tandy Color Computer, MS-DOS, Atari 8-bit, and Apple II.

## Features

- FujiNet lobby support for player tables, ready state, and game joins
- Lobby username and server endpoint persistence using FujiNet appkeys
- Local server debug mode via appkey preferences
- Up to four local players sharing one system
- The same tune set on every platform, matched to the CoCo 3 reference

## Supported platforms

| Platform | Screen | Output |
| --- | --- | --- |
| CoCo 1/2 | 32x24 | `r2r/coco/fujirkle.dsk` (combined) |
| CoCo 3 | 40x25 | `r2r/coco/fujirkle.dsk` (combined) |
| MS-DOS | 40x25 | `r2r/msdos/fujirkle.img` |
| Atari 8-bit | 40x26 | `r2r/atari/fujirkle.atr` |
| Apple II | 40x24 | `r2r/apple2/fujirkle.po` |

The game board uses one of two layouts, chosen by screen width rather than by
model. Forty columns get the split dice strip with 3x3 button tiles; 32 columns
get a tighter strip with vertical button words.

## Requirements

- GNU Make
- `cmoc` and `decb` for CoCo
- Open Watcom (`wcc`) and `mtools` for MS-DOS
- `cl65` (cc65) for Atari and Apple II
- A compatible FujiNet library for the target platform (`fujinet-lib`)

MS-DOS builds against `fujinet-lib-experimental`, which carries the RS-232
support not yet in the stable release.

## Build commands

From the repository root:

```sh
make clean
make coco-dist
make msdos
make atari
make apple2
```

`coco-dist` builds both CoCo binaries onto one disk. It runs `make clean` first,
which removes the other platforms' output - build it before the others, or
rebuild them afterwards.

## Running the game

Boot the generated disk image on the target system. The combined CoCo disk
contains a loader that auto-detects the model and runs the correct binary. The
MS-DOS image carries an `AUTOEXEC.BAT` that launches the game.

## Fujinet behavior

- Default server endpoint: `https://fujirkle.carr-designs.com/`
- Lobby server and username data are stored using FujiNet appkeys
- Lobby support uses the registered FujiNet lobby app key for table selection
- If the debug appkey flag is `0xFF`, the client uses `http://127.0.0.1:8080/` for local server testing

## Local development

The local development endpoint is controlled by preferences stored in FujiNet appkeys. The client switches to the local server when a special debug flag is set in prefs.

A local server can be started from the related server repository with:

```sh
cd ~/servers/fujinet-game-system/fujirkle/server
go run .
```

`support/host/` compiles the shared game logic natively against stub graphics
that log every draw call, so a scripted sequence of polls can be inspected
without an emulator or a server:

```sh
./support/host/build.sh && ./support/host/fujirkle-host
```

## Server / Api details

### Endianness

The server defaults to little-endian values for 16 bit values. To request
big-endian from the server, define `QUERY_SUFFIX` as follows in
`src/[platform]/vars.h`:

```c
#define QUERY_SUFFIX "&be=1"
```

Only the CoCo is big-endian. Scores are carried as byte pairs and recombined
through `WIRE_WORD` in `src/misc.h`, which keys off `WIRE_BIG_ENDIAN` - asking
for the wrong order reads every score byte-swapped.

Please visit the server page for more information:

https://github.com/FujiNetWIFI/servers/tree/main/fujinet-game-system/fujirkle/server#readme

## License

This project is licensed under GPL v3.
