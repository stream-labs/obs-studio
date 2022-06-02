#pragma once

#include "MediaSoupInterface.h"

#include <obs-module.h>
#include <iostream>
#include <third_party/libyuv/include/libyuv.h>

static void createInterfaceObject(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& routerRtpCapabilities_Raw, calldata_t* cd);
static bool createVideoProducerTrack(const std::string& roomId, calldata_t* cd);
static bool createAudioProducerTrack(const std::string& roomId, calldata_t* cd);
static bool createProducerTrack(std::shared_ptr<MediaSoupInterface> soupClient, const std::string& kind, calldata_t* cd);
static bool createConsumer(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, const std::string& kind, calldata_t* cd);
static bool createSender(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, calldata_t* cd);
static bool createReceiver(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, calldata_t* cd);

static void func_routerRtpCapabilities(void* data, calldata_t* cd);
static void func_send_transport_response(void* data, calldata_t* cd);
static void func_receive_transport_response(void* data, calldata_t* cd);
static void func_video_consumer_response(void* data, calldata_t* cd);
static void func_audio_consumer_response(void* data, calldata_t* cd);
static void func_create_audio_producer(void* data, calldata_t* cd);
static void func_create_video_producer(void* data, calldata_t* cd);
static void func_produce_result(void* data, calldata_t* cd);
static void func_connect_result(void* data, calldata_t* cd);
static void func_stop_receiver(void* data, calldata_t* cd);
static void func_stop_sender(void* data, calldata_t* cd);
static void func_stop_consumer(void* data, calldata_t* cd);
static void func_change_playback_volume(void* data, calldata_t* cd);
static void func_get_playback_devices(void* data, calldata_t* cd);
static void func_change_playback_device(void* data, calldata_t* cd);
static void func_toggle_direct_audio_broadcast(void* data, calldata_t* cd);
