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
		OUTPUT_MODE_STREAMING = 1,
		OUTPUT_MODE_RECORDING = 2,
		OUTPUT_MODE_STREAMING_OR_RECORDING = 3,
	};

public:
	void stop_outputs();
	void stop_fileOutput();
	void stop_replayOutput();
	void stop_streamOutput();
	void joinAll();
	void refresh_recording_streaming();

public:
	static void start_file_output_thread(SourceRecordContext *context, bool *inUse);
	static void start_stream_output_thread(SourceRecordContext *context, bool *inUse);
	static void start_replay_thread(SourceRecordContext *context, bool *inUse);
	static void force_stop_output_thread(obs_output_t *fileOutput);

public:
	std::pair<std::thread, bool> m_start_replay_thread;
	std::pair<std::thread, bool> m_start_stream_output_thread;
	std::pair<std::thread, bool> m_start_file_output_thread;
	std::thread m_force_stop_output_thread;
	OutputMode m_outputMode{OUTPUT_MODE_NONE};

public:
	obs_source_t *source{nullptr};
	video_t *video_output{nullptr};
	audio_t *audio_output{nullptr};
	gs_texrender_t *texrender{nullptr};
	gs_stagesurf_t *stagesurface{nullptr};
	obs_hotkey_pair_id enableHotkey{0};
	obs_weak_source_t *audio_source{nullptr};

	obs_output_t *fileOutput{nullptr};
	obs_output_t *streamOutput{nullptr};
	obs_output_t *replayOutput{nullptr};
	obs_encoder_t *encoder{nullptr};
	obs_encoder_t *aacTrack{nullptr};
	obs_service_t *service{nullptr};

	uint8_t *video_data{nullptr};
	uint32_t video_linesize{0};
	uint32_t width{0};
	uint32_t height{0};
	uint64_t last_frame_time_ns{0};

	bool output_active{false};
	bool restart{false};
	bool record{false};
	bool stream{false};
	bool closing{false};
	bool replayBuffer{false};

	int audio_track{0};

	long long replay_buffer_duration{0};
};
