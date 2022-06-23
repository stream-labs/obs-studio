
set DEPS=windows-deps-2022-01-31
set DepsURL=https://obs-studio-deployment.s3-us-west-2.amazonaws.com/%DEPS%.zip
set VLCURL=https://obsproject.com/downloads/vlc.zip
set CEFURL=https://streamlabs-cef-dist.s3.us-west-2.amazonaws.com

set CefFileName=cef_binary_%CEF_VERSION%_windows_x64
set OBS_VIRTUALCAM=obs-virtualsource_32bit
set OBS_VIRTUALCAM_URL=https://obs-studio-deployment.s3-us-west-2.amazonaws.com/%OBS_VIRTUALCAM%.zip

mkdir build\deps
cd build\deps

if exist deps_bin\ (
    echo "binary dependencies already installed"
) else (
    if exist %DEPS%.zip (curl -kLO %DepsURL% -f --retry 5 -z %DEPS%.zip) else (curl -kLO %DepsURL% -f --retry 5 -C -)
    7z x %DEPS%.zip -aoa -odeps_bin
)

if exist vlc\ (
    echo "VLC already installed"
) else (
    if exist vlc.zip (curl -kLO %VLCURL% -f --retry 5 -z vlc.zip) else (curl -kLO %VLCURL% -f --retry 5 -C -)
    7z x vlc.zip -aoa -ovlc
)

if exist %OBS_VIRTUALCAM%\ (
    echo "virtual cam deps already installed"
) else (
    if exist %OBS_VIRTUALCAM%.zip (curl -kLO %OBS_VIRTUALCAM_URL% -f --retry 5 -z %OBS_VIRTUALCAM%.zip) else (curl -kLO %OBS_VIRTUALCAM_URL% -f --retry 5 -C -)
    7z x %OBS_VIRTUALCAM%.zip -aoa -o%OBS_VIRTUALCAM%
)

set CEFPATH=%CD%\CEF\%CefFileName%

if exist CEF\ (
    echo "CEF already installed"
) else (
    if exist %CefFileName%.zip (curl -kLO %CEFURL%/%CefFileName%.zip -f --retry 5 -z %CefFileName%.zip) else (curl -kLO %CEFURL%/%CefFileName%.zip -f --retry 5 -C -)
    7z x %CefFileName%.zip -aoa -oCEF

    if "%CefBuildConfig%" == "Debug" (
        cmake -G"%CMakeGenerator%" -A x64 -H%CEFPATH% -B%CEFPATH%\build -DCEF_RUNTIME_LIBRARY_FLAG="/MD" -DUSE_SANDBOX=false
    ) else (
        cmake -G"%CMakeGenerator%" -A x64 -H%CEFPATH% -B%CEFPATH%\build -DCEF_RUNTIME_LIBRARY_FLAG="/MD"
    )

    cmake --build %CEFPATH%\build --config %CefBuildConfig% --target libcef_dll_wrapper -v
)

cd ..\..

