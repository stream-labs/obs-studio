#include "SourceRecordContext.h"
#include "util/dstr.h"
#include "media-io/video-frame.h"
#include "SourceRecordAPI.h"

#include <string>
#include <Windows.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("source-record", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Source Record Filter";
}

static const char *source_record_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Source Record";
}

static void source_record_filter_offscreen_render(void *data, uint32_t cx, uint32_t cy)
{
	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
	SourceRecordContext *filter = reinterpret_cast<SourceRecordContext *>(data);

	const uint64_t frame_time_ns = obs_get_video_frame_time();
	const int count = filter->m_last_frame_time_ns ? (int)((frame_time_ns - filter->m_last_frame_time_ns) / obs_get_frame_interval_ns()) : 1;
	filter->m_last_frame_time_ns = frame_time_ns;

	if (!count)
		return;

	if (filter->m_closing)
		return;
	if (!obs_source_enabled(filter->m_source))
		return;

	obs_source_t *parent = obs_filter_get_parent(filter->m_source);
	if (!parent)
		return;

	if (!filter->m_width || !filter->m_height)
		return;

	if (!filter->m_video_output || video_output_stopped(filter->m_video_output))
		return;

	gs_texrender_reset(filter->m_texrender);

	if (!gs_texrender_begin(filter->m_texrender, filter->m_width, filter->m_height))
		return;

	struct vec4 background;
	vec4_zero(&background);

	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, (float)filter->m_width, 0.0f, (float)filter->m_height, -100.0f, 100.0f);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	obs_source_video_render(parent);

	gs_blend_state_pop();
	gs_texrender_end(filter->m_texrender);

	struct video_frame output_frame;
	if (!video_output_lock_frame(filter->m_video_output, &output_frame, count, frame_time_ns))
		return;

	if (gs_stagesurface_get_width(filter->m_stagesurface) != filter->m_width || gs_stagesurface_get_height(filter->m_stagesurface) != filter->m_height) {
		gs_stagesurface_destroy(filter->m_stagesurface);
		filter->m_stagesurface = NULL;
	}
	if (filter->m_video_data) {
		gs_stagesurface_unmap(filter->m_stagesurface);
		filter->m_video_data = NULL;
	}
	if (!filter->m_stagesurface)
		filter->m_stagesurface = gs_stagesurface_create(filter->m_width, filter->m_height, GS_BGRA);

	gs_stage_texture(filter->m_stagesurface, gs_texrender_get_texture(filter->m_texrender));
	if (!gs_stagesurface_map(filter->m_stagesurface, &filter->m_video_data, &filter->m_video_linesize)) {
		video_output_unlock_frame(filter->m_video_output);
		return;
	}

	if (filter->m_video_data && filter->m_video_linesize) {
		const uint32_t linesize = output_frame.linesize[0];
		if (filter->m_video_linesize == linesize) {
			memcpy(output_frame.data[0], filter->m_video_data, linesize * filter->m_height);
		} else {
			for (uint32_t i = 0; i < filter->m_height; ++i) {
				const uint32_t dst_offset = linesize * i;
				const uint32_t src_offset = filter->m_video_linesize * i;
				memcpy(output_frame.data[0] + dst_offset, filter->m_video_data + src_offset, linesize);
			}
		}
	}

	video_output_unlock_frame(filter->m_video_output);
}

static void source_record_filter_update(void *data, obs_data_t *settings)
{
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	context->refresh();
}

static void source_record_filter_save(void *data, obs_data_t *settings)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(settings);
}

