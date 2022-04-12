#include "MediaSoupClients.h"
#include "temp_httpclass.h"

#include <util/platform.h>
#include <util/dstr.h>
#include <obs-module.h>
#include <mutex>
#include <iostream>

#include <third_party/libyuv/include/libyuv.h>
#include <api/video/i420_buffer.h>

bool consoleAlloc = false;
std::string serverUrl = "https://v3demo.mediasoup.org:4443";
std::string roomId;// = "nkhv8wkv";
#define baseUrl (serverUrl + "/rooms/" + roomId)

/**
* Source
*/

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mediasoup-connector", "en-US")
MODULE_EXPORT const char* obs_module_description(void)
{
	return "Streamlabs Join";
}

static const char *msoup_getname(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("MediaSoupConnector");
}

// Destroy
static void msoup_destroy(void* data)
{
	sMedaSoupClients->unregisterInterface(roomId);
}

// Create
static void* msoup_create(obs_data_t* settings, obs_source_t* source)
{
	if (!consoleAlloc)
	{
		consoleAlloc = true;		
		AllocConsole();
		freopen("conin$","r",stdin);
		freopen("conout$","w",stdout);
		freopen("conout$","w",stderr);
		printf("Debugging Window:\n");
	}

	printf("Enter Room Id: ");
	std::cin >> roomId;
	printf("Connecting, please wait...\n");

	auto soupClient = sMedaSoupClients->registerInterface(roomId);

	if (soupClient == nullptr)
		return nullptr;

	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP
	WSHTTPGenericRequestToStream(baseUrl, "GET", "", &httpOut, 2000, strResponse);

	if (httpOut != 200)
		return nullptr;

	json deviceRtpCapabilities;
	json deviceSctpCapabilities;
	json rotuerRtpCapabilities;

	try
	{
		rotuerRtpCapabilities = json::parse(strResponse);
	}
	catch (...)
	{
		return nullptr;
	}

	// lib - Create device
	if (!soupClient->loadDevice(rotuerRtpCapabilities, deviceRtpCapabilities, deviceSctpCapabilities))
		return nullptr;

	// HTTP - Join lobby
	WSHTTPGenericRequestToStream(baseUrl + "/broadcasters", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "id",          soupClient->getId()			},
			{ "displayName", "broadcaster"				},
			{ "device",
				{
					{ "name",    "libmediasoupclient"       },
					{ "version", mediasoupclient::Version() }
				}
			},
			{ "rtpCapabilities", deviceRtpCapabilities	}}.dump());

	if (httpOut != 200)
		return nullptr;
	
	std::string producerAudio;
	std::string producerVideo;
	
	std::string receiverId;
	std::string senderId;

	std::atomic<bool> waiting = true;

	try
	{
		auto roomInfo = json::parse(strResponse);
	
		if (roomInfo["peers"].empty())
			return nullptr;
	
		if (roomInfo["peers"].begin().value().empty())
			return nullptr;
	
		auto itrlist = roomInfo["peers"].begin().value()["producers"];
	
		for (auto& itr : itrlist)
		{
			auto kind = itr["kind"].get<std::string>();
	
			if (kind == "audio")
				producerAudio = itr["id"].get<std::string>();
			else if (kind == "video")
				producerVideo = itr["id"].get<std::string>();
		}
	}
	catch (...)
	{
		return nullptr;
	}

	// HTTP - Register receive transport
	WSHTTPGenericRequestToStream(baseUrl + "/broadcasters/" + soupClient->getId() + "/transports", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc"				},
			{ "rtcpMux", true				},
			{ "sctpCapabilities", deviceSctpCapabilities	}}.dump());

	if (httpOut != 200)
		return nullptr;

	try
	{
		// lib - Create receiver
		auto response = json::parse(strResponse);
		if (!soupClient->createReceiver(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], response["sctpParameters"], receiverId))
			return nullptr;
	}
	catch (...)
	{
		return nullptr;
	}

	// HTTP - Register send transport
	WSHTTPGenericRequestToStream(baseUrl + "/broadcasters/" + soupClient->getId() + "/transports", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return nullptr;

	try
	{
		// lib - Create sender
		auto response = json::parse(strResponse);
		if (!soupClient->createSender(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], senderId))
			return nullptr;
	}
	catch (...)
	{
		return nullptr;
	}
	
	waiting = true;	
	soupClient->registerOnProduce([&](const std::string& kind, json& rtpParameters, std::string& output_value)
	{
		// HTTP - Finalize connection
		WSHTTPGenericRequestToStream(baseUrl + "/broadcasters/" + soupClient->getId() + "/transports/" + senderId + "/producers", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
			json{	{ "kind",          kind          },
				{ "rtpParameters", rtpParameters }}.dump()); 

		waiting = false;
		return httpOut == 200;
	});

	soupClient->registerOnConnect([&](const json& out_dtlsParameters, const std::string& transportId)
	{
		// HTTP - Finalize connection
		WSHTTPGenericRequestToStream(baseUrl + "/broadcasters/" + soupClient->getId() + "/transports/" + transportId + "/connect", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
			json{{ "dtlsParameters", out_dtlsParameters }}.dump()); 

		waiting = false;
		return httpOut == 200;
	});

	try
	{
		// lib - Create sender tracks
		soupClient->createProducerTracks();
	}
	catch (...)
	{
		return nullptr;
	}

	while (waiting)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	
	// HTTP - Register video consumer
	WSHTTPGenericRequestToStream(baseUrl + "/broadcasters/" + soupClient->getId() + "/transports/" + receiverId + "/consume?producerId=" + producerVideo, "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{{ "rtpCapabilities", deviceRtpCapabilities }}.dump());

	if (httpOut != 200)
		return nullptr;

	waiting = true;

	try
	{
		auto response = json::parse(strResponse);
		auto id = response["id"].get<std::string>();
		auto producerId = response["producerId"].get<std::string>();
		auto rtpParam = response["rtpParameters"].get<json>();
		
		// lib - Create video consumer
		if (!soupClient->createVideoConsumer(id, producerId, &rtpParam))
			return nullptr;
	}
	catch (...)
	{
		return nullptr;
	}

	while (waiting)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	if (httpOut != 200)
		return nullptr;
	
	// HTTP - Register audio consumer
	WSHTTPGenericRequestToStream(baseUrl + "/broadcasters/" + soupClient->getId() + "/transports/" + receiverId + "/consume?producerId=" + producerAudio, "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{{ "rtpCapabilities", deviceRtpCapabilities }}.dump());

	if (httpOut != 200)
		return nullptr;

	try
	{
		auto response = json::parse(strResponse);
		auto id = response["id"].get<std::string>();
		auto producerId = response["producerId"].get<std::string>();
		auto rtpParam = response["rtpParameters"].get<json>();
		
		// lib - Create audio consumer
		if (!soupClient->createAudioConsumer(id, producerId, &rtpParam))
			return nullptr;
	}
	catch (...)
	{
		return nullptr;
	}

	obs_source_update(source, NULL);
	soupClient->m_source = source;
	return soupClient.get();
}

static uint32_t msoup_width(void* data)
{
	return 1280;
}

static uint32_t msoup_height(void* data)
{
	return 720;
}

// Video Render
static void msoup_video_render(void* data, gs_effect_t* e)
{
	auto soupClient = sMedaSoupClients->getInterface(roomId);
	UNREFERENCED_PARAMETER(e);

	if (soupClient == nullptr || !soupClient->downloadVideoReady())
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
		DWORD biSizeImage = i420buffer->width() * i420buffer->height() * (biBitCount >> 3);
		
		std::unique_ptr<uint8_t[]> image_buffer;
		image_buffer.reset(new uint8_t[biSizeImage]); // I420 to 
		libyuv::I420ToABGR(i420buffer->DataY(), i420buffer->StrideY(), i420buffer->DataU(), i420buffer->StrideU(), i420buffer->DataV(), i420buffer->StrideV(), image_buffer.get(), i420buffer->width() * biBitCount / 8, i420buffer->width(), i420buffer->height());
		
		soupClient->initDrawTexture(i420buffer->width(), i420buffer->height());
		gs_texture_set_image(soupClient->m_texture, image_buffer.get(), i420buffer->width() * 4, false);
		
	}

	if (soupClient->m_texture == nullptr)
		return;

	gs_enable_framebuffer_srgb(false);
	gs_enable_blending(false);
	
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");

	gs_effect_set_texture(image, soupClient->m_texture);

	const uint32_t flip = 0;
	const size_t passes = gs_technique_begin(tech);

	for (size_t i = 0; i < passes; i++)
	{
		if (gs_technique_begin_pass(tech, i))
		{
			gs_draw_sprite(soupClient->m_texture, flip, 0, 0);
			gs_technique_end_pass(tech);
		}
	}

	gs_technique_end(tech);
	gs_enable_blending(true);
}

