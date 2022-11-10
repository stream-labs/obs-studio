#include "SourceRecordContext.h"
#include <string>

void SourceRecordContext::ensure_directory(char *path)
{
#ifdef _WIN32
	char *backslash = strrchr(path, '\\');
	if (backslash)
		*backslash = '/';
#endif

	char *slash = strrchr(path, '/');
	if (slash) {
		*slash = 0;
		os_mkdirs(path);
		*slash = '/';
	}

#ifdef _WIN32
	if (backslash)
		*backslash = '\\';
#endif
}

void SourceRecordContext::start_file_output(obs_data_t *settings)
{
	obs_data_t *s = obs_data_create();

	std::string filepath = std::string(obs_data_get_string(settings, "filepath"));
	std::string path = std::string(obs_data_get_string(settings, "filepath")) + "/" + obs_data_get_string(settings, "filename");

	ensure_directory(const_cast<char*>(filepath.c_str()));
	obs_data_set_string(s, "path", path.c_str());

	if (!m_fileOutput) {
		m_fileOutput = obs_output_create("ffmpeg_muxer", obs_source_get_name(m_source), s, NULL);
	} else {
		obs_output_update(m_fileOutput, s);
	}

	obs_data_release(s);

	if (m_encoder) {
		obs_encoder_set_video(m_encoder, m_video_output);
		obs_output_set_video_encoder(m_fileOutput, m_encoder);
	}

	if (m_aacTrack) {
		obs_encoder_set_audio(m_aacTrack, m_audio_output);
		obs_output_set_audio_encoder(m_fileOutput, m_aacTrack, 0);
	}

	if (m_start_file_output_thread.first.joinable())
		m_start_file_output_thread.first.join();

	m_start_file_output_thread.second = true;
	m_start_file_output_thread.first = std::thread(SourceRecordContext::start_file_output_thread, this, &m_start_file_output_thread.second);
}

void SourceRecordContext::refresh()
{
	auto settings = obs_source_get_settings(m_source);

	int audio_track = obs_data_get_int(settings, "audio_track");
	bool record = m_outputMode == SourceRecordContext::OUTPUT_MODE_RECORDING;

	if (record) {
		const char *enc_id = get_encoder_id(settings);
		if (!m_encoder || strcmp(obs_encoder_get_id(m_encoder), enc_id) != 0) {
			obs_encoder_release(m_encoder);
			m_encoder = obs_video_encoder_create(enc_id, obs_source_get_name(m_source), settings, NULL);
			obs_encoder_set_scaled_size(m_encoder, 0, 0);
			obs_encoder_set_video(m_encoder, m_video_output);

			if (m_fileOutput && obs_output_get_video_encoder(m_fileOutput) != m_encoder)
				obs_output_set_video_encoder(m_fileOutput, m_encoder);

		} else if (!obs_encoder_active(m_encoder)) {
			obs_encoder_update(m_encoder, settings);
		}

		if (!m_audio_output) {
			if (audio_track > 0) {
				m_audio_output = obs_get_audio();
			} else {
				struct audio_output_info oi = {0};
				oi.name = obs_source_get_name(m_source);
				oi.speakers = SPEAKERS_STEREO;
				oi.samples_per_sec = audio_output_get_sample_rate(obs_get_audio());
				oi.format = AUDIO_FORMAT_FLOAT_PLANAR;
				oi.input_param = this;
				oi.input_callback = audio_input_callback;
				audio_output_open(&m_audio_output, &oi);
			}
		} else if (audio_track > 0 && m_audio_track <= 0) {
			audio_output_close(m_audio_output);
			m_audio_output = obs_get_audio();
		} else if (audio_track <= 0 && m_audio_track > 0) {
			m_audio_output = NULL;
			struct audio_output_info oi = {0};
			oi.name = obs_source_get_name(m_source);
			oi.speakers = SPEAKERS_STEREO;
			oi.samples_per_sec = audio_output_get_sample_rate(obs_get_audio());
			oi.format = AUDIO_FORMAT_FLOAT_PLANAR;
			oi.input_param = this;
			oi.input_callback = audio_input_callback;
			audio_output_open(&m_audio_output, &oi);
		}

		if (!m_aacTrack || m_audio_track != audio_track) {
			if (m_aacTrack) {
				obs_encoder_release(m_aacTrack);
				m_aacTrack = NULL;
			}
			if (audio_track > 0) {
				m_aacTrack = obs_audio_encoder_create("ffmpeg_aac", obs_source_get_name(m_source), NULL, audio_track - 1, NULL);
			} else {
				m_aacTrack = obs_audio_encoder_create("ffmpeg_aac", obs_source_get_name(m_source), NULL, 0, NULL);
			}

			if (m_audio_output)
				obs_encoder_set_audio(m_aacTrack, m_audio_output);

			if (m_fileOutput)
				obs_output_set_audio_encoder(m_fileOutput, m_aacTrack, 0);
		}

		m_audio_track = audio_track;
	}

	const char *source_name = obs_data_get_string(settings, "audio_source");

	if (source_name != nullptr && strlen(source_name) > 0) {
		obs_source_t *source = obs_weak_source_get_source(m_audio_source);
		if (source)
			obs_source_release(source);
		if (!source || strcmp(source_name, obs_source_get_name(source)) != 0) {
			if (m_audio_source) {
				obs_weak_source_release(m_audio_source);
				m_audio_source = NULL;
			}
			source = obs_get_source_by_name(source_name);
			if (source) {
				m_audio_source = obs_source_get_weak_source(source);
				obs_source_release(source);
			}
		}

	} else if (m_audio_source) {
		obs_weak_source_release(m_audio_source);
		m_audio_source = NULL;
	}

	if (record != m_record) {

		if (record) {
			if (obs_source_enabled(m_source) && m_video_output)
				start_file_output(settings);
		} else {
			stop_fileOutput();
		}

		m_record = record;
	}

	obs_data_release(settings);
}

