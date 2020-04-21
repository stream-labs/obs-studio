cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)
project(vcam-assistant)

find_library(COREFOUNDATION CoreFoundation)

#############################
# Project source
#############################
SET(PROJECT_SOURCE
	"${PROJECT_SOURCE_DIR}/main.cpp"
	"${PROJECT_SOURCE_DIR}/assistant.hpp"
	"${PROJECT_SOURCE_DIR}/assistant.cpp"
)

SET(PROJECT_INCLUDE_PATHS
	"${CMAKE_SOURCE_DIR}/source/"
	${COREFOUNDATION}
	"${vcam-utils_SOURCE_DIR}/include/"
)

ADD_EXECUTABLE(
	${PROJECT_NAME}
    ${PROJECT_SOURCE}
)

TARGET_LINK_LIBRARIES(
	${PROJECT_NAME}
	${COREFOUNDATION}
	vcam-utils
)

TARGET_INCLUDE_DIRECTORIES(
	${PROJECT_NAME}
	PUBLIC ${PROJECT_INCLUDE_PATHS}
)

set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
		CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
		CMAKE_C_FLAGS_RELWITHDEBINFO
        )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()

install(TARGETS vcam-assistant RUNTIME DESTINATION "./data/obs-plugins/slobs-virtual-cam" PERMISSIONS ${PROGRAM_PERMISSIONS_DEFAULT} COMPONENT Runtime)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/" DESTINATION "./data/obs-plugins/slobs-virtual-cam" FILES_MATCHING PATTERN "*.plist")