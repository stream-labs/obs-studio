#pragma once

#include "MediaSoupInterface.h"

#include <obs-module.h>
#include <iostream>
#include <third_party/libyuv/include/libyuv.h>

struct ConnectorFrontApi
{
	static void func_load_device(void* data, calldata_t* cd);
	static void func_create_send_transport(void* data, calldata_t* cd);
	static void func_create_receive_transport(void* data, calldata_t* cd);
	static void func_video_consumer_response(void* data, calldata_t* cd);
	static void func_audio_consumer_response(void* data, calldata_t* cd);
	static void func_create_audio_producer(void* data, calldata_t* cd);
	static void func_create_video_producer(void* data, calldata_t* cd);
	static void func_produce_result(void* data, calldata_t* cd);
	static void func_connect_result(void* data, calldata_t* cd);
	static void func_stop_receiver(void* data, calldata_t* cd);
	static void func_stop_sender(void* data, calldata_t* cd);
	static void func_stop_consumer(void* data, calldata_t* cd);
};

struct ConnectorFrontApiHelper
{
	static void createInterfaceObject(const std::string& routerRtpCapabilities_Raw, calldata_t* cd);
	static bool createVideoProducerTrack(calldata_t* cd, const std::string& input);
	static bool createAudioProducerTrack(calldata_t* cd, const std::string& input);
	static bool createProducerTrack(const std::string& kind, calldata_t* cd, const std::string& input);
	static bool createConsumer(MediaSoupInterface::ObsSourceInfo& obsSourceInfo, const std::string& params, const std::string& kind, calldata_t* cd);
	static bool createSender(const std::string& params, calldata_t* cd);
	static bool createReceiver(const std::string& params, calldata_t* cd);

	static bool onConnect(const std::string& clientId, const std::string& transportId, const json& dtlsParameters);
	static bool onProduce(const std::string& clientId, const std::string& transportId, const std::string& kind, const json& rtpParameters, std::string& output_value);

private:
	static clock_t getWaitTimeoutDuration() { return clock_t(30000); }
};
