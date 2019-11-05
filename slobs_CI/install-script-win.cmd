set DepsURL=https://obs-studio-deployment.s3-us-west-2.amazonaws.com/dependencies2017.zip 
set VLCURL=https://obsproject.com/downloads/vlc.zip
set CEFURL=https://s3-us-west-2.amazonaws.com/streamlabs-cef-dist
set CMakeGenerator=Visual Studio 15 2017

if exist dependencies2017.zip (curl -kLO %DepsURL% -f --retry 5 -z dependencies2017.zip) else (curl -kLO %DepsURL% -f --retry 5 -C -)
if exist vlc.zip (curl -kLO %VLCURL% -f --retry 5 -z vlc.zip) else (curl -kLO %VLCURL% -f --retry 5 -C -)
if exist cef_binary_%CEF_VERSION%_windows64_minimal.zip (curl -kLO %CEFURL%/cef_binary_%CEF_VERSION%_windows64_minimal.zip -f --retry 5 -z cef_binary_%CEF_VERSION%_windows64_minimal.zip) else (curl -kLO %CEFURL%/cef_binary_%CEF_VERSION%_windows64_minimal.zip -f --retry 5 -C -)

mkdir build

7z x dependencies2017.zip -odependencies2017
7z x vlc.zip -ovlc
7z x cef_binary_%CEF_VERSION%_windows64_minimal.zip -oCEF

set CEFPATH=%CD%\CEF\cef_binary_%CEF_VERSION%_windows64

cmake -G"%CMakeGenerator%" -A x64 -H%CEFPATH% -B%CEFPATH%\build -DCEF_RUNTIME_LIBRARY_FLAG="/MD"
cmake --build %CEFPATH%\build --config %CefBuildConfig% --target libcef_dll_wrapper -v

cmake -H. ^
         -B%CD%\build ^
         -G"%CmakeGenerator%" ^
         -A x64 ^
         -DCMAKE_INSTALL_PREFIX=%CD%\%InstallPath% ^
         -DDepsPath=%CD%\dependencies2017\win64 ^
         -DVLCPath=%CD%\vlc ^
         -DCEF_ROOT_DIR=%CEFPATH% ^
         -DENABLE_UI=false ^
         -DCOPIED_DEPENDENCIES=false ^
         -DCOPY_DEPENDENCIES=true ^
         -DENABLE_SCRIPTING=false ^
         -DBUILD_CAPTIONS=false ^
         -DCOMPILE_D3D12_HOOK=true ^
         -DBUILD_BROWSER=true ^
         -DBROWSER_FRONTEND_API_SUPPORT=false ^
         -DBROWSER_PANEL_SUPPORT=false ^
         -DBROWSER_USE_STATIC_CRT=false ^
         -DEXPERIMENTAL_SHARED_TEXTURE_SUPPORT=true

cmake --build %CD%\build --target install --config %BuildConfig% -v