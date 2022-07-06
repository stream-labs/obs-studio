#ifndef _DEBUG

#include "ConnectorFrontApi.h"
#include "MediaSoupClients.h"
#include "MyLogSink.h"

#include <third_party/libyuv/include/libyuv.h>
#include <util/platform.h>
#include <util/dstr.h>

#include "temp_demodebugging.h"

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

// Create
static void* msoup_create(obs_data_t* settings, obs_source_t* source)
{	
	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(ph, "void func_load_device(in string input, out string output)", ConnectorFrontApi::func_load_device, source);
	proc_handler_add(ph, "void func_create_send_transport(in string input, out string output)", ConnectorFrontApi::func_create_send_transport, source);
	proc_handler_add(ph, "void func_create_receive_transport(in string input, out string output)", ConnectorFrontApi::func_create_receive_transport, source);
	proc_handler_add(ph, "void func_video_consumer_response(in string input, out string output)", ConnectorFrontApi::func_video_consumer_response, source);
	proc_handler_add(ph, "void func_audio_consumer_response(in string input, out string output)", ConnectorFrontApi::func_audio_consumer_response, source);
	proc_handler_add(ph, "void func_create_audio_producer(in string input, out string output)", ConnectorFrontApi::func_create_audio_producer, source);
	proc_handler_add(ph, "void func_create_video_producer(in string input, out string output)", ConnectorFrontApi::func_create_video_producer, source);
	proc_handler_add(ph, "void func_produce_result(in string input, out string output)", ConnectorFrontApi::func_produce_result, source);
	proc_handler_add(ph, "void func_connect_result(in string input, out string output)", ConnectorFrontApi::func_connect_result, source);
	proc_handler_add(ph, "void func_stop_receiver(in string input, out string output)", ConnectorFrontApi::func_stop_receiver, source);
	proc_handler_add(ph, "void func_stop_sender(in string input, out string output)", ConnectorFrontApi::func_stop_sender, source);
	proc_handler_add(ph, "void func_stop_consumer(in string input, out string output)", ConnectorFrontApi::func_stop_consumer, source);
	proc_handler_add(ph, "void func_change_playback_volume(in string input, out string output)", ConnectorFrontApi::func_change_playback_volume, source);
	proc_handler_add(ph, "void func_get_playback_devices(in string input, out string output)", ConnectorFrontApi::func_get_playback_devices, source);
	proc_handler_add(ph, "void func_change_playback_device(in string input, out string output)", ConnectorFrontApi::func_change_playback_device, source);
	proc_handler_add(ph, "void func_toggle_direct_audio_broadcast(in string input, out string output)", ConnectorFrontApi::func_toggle_direct_audio_broadcast, source);

	obs_source_set_audio_active(source, true);

	if (g_debugging)
		initDebugging(settings, source);

	// Singleton, captures webrtc debug msgs
	MyLogSink::instance();
	return source;
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

// Video Render
static void msoup_video_render(void* data, gs_effect_t* e)
{
	UNREFERENCED_PARAMETER(e);

	auto settings = obs_source_get_settings((obs_source_t*)data);
	auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room"));
	obs_data_release(settings);

	if (soupClient == nullptr || !soupClient->getTransceiver()->DownloadVideoReady())
		return;

	std::unique_ptr<webrtc::VideoFrame> frame;
	soupClient->getMailboxPtr()->pop_receieved_videoFrames(frame);

	// A new frame arrived, replace the old one that we were drawing
	if (frame != nullptr)
		soupClient->applyVideoFrameToObsTexture(*frame);

	if (soupClient->m_obs_scene_texture == nullptr)
		return;

	gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t* tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t* image = gs_effect_get_param_by_name(effect, "image");
	
	gs_enable_framebuffer_srgb(false);
	gs_enable_blending(false);
	gs_effect_set_texture(image, soupClient->m_obs_scene_texture);

	if (gs_technique_begin_pass(tech, 0))
	{
		// Position, center letterbox
		int xPos = 0;
		int yPos = 0;

		if (soupClient->getTextureWidth() < soupClient->getHardObsTextureWidth())
		{
			int diff = soupClient->getHardObsTextureWidth() - soupClient->getTextureWidth();
			xPos = diff / 2;
		}

		if (soupClient->getTextureHeight() < soupClient->getHardObsTextureHeight())
		{
			int diff = soupClient->getHardObsTextureHeight() - soupClient->getTextureHeight();
			yPos = diff / 2;
		}

		if (xPos != 0 || yPos != 0)
		{
			gs_matrix_push();
			gs_matrix_translate3f((float)xPos, (float)yPos, 0.0f);
		}

		gs_draw_sprite(soupClient->m_obs_scene_texture, 0, 0, 0);
		
		if (xPos != 0 || yPos != 0)
			gs_matrix_pop();

		gs_technique_end_pass(tech);
		gs_technique_end(tech);
	}

	gs_enable_blending(true);
}

static uint32_t msoup_width(void* data)
{
	return uint32_t(MediaSoupInterface::getHardObsTextureWidth());
}

static uint32_t msoup_height(void* data)
{
	return uint32_t(MediaSoupInterface::getHardObsTextureHeight());
}

static obs_properties_t* msoup_properties(void* data)
{
	obs_properties_t* ppts = obs_properties_create();	
	return ppts;
}

static void msoup_video_tick(void* data, float seconds)
{
	UNREFERENCED_PARAMETER(seconds);

	auto settings = obs_source_get_settings((obs_source_t*)data);
	auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room"));
	obs_data_release(settings);

	if (soupClient == nullptr || !soupClient->getTransceiver()->DownloadAudioReady())
		return;

	std::vector<std::unique_ptr<MediaSoupMailbox::SoupRecvAudioFrame>> frames;
	soupClient->getMailboxPtr()->pop_receieved_audioFrames(frames);

	if (soupClient->getBoolDirectAudioBroadcast())
	{
		for (auto& frame : frames)
		{
			obs_source_audio sdata;
			sdata.data[0] = frame->audio_data.data();
			sdata.frames = uint32_t(frame->number_of_frames);
			sdata.speakers = static_cast<speaker_layout>(frame->number_of_channels);
			sdata.samples_per_sec = frame->sample_rate;
			sdata.format = MediaSoupTransceiver::GetDefaultAudioFormat();
			sdata.timestamp = frame->timestamp;
			obs_source_output_audio(soupClient->m_obs_source, &sdata);
		}
	}
}

static void msoup_update(void* source, obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(source);
	UNREFERENCED_PARAMETER(settings);
}

static void msoup_activate(void* data)
{
	UNREFERENCED_PARAMETER(data);
}

static void msoup_deactivate(void* data)
{
	UNREFERENCED_PARAMETER(data);
}

static void msoup_enum_sources(void* data, obs_source_enum_proc_t cb, void* param)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(cb);
	UNREFERENCED_PARAMETER(param);
}

static void msoup_defaults(obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(settings);
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
	if (g_debugging)
		getRoomFromConsole(settings, source);

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
	if (g_debugging)
		getRoomFromConsole(settings, source);

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
	//VIDEO_FORMAT_Y800
	//VIDEO_FORMAT_I40A
	//VIDEO_FORMAT_I42A
	//VIDEO_FORMAT_AYUV
	//VIDEO_FORMAT_YVYU
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
	case VIDEO_FORMAT_I422:
		libyuv::I422ToI420(frame->data[0],  static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), frame->data[2],  static_cast<int>(frame->linesize[2]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_I444:
		libyuv::I444ToI420(frame->data[0],  static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), frame->data[2],  static_cast<int>(frame->linesize[2]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_NV12:
		libyuv::NV12ToI420(frame->data[0], static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		ptr->getMailboxPtr()->push_outgoing_videoFrame(dest);
		break;
	}
	
	return frame;
}

static void msoup_fvideo_update(void* data, obs_data_t* settings)
{
	//
}

static void msoup_fvideo_defaults(obs_data_t* settings)
{
	//
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

#endif
