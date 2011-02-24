#!/bin/bash

mkdir build
cd build
cp ../icons/* .
cp ../projekt7.desktop .
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME
make install
