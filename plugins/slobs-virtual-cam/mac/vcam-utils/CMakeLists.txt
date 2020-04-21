cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)
project(vcam-utils)

#############################
# Project source
#############################
SET(PROJECT_SOURCE
	"${PROJECT_SOURCE_DIR}/src/logger.cpp"
)

SET(PROJECT_INCLUDE_PATHS
	"${CMAKE_SOURCE_DIR}/source/"
	"${PROJECT_SOURCE_DIR}/include/"
)

ADD_LIBRARY(${PROJECT_NAME} STATIC
	${PROJECT_SOURCE}
)

TARGET_INCLUDE_DIRECTORIES(
	${PROJECT_NAME}
	PUBLIC ${PROJECT_INCLUDE_PATHS}
)