#!/bin/bash
cd "$(dirname "$0")"
export LD_PRELOAD=$LD_PRELOAD:./minqlx.so
LD_LIBRARY_PATH="./linux64:$LD_LIBRARY_PATH" exec ./qzeroded.x64 "$@"
