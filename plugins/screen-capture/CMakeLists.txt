project(screen-capture)


set(screen-capture_HEADERS
	helpers.h
)

set(screen-capture_SOURCES
	screen-capture.cpp
	helpers.cpp
)

add_library(screen-capture MODULE
	${screen-capture_SOURCES}
	${screen-capture_HEADERS})
target_link_libraries(screen-capture
	Dwmapi
	psapi
	libobs)

install_obs_plugin_with_data(screen-capture data)