/*static*/
bool SourceRecordContext::is_encoder_available(const char *encoder)
{
	const char *val;
	int i = 0;

	while (obs_enum_encoder_types(i++, &val))
		if (strcmp(val, encoder) == 0)
			return true;

	return false;
}

/*static*/
void SourceRecordContext::calc_min_ts(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	uint64_t *min_ts = ((uint64_t *)param);
	if (!child || obs_source_audio_pending(child))
		return;
	const uint64_t ts = obs_source_get_audio_timestamp(child);
	if (!ts)
		return;
	if (!*min_ts || ts < *min_ts)
		*min_ts = ts;
}

/*static*/
void SourceRecordContext::mix_audio(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	if (!child || obs_source_audio_pending(child))
		return;
	const uint64_t ts = obs_source_get_audio_timestamp(child);
	if (!ts)
		return;

	obs_source_audio *mixed_audio = static_cast<obs_source_audio *>(param);
	const size_t pos = ns_to_audio_frames(mixed_audio->samples_per_sec, ts - mixed_audio->timestamp);

	if (pos > AUDIO_OUTPUT_FRAMES)
		return;

	const size_t count = AUDIO_OUTPUT_FRAMES - pos;

	struct obs_source_audio_mix child_audio;
	obs_source_get_audio_mix(child, &child_audio);
	for (size_t ch = 0; ch < mixed_audio->speakers; ch++) {
		float *out = ((float *)mixed_audio->data[ch]) + pos;
		float *in = child_audio.output[0].data[ch];
		if (!in)
			continue;
		for (size_t i = 0; i < count; i++) {
			out[i] += in[i];
		}
	}
}

/*static*/
bool SourceRecordContext::audio_input_callback(void *param, uint64_t start_ts_in, uint64_t end_ts_in, uint64_t *out_ts, uint32_t mixers,
					       struct audio_output_data *mixes)
{
	UNUSED_PARAMETER(end_ts_in);
	SourceRecordContext *filter = reinterpret_cast<SourceRecordContext *>(param);
	if (filter->m_closing || obs_source_removed(filter->m_source)) {
		*out_ts = start_ts_in;
		return true;
	}

	obs_source_t *audio_source = NULL;
	if (filter->m_audio_source) {
		audio_source = obs_weak_source_get_source(filter->m_audio_source);
		if (audio_source)
			obs_source_release(audio_source);
	} else {
		audio_source = obs_filter_get_parent(filter->m_source);
	}
	if (!audio_source || obs_source_removed(audio_source)) {
		*out_ts = start_ts_in;
		return true;
	}

