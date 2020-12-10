#!/bin/sh
mkdir -p obs-studio/build
cd obs-studio/build
cmake -DENABLE_SPARKLE_UPDATER=ON -DDISABLE_UI=true -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 -DQTDIR="/tmp/obsdeps" -DSWIGDIR="/tmp/obsdeps" -DDepsPath="/tmp/obsdeps" -DVLCPath="../../obs-build-dependencies/vlc-3.0.8" -DBUILD_BROWSER=ON -DWITH_RTMPS=ON -DCEF_ROOT_DIR="../../obs-build-dependencies/cef_binary_3770_macosx64" -DDISABLE_PYTHON=ON -DCMAKE_INSTALL_PREFIX="/Users/diez/projects/streamlabs/obs-studio-node/build/libobs-src" ..
cd ../../
