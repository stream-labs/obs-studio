#include "MediaSoupClients.h"
#include "temp_httpclass.h"

#include <util/platform.h>
#include <util/dstr.h>
#include <obs-module.h>
#include <mutex>
#include <iostream>

#include <third_party/libyuv/include/libyuv.h>
#include <api/video/i420_buffer.h>

const std::string g_baseServerUrl = "https://v3demo.mediasoup.org:4443";
static void msoup_update(void* source, obs_data_t* settings);

static void getRoomFromConsole(obs_data_t* settings, obs_source_t* source)
{	
	static bool consoleAlloc = false;
	
	if (!consoleAlloc)
	{
		consoleAlloc = true;		
		AllocConsole();
		freopen("conin$","r", stdin);
		freopen("conout$","w", stdout);
		freopen("conout$","w", stderr);
		printf("Debugging Window\n\n");
	}

	printf("%s...\n", obs_source_get_name(source));

	std::string roomId;
	printf("Enter Room: ");
	std::cin >> roomId;

	obs_data_set_string(settings, "room", roomId.c_str());
	obs_source_update(source, settings);
}

static void emulate_frontend_finalize_produce(obs_data_t* settings, obs_source_t* source)
{
	const std::string roomId = obs_data_get_string(settings, "room");

	DWORD httpOut = 0;
	std::string strResponse;

	try
	{
		json data = json::parse(obs_data_get_string(settings, "produce_params"));
		
		WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + data["clientId"].get<std::string>() + "/transports/" + data["transportId"].get<std::string>() +"/producers", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
			json{	{ "kind",          data["kind"].get<std::string>()	},
				{ "rtpParameters", data["rtpParameters"].get<json>()	}}.dump());
	}
	catch (...)
	{
		
	}
	
	obs_data_set_string(settings, "produce_result", httpOut == 200 ? "true" : "false");
	obs_source_update(source, settings);
}

static void emulate_frontend_finalize_connect(obs_data_t* settings, obs_source_t* source)
{
	const std::string roomId = obs_data_get_string(settings, "room");

	DWORD httpOut = 0;
	std::string strResponse;

	try
	{
		json data = json::parse(obs_data_get_string(settings, "connect_params"));

		WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + data["clientId"].get<std::string>() + "/transports/" + data["transportId"].get<std::string>() + "/connect", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
			json{ { "dtlsParameters", data["dtlsParameters"].get<json>() } }.dump());
	}
	catch (...)
	{
		
	}
	
	obs_data_set_string(settings, "connect_result", httpOut == 200 ? "true" : "false");
	obs_source_update(source, settings);
}

