# Fujirkle

Fujirkle is a FujiNet-enabled game built with the MekkoGX retro build system. The current implementation targets CoCo platforms, but the project is designed so future ports can be added for Atari, Apple II, MSDOS, Coleco Adam, and other retro targets.

## Features

- FujiNet lobby support for player tables, ready state, and game joins
- Lobby username and server endpoint persistence using FujiNet appkeys
- Local server debug mode via appkey preferences
- Current CoCo build includes both CoCo 1/2 and CoCo 3 binaries

## Requirements

- GNU Make
- `cmoc`
- `decb`
- A compatible FujiNet library for the target platform (`fujinet-lib`)

## Build commands

From the repository root:

```sh
make clean
make coco-dist
```

The current CoCo-specific output is:

- `r2r/coco/fujirkle.dsk` — combined CoCo disk image
- `r2r/coco/fujirkle1.bin` — CoCo 1/2 binary
- `r2r/coco/fujirkle3.bin` — CoCo 3 binary

Single CoCo platform builds:

```sh
make coco
make coco3
```

Layout-demo builds:

```sh
make coco-demo
make coco3-demo
```

## Running the game

Boot the generated disk image on the appropriate CoCo system. The current combined disk image contains an auto-detecting loader that chooses the correct CoCo binary at boot.

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

## Future ports

This repository is currently set up for CoCo, but the underlying MekkoGX build system supports adding new platforms without changing the game’s core logic. Planned future ports may include:

- Atari
- Apple II
- MSDOS
- Coleco Adam

## Server / Api details

## Endianness
The server defaults to little-endian values for 16 bit values. To request big-endian from the server, define `QUERY_SUFFIX` as follows in `src/[platform]/vars.h`:

```c
#define QUERY_SUFFIX "&be=1"
```

Please visit the server page for more information:

https://github.com/FujiNetWIFI/servers/tree/main/fujinet-game-system/fujirkle/server#readme

## License

This project is licensed under GPL v3.