static void source_record_filter_defaults(obs_data_t *settings)
{
	const char *mode = obs_data_get_string(settings, "OutputMode");
	const char *type = obs_data_get_string(settings, "AdvOutRecType");
	const char *adv_path = strcmp(type, "Standard") != 0 || strcmp(type, "standard") != 0 ? obs_data_get_string(settings, "AdvOutFFFilePath")
											      : obs_data_get_string(settings, "AdvOutRecFilePath");
	bool adv_out = strcmp(mode, "Advanced") == 0 || strcmp(mode, "advanced") == 0;
	const char *rec_path = adv_out ? adv_path : obs_data_get_string(settings, "SimpleOutputFilePath");

	obs_data_set_default_string(settings, "path", rec_path);
	obs_data_set_default_string(settings, "filename_formatting", obs_data_get_string(settings, "OutputFilenameFormatting"));
	obs_data_set_default_string(settings, "rec_format",
				    obs_data_get_string(settings, std::string((adv_out ? "AdvOut" : "SimpleOutput") + std::string("RecFormat")).c_str()));

	const char *enc_id;
	if (adv_out) {
		enc_id = obs_data_get_string(settings, "AdvOutRecEncoder");
		if (strcmp(enc_id, "none") == 0 || strcmp(enc_id, "None") == 0)
			enc_id = obs_data_get_string(settings, "AdvOutEncoder");
		else if (strcmp(enc_id, "jim_nvenc") == 0)
			enc_id = "nvenc";
		else
			obs_data_set_default_string(settings, "encoder", enc_id);
	} else {
		const char *quality = obs_data_get_string(settings, "SimpleOutputRecQuality");
		if (strcmp(quality, "Stream") == 0 || strcmp(quality, "stream") == 0) {
			enc_id = obs_data_get_string(settings, "SimpleOutputStreamEncoder");
		} else if (strcmp(quality, "Lossless") == 0 || strcmp(quality, "lossless") == 0) {
			enc_id = "ffmpeg_output";
		} else {
			enc_id = obs_data_get_string(settings, "SimpleOutputRecEncoder");
		}
		obs_data_set_default_string(settings, "encoder", enc_id);
	}
	obs_data_set_default_int(settings, "replay_duration", 5);
}

static void *source_record_filter_create(obs_data_t *settings, obs_source_t *source)
{
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("Debugging Window:\n");

	SourceRecordContext *context = new SourceRecordContext{};
	context->m_source = source;
	context->m_texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	context->m_enableHotkey = OBS_INVALID_HOTKEY_PAIR_ID;

	source_record_filter_update(context, settings);
	obs_add_main_render_callback(source_record_filter_offscreen_render, context);

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(ph, "void func_load_device(in string input, out string output)", SourceRecordAPI::api_set_output_mode, context);
	return context;
}

static void source_record_filter_destroy(void *data)
{
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	context->m_closing = true;
	context->join();

	if (context->m_output_active) {
		obs_source_dec_showing(obs_filter_get_parent(context->m_source));
		context->m_output_active = false;
	}

	obs_remove_main_render_callback(source_record_filter_offscreen_render, context);

	context->stop_fileOutput();

	video_output_stop(context->m_video_output);

	if (context->m_enableHotkey != OBS_INVALID_HOTKEY_PAIR_ID)
		obs_hotkey_pair_unregister(context->m_enableHotkey);

	video_t *o = context->m_video_output;
	context->m_video_output = NULL;

	obs_encoder_release(context->m_aacTrack);
	obs_encoder_release(context->m_encoder);

	obs_weak_source_release(context->m_audio_source);
	context->m_audio_source = NULL;

	if (context->m_audio_track <= 0)
		audio_output_close(context->m_audio_output);

	video_output_close(o);

	obs_service_release(context->m_service);

	obs_enter_graphics();

	gs_stagesurface_unmap(context->m_stagesurface);
	gs_stagesurface_destroy(context->m_stagesurface);
	gs_texrender_destroy(context->m_texrender);

	obs_leave_graphics();

	delete context;
}

static bool source_record_enable_hotkey(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	if (!pressed)
		return false;

	if (obs_source_enabled(context->m_source))
		return false;

	obs_source_set_enabled(context->m_source, true);
	return true;
}

static bool source_record_disable_hotkey(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	if (!pressed)
		return false;
	if (!obs_source_enabled(context->m_source))
		return false;
	obs_source_set_enabled(context->m_source, false);
	return true;
}

