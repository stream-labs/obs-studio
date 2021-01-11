cd obs-studio-node
mkdir -p build
cd build
cmake .. -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 -DCMAKE_INSTALL_PREFIX="C:\Users\fl\source\repos\streamlabs\streamlabs-obs\node_modules\obs-studio-node"  -DSTREAMLABS_BUILD=OFF  -DLIBOBS_BUILD_TYPE=debug  -DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 16 2019"
cd ../../