static obs_properties_t* msoup_properties(void* data)
{
	obs_properties_t* ppts = obs_properties_create();
	
	return ppts;
}

static void msoup_video_tick(void* d, float seconds)
{
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

static void msoup_update(void* d, obs_data_t* settings)
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

static void msoup_defaults(obs_data_t *settings)
{

}

/**
* Filter (Audio)
*/

static const char* msoup_faudio_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("mediasoup-audio-filter");
}

static void* msoup_faudio_create(obs_data_t *settings, obs_source_t *filter)
{
	return filter;
}

static void msoup_faudio_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static struct obs_audio_data* msoup_faudio_filter_audio(void *data, struct obs_audio_data *audio)
{
	UNUSED_PARAMETER(data);
	auto ptr = sMedaSoupClients->getInterface(roomId);

	if (ptr == nullptr || !ptr->uploadAudioReady())
		return audio;

	const struct audio_output_info *aoi = audio_output_get_info(obs_get_audio());

	ptr->getMailboxPtr()->assignOutgoingAudioParams(aoi->format, aoi->speakers, static_cast<int>(get_audio_size(aoi->format, aoi->speakers, 1)), static_cast<int>(audio_output_get_channels(obs_get_audio())), static_cast<int>(audio_output_get_sample_rate(obs_get_audio())));
	ptr->getMailboxPtr()->push_outgoing_audioFrame((const uint8_t**)audio->data, audio->frames);
	return audio;
}

static obs_properties_t* msoup_faudio_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	UNUSED_PARAMETER(data);
	return props;
}

static void msoup_faudio_update(void *data, obs_data_t *settings)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(settings);
}

static void msoup_faudio_save(void *data, obs_data_t *settings)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(settings);
}

/**
* Filter (Video)
*/

static const char *msoup_fvideo_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("mediasoup-video-filter");
}

static obs_properties_t *msoup_fvideo_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	return props;
}

static void *msoup_fvideo_create(obs_data_t *settings, obs_source_t *context)
{
	return context;
}

static void msoup_fvideo_destroy(void *data)
{
}

static struct obs_source_frame* msoup_fvideo_filter_video(void *data, struct obs_source_frame *frame)
{
	UNUSED_PARAMETER(data);
	auto ptr = sMedaSoupClients->getInterface(roomId);

	if (ptr == nullptr || !ptr->uploadVideoReady())
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

static void msoup_fvideo_update(void *data, obs_data_t *settings)
{

}

static void msoup_fvideo_defaults(obs_data_t *settings)
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