static void source_record_filter_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	if (context->m_closing)
		return;

	obs_source_t *parent = obs_filter_get_parent(context->m_source);
	if (!parent)
		return;

	if (context->m_enableHotkey == OBS_INVALID_HOTKEY_PAIR_ID)
		context->m_enableHotkey = obs_hotkey_pair_register_source(parent, "source_record.enable", obs_module_text("SourceRecordEnable"),
									  "source_record.disable", obs_module_text("SourceRecordDisable"),
									  source_record_enable_hotkey, source_record_disable_hotkey, context, context);

	uint32_t width = obs_source_get_width(parent);
	width += (width & 1);
	uint32_t height = obs_source_get_height(parent);
	height += (height & 1);

	if (context->m_width != width || context->m_height != height || (!context->m_video_output && width && height)) {
		struct obs_video_info ovi = {0};
		obs_get_video_info(&ovi);

		struct video_output_info vi = {0};
		vi.format = VIDEO_FORMAT_BGRA;
		vi.width = width;
		vi.height = height;
		vi.fps_den = ovi.fps_den;
		vi.fps_num = ovi.fps_num;
		vi.cache_size = 16;
		vi.colorspace = VIDEO_CS_DEFAULT;
		vi.range = VIDEO_RANGE_DEFAULT;
		vi.name = obs_source_get_name(context->m_source);

		video_t *videoOutput = context->m_video_output;
		context->m_video_output = NULL;

		if (videoOutput) {
			video_output_stop(videoOutput);
			video_output_close(videoOutput);
		}

		if (video_output_open(&context->m_video_output, &vi) == VIDEO_OUTPUT_SUCCESS) {
			context->m_width = width;
			context->m_height = height;
			if (videoOutput)
				context->m_restart = true;
		}
	}

	if (context->m_restart && context->m_output_active) {
		context->stop_fileOutput();
		context->m_restart = false;

		// If not outputting, yet the source is enabled, then begin
	} else if (!context->m_output_active && obs_source_enabled(context->m_source)) {

		if (context->m_start_file_output_thread.second || !context->m_video_output) {
			// Unless we already are, or there's no video output
			return;
		}

		obs_data_t *s = obs_source_get_settings(context->m_source);

		if (context->m_record)
			context->start_file_output(s);

		obs_data_release(s);

	} else if (context->m_output_active && !obs_source_enabled(context->m_source)) {
		context->stop_fileOutput();
		obs_source_dec_showing(obs_filter_get_parent(context->m_source));
	}
}

static bool encoder_changed(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(property);
	UNUSED_PARAMETER(settings);
	obs_properties_remove_by_name(props, "encoder_group");
	const char *enc_id = SourceRecordContext::get_encoder_id(settings);
	obs_properties_t *enc_props = obs_get_encoder_properties(enc_id);
	if (enc_props) {
		obs_properties_add_group(props, "encoder_group", obs_encoder_get_display_name(enc_id), OBS_GROUP_NORMAL, enc_props);
	}
	return true;
}

static bool list_add_audio_sources(void *data, obs_source_t *source)
{
	obs_property_t *p = reinterpret_cast<obs_property_t *>(data);
	const uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_COMPOSITE) != 0) {
		obs_property_list_add_string(p, obs_source_get_name(source), obs_source_get_name(source));
	} else if ((flags & OBS_SOURCE_AUDIO) != 0) {
		obs_property_list_add_string(p, obs_source_get_name(source), obs_source_get_name(source));
	}
	return true;
}