static bool emulate_frontend_join_lobby(obs_data_t* settings, obs_source_t* source)
{
	const std::string roomId = obs_data_get_string(settings, "room");
	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (soupClient == nullptr)
		return false;

	const std::string version = obs_data_get_string(settings, "version");
	const std::string clientId = obs_data_get_string(settings, "clientId");
	const std::string deviceRtpCapabilities_Raw = obs_data_get_string(settings, "deviceRtpCapabilities");
	const std::string deviceSctpCapabilities_Raw = obs_data_get_string(settings, "deviceSctpCapabilities");

	json deviceRtpCapabilities;
	json deviceSctpCapabilities;

	try
	{
		deviceRtpCapabilities = json::parse(deviceRtpCapabilities_Raw);
		deviceSctpCapabilities = json::parse(deviceSctpCapabilities_Raw);
	}
	catch (...)
	{
		return false;
	}

	DWORD httpOut = 0;
	std::string strResponse;

	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "id",          clientId				},
			{ "displayName", "broadcaster"				},
			{ "device",
				{
					{ "name",    "libmediasoupclient"       },
					{ "version", version			}
				}
			},
			{ "rtpCapabilities", deviceRtpCapabilities	}}.dump());

	if (httpOut != 200)
		return false;

	try
	{
		auto roomInfo = json::parse(strResponse);
	
		if (roomInfo["peers"].empty())
			return true;
	
		if (roomInfo["peers"].begin().value().empty())
			return true;
	
		auto itrlist = roomInfo["peers"].begin().value()["producers"];
	
		for (auto& itr : itrlist)
		{
			auto kind = itr["kind"].get<std::string>();
	
			if (kind == "audio")
				obs_data_set_string(settings, "consume_audio_trackId", itr["id"].get<std::string>().c_str());
			else if (kind == "video")
				obs_data_set_string(settings, "consume_video_trackId", itr["id"].get<std::string>().c_str());
		}
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_query_rotuerRtpCapabilities(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP
	const std::string roomId = obs_data_get_string(settings, "room");
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId, "GET", "", &httpOut, 2000, strResponse);

	if (httpOut != 200)
		return nullptr;

	try
	{
		// Just parsing to verify it's actually json and not html, this is a temporay function anyway
		obs_data_set_string(settings, "rotuerRtpCapabilities", json::parse(strResponse).dump().c_str());
		obs_source_update(source, settings);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

static bool emulate_frontend_register_send_transport(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register send transport
	const std::string roomId = obs_data_get_string(settings, "room");
	const std::string clientId = obs_data_get_string(settings, "clientId");
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return false;
	
	try
	{
		// Just parsing to verify it's actually json and not html, this is a temporay function anyway
		obs_data_set_string(settings, "send_transport_response", json::parse(strResponse).dump().c_str());
		obs_source_update(source, settings);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_register_receive_transport(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string roomId = obs_data_get_string(settings, "room");
	const std::string clientId = obs_data_get_string(settings, "clientId");
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return false;
	
	try
	{
		// Just parsing to verify it's actually json and not html, this is a temporay function anyway
		obs_data_set_string(settings, "receive_transport_response", json::parse(strResponse).dump().c_str());
		obs_source_update(source, settings);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_create_video_consumer(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string roomId = obs_data_get_string(settings, "room");
	const std::string clientId = obs_data_get_string(settings, "clientId");
	const std::string receiverId = obs_data_get_string(settings, "receiverId");
	const std::string consume_video_trackId = obs_data_get_string(settings, "consume_video_trackId");
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports/" + receiverId + "/consume?producerId=" + consume_video_trackId, "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return false;
	
	try
	{
		// Just parsing to verify it's actually json and not html, this is a temporay function anyway
		obs_data_set_string(settings, "video_consumer_response", json::parse(strResponse).dump().c_str());
		obs_source_update(source, settings);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_create_audio_consumer(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string roomId = obs_data_get_string(settings, "room");
	const std::string clientId = obs_data_get_string(settings, "clientId");
	const std::string receiverId = obs_data_get_string(settings, "receiverId");
	const std::string consume_audio_trackId = obs_data_get_string(settings, "consume_audio_trackId");
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports/" + receiverId + "/consume?producerId=" + consume_audio_trackId, "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return false;
	
	try
	{
		// Just parsing to verify it's actually json and not html, this is a temporay function anyway
		obs_data_set_string(settings, "audio_consumer_response", json::parse(strResponse).dump().c_str());
		obs_source_update(source, settings);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}


/**
* Source
*/

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mediasoup-connector", "en-US")
MODULE_EXPORT const char* obs_module_description(void)
{
	return "Streamlabs Join";
}

static const char* msoup_getname(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("MediaSoupConnector");
}

// Destroy
static void msoup_destroy(void* data)
{
	if (data != nullptr)
		sMediaSoupClients->unregisterInterface(obs_data_get_string(obs_source_get_settings((obs_source_t*)data), "room"));
}

// Create
static void* msoup_create(obs_data_t* settings, obs_source_t* source)
{
	const std::string roomId = obs_data_get_string(settings, "room");
	const std::string routerRtpCapabilities_Raw = obs_data_get_string(settings, "rotuerRtpCapabilities");

	if (roomId.empty() || routerRtpCapabilities_Raw.empty())
		return nullptr;

	auto soupClient = std::make_shared<MediaSoupInterface>();
	soupClient->m_obs_source = source;

	json rotuerRtpCapabilities;
	json deviceRtpCapabilities;
	json deviceSctpCapabilities;

	try
	{
		rotuerRtpCapabilities = json::parse(routerRtpCapabilities_Raw);
	}
	catch (...)
	{
		blog(LOG_ERROR, "msoup_create json error parsing rotuerRtpCapabilities %s", routerRtpCapabilities_Raw.c_str());
		return nullptr;
	}

	// lib - Create device
	if (!soupClient->getTransceiver()->LoadDevice(rotuerRtpCapabilities, deviceRtpCapabilities, deviceSctpCapabilities))
	{
		blog(LOG_ERROR, "msoup_create LoadDevice failed error = '%s'", soupClient->getTransceiver()->PopLastError().c_str());
		return nullptr;
	}

	sMediaSoupClients->registerInterface(roomId, soupClient);

	// Callback function
	static auto onProduceFunc = [](MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const std::string& kind, const json& rtpParameters, std::string& output_value)
	{
		auto settings = obs_source_get_settings(soupClient->m_obs_source);

		if (settings == nullptr)
			return false;
		
		json data;
		data["clientId"] = clientId;
		data["transportId"] = transportId;
		data["rtpParameters"] = rtpParameters;
		data["kind"] = kind;
		obs_data_set_string(settings, "produce_params", data.dump().c_str());
		obs_source_update(soupClient->m_obs_source, settings);
		soupClient->setProduceIsWaiting(true);

		while (soupClient->isProduceWaiting() && soupClient->isThreadInProgress() && !soupClient->isDataReadyForProduce())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		
		soupClient->setProduceIsWaiting(false);

		if (soupClient->isDataReadyForProduce())
			return std::string(obs_data_get_string(settings, "produce_result")) == "true";

		return false;
	};
	
	// Callback function
	static auto onConnectFunc = [](MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const json& dtlsParameters)
	{
		auto settings = obs_source_get_settings(soupClient->m_obs_source);

		if (settings == nullptr)
			return false;
		
		json data;
		data["clientId"] = clientId;
		data["transportId"] = transportId;
		data["dtlsParameters"] = dtlsParameters;
		obs_data_set_string(settings, "connect_params", data.dump().c_str());
		obs_source_update(soupClient->m_obs_source, settings);
		soupClient->setConnectIsWaiting(true);

		while (soupClient->isConnectWaiting() && soupClient->isThreadInProgress() && !soupClient->isDataReadyForConnect())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		soupClient->setConnectIsWaiting(false);

		if (soupClient->isDataReadyForConnect())
			return std::string(obs_data_get_string(settings, "connect_result")) == "true";

		return false;
	};

	soupClient->getTransceiver()->RegisterOnProduce(onProduceFunc);	
	soupClient->getTransceiver()->RegisterOnConnect(onConnectFunc);
	
	obs_data_set_string(settings, "deviceRtpCapabilities", deviceRtpCapabilities.dump().c_str());
	obs_data_set_string(settings, "deviceSctpCapabilities", deviceSctpCapabilities.dump().c_str());
	obs_data_set_string(settings, "version", mediasoupclient::Version().c_str());
	obs_data_set_string(settings, "clientId", soupClient->getTransceiver()->GetId().c_str());
	obs_source_update(source, settings);
	return source;
}

static uint32_t msoup_width(void* data)
{
	if (auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(obs_source_get_settings((obs_source_t*)data), "room")))
	{
		if (soupClient->m_obs_scene_texture != nullptr)
			return soupClient->getTextureWidth();
	}

	return 1280;
}

static uint32_t msoup_height(void* data)
{
	if (auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(obs_source_get_settings((obs_source_t*)data), "room")))
	{
		if (soupClient->m_obs_scene_texture != nullptr)
			return soupClient->getTextureHeight();
	}

	return 720;
}

static obs_properties_t* msoup_properties(void* data)
{
	obs_properties_t* ppts = obs_properties_create();	
	return ppts;
}

// Video Render
static void msoup_video_render(void* data, gs_effect_t* e)
{
	auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(obs_source_get_settings((obs_source_t*)data), "room"));
	UNREFERENCED_PARAMETER(e);

	if (soupClient == nullptr || !soupClient->getTransceiver()->DownloadVideoReady())
		return;

	std::vector<std::unique_ptr<webrtc::VideoFrame>> frames;
	soupClient->getMailboxPtr()->pop_receieved_videoFrames(frames);

	if (!frames.empty())
	{
		// I guess just discard missed frames? Not sure what we could do with them
		// webrtc has its own thread, and this is our own thread, and webrtc notifies when it has a frame ready for immediate play, and we try to play it as soon as possible here
		// So anyway, play the most recent frame I guess, not sure yet
		auto& itr = frames[frames.size() - 1];

		// Mediasoup stuff
		rtc::scoped_refptr<webrtc::I420BufferInterface> i420buffer(itr->video_frame_buffer()->ToI420());

		if (itr->rotation() != webrtc::kVideoRotation_0) 
			i420buffer = webrtc::I420Buffer::Rotate(*i420buffer, itr->rotation());
		
		int cropWidth = std::min(i420buffer->width(), (int)msoup_width(data));
		int cropHeight = std::min(i420buffer->height(), (int)msoup_height(data));
		i420buffer = rtc::scoped_refptr<webrtc::I420BufferInterface>(i420buffer->CropAndScale(0, 0, cropWidth, cropHeight, cropWidth, cropHeight)->ToI420());

		DWORD biBitCount = 32;
		DWORD biSizeImage = i420buffer->width()*  i420buffer->height()*  (biBitCount >> 3);
		
		std::unique_ptr<uint8_t[]> image_buffer;
		image_buffer.reset(new uint8_t[biSizeImage]); // I420 to 
		libyuv::I420ToABGR(i420buffer->DataY(), i420buffer->StrideY(), i420buffer->DataU(), i420buffer->StrideU(), i420buffer->DataV(), i420buffer->StrideV(), image_buffer.get(), i420buffer->width()*  biBitCount / 8, i420buffer->width(), i420buffer->height());
		
		soupClient->initDrawTexture(i420buffer->width(), i420buffer->height());
		gs_texture_set_image(soupClient->m_obs_scene_texture, image_buffer.get(), i420buffer->width()*  4, false);
	}

	if (soupClient->m_obs_scene_texture == nullptr)
		return;

	gs_enable_framebuffer_srgb(false);
	gs_enable_blending(false);
	
	gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t* tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t* image = gs_effect_get_param_by_name(effect, "image");

	gs_effect_set_texture(image, soupClient->m_obs_scene_texture);

	const uint32_t flip = 0;
	const size_t passes = gs_technique_begin(tech);

	for (size_t i = 0; i < passes; i++)
	{
		if (gs_technique_begin_pass(tech, i))
		{
			gs_draw_sprite(soupClient->m_obs_scene_texture, flip, 0, 0);
			gs_technique_end_pass(tech);
		}
	}

	gs_technique_end(tech);
	gs_enable_blending(true);
}

static void msoup_video_tick(void* data, float seconds)
{
	auto source = (obs_source_t*)data;
	auto settings = obs_source_get_settings(source);
	auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room"));

	if (soupClient == nullptr)
		return;
	
	const bool senderReady = soupClient->getTransceiver()->SenderReady();
	const bool receiverReady = soupClient->getTransceiver()->ReceiverReady();

	if (std::string(obs_data_get_string(settings, "senderReady")) == "true" && !senderReady)
	{
		// Sender went from True to False
		//notify frontend
	}

	if (std::string(obs_data_get_string(settings, "receiverReady")) == "true" && !senderReady)
	{
		// Receiver went from True to False
		//notify frontend
	}

	obs_data_set_string(settings, "senderReady", senderReady ? "true" : "false");
	obs_data_set_string(settings, "receiverReady", receiverReady ? "true" : "false");
	obs_source_update(source, settings);

	//auto soupClient = sMedaSoupClients->getInterface(roomId);
	//UNREFERENCED_PARAMETER(d);
	//
	//if (soupClient == nullptr || !soupClient->downloadAudioReady())
	//	return;
	//
	//std::vector<std::unique_ptr<MediaSoupMailbox::SoupRecvAudioFrame>> frames;
	//soupClient->getMailboxPtr()->pop_receieved_audioFrames(frames);
	//obs_source_output_audio(soupClient->m_source, &sdata);
}

static bool createReceiver(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params)
{
	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createReceiver but !transceiverCreated", obs_module_description());
		return false;
	}

	if (soupClient->getTransceiver()->ReceiverReady())
	{
		blog(LOG_WARNING, "%s createReceiver already ready", obs_module_description());
		return false;
	}

	try
	{
		// lib - Create receiver
		auto response = json::parse(params);

		if (!soupClient->getTransceiver()->CreateReceiver(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], response["sctpParameters"]))
			return false;
	}
	catch (...)
	{
		blog(LOG_ERROR, "%s createReceiver exception", obs_module_description());
		return false;
	}
	
	obs_data_set_string(settings, "receiverId", soupClient->getTransceiver()->GetReceiverId().c_str());
	obs_source_update(source, settings);
	return true;
}

static bool createSender(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params)
{
	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createSender but !transceiverCreated", obs_module_description());
		return false;
	}

	if (soupClient->getTransceiver()->SenderReady())
	{
		blog(LOG_WARNING, "%s createSender already ready", obs_module_description());
		return false;
	}

	try
	{
		// lib - Create sender
		auto response = json::parse(params.c_str());

		if (!soupClient->getTransceiver()->CreateSender(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"]))
		{
			blog(LOG_ERROR, "%s createSender CreateSender failed, error '%s'", obs_module_description(), soupClient->getTransceiver()->PopLastError().c_str());
			return false;
		}
	}
	catch (...)
	{
		blog(LOG_ERROR, "%s createSender exception", obs_module_description());
		return false;
	}
	
	obs_data_set_string(settings, "senderId", soupClient->getTransceiver()->GetSenderId().c_str());
	obs_source_update(source, settings);
	return true;
}

static bool createConsumer(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, const std::string& kind)
{
	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createConsumer but !transceiverCreated", obs_module_description());
		return false;
	}
	
	if (!soupClient->getTransceiver()->ReceiverReady())
	{
		blog(LOG_WARNING, "%s createConsumer but receiver not ready", obs_module_description());
		return false;
	}
	
	auto func = [](MediaSoupInterface* soupClient, const std::string params, const std::string kind)
	{
		try
		{
			auto response = json::parse(params);
			auto id = response["id"].get<std::string>();
			auto producerId = response["producerId"].get<std::string>();
			auto rtpParam = response["rtpParameters"].get<json>();

			if (kind == "audio")
				soupClient->getTransceiver()->CreateAudioConsumer(id, producerId, &rtpParam);

			if (kind == "video")
				soupClient->getTransceiver()->CreateVideoConsumer(id, producerId, &rtpParam);
		}
		catch (...)
		{
			blog(LOG_ERROR, "%s createVideoConsumer exception", obs_module_description());
		}

		soupClient->setThreadIsProgress(false);
	};

	// Connect handshake is not needed more than once on the transport
	if (soupClient->getTransceiver()->DownloadAudioReady() || soupClient->getTransceiver()->DownloadVideoReady())
	{
		// In which case, just do on this thread
		func(soupClient.get(), params, kind);
		return soupClient->getTransceiver()->PopLastError().empty();
	}
	else
	{
		if (soupClient->isThreadInProgress())
		{
			blog(LOG_WARNING, "%s createConsumer '%s' but already a thread in progress", obs_module_description(), kind.c_str());
			return false;
		}

		soupClient->setThreadIsProgress(true);
		std::unique_ptr<std::thread> thr = std::make_unique<std::thread>(func, soupClient.get(), params, kind);
	
		while (true)
		{
			if (!soupClient->isThreadInProgress())
				break;

			if (soupClient->isConnectWaiting())
				break;

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (!soupClient->isThreadInProgress())
			thr->join();
		else
			soupClient->setConnectionThread(std::move(thr));

		return soupClient->isThreadInProgress();
	}
}

static bool createProducerTrack(std::shared_ptr<MediaSoupInterface> soupClient, const std::string& kind)
{
	if (soupClient->isThreadInProgress())
	{
		blog(LOG_WARNING, "%s createProducerTrack but already a thread in progress", obs_module_description());
		return false;
	}

	auto func = [](MediaSoupInterface* soupClient, const std::string kind)
	{
		try
		{
			if (kind == "audio")
				soupClient->getTransceiver()->CreateAudioProducerTrack();

			if (kind == "video")
				soupClient->getTransceiver()->CreateVideoProducerTrack();
		}
		catch (...)
		{
			blog(LOG_ERROR, "%s createProducerTrack exception %s", obs_module_description(), soupClient->getTransceiver()->PopLastError().c_str());
		}

		soupClient->setThreadIsProgress(false);
	};
	
	soupClient->setThreadIsProgress(true);
	soupClient->setExpectingProduceFollowup(true);
	std::unique_ptr<std::thread> thr = std::make_unique<std::thread>(func, soupClient.get(), kind);

	while (true)
	{
		if (!soupClient->isThreadInProgress())
			break;
		
		if (soupClient->isConnectWaiting())
			break;

		if (soupClient->isProduceWaiting())
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (!soupClient->isThreadInProgress())
		thr->join();
	else
		soupClient->setConnectionThread(std::move(thr));

	return soupClient->isThreadInProgress();
}

static bool createAudioProducerTrack(const std::string& roomId)
{
	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createProducerTracks but !transceiverCreated", obs_module_description());
		return false;
	}

	if (!soupClient->getTransceiver()->SenderReady())
	{
		blog(LOG_WARNING, "%s createProducerTracks but not senderReady", obs_module_description());
		return false;
	}

	return createProducerTrack(soupClient, "audio");
}

static bool createVideoProducerTrack(const std::string& roomId)
{
	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createVideoProducerTrack but !transceiverCreated", obs_module_description());
		return false;
	}

	if (!soupClient->getTransceiver()->SenderReady())
	{
		blog(LOG_WARNING, "%s createVideoProducerTrack but not senderReady", obs_module_description());
		return false;
	}

	return createProducerTrack(soupClient, "video");
}

static void msoup_update(void* source, obs_data_t* settings)
{
	std::string room = obs_data_get_string(settings, "room");
	auto soupClient = sMediaSoupClients->getInterface(room);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s msoup_update but !transceiverCreated", obs_module_description());
		return;
	}

	std::string send_transport_response = obs_data_get_string(settings, "send_transport_response");
	std::string receive_transport_response = obs_data_get_string(settings, "receive_transport_response");
	std::string video_consumer_response = obs_data_get_string(settings, "video_consumer_response");
	std::string audio_consumer_response = obs_data_get_string(settings, "audio_consumer_response");
	std::string create_audio_producer = obs_data_get_string(settings, "create_audio_producer");
	std::string create_video_producer = obs_data_get_string(settings, "create_video_producer");
	std::string produce_result = obs_data_get_string(settings, "produce_result");
	std::string connect_result = obs_data_get_string(settings, "connect_result");
	std::string stop_receiver = obs_data_get_string(settings, "stop_receiver");
	std::string stop_sender = obs_data_get_string(settings, "stop_sender");
	std::string stop_consumer = obs_data_get_string(settings, "stop_consumer");
	
	if (!stop_receiver.empty())
	{
		soupClient->getTransceiver()->StopReceiver();
		obs_data_set_string(settings, "stop_receiver", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!stop_sender.empty())
	{
		soupClient->getTransceiver()->StopSender();
		obs_data_set_string(settings, "stop_sender", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!stop_consumer.empty())
	{
		soupClient->getTransceiver()->StopConsumerById(stop_consumer);
		obs_data_set_string(settings, "stop_consumer", "");
		obs_source_update((obs_source_t*)source, settings);
	}
	
	if (!connect_result.empty())
	{
		if (!soupClient->isThreadInProgress() || !soupClient->isConnectWaiting())
		{
			blog(LOG_ERROR, "%s msoup_update has connect_result but thread is not in good state", obs_module_description());
		}
		else
		{
			soupClient->setIsDataReadyForConnect(true);

			if (soupClient->isExpectingProduceFollowup())
			{
				while (!soupClient->isProduceWaiting() && soupClient->isThreadInProgress())
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else
			{
				soupClient->joinWaitingThread();
				soupClient->resetThreadBools();
			}
		}

		obs_data_set_string(settings, "connect_result", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!produce_result.empty())
	{
		if (!soupClient->isThreadInProgress() || !soupClient->isProduceWaiting())
		{
			blog(LOG_ERROR, "%s msoup_update has produce_result but thread is not in good state", obs_module_description());
			return;
		}

		soupClient->setIsDataReadyForProduce(true);
		soupClient->joinWaitingThread();
		soupClient->resetThreadBools();

		obs_data_set_string(settings, "produce_result", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!send_transport_response.empty())
	{
		createSender(settings, (obs_source_t*)source, room, send_transport_response);
		obs_data_set_string(settings, "send_transport_response", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!create_audio_producer.empty())
	{
		createAudioProducerTrack(room);
		obs_data_set_string(settings, "create_audio_producer", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!create_video_producer.empty())
	{
		createVideoProducerTrack(room);
		obs_data_set_string(settings, "create_video_producer", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!receive_transport_response.empty())
	{
		createReceiver(settings, (obs_source_t*)source, room, receive_transport_response);
		obs_data_set_string(settings, "receive_transport_response", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!video_consumer_response.empty())
	{
		createConsumer(settings, (obs_source_t*)source, room, video_consumer_response, "video");
		obs_data_set_string(settings, "video_consumer_response", "");
		obs_source_update((obs_source_t*)source, settings);
	}

	if (!audio_consumer_response.empty())
	{
		createConsumer(settings, (obs_source_t*)source, room, audio_consumer_response, "audio");
		obs_data_set_string(settings, "audio_consumer_response", "");
		obs_source_update((obs_source_t*)source, settings);
	}
}

static void msoup_activate(void* data)
{

}

static void msoup_deactivate(void* data)
{

}

static void msoup_enum_sources(void* data, obs_source_enum_proc_t cb, void* param)
{

}

static void msoup_defaults(obs_data_t* settings)
{

}

/**
* Filter (Audio)
*/

static const char* msoup_faudio_name(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("mediasoup-audio-filter");
}

// Create
static void* msoup_faudio_create(obs_data_t* settings, obs_source_t* source)
{
	const std::string roomId = obs_data_get_string(settings, "room");

	if (roomId.empty())
		return nullptr;

	return source;
}

// Destroy
static void msoup_faudio_destroy(void* data)
{
	UNUSED_PARAMETER(data);
}

static struct obs_audio_data* msoup_faudio_filter_audio(void* data, struct obs_audio_data* audio)
{
	auto ptr = sMediaSoupClients->getInterface(obs_data_get_string(obs_source_get_settings((obs_source_t*)data), "room"));

	if (ptr == nullptr || !ptr->getTransceiver()->UploadAudioReady())
		return audio;

	const struct audio_output_info* aoi = audio_output_get_info(obs_get_audio());

	ptr->getMailboxPtr()->assignOutgoingAudioParams(aoi->format, aoi->speakers, static_cast<int>(get_audio_size(aoi->format, aoi->speakers, 1)), static_cast<int>(audio_output_get_channels(obs_get_audio())), static_cast<int>(audio_output_get_sample_rate(obs_get_audio())));
	ptr->getMailboxPtr()->push_outgoing_audioFrame((const uint8_t**)audio->data, audio->frames);
	return audio;
}

static obs_properties_t* msoup_faudio_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();
	UNUSED_PARAMETER(data);
	return props;
}

static void msoup_faudio_update(void* data, obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(settings);
}

static void msoup_faudio_save(void* data, obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(settings);
}

/**
* Filter (Video)
*/

static const char* msoup_fvideo_get_name(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("mediasoup-video-filter");
}

// Create
static void* msoup_fvideo_create(obs_data_t* settings, obs_source_t* source)
{
	const std::string roomId = obs_data_get_string(settings, "room");

	if (roomId.empty())
		return nullptr;

	return source;
}

// Destroy
static void msoup_fvideo_destroy(void* data)
{

}

static obs_properties_t* msoup_fvideo_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();
	return props;
}

static struct obs_source_frame* msoup_fvideo_filter_video(void* data, struct obs_source_frame* frame)
{
	auto ptr = sMediaSoupClients->getInterface(obs_data_get_string(obs_source_get_settings((obs_source_t*)data), "room"));

	if (ptr == nullptr || !ptr->getTransceiver()->UploadVideoReady())
		return frame;
	
	rtc::scoped_refptr<webrtc::I420Buffer> dest = webrtc::I420Buffer::Create(frame->width, frame->height);
	
	switch (frame->format)
	{
	//VIDEO_FORMAT_NV12  ??
	//VIDEO_FORMAT_BGRX
	//VIDEO_FORMAT_Y800
	//VIDEO_FORMAT_I444
	//VIDEO_FORMAT_I40A
	//VIDEO_FORMAT_I42A
	//VIDEO_FORMAT_AYUV
	case VIDEO_FORMAT_YVYU:
		libyuv::UYVYToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_YUY2:
		libyuv::YUY2ToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_UYVY:
		libyuv::UYVYToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_RGBA:
		libyuv::RGBAToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_BGRA:
		libyuv::BGRAToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	}

	return frame;
}

static void msoup_fvideo_update(void* data, obs_data_t* settings)
{

}

static void msoup_fvideo_defaults(obs_data_t* settings)
{

}

bool obs_module_load(void)
{
	struct obs_source_info mediasoup_connector	= {};
	mediasoup_connector.id				= "mediasoupconnector";
	mediasoup_connector.type			= OBS_SOURCE_TYPE_INPUT;
	mediasoup_connector.icon_type			= OBS_ICON_TYPE_SLIDESHOW;
	mediasoup_connector.output_flags		= OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_DO_NOT_SELF_MONITOR;
	mediasoup_connector.get_name			= msoup_getname;

	mediasoup_connector.create			= msoup_create;
	mediasoup_connector.destroy			= msoup_destroy;

	mediasoup_connector.update			= msoup_update;
	mediasoup_connector.video_render		= msoup_video_render;
	mediasoup_connector.video_tick			= msoup_video_tick;
	mediasoup_connector.activate			= msoup_activate;

	mediasoup_connector.deactivate			= msoup_deactivate;
	mediasoup_connector.enum_active_sources		= msoup_enum_sources;

	mediasoup_connector.get_width			= msoup_width;
	mediasoup_connector.get_height			= msoup_height;
	mediasoup_connector.get_defaults		= msoup_defaults;
	mediasoup_connector.get_properties		= msoup_properties;

	obs_register_source(&mediasoup_connector);

	// Filter (Audio)
	struct obs_source_info mediasoup_filter_audio	= {};
	mediasoup_filter_audio.id			= "mediasoupconnector_afilter";
	mediasoup_filter_audio.type			= OBS_SOURCE_TYPE_FILTER;
	mediasoup_filter_audio.output_flags		= OBS_SOURCE_AUDIO;
	mediasoup_filter_audio.get_name			= msoup_faudio_name;
	mediasoup_filter_audio.create			= msoup_faudio_create;
	mediasoup_filter_audio.destroy			= msoup_faudio_destroy;
	mediasoup_filter_audio.update			= msoup_faudio_update;
	mediasoup_filter_audio.filter_audio		= msoup_faudio_filter_audio;
	mediasoup_filter_audio.get_properties		= msoup_faudio_properties;
	mediasoup_filter_audio.save			= msoup_faudio_save;
	
	obs_register_source(&mediasoup_filter_audio);
	
	// Filter (Video)
	struct obs_source_info mediasoup_filter_video	= {};
	mediasoup_filter_video.id			= "mediasoupconnector_vfilter";
	mediasoup_filter_video.type			= OBS_SOURCE_TYPE_FILTER,
	mediasoup_filter_video.output_flags		= OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC,
	mediasoup_filter_video.get_name			= msoup_fvideo_get_name,
	mediasoup_filter_video.create			= msoup_fvideo_create,
	mediasoup_filter_video.destroy			= msoup_fvideo_destroy,
	mediasoup_filter_video.update			= msoup_fvideo_update,
	mediasoup_filter_video.get_defaults		= msoup_fvideo_defaults,
	mediasoup_filter_video.get_properties		= msoup_fvideo_properties,
	mediasoup_filter_video.filter_video		= msoup_fvideo_filter_video,
			
	obs_register_source(&mediasoup_filter_video);
	return true;
}
