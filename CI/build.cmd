cmake ^
	-G"%CmakeGenerator%" ^
	-A x64 ^
	-T "host=x64" ^
	-H"%CefPath%" ^
	-B"%CefBuildPath%" ^
	-DCEF_RUNTIME_LIBRARY_FLAG="/MD"

cmake --build "%CefBuildPath%" --config Release

cmake ^
	-G"%CmakeGenerator%" ^
	-H"%APPVEYOR_BUILD_FOLDER%" ^
	-B"%BuildPath32%" ^
	-DCMAKE_INSTALL_PREFIX="%InstallPath%" ^
	-DENABLE_UI=false ^
	-DCOPIED_DEPENDENCIES=false ^
	-DCOPY_DEPENDENCIES=true ^
	-DENABLE_SCRIPTING=false ^
	-DCOMPILE_D3D12_HOOK=true

cmake ^
	-G"%CmakeGenerator%" ^
	-H"%APPVEYOR_BUILD_FOLDER%" ^
	-B"%BuildPath64%" ^
	-A x64 ^
	-DCMAKE_INSTALL_PREFIX="%InstallPath%" ^
	-DENABLE_UI=false ^
	-DCOPIED_DEPENDENCIES=false ^
	-DCOPY_DEPENDENCIES=true ^
	-DENABLE_SCRIPTING=false ^
	-DCEF_ROOT_DIR=%CefPath% ^
	-DCEF_WRAPPER_DIR="%CefBuildPath%\libcef_dll_wrapper\Release" ^
	-DUSE_STATIC_CRT=false ^
	-DBUILD_BROWSER=true ^
	-DCOMPILE_D3D12_HOOK=true

cmake --build "%BuildPath32%\plugins\win-capture\get-graphics-offsets" --config "%BuildConfig%" --target install
cmake --build "%BuildPath32%\plugins\win-capture\graphics-hook" --config "%BuildConfig%" --target install
cmake --build "%BuildPath32%\plugins\win-capture\inject-helper" --config "%BuildConfig%" --target install
cmake --build "%BuildPath64%" --config "%BuildConfig%" --target install