	const uint32_t flags = obs_source_get_output_flags(audio_source);
	if ((flags & OBS_SOURCE_COMPOSITE) != 0) {
		uint64_t min_ts = 0;
		obs_source_enum_active_tree(audio_source, calc_min_ts, &min_ts);
		if (min_ts) {
			struct obs_source_audio mixed_audio = {0};
			for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
				mixed_audio.data[i] = (uint8_t *)mixes->data[i];
			}
			mixed_audio.timestamp = min_ts;
			mixed_audio.speakers = (speaker_layout)audio_output_get_channels(filter->m_audio_output);
			mixed_audio.samples_per_sec = audio_output_get_sample_rate(filter->m_audio_output);
			mixed_audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
			obs_source_enum_active_tree(audio_source, mix_audio, &mixed_audio);

			for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
				if ((mixers & (1 << mix_idx)) == 0)
					continue;
				// clamp audio
				for (size_t ch = 0; ch < mixed_audio.speakers; ch++) {
					float *mix_data = mixes[mix_idx].data[ch];
					float *mix_end = &mix_data[AUDIO_OUTPUT_FRAMES];

					while (mix_data < mix_end) {
						float val = *mix_data;
						val = (val > 1.0f) ? 1.0f : val;
						val = (val < -1.0f) ? -1.0f : val;
						*(mix_data++) = val;
					}
				}
			}
			*out_ts = min_ts;
		} else {
			*out_ts = start_ts_in;
		}
		return true;
	}
	if ((flags & OBS_SOURCE_AUDIO) == 0) {
		*out_ts = start_ts_in;
		return true;
	}

	const uint64_t source_ts = obs_source_get_audio_timestamp(audio_source);
	if (!source_ts) {
		*out_ts = start_ts_in;
		return true;
	}

	if (obs_source_audio_pending(audio_source))
		return false;

	struct obs_source_audio_mix audio;
	obs_source_get_audio_mix(audio_source, &audio);

	const size_t channels = audio_output_get_channels(filter->m_audio_output);
	for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
		if ((mixers & (1 << mix_idx)) == 0)
			continue;
		for (size_t ch = 0; ch < channels; ch++) {
			float *out = mixes[mix_idx].data[ch];
			float *in = audio.output[0].data[ch];
			if (!in)
				continue;
			for (size_t i = 0; i < AUDIO_OUTPUT_FRAMES; i++) {
				out[i] += in[i];
				if (out[i] > 1.0f)
					out[i] = 1.0f;
				if (out[i] < -1.0f)
					out[i] = -1.0f;
			}
		}
	}

	*out_ts = source_ts;
	return true;
}

/*static*/
const char *SourceRecordContext::get_encoder_id(obs_data_t *settings)
{
	const char *enc_id = obs_data_get_string(settings, "encoder");

	if (strcmp(enc_id, "qsv") == 0) {
		enc_id = "obs_qsv11";
	} else if (strcmp(enc_id, "amd") == 0) {
		enc_id = "amd_amf_h264";
	} else if (strcmp(enc_id, "nvenc") == 0) {
		//enc_id = EncoderAvailable("jim_nvenc") ? "jim_nvenc" : "ffmpeg_nvenc";
		enc_id = "ffmpeg_nvenc";
	} else if (strcmp(enc_id, "x264") == 0 || strcmp(enc_id, "x264_lowcpu") == 0) {
		enc_id = "obs_x264";
	}

	return enc_id;
}

void SourceRecordContext::stop_fileOutput()
{
	if (m_fileOutput) {
		if (m_force_stop_output_thread.joinable())
			m_force_stop_output_thread.join();
		m_force_stop_output_thread = std::thread(SourceRecordContext::force_stop_output_thread, m_fileOutput);
		m_fileOutput = NULL;
	}

	m_output_active = false;
	obs_source_dec_showing(obs_filter_get_parent(m_source));
}

void SourceRecordContext::join()
{
	if (m_start_file_output_thread.first.joinable())
		m_start_file_output_thread.first.join();

	if (m_force_stop_output_thread.joinable())
		m_force_stop_output_thread.join();
}

/*static*/
void SourceRecordContext::force_stop_output_thread(obs_output_t *fileOutput)
{
	obs_output_force_stop(fileOutput);
	obs_output_release(fileOutput);
}

/*static*/
void SourceRecordContext::start_file_output_thread(SourceRecordContext *context, bool *inUse)
{
	if (obs_output_start(context->m_fileOutput)) {
		if (!context->m_output_active) {
			context->m_output_active = true;
			obs_source_inc_showing(obs_filter_get_parent(context->m_source));
		}
	}

	inUse = false;
}
