#!/usr/bin/env sh

INC="-I/usr/inlcude/SDL3 -I./glad/include/ -I../"
SRC="glad/src/glad.c main.c"
FLAGS="-Wall -std=c89 -lm -g -lGL -lSDL3"

gcc $FLAGS $SRC $INC -o app
