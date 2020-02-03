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
    @rpath/Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    $PWD/../packed_build/obs-plugins/obs-browser.so

sudo install_name_tool -change \
    @executable_path/../Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    @rpath/../Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework \
    $PWD/../packed_build/obs-plugins/obs-browser-page

# Install obs dependencies
cp -R /tmp/obsdeps/bin/. $PWD/../packed_build/bin/

cp -R /tmp/obsdeps/lib/. $PWD/../packed_build/bin/

# Change load path
sudo install_name_tool -change /tmp/obsdeps/bin/libavcodec.58.dylib @executable_path/libavcodec.58.dylib $PWD/../packed_build/bin/libobs.0.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libavformat.58.dylib @executable_path/libavformat.58.dylib $PWD/../packed_build/bin/libobs.0.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libavutil.56.dylib @executable_path/libavutil.56.dylib $PWD/../packed_build/bin/libobs.0.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libswscale.5.dylib @executable_path/libswscale.5.dylib $PWD/../packed_build/bin/libobs.0.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libswresample.3.dylib @executable_path/libswresample.3.dylib $PWD/../packed_build/bin/libobs.0.dylib

sudo install_name_tool -change /tmp/obsdeps/bin/libavcodec.58.dylib @executable_path/libavcodec.58.dylib $PWD/../packed_build/bin/libavcodec.58.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libswresample.3.dylib @executable_path/libswresample.3.dylib $PWD/../packed_build/bin/libavcodec.58.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libavutil.56.dylib @executable_path/libavutil.56.dylib $PWD/../packed_build/bin/libavcodec.58.dylib

sudo install_name_tool -change /tmp/obsdeps/bin/libavformat.58.dylib @executable_path/libavformat.58.dylib $PWD/../packed_build/bin/libavformat.58.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libavcodec.58.dylib @executable_path/libavcodec.58.dylib $PWD/../packed_build/bin/libavformat.58.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libswresample.3.dylib @executable_path/libswresample.3.dylib $PWD/../packed_build/bin/libavformat.58.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libavutil.56.dylib @executable_path/libavutil.56.dylib $PWD/../packed_build/bin/libavformat.58.dylib

sudo install_name_tool -change /tmp/obsdeps/bin/libavutil.56.dylib @executable_path/libavutil.56.dylib $PWD/../packed_build/bin/libavutil.56.dylib

sudo install_name_tool -change /tmp/obsdeps/bin/libswscale.5.dylib @executable_path/libswscale.5.dylib $PWD/../packed_build/bin/libswscale.5.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libavutil.56.dylib @executable_path/libavutil.56.dylib $PWD/../packed_build/bin/libswscale.5.dylib

sudo install_name_tool -change /tmp/obsdeps/bin/libswresample.3.dylib @executable_path/libswresample.3.dylib $PWD/../packed_build/bin/libswresample.3.dylib
sudo install_name_tool -change /tmp/obsdeps/bin/libavutil.56.dylib @executable_path/libavutil.56.dylib $PWD/../packed_build/bin/libswresample.3.dylib

sudo install_name_tool -change /tmp/obsdeps/bin/libx264.155.dylib @executable_path/../libx264.155.dylib $PWD/../packed_build/obs-plugins/obs-x264.so

sudo install_name_tool -change /tmp/obsdeps/bin/libavcodec.58.dylib @executable_path/../libavcodec.58.dylib $PWD/../packed_build/obs-plugins/obs-ffmpeg.so
sudo install_name_tool -change /tmp/obsdeps/bin/libavfilter.7.dylib @executable_path/../libavfilter.7.dylib $PWD/../packed_build/obs-plugins/obs-ffmpeg.so
sudo install_name_tool -change /tmp/obsdeps/bin/libavdevice.58.dylib @executable_path/../libavdevice.58.dylib $PWD/../packed_build/obs-plugins/obs-ffmpeg.so
sudo install_name_tool -change /tmp/obsdeps/bin/libavutil.56.dylib @executable_path/../libavutil.56.dylib $PWD/../packed_build/obs-plugins/obs-ffmpeg.so
sudo install_name_tool -change /tmp/obsdeps/bin/libswscale.5.dylib @executable_path/../libswscale.5.dylib $PWD/../packed_build/obs-plugins/obs-ffmpeg.so
sudo install_name_tool -change /tmp/obsdeps/bin/libavformat.58.dylib @executable_path/../libavformat.58.dylib $PWD/../packed_build/obs-plugins/obs-ffmpeg.so
sudo install_name_tool -change /tmp/obsdeps/bin/libswresample.3.dylib @executable_path/../libswresample.3.dylib $PWD/../packed_build/obs-plugins/obs-ffmpeg.so