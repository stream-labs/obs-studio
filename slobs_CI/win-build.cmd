set CMakeGenerator=Visual Studio 16 2019
set GPUPriority=1

call slobs_CI\win-install-protobuf.cmd
call slobs_CI\/win-install-grpc.cmd
call slobs_CI\/win-install-dependency.cmd

set

echo %CD%

cmake -H. ^
         -B%CD%\build ^
         -G"%CmakeGenerator%" ^
         -A x64 ^
         -DCMAKE_SYSTEM_VERSION=10.0 ^
         -DCMAKE_INSTALL_PREFIX=%CD%\%InstallPath% ^
         -DDepsPath=%CD%\build\deps\bin_deps\win64 ^
         -DVLCPath=%CD%\build\deps\vlc ^
         -DCEF_ROOT_DIR=%CEFPATH% ^
         -DUSE_UI_LOOP=false ^
         -DENABLE_UI=false ^
         -DCOPIED_DEPENDENCIES=false ^
         -DCOPY_DEPENDENCIES=true ^
         -DENABLE_SCRIPTING=false ^
         -DGPU_PRIORITY_VAL="%GPUPriority%" ^
         -DBUILD_CAPTIONS=false ^
         -DCOMPILE_D3D12_HOOK=true ^
         -DBUILD_BROWSER=true ^
         -DBROWSER_FRONTEND_API_SUPPORT=false ^
         -DBROWSER_PANEL_SUPPORT=false ^
         -DBROWSER_USE_STATIC_CRT=false ^
         -DEXPERIMENTAL_SHARED_TEXTURE_SUPPORT=true ^
         -DCHECK_FOR_SERVICE_UPDATES=true ^
         -DProtobuf_DIR="%GRPC_DIST%" ^
         -DgRPC_DIR="%PROTOBUF_DIST%""

cmake --build %CD%\build --target install --config %BuildConfig% -v

cmake --build %CD%\build --target check_dependencies --config %BuildConfig% -v
if %errorlevel% neq 0 exit /b %errorlevel%

mkdir %CD%\%InstallPath%\data\obs-plugins\obs-virtualoutput
move %CD%\build\deps\%OBS_VIRTUALCAM% %CD%\%InstallPath%\data\obs-plugins\obs-virtualoutput\%OBS_VIRTUALCAM%