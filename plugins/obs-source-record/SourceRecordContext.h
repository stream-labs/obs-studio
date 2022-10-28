#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <util/threading.h>
#include <thread>
#include <vector>

#define FTL_PROTOCOL "ftl"
#define RTMP_PROTOCOL "rtmp"

class SourceRecordContext {
public:
	enum OutputMode {
		OUTPUT_MODE_NONE = 0,
		OUTPUT_MODE_RECORDING = 1,
	};

public:
	void start_file_output(obs_data_t *settings);
	void stop_fileOutput();
	void join();
	void refresh();

public:
	static const char *get_encoder_id(obs_data_t *settings);
	static bool is_encoder_available(const char *encoder);

public:
	std::thread m_force_stop_output_thread;
	std::pair<std::thread, bool> m_start_file_output_thread;

	OutputMode m_outputMode{OUTPUT_MODE_NONE};

	obs_source_t *m_source{nullptr};
	video_t *m_video_output{nullptr};
	audio_t *m_audio_output{nullptr};
	gs_texrender_t *m_texrender{nullptr};
	gs_stagesurf_t *m_stagesurface{nullptr};
	obs_hotkey_pair_id m_enableHotkey{0};
	obs_weak_source_t *m_audio_source{nullptr};

	obs_output_t *m_fileOutput{nullptr};
	obs_encoder_t *m_encoder{nullptr};
	obs_encoder_t *m_aacTrack{nullptr};
	obs_service_t *m_service{nullptr};

	uint8_t *m_video_data{nullptr};
	uint32_t m_video_linesize{0};
	uint32_t m_width{0};
	uint32_t m_height{0};
	uint64_t m_last_frame_time_ns{0};

	bool m_output_active{false};
	bool m_restart{false};
	bool m_record{false};
	bool m_closing{false};

	int m_audio_track{0};

private:
	void ensure_directory(char *path);

private:
	static bool audio_input_callback(void *param, uint64_t start_ts_in, uint64_t end_ts_in, uint64_t *out_ts, uint32_t mixers,
					 struct audio_output_data *mixes);
	static void calc_min_ts(obs_source_t *parent, obs_source_t *child, void *param);
	static void mix_audio(obs_source_t *parent, obs_source_t *child, void *param);

private:
	// Disk interaction might be slow
	static void start_file_output_thread(SourceRecordContext *context, bool *inUse);
	static void force_stop_output_thread(obs_output_t *fileOutput);
};
