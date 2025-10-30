#!/usr/bin/env bash

mkdir -p tomviz-build && cd tomviz-build
cmake -G"Ninja" -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
  -DCMAKE_INSTALL_LIBDIR:STRING=lib \
  -DParaView_DIR:PATH=../paraview-build \
  -DENABLE_TESTING:BOOL=ON \
  -DPython3_FIND_STRATEGY:STRING=LOCATION \
  ../tomviz
ninja
