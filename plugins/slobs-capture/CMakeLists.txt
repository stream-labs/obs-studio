project(slobs-capture)


set(slobs-capture_HEADERS
	helpers.h
)

set(slobs-capture_SOURCES
	slobs-capture.cpp
	helpers.cpp
)

add_library(slobs-capture MODULE
	${slobs-capture_SOURCES}
	${slobs-capture_HEADERS})
target_link_libraries(slobs-capture
	Dwmapi
	psapi
	libobs)

install_obs_plugin_with_data(slobs-capture data)