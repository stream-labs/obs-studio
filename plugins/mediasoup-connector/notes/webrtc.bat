mkdir webrtc-checkout
cd webrtc-checkout
fetch --nohooks webrtc
gclient sync
cd src
git checkout -b m94 refs/remotes/branch-heads/4606
gclient sync
gn gen out/m94 --args='is_debug=false is_component_build=false is_clang=false rtc_include_tests=true use_rtti=true use_custom_libcxx=false treat_warnings_as_errors=false'
ninja -C out/m94

RTC_DCHECK_IS_ON=0

pause