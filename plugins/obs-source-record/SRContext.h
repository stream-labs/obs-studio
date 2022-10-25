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
		OUTPUT_MODE_ALWAYS = 1,
		OUTPUT_MODE_STREAMING = 2,
		OUTPUT_MODE_RECORDING = 3,
		OUTPUT_MODE_STREAMING_OR_RECORDING = 4,
		OUTPUT_MODE_VIRTUAL_CAMERA = 5
	};

public:
	static void start_file_output_thread(SourceRecordContext *context, bool *inUse);
	static void start_stream_output_thread(SourceRecordContext *context, bool *inUse);
	static void start_replay_thread(SourceRecordContext *context, bool *inUse);
	static void force_stop_output_thread(obs_output_t *fileOutput);

public:
	void stop_outputs();
	void stop_fileOutput();
	void stop_replayOutput();
	void stop_streamOutput();

	void joinAll();

public:
	std::pair<std::thread, bool> m_start_replay_thread;
	std::pair<std::thread, bool> m_start_stream_output_thread;
	std::pair<std::thread, bool> m_start_file_output_thread;
	std::thread m_force_stop_output_thread;

public:
	obs_source_t *source{nullptr};
	uint8_t *video_data{nullptr};
	uint32_t video_linesize{0};
	video_t *video_output{nullptr};
	audio_t *audio_output{nullptr};
	bool output_active{false};
	uint32_t width{0};
	uint32_t height{0};
	uint64_t last_frame_time_ns{0};
	gs_texrender_t *texrender{nullptr};
	gs_stagesurf_t *stagesurface{nullptr};
	bool restart{false};
	obs_output_t *fileOutput{nullptr};
	obs_output_t *streamOutput{nullptr};
	obs_output_t *replayOutput{nullptr};
	obs_encoder_t *encoder{nullptr};
	obs_encoder_t *aacTrack{nullptr};
	obs_service_t *service{nullptr};
	bool record{false};
	bool stream{false};
	bool replayBuffer{false};
	obs_hotkey_pair_id enableHotkey{0};
	int audio_track{0};
	obs_weak_source_t *audio_source{nullptr};
	bool closing{false};
	long long replay_buffer_duration{0};
};
