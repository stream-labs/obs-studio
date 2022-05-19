#include "MediaSoupClients.h"

#include <util/platform.h>
#include <util/dstr.h>
#include <obs-module.h>
#include <mutex>
#include <iostream>

#include <third_party/libyuv/include/libyuv.h>
#include <api/video/i420_buffer.h>

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
	if (auto settings = obs_source_get_settings((obs_source_t*)data))
	{
		sMediaSoupClients->unregisterInterface(obs_data_get_string(settings, "room"));
		obs_data_release(settings);
	}
}

// Create
static void* msoup_create(obs_data_t* settings, obs_source_t* source)
{	
	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(ph, "void func_routerRtpCapabilities(in string input, out string output)", func_routerRtpCapabilities, source);
	proc_handler_add(ph, "void func_send_transport_response(in string input, out string output)", func_send_transport_response, source);
	proc_handler_add(ph, "void func_receive_transport_response(in string input, out string output)", func_receive_transport_response, source);
	proc_handler_add(ph, "void func_video_consumer_response(in string input, out string output)", func_video_consumer_response, source);
	proc_handler_add(ph, "void func_audio_consumer_response(in string input, out string output)", func_audio_consumer_response, source);
	proc_handler_add(ph, "void func_create_audio_producer(in string input, out string output)", func_create_audio_producer, source);
	proc_handler_add(ph, "void func_create_video_producer(in string input, out string output)", func_create_video_producer, source);
	proc_handler_add(ph, "void func_produce_result(in string input, out string output)", func_produce_result, source);
	proc_handler_add(ph, "void func_connect_result(in string input, out string output)", func_connect_result, source);
	proc_handler_add(ph, "void func_stop_receiver(in string input, out string output)", func_stop_receiver, source);
	proc_handler_add(ph, "void func_stop_sender(in string input, out string output)", func_stop_sender, source);
	proc_handler_add(ph, "void func_stop_consumer(in string input, out string output)", func_stop_consumer, source);
	proc_handler_add(ph, "void func_change_playback_volume(in string input, out string output)", func_change_playback_volume, source);
	proc_handler_add(ph, "void func_get_playback_devices(in string input, out string output)", func_get_playback_devices, source);
	proc_handler_add(ph, "void func_change_playback_device(in string input, out string output)", func_change_playback_device, source);

	return source;
}

