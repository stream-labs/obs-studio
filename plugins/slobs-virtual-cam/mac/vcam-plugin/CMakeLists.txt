cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)
project(vcam-plugin)

find_library(COREAUDIO CoreAudio)
find_library(COREFOUNDATION CoreFoundation)
find_library(IOSURF IOSurface)
find_library(COCOA Cocoa)
find_library(COREMEDIAIO CoreMediaIO)
find_library(IOKit IOKit)
find_library(COREMEDIA CoreMedia)
find_library(COREVIDEO CoreVideo)

#############################
# Source
#############################
SET(PROJECT_SOURCE
	"${PROJECT_SOURCE_DIR}/src/clock.cpp"
	"${PROJECT_SOURCE_DIR}/src/device.cpp"
	"${PROJECT_SOURCE_DIR}/src/object.cpp"
	"${PROJECT_SOURCE_DIR}/src/objectinterface.cpp"
	"${PROJECT_SOURCE_DIR}/src/objectproperties.cpp"
	"${PROJECT_SOURCE_DIR}/src/main.cpp"
	"${PROJECT_SOURCE_DIR}/src/plugininterface.cpp"
	"${PROJECT_SOURCE_DIR}/src/stream.cpp"
	"${PROJECT_SOURCE_DIR}/src/util.cpp"
)

SET(PROJECT_INCLUDE_PATHS
	"${CMAKE_SOURCE_DIR}/source/"
	${COREAUDIO}
	${COREFOUNDATION}
	${IOSURF}
	${COCOA}
	${COREMEDIAIO}
	${IOKit}
	${COREMEDIA}
	${COREVIDEO}
	"${PROJECT_SOURCE_DIR}/include/"
	"${vcam-utils_SOURCE_DIR}/include/"
	"${vcam-ipc_SOURCE_DIR}/include/"
)

ADD_LIBRARY(${PROJECT_NAME} SHARED
	${PROJECT_SOURCE}
)

TARGET_LINK_LIBRARIES(
	${PROJECT_NAME}
	${COREAUDIO}
	${COREFOUNDATION}
	${IOSURF}
	${COCOA}
	${COREMEDIAIO}
	${IOKit}
	${COREMEDIA}
	${COREVIDEO}
	vcam-utils
	vcam-ipc
)

TARGET_INCLUDE_DIRECTORIES(
	${PROJECT_NAME}
	PUBLIC ${PROJECT_INCLUDE_PATHS}
)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "./data/obs-plugins/slobs-virtual-cam" COMPONENT Runtime
    RUNTIME DESTINATION "./data/obs-plugins/slobs-virtual-cam" COMPONENT Runtime
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/" DESTINATION "./data/obs-plugins/slobs-virtual-cam" FILES_MATCHING PATTERN "*.plist")