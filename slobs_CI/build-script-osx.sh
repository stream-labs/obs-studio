export PATH=/usr/local/opt/ccache/libexec:$PATH

git fetch --tags

mkdir build packed_build

cd build
cmake -DDepsPath=/tmp/obsdeps \
-DCMAKE_INSTALL_PREFIX=$PWD/../packed_build \
-DVLCPath=$PWD/../../vlc-3.0.4 \
-DENABLE_UI=false \
-DDISABLE_UI=true \
-DCOPIED_DEPENDENCIES=false \
-DCOPY_DEPENDENCIES=true \
-DBUILD_BROWSER=ON \
-DUSE_UI_LOOP=true \
-DCEF_ROOT_DIR=$PWD/../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64 ..

cd ..

cmake --build build --target install --config %BuildConfig% -v

# Install Chromium Embedded Framework
cd packed_build
mkdir Frameworks

cp -R \
../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64/Release/Chromium\ Embedded\ Framework.framework \
Frameworks/Chromium\ Embedded\ Framework.framework

cp ../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64/Release/Chromium\ Embedded\ Framework.framework/Libraries/libEGL.dylib \
./obs-plugins/libEGL.dylib

cp ../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64/Release/Chromium\ Embedded\ Framework.framework/Libraries/libGLESv2.dylib \
./obs-plugins/libGLESv2.dylib

cp ../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64/Release/Chromium\ Embedded\ Framework.framework/Libraries/libswiftshader_libEGL.dylib \
./obs-plugins/libswiftshader_libEGL.dylib

cp ../../cef_binary_${CEF_MAC_BUILD_VERSION}_macosx64/Release/Chromium\ Embedded\ Framework.framework/Libraries/libswiftshader_libGLESv2.dylib \
./obs-plugins/libswiftshader_libGLESv2.dylib

# Apply new Framework load path
sudo install_name_tool -change \
    @executable_path/../Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    @executable_path/Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    ./obs-plugins/obs-browser.so

sudo install_name_tool -change \
    @executable_path/../Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    @executable_path/Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    ./obs-plugins/obs-browser-page