static obs_properties_t *source_record_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_t *record = obs_properties_create();

	obs_property_t *p = obs_properties_add_list(record, "record_mode", obs_module_text("RecordMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(p, obs_module_text("None"), SourceRecordContext::OUTPUT_MODE_NONE);
	obs_property_list_add_int(p, obs_module_text("Recording"), SourceRecordContext::OUTPUT_MODE_RECORDING);

	obs_properties_add_text(record, "path", obs_module_text("Path"), OBS_TEXT_DEFAULT);
	obs_properties_add_text(record, "filename_formatting", obs_module_text("FilenameFormatting"), OBS_TEXT_DEFAULT);
	p = obs_properties_add_list(record, "rec_format", obs_module_text("RecFormat"), OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, "flv", "flv");
	obs_property_list_add_string(p, "mp4", "mp4");
	obs_property_list_add_string(p, "mov", "mov");
	obs_property_list_add_string(p, "mkv", "mkv");
	obs_property_list_add_string(p, "m3u8", "m3u8");
	obs_property_list_add_string(p, "ts", "ts");

	obs_properties_add_group(props, "record", obs_module_text("Record"), OBS_GROUP_NORMAL, record);

	obs_properties_t *replay = obs_properties_create();

	p = obs_properties_add_int(replay, "replay_duration", obs_module_text("Duration"), 1, 1000, 1);
	obs_property_int_set_suffix(p, "s");

	obs_properties_t *stream = obs_properties_create();

	p = obs_properties_add_list(stream, "stream_mode", obs_module_text("StreamMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(p, obs_module_text("None"), SourceRecordContext::OUTPUT_MODE_NONE);
	obs_property_list_add_int(p, obs_module_text("Recording"), SourceRecordContext::OUTPUT_MODE_RECORDING);

	obs_properties_t *audio = obs_properties_create();

	p = obs_properties_add_list(audio, "audio_track", obs_module_text("AudioTrack"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("None"), 0);
	const char *track = obs_module_text("Track");
	for (int i = 1; i <= MAX_AUDIO_MIXES; i++) {
		char buffer[64];
		snprintf(buffer, 64, "%s %i", track, i);
		obs_property_list_add_int(p, buffer, i);
	}

	p = obs_properties_add_list(audio, "audio_source", obs_module_text("Source"), OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_enum_sources(list_add_audio_sources, p);
	obs_enum_scenes(list_add_audio_sources, p);

	p = obs_properties_add_list(props, "encoder", obs_module_text("Encoder"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p, obs_module_text("Software"), "x264");
	if (SourceRecordContext::is_encoder_available("obs_qsv11"))
		obs_property_list_add_string(p, obs_module_text("QSV"), "qsv");
	if (SourceRecordContext::is_encoder_available("ffmpeg_nvenc"))
		obs_property_list_add_string(p, obs_module_text("NVENC"), "nvenc");
	if (SourceRecordContext::is_encoder_available("amd_amf_h264"))
		obs_property_list_add_string(p, obs_module_text("AMD"), "amd");

	const char *enc_id = NULL;
	size_t i = 0;
	while (obs_enum_encoder_types(i++, &enc_id)) {
		if (obs_get_encoder_type(enc_id) != OBS_ENCODER_VIDEO)
			continue;
		const uint32_t caps = obs_get_encoder_caps(enc_id);
		if ((caps & (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL)) != 0)
			continue;
		const char *name = obs_encoder_get_display_name(enc_id);
		obs_property_list_add_string(p, name, enc_id);
	}
	obs_property_set_modified_callback2(p, encoder_changed, data);

	obs_properties_t *group = obs_properties_create();
	obs_properties_add_group(props, "encoder_group", obs_module_text("Encoder"), OBS_GROUP_NORMAL, group);

	return props;
}

static void source_record_filter_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	obs_source_skip_video_filter(context->m_source);
}

static void source_record_filter_filter_remove(void *data, obs_source_t *parent)
{
	UNUSED_PARAMETER(parent);
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	context->m_closing = true;
	context->stop_fileOutput();

	obs_remove_main_render_callback(source_record_filter_offscreen_render, context);
}

bool obs_module_load(void)
{
	struct obs_source_info info = {};
	info.id = "source_record_filter";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = source_record_filter_get_name;
	info.create = source_record_filter_create;
	info.destroy = source_record_filter_destroy;
	info.update = source_record_filter_update;
	info.load = source_record_filter_update;
	info.save = source_record_filter_save;
	info.get_defaults = source_record_filter_defaults;
	info.video_render = source_record_filter_render;
	info.video_tick = source_record_filter_tick;
	info.get_properties = source_record_filter_properties;
	info.filter_remove = source_record_filter_filter_remove;

	obs_register_source(&info);
	return true;
}
