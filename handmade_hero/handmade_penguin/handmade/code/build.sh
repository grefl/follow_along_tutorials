#!/bin/bash

mkdir -p ../../build
pushd ../../build
c++ ../handmade/code/sdl_handmade.cpp -o handmadehero -g `sdl2-config --cflags --libs`
popd
