cd dist
rm -rf vcam-plugin.plugin
mkdir vcam-plugin.plugin
cd vcam-plugin.plugin

mkdir Contents
cd Contents
mkdir MacOS
mkdir Resources

cd ..
cd ..

mv data/obs-plugins/slobs-virtual-cam/libvcam-plugin.dylib ./vcam-plugin.plugin/Contents/MacOS/vcam-plugin
mv ./data/obs-plugins/slobs-virtual-cam/vcam-assistant ./vcam-plugin.plugin/Contents/Resources/vcam-assistant

mv ./data/obs-plugins/slobs-virtual-cam/Info.plist ./vcam-plugin.plugin/Contents/Info.plist

rm -rf ./data/obs-plugins/slobs-virtual-cam/include
rm -rf ./data/obs-plugins/slobs-virtual-cam/src

rm -rf /Library/CoreMediaIO/Plug-Ins/DAL/vcam-plugin.plugin
cp -a ./vcam-plugin.plugin/. /Library/CoreMediaIO/Plug-Ins/DAL/vcam-plugin.plugin

rm -rf vcam-plugin.plugin
