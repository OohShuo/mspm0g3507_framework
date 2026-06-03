#!/bin/bash

mkdir -p build
cd build
cmake --graphviz=framework.dot -G Ninja ..
ninja -j$(nproc)

cd ..