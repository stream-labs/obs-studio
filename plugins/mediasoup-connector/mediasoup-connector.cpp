#ifndef _DEBUG

#include "ConnectorFrontApi.h"
#include "MyLogSink.h"
#include "MediaSoupMailbox.h"

#include <third_party/libyuv/include/libyuv.h>
#include <util/platform.h>
#include <util/dstr.h>

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
	MediaSoupInterface::ObsSourceInfo* data = new MediaSoupInterface::ObsSourceInfo{};
	data->m_obs_source = source;

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(ph, "void func_load_device(in string input, out string output)", ConnectorFrontApi::func_load_device, data);
	proc_handler_add(ph, "void func_create_send_transport(in string input, out string output)", ConnectorFrontApi::func_create_send_transport, data);
	proc_handler_add(ph, "void func_create_receive_transport(in string input, out string output)", ConnectorFrontApi::func_create_receive_transport, data);
	proc_handler_add(ph, "void func_video_consumer_response(in string input, out string output)", ConnectorFrontApi::func_video_consumer_response, data);
	proc_handler_add(ph, "void func_audio_consumer_response(in string input, out string output)", ConnectorFrontApi::func_audio_consumer_response, data);
	proc_handler_add(ph, "void func_create_audio_producer(in string input, out string output)", ConnectorFrontApi::func_create_audio_producer, data);
	proc_handler_add(ph, "void func_create_video_producer(in string input, out string output)", ConnectorFrontApi::func_create_video_producer, data);
	proc_handler_add(ph, "void func_produce_result(in string input, out string output)", ConnectorFrontApi::func_produce_result, data);
	proc_handler_add(ph, "void func_connect_result(in string input, out string output)", ConnectorFrontApi::func_connect_result, data);
	proc_handler_add(ph, "void func_stop_receiver(in string input, out string output)", ConnectorFrontApi::func_stop_receiver, data);
	proc_handler_add(ph, "void func_stop_sender(in string input, out string output)", ConnectorFrontApi::func_stop_sender, data);
	proc_handler_add(ph, "void func_stop_consumer(in string input, out string output)", ConnectorFrontApi::func_stop_consumer, data);

	obs_source_set_audio_active(source, true);

	// Captures webrtc debug msgs
	MyLogSink::instance();
	
	++MediaSoupInterface::instance().m_sourceCounter;
	
	return data;
}

// Destroy
static void msoup_destroy(void* data)
{
	MediaSoupInterface::ObsSourceInfo* sourceInfo = static_cast<MediaSoupInterface::ObsSourceInfo*>(data);
	--MediaSoupInterface::instance().m_sourceCounter;

	if (sourceInfo->m_obs_scene_texture != nullptr)
		gs_texture_destroy(sourceInfo->m_obs_scene_texture);
	
	MediaSoupInterface::instance().getTransceiver()->StopConsumerById(sourceInfo->m_consumer_audio);
	MediaSoupInterface::instance().getTransceiver()->StopConsumerById(sourceInfo->m_consumer_video);

	// We're the last one, final cleanup
	if (MediaSoupInterface::instance().m_sourceCounter <= 0)
		MediaSoupInterface::instance().reset();

	delete sourceInfo;
}

