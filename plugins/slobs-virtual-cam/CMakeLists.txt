cmake_minimum_required(VERSION 3.0)
project(slobs-virtual-cam)

if (WIN32)
    add_subdirectory(windows)
elseif (APPLE)
    add_subdirectory(mac)
endif ()