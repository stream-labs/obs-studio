project(win-wasapi)


set(win-wasapi_HEADERS
	enum-wasapi.hpp win-wasapi-app.hpp)

set(MODULE_DESCRIPTION "OBS WASAPI module")
configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in win-wasapi.rc)
set(win-wasapi_SOURCES
	win-wasapi.cpp
	win-wasapi-app.cpp
	enum-wasapi.cpp
	plugin-main.cpp
	win-wasapi.rc)

add_library(win-wasapi MODULE
	${win-wasapi_SOURCES}
	${win-wasapi_HEADERS})

set_property(TARGET win-wasapi PROPERTY CXX_STANDARD 17)

target_link_libraries(win-wasapi
	Avrt
	libobs Mmdevapi.lib)
set_target_properties(win-wasapi PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(win-wasapi data)
