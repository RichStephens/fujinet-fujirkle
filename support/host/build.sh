#!/bin/sh
# Host build of the client's drawing logic against logging stubs.
set -e
cd "$(dirname "$0")/../.."
gcc -std=c99 -g -O0 -Wall -Wno-unused-parameter \
    -DCOCO3 \
    -include support/host/host_vars.h \
    -I support/host -I src -I src/platform-specific \
    src/gamelogic.c support/host/host_stubs.c \
    -o support/host/fujirkle-host
