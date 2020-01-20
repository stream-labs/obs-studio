export PATH=/usr/local/opt/ccache/libexec:$PATH

git fetch --tags

mkdir build packed_build

cd build
cmake -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 \
-DDepsPath=/tmp/obsdeps \
-DCMAKE_INSTALL_PREFIX=$PWD/../packed_build \
-DVLCPath=$PWD/../../vlc-3.0.4 \
-DENABLE_UI=false \
-DDISABLE_UI=true \
-DCOPIED_DEPENDENCIES=false \
-DCOPY_DEPENDENCIES=true \
-DENABLE_SCRIPTING=false \
-DBUILD_BROWSER=ON \
-DBROWSER_DEPLOY=ON \
-DBUILD_CAPTIONS=ON \
-DBROWSER_FRONTEND_API_SUPPORT=false \
-DBROWSER_PANEL_SUPPORT=false \
-DUSE_UI_LOOP=true \
-DCEF_ROOT_DIR=$PWD/../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64 ..

cd ..

cmake --build build --target install --config %BuildConfig% -v
echo $PWD
# Install Chromium Embedded Framework
cd packed_build
echo $PWD
mkdir Frameworks
echo $PWD
cd ../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64/Release
echo $PWD

cp -R \
./Chromium\ Embedded\ Framework.framework \
../../../packed_build/Frameworks/Chromium\ Embedded\ Framework.framework

cp ./Chromium\ Embedded\ Framework.framework/Libraries/libEGL.dylib \
$PWD/../packed_build/obs-plugins/libEGL.dylib

cp ./Chromium\ Embedded\ Framework.framework/Libraries/libGLESv2.dylib \
$PWD/../packed_build/obs-plugins/libGLESv2.dylib

cp ./Chromium\ Embedded\ Framework.framework/Libraries/libswiftshader_libEGL.dylib \
$PWD/../packed_build/obs-plugins/libswiftshader_libEGL.dylib

cp ./Chromium\ Embedded\ Framework.framework/Libraries/libswiftshader_libGLESv2.dylib\
$PWD/../packed_build/obs-plugins/libswiftshader_libGLESv2.dylib

# Apply new Framework load path
sudo install_name_tool -change \
    @executable_path/../Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    @executable_path/Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    $PWD/../packed_build/obs-plugins/obs-browser.so

sudo install_name_tool -change \
    @executable_path/../Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    @executable_path/Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    $PWD/../packed_build/obs-plugins/obs-browser-page