static uint32_t msoup_width(void* data)
{
	auto settings = obs_source_get_settings((obs_source_t*)data);

	if (auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room")))
	{
		if (soupClient->m_obs_scene_texture != nullptr)
		{
			obs_data_release(settings);
			return soupClient->getTextureWidth();
		}
	}
	
	obs_data_release(settings);
	return 1280;
}

static uint32_t msoup_height(void* data)
{
	auto settings = obs_source_get_settings((obs_source_t*)data);

	if (auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room")))
	{
		if (soupClient->m_obs_scene_texture != nullptr)
		{
			obs_data_release(settings);
			return soupClient->getTextureHeight();
		}
	}

	obs_data_release(settings);
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
	UNREFERENCED_PARAMETER(e);

	auto settings = obs_source_get_settings((obs_source_t*)data);
	auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room"));
	obs_data_release(settings);

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
	//const bool senderReady = soupClient->getTransceiver()->SenderReady();
	//const bool receiverReady = soupClient->getTransceiver()->ReceiverReady();

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

static void msoup_update(void* source, obs_data_t* settings)
{

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

static void func_get_playback_devices(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");

	blog(LOG_DEBUG, "func_get_playback_devices %s", input.c_str());

	if (auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room")))
	{
		std::map<int16_t, std::string> devices;
		soupClient->getTransceiver()->GetPlayoutDevices(devices);

		if (!devices.empty())
		{
			try
			{
				json devicesJson;

				for (auto& itr : devices)
				{
					json blob
					{
						{ "id", std::to_string(itr.first)	},
						{ "name", itr.second			}
					};

					devicesJson.push_back(blob);
				}

				calldata_set_string(cd, "output", devicesJson.dump().c_str());
			}
			catch (...)
			{
				
			}
		}
	}

	obs_data_release(settings);		
}

static void func_routerRtpCapabilities(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");

	blog(LOG_DEBUG, "func_routerRtpCapabilities %s", input.c_str());

	createInterfaceObject(settings, (obs_source_t*)source, room, input, cd);

	obs_data_release(settings);
}

static void func_change_playback_volume(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_change_playback_volume %s", input.c_str());

	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->SetPlayoutDevice(uint16_t(atoi(input.c_str())));
	}
	else
	{
		blog(LOG_ERROR, "%s func_change_playback_device but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

static void func_change_playback_device(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_change_playback_device %s", input.c_str());

	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->SetPlayoutDevice(uint16_t(atoi(input.c_str())));
	}
	else
	{
		blog(LOG_ERROR, "%s func_change_playback_device but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

static void func_stop_receiver(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_stop_receiver %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->StopReceiver();
	}
	else
	{
		blog(LOG_ERROR, "%s func_stop_receiver but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

static void func_stop_sender(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_stop_sender %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->StopSender();
	}
	else
	{
		blog(LOG_ERROR, "%s func_stop_sender but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

static void func_stop_consumer(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_stop_consumer %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->StopConsumerById(input);
	}
	else
	{
		blog(LOG_ERROR, "%s func_stop_consumer but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

static void func_connect_result(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_connect_result %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		if (!soupClient->isThreadInProgress() || !soupClient->isConnectWaiting())
		{
			blog(LOG_ERROR, "%s func_connect_result but thread is not in good state", obs_module_description());
		}
		else
		{
			soupClient->setDataReadyForConnect(input);

			if (soupClient->isExpectingProduceFollowup())
			{
				while (!soupClient->isProduceWaiting() && soupClient->isThreadInProgress())
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else
			{
				soupClient->joinWaitingThread();
				soupClient->resetThreadCache();
			}

			std::string params;

			if (soupClient->popProduceParams(params))
			{
				json output;
				output["produce_params"] = params;
				calldata_set_string(cd, "output", output.dump().c_str());
			}
		}
	}
	else
	{
		blog(LOG_ERROR, "%s func_connect_result but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

static void func_produce_result(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_produce_result %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		if (!soupClient->isThreadInProgress() || !soupClient->isProduceWaiting())
		{
			blog(LOG_ERROR, "%s func_produce_result but thread is not in good state", obs_module_description());
			return;
		}

		soupClient->setDataReadyForProduce(input);
		soupClient->joinWaitingThread();
		soupClient->resetThreadCache();
	}
	else
	{
		blog(LOG_ERROR, "%s func_produce_result but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

static void func_send_transport_response(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_send_transport_response %s", input.c_str());
	
	createSender(settings, (obs_source_t*)source, room, input, cd);

	obs_data_release(settings);
}

static void func_create_audio_producer(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_create_audio_producer %s", input.c_str());
	
	createAudioProducerTrack(room, cd);

	obs_data_release(settings);
}

static void func_create_video_producer(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_create_video_producer %s", input.c_str());
	
	createVideoProducerTrack(room, cd);

	obs_data_release(settings);
}

static void func_receive_transport_response(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_receive_transport_response %s", input.c_str());
	
	createReceiver(settings, (obs_source_t*)source, room, input, cd);

	obs_data_release(settings);
}

static void func_video_consumer_response(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_video_consumer_response %s", input.c_str());
	
	createConsumer(settings, (obs_source_t*)source, room, input, "video", cd);

	obs_data_release(settings);
}

static void func_audio_consumer_response(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_audio_consumer_response %s", input.c_str());
	
	createConsumer(settings, (obs_source_t*)source, room, input, "audio", cd);

	obs_data_release(settings);
}

static bool createReceiver(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, calldata_t* cd)
{
	blog(LOG_DEBUG, "createReceiver start");

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

		json sctpParameters;
		try { sctpParameters = response["sctpParameters"]; } catch (...) { }
		
		json iceServers;
		try { iceServers = response["iceServers"]; } catch (...) { }

		if (!soupClient->getTransceiver()->CreateReceiver(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], sctpParameters.empty() ? nullptr : &sctpParameters, iceServers.empty() ? nullptr : &iceServers))
			return false;
	}
	catch (...)
	{
		blog(LOG_ERROR, "%s createReceiver exception", obs_module_description());
		return false;
	}
	
	json output;
	output["receiverId"] = soupClient->getTransceiver()->GetReceiverId().c_str();
	calldata_set_string(cd, "output", output.dump().c_str());
	return true;
}

static bool createSender(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, calldata_t* cd)
{
	blog(LOG_DEBUG, "createSender start");

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
		
		json iceServers;
		try { iceServers = response["iceServers"]; } catch (...) { }

		if (!soupClient->getTransceiver()->CreateSender(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], iceServers.empty() ? nullptr : &iceServers))
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

	json output;
	output["senderId"] = soupClient->getTransceiver()->GetSenderId();
	calldata_set_string(cd, "output", output.dump().c_str());
	return true;
}

static bool createConsumer(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, const std::string& kind, calldata_t* cd)
{
	blog(LOG_DEBUG, "createConsumer start");

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

static bool createProducerTrack(std::shared_ptr<MediaSoupInterface> soupClient, const std::string& kind, calldata_t* cd)
{
	blog(LOG_DEBUG, "createProducerTrack start");

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

	std::string params;

	// Sent to the frontend
	if (soupClient->popConnectParams(params))
	{
		json output;
		output["connect_params"] = params;
		calldata_set_string(cd, "output", output.dump().c_str());
	}
	else if (soupClient->popProduceParams(params))
	{
		json output;
		output["produce_params"] = params;
		calldata_set_string(cd, "output", output.dump().c_str());
	}

	return soupClient->isThreadInProgress();
}

static bool createAudioProducerTrack(const std::string& roomId, calldata_t* cd)
{
	blog(LOG_DEBUG, "createAudioProducerTrack start");

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

	return createProducerTrack(soupClient, "audio", cd);
}

static bool createVideoProducerTrack(const std::string& roomId, calldata_t* cd)
{
	blog(LOG_DEBUG, "createVideoProducerTrack start");

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

	return createProducerTrack(soupClient, "video", cd);
}

static void createInterfaceObject(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& routerRtpCapabilities_Raw, calldata_t* cd)
{
	blog(LOG_DEBUG, "createInterfaceObject start");

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
		blog(LOG_ERROR, "msoup_create json error parsing routerRtpCapabilities_Raw %s", routerRtpCapabilities_Raw.c_str());
		return;
	}

	// lib - Create device
	if (!soupClient->getTransceiver()->LoadDevice(rotuerRtpCapabilities, deviceRtpCapabilities, deviceSctpCapabilities))
	{
		blog(LOG_ERROR, "msoup_create LoadDevice failed error = '%s'", soupClient->getTransceiver()->PopLastError().c_str());
		return;
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
		soupClient->setProduceParams(data.dump());
		soupClient->setProduceIsWaiting(true);

		std::string dataReady;

		while (soupClient->isProduceWaiting() && soupClient->isThreadInProgress() && !soupClient->popDataReadyForProduce(dataReady))
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		soupClient->setProduceIsWaiting(false);

		if (!dataReady.empty() || soupClient->popDataReadyForProduce(dataReady))
			return dataReady == "true";

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
		soupClient->setConnectParams(data.dump());
		soupClient->setConnectIsWaiting(true);

		std::string dataReady;

		while (soupClient->isConnectWaiting() && soupClient->isThreadInProgress() && !soupClient->popDataReadyForConnect(dataReady))
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		soupClient->setConnectIsWaiting(false);

		if (!dataReady.empty() || soupClient->popDataReadyForConnect(dataReady))
			return dataReady == "true";

		return false;
	};

	soupClient->getTransceiver()->RegisterOnProduce(onProduceFunc);	
	soupClient->getTransceiver()->RegisterOnConnect(onConnectFunc);

	json output;
	output["deviceRtpCapabilities"] = deviceRtpCapabilities;
	output["deviceSctpCapabilities"] = deviceSctpCapabilities;
	output["version"] = mediasoupclient::Version();
	output["clientId"] = soupClient->getTransceiver()->GetId();
	calldata_set_string(cd, "output", output.dump().c_str());
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
	return source;
}

// Destroy
static void msoup_faudio_destroy(void* data)
{
	UNUSED_PARAMETER(data);
}

static struct obs_audio_data* msoup_faudio_filter_audio(void* data, struct obs_audio_data* audio)
{
	auto settings = obs_source_get_settings((obs_source_t*)data);
	auto ptr = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room"));
	obs_data_release(settings);

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
	auto settings = obs_source_get_settings((obs_source_t*)data);
	auto ptr = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room"));
	obs_data_release(settings);

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
