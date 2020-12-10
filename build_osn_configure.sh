#!/bin/sh
cd obs-studio-node
mkdir -p build
cd build
cmake .. -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 -DCMAKE_INSTALL_PREFIX=/Users/diez/projects/streamlabs/streamlabs-obs/node_modules/obs-studio-node/  -DSTREAMLABS_BUILD=OFF  -DLIBOBS_BUILD_TYPE=debug  -DCMAKE_BUILD_TYPE=Debug -G Xcode
cd ../../

