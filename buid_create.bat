REM https://github.com/stream-labs/obs-studio/blob/streamlabs/slobs_CI/install-script-win.cmd


set STREAMLABS_ROOTDIR=C:\Users\fl\source\repos\streamlabs
set STREAMLABS_OBS_DEPS=C:\Users\fl\source\repos\streamlabs\dependencies\win64
set Qt5Widgets_DIR=C:\Users\fl\Qt5\5.10.1\msvc2017_64
set AMD_OLD=enc-amf_old
set AMD_URL=https://obs-studio-deployment.s3-us-west-2.amazonaws.com/%AMD_OLD%.zip


REM mkdir -p build64
cd build64
if exist %AMD_OLD%.zip (curl -kLO %AMD_URL% -f --retry 5 -z %AMD_OLD%.zip) else (curl -kLO %AMD_URL% -f --retry 5 -C -)
unzip %AMD_OLD%.zip -d %AMD_OLD%

REM The next commented also uses VLC so skip for now
REM Also need to get the ./slobs_CI/install-script-win.cmd
REM cmake -G "Visual Studio 16 2019"  -A x64 -DCMAKE_INSTALL_PREFIX="%STREAMLABS_ROOTDIR%\obs-studio-node\build\libobs-src" -DCMAKE_SYSTEM_VERSION=10.0 -DDepsPath="%STREAMLABS_OBS_DEPS%" -DVLCPath="C:\work\libs\obs_studio_dep\vlc_24" -DCEF_ROOT_DIR="C:\work\libs\obs_studio_dep\CEF_64_24\cef_binary_75.1.16_g16a67c4_chromium-75.0.3770.100_windows64_minimal"
cmake -G "Visual Studio 16 2019"  -A x64 -DCMAKE_INSTALL_PREFIX="%STREAMLABS_ROOTDIR%\obs-studio-node\build\libobs-src" -DCMAKE_SYSTEM_VERSION=10.0 ^
-DDepsPath="%STREAMLABS_OBS_DEPS%" -DENABLE_UI=false  -DDISABLE_UI=true -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true -DENABLE_SCRIPTING=false -DBUILD_CAPTIONS=false ^
-DCOMPILE_D3D12_HOOK=true -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true -DENABLE_SCRIPTING=false -DBUILD_CAPTIONS=false -DCOMPILE_D3D12_HOOK=true ^
-DBUILD_BROWSER=true -DBROWSER_FRONTEND_API_SUPPORT=false -DBROWSER_PANEL_SUPPORT=false -DBROWSER_USE_STATIC_CRT=false -DGPU_PRIORITY_VAL=1 -DEXPERIMENTAL_SHARED_TEXTURE_SUPPORT=true ^
-DUSE_UI_LOOP=false ..

REM build and copy into obs-studio-node CMAKE_INMSTALL_PREIFX location

cmake --build . --target install
echo "Moving enc-amf_old to OSN install path.."
move %CD%\%AMD_OLD% %STREAMLABS_ROOTDIR%\obs-studio-node\build\libobs-src\