// Video Render
static void msoup_video_render(void* data, gs_effect_t* e)
{
	MediaSoupInterface::ObsSourceInfo* sourceInfo = static_cast<MediaSoupInterface::ObsSourceInfo*>(data);
	UNREFERENCED_PARAMETER(e);

	if (!MediaSoupInterface::instance().getTransceiver()->ConsumerReady(sourceInfo->m_consumer_video))
		return;

	//GetConsumerMailbox
	std::unique_ptr<webrtc::VideoFrame> frame;
	auto mailbox = MediaSoupInterface::instance().getTransceiver()->GetConsumerMailbox(sourceInfo->m_consumer_video);

	if (mailbox == nullptr)
		return;

	mailbox->pop_receieved_videoFrames(frame);

	// A new frame arrived, apply it to our cached texture with gs_texture_set_image
	if (frame != nullptr)
		MediaSoupInterface::instance().applyVideoFrameToObsTexture(*frame, *sourceInfo);

	if (sourceInfo->m_obs_scene_texture == nullptr)
		return;

	gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t* tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t* image = gs_effect_get_param_by_name(effect, "image");
	
	gs_enable_framebuffer_srgb(false);
	gs_enable_blending(false);
	gs_effect_set_texture(image, sourceInfo->m_obs_scene_texture);

	if (gs_technique_begin_pass(tech, 0))
	{
		// Position, center letterbox
		int xPos = 0;
		int yPos = 0;

		if (sourceInfo->m_textureWidth < MediaSoupInterface::instance().getHardObsTextureWidth())
		{
			int diff = MediaSoupInterface::instance().getHardObsTextureWidth() - sourceInfo->m_textureWidth;
			xPos = diff / 2;
		}

		if (sourceInfo->m_textureHeight < MediaSoupInterface::instance().getHardObsTextureHeight())
		{
			int diff = MediaSoupInterface::instance().getHardObsTextureHeight() - sourceInfo->m_textureHeight;
			yPos = diff / 2;
		}

		if (xPos != 0 || yPos != 0)
		{
			gs_matrix_push();
			gs_matrix_translate3f(float(xPos), float(yPos), 0.0f);
		}

		gs_draw_sprite(sourceInfo->m_obs_scene_texture, 0, 0, 0);
		
		if (xPos != 0 || yPos != 0)
			gs_matrix_pop();

		gs_technique_end_pass(tech);
		gs_technique_end(tech);
	}

	gs_enable_blending(true);
}

static void msoup_video_tick(void* data, float seconds)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(seconds);
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
	return source;
}

// Destroy
static void msoup_faudio_destroy(void* data)
{
	UNUSED_PARAMETER(data);
}

static struct obs_audio_data* msoup_faudio_filter_audio(void* data, struct obs_audio_data* audio)
{
	if (!MediaSoupInterface::instance().getTransceiver()->AudioProducerReady())
		return audio;

	const struct audio_output_info* aoi = audio_output_get_info(obs_get_audio());

	auto mailbox = MediaSoupInterface::instance().getTransceiver()->GetProducerMailbox();
	mailbox->assignOutgoingAudioParams(aoi->format, aoi->speakers, static_cast<int>(get_audio_size(aoi->format, aoi->speakers, 1)), static_cast<int>(audio_output_get_channels(obs_get_audio())), static_cast<int>(audio_output_get_sample_rate(obs_get_audio())));
	mailbox->push_outgoing_audioFrame((const uint8_t**)audio->data, audio->frames);
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
	if (!MediaSoupInterface::instance().getTransceiver()->VideoProducerReady())
		return frame;
	
	rtc::scoped_refptr<webrtc::I420Buffer> dest = MediaSoupInterface::instance().getProducerFrameBuffer(frame->width, frame->height);
	auto mailbox = MediaSoupInterface::instance().getTransceiver()->GetProducerMailbox();

	switch (frame->format)
	{
	//VIDEO_FORMAT_Y800
	//VIDEO_FORMAT_I40A
	//VIDEO_FORMAT_I42A
	//VIDEO_FORMAT_AYUV
	//VIDEO_FORMAT_YVYU
	case VIDEO_FORMAT_YUY2:
		libyuv::YUY2ToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		mailbox->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_UYVY:
		libyuv::UYVYToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		mailbox->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_RGBA:
		libyuv::RGBAToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		mailbox->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_BGRA:
		libyuv::BGRAToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		mailbox->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_I422:
		libyuv::I422ToI420(frame->data[0],  static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), frame->data[2],  static_cast<int>(frame->linesize[2]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		mailbox->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_I444:
		libyuv::I444ToI420(frame->data[0],  static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), frame->data[2],  static_cast<int>(frame->linesize[2]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		mailbox->push_outgoing_videoFrame(dest);
		break;
	case VIDEO_FORMAT_NV12:
		libyuv::NV12ToI420(frame->data[0], static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		mailbox->push_outgoing_videoFrame(dest);
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
