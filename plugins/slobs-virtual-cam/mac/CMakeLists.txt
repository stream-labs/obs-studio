#cmake_minimum_required(VERSION 3.0)
project(slobs-virtual-cam)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_library(COREAUDIO CoreAudio)
find_library(AUDIOUNIT AudioUnit)
find_library(COREFOUNDATION CoreFoundation)
find_library(IOSURF IOSurface)
find_library(COCOA Cocoa)
find_library(GLKIT GLKit)

add_subdirectory(vcam-assistant)
add_subdirectory(vcam-ipc)
add_subdirectory(vcam-plugin)
add_subdirectory(vcam-utils)

include_directories(
	${CMAKE_SOURCE_DIR}/libobs
	"${vcam-ipc_SOURCE_DIR}/include/"
	${COREAUDIO}
	${AUDIOUNIT}
	${COREFOUNDATION}
	${IOSURF}
	${COCOA}
	${GLKIT}
	${FFMPEG_INCLUDE_DIRS}
)

set(PROJECT_SOURCE
	"${PROJECT_SOURCE_DIR}/vcam-output.cpp"
	"${PROJECT_SOURCE_DIR}/vcam.mm"
	"${PROJECT_SOURCE_DIR}/vcam.hpp"
)

add_library(${PROJECT_NAME} MODULE
	${PROJECT_SOURCE}
)

target_link_libraries(
    slobs-virtual-cam
	libobs
	vcam-ipc
	${COREAUDIO}
	${AUDIOUNIT}
	${COREFOUNDATION}
	${IOSURF}
	${COCOA}
	${GLKIT}
	${FFMPEG_LIBRARIES}
)

install_obs_plugin_with_data(slobs-virtual-cam data)