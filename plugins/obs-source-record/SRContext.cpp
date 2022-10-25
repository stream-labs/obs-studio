#include "SRContext.h"

void SourceRecordContext::stop_outputs()
{
	if (fileOutput) {
		if (m_force_stop_output_thread.joinable())
			m_force_stop_output_thread.join();
		m_force_stop_output_thread = std::thread(SourceRecordContext::force_stop_output_thread, fileOutput);
		fileOutput = NULL;
	}
	if (streamOutput) {
		if (m_force_stop_output_thread.joinable())
			m_force_stop_output_thread.join();
		m_force_stop_output_thread = std::thread(SourceRecordContext::force_stop_output_thread, streamOutput);
		streamOutput = NULL;
	}
	if (replayOutput) {
		if (m_force_stop_output_thread.joinable())
			m_force_stop_output_thread.join();
		m_force_stop_output_thread = std::thread(SourceRecordContext::force_stop_output_thread, replayOutput);
		replayOutput = NULL;
	}

	output_active = false;
	obs_source_dec_showing(obs_filter_get_parent(source));
}

void SourceRecordContext::stop_fileOutput()
{
	if (fileOutput) {
		if (m_force_stop_output_thread.joinable())
			m_force_stop_output_thread.join();
		m_force_stop_output_thread = std::thread(SourceRecordContext::force_stop_output_thread, fileOutput);
		fileOutput = NULL;
	}
}

void SourceRecordContext::stop_replayOutput()
{
	if (replayOutput) {
		if (m_force_stop_output_thread.joinable())
			m_force_stop_output_thread.join();
		m_force_stop_output_thread = std::thread(SourceRecordContext::force_stop_output_thread, replayOutput);
		replayOutput = NULL;
	}
}

void SourceRecordContext::stop_streamOutput()
{
	if (streamOutput) {
		if (m_force_stop_output_thread.joinable())
			m_force_stop_output_thread.join();
		m_force_stop_output_thread = std::thread(SourceRecordContext::force_stop_output_thread, streamOutput);
		streamOutput = NULL;
	}
}

void SourceRecordContext::joinAll()
{
	if (m_start_replay_thread.first.joinable())
		m_start_replay_thread.first.join();

	if (m_start_stream_output_thread.first.joinable())
		m_start_stream_output_thread.first.join();

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
	if (obs_output_start(context->fileOutput)) {
		if (!context->output_active) {
			context->output_active = true;
			obs_source_inc_showing(obs_filter_get_parent(context->source));
		}
	}

	inUse = false;
}

/*static*/
void SourceRecordContext::start_stream_output_thread(SourceRecordContext *context, bool *inUse)
{
	if (obs_output_start(context->streamOutput)) {
		if (!context->output_active) {
			context->output_active = true;
			obs_source_inc_showing(obs_filter_get_parent(context->source));
		}
	}

	inUse = false;
}

/*static*/
void SourceRecordContext::start_replay_thread(SourceRecordContext *context, bool *inUse)
{
	if (obs_output_start(context->replayOutput)) {
		if (!context->output_active) {
			context->output_active = true;
			obs_source_inc_showing(obs_filter_get_parent(context->source));
		}
	}

	inUse = false;
}
