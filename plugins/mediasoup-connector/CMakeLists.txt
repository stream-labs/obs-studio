# Project configuration.
set(WEBRTC_INCLUDE_PATH "" CACHE STRING "webrtc include path")
set(WEBRTC_LIB_PATH "" CACHE STRING "webrtc .lib path")
set(MEDIASOUP_INCLUDE_PATH "" CACHE STRING "libmediasoup include path")
set(MEDIASOUP_LIB_PATH "" CACHE STRING "libmediasoup .lib path")
set(MEDIASOUP_SDP_LIB_PATH "" CACHE STRING "sdptransform .lib path")
set(MEDIASOUP_SDP_INCLUDE_PATH "" CACHE STRING "sdptransform include path")

find_package(OpenSSL)

if(NOT WEBRTC_INCLUDE_PATH)
	message(FATAL_ERROR "mediasoup-connector: WEBRTC_INCLUDE_PATH not provided")
endif()

if(NOT WEBRTC_LIB_PATH)
	message(FATAL_ERROR "mediasoup-connector: WEBRTC_LIB_PATH missing")
endif()

if(NOT MEDIASOUP_INCLUDE_PATH)
	message(FATAL_ERROR "mediasoup-connector: MEDIASOUP_INCLUDE_PATH missing")
endif()

if(NOT MEDIASOUP_LIB_PATH)
	message(FATAL_ERROR "mediasoup-connector: MEDIASOUP_LIB_PATH missing")
endif()

if(NOT MEDIASOUP_SDP_LIB_PATH)
	message(FATAL_ERROR "mediasoup-connector: MEDIASOUP_SDP_LIB_PATH missing")
endif()

# Common name for openssl
if(NOT OPENSSL_INCLUDE_DIR)
	message(FATAL_ERROR "mediasoup-connector: OPENSSL_INCLUDE_DIR missing")
endif()

foreach(var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
      endif()
    endforeach()

# ----------------
# -- webrtc LIB --
# ----------------

set(webrtc-test_SOURCES
	${WEBRTC_INCLUDE_PATH}/api/test/create_frame_generator.cc
	${WEBRTC_INCLUDE_PATH}/media/base/fake_frame_source.cc
	${WEBRTC_INCLUDE_PATH}/pc/test/fake_audio_capture_module.cc
	${WEBRTC_INCLUDE_PATH}/rtc_base/task_queue_for_test.cc
	${WEBRTC_INCLUDE_PATH}/test/frame_generator.cc
	${WEBRTC_INCLUDE_PATH}/test/frame_generator_capturer.cc
	${WEBRTC_INCLUDE_PATH}/test/frame_utils.cc
	${WEBRTC_INCLUDE_PATH}/test/test_video_capturer.cc
	${WEBRTC_INCLUDE_PATH}/test/testsupport/file_utils.cc
	${WEBRTC_INCLUDE_PATH}/test/testsupport/file_utils_override.cc
	${WEBRTC_INCLUDE_PATH}/test/testsupport/ivf_video_frame_generator.cc
	${WEBRTC_INCLUDE_PATH}/test/vcm_capturer.cc)
	
include_directories(${WEBRTC_INCLUDE_PATH})
include_directories(${WEBRTC_INCLUDE_PATH}/third_party/abseil-cpp)

add_library(webrtc-testlibs STATIC
	${webrtc-test_SOURCES})

# webrtc's warnings, we don't need to see them)
target_compile_options(webrtc-testlibs PRIVATE /wd4100 /wd4244 /wd4267)

target_link_libraries(webrtc-testlibs
	${WEBRTC_LIB_PATH})
	
set_property(TARGET webrtc-testlibs PROPERTY CXX_STANDARD 14)

# Compile definitions for libwebrtc.
target_compile_definitions(webrtc-testlibs PUBLIC
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

# ----------------------
# -- mediasoup PLUGIN --
# ----------------------

project(mediasoup-connector)
set(MODULE_DESCRIPTION "Streamlabs Join")

# WEBRTC_INCLUDE_PATH must use forward slashes
set(mediasoup-connector_SOURCES
	mediasoup-connector.cpp
	ConnectorFrontApi.h
	ConnectorFrontApi.cpp
	ConnectorFrontApiHelper.cpp
	MediaSoupTransceiver.cpp
	MediaSoupTransceiver.h
	MediaSoupInterface.cpp
	MediaSoupInterface.h
	MyProducerAudioDeviceModule.h
	MediaSoupMailbox.h
	MediaSoupMailbox.cpp
	MyFrameGeneratorInterface.cpp
	MyFrameGeneratorInterface.h
	MyLogSink.cpp
	MyLogSink.h)

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${MEDIASOUP_INCLUDE_PATH})
include_directories(${MEDIASOUP_SDP_INCLUDE_PATH})

include_directories(${WEBRTC_INCLUDE_PATH})
include_directories(${WEBRTC_INCLUDE_PATH}/third_party/abseil-cpp)

add_library(mediasoup-connector MODULE
	${mediasoup-connector_SOURCES})
	
target_compile_options(mediasoup-connector PRIVATE /wd4100)

target_link_libraries(mediasoup-connector
	libobs
	w32-pthreads
	webrtc-testlibs
	${WEBRTC_LIB_PATH}
	${MEDIASOUP_LIB_PATH}
	${MEDIASOUP_SDP_LIB_PATH})

set_target_properties(mediasoup-connector PROPERTIES FOLDER "plugins")
set_property(TARGET mediasoup-connector PROPERTY CXX_STANDARD 14)

# Compile definitions for libwebrtc.
target_compile_definitions(mediasoup-connector PUBLIC
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

set_target_properties(webrtc-testlibs PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG TRUE)
set_target_properties(mediasoup-connector PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG TRUE)

install_obs_plugin(mediasoup-connector)
