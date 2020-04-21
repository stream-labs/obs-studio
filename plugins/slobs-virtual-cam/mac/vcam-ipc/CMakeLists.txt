cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)
project(vcam-ipc)

find_library(COREFOUNDATION CoreFoundation)

#############################
# Project source
#############################
SET(PROJECT_SOURCE
	"${PROJECT_SOURCE_DIR}/src/vcam-ipc.mm"
	"${PROJECT_SOURCE_DIR}/src/vcam-ipc-client.mm"
	"${PROJECT_SOURCE_DIR}/src/vcam-ipc-server.mm"
)

SET(PROJECT_INCLUDE_PATHS
	"${CMAKE_SOURCE_DIR}/source/"
	${COREFOUNDATION}
	"${PROJECT_SOURCE_DIR}/include/"
	"${vcam-utils_SOURCE_DIR}/include/"
)

ADD_LIBRARY(${PROJECT_NAME} STATIC
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