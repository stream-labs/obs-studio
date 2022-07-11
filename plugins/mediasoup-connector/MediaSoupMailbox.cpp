#ifndef _DEBUG

#include "MediaSoupMailbox.h"

#include "common_audio/include/audio_util.h"

/**
* MediaSoupMailbox
*/

MediaSoupMailbox::~MediaSoupMailbox()
{

}

void MediaSoupMailbox::push_received_videoFrame(std::unique_ptr<webrtc::VideoFrame> ptr)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_video);
	m_received_video_frame = std::move(ptr);
}

void MediaSoupMailbox::pop_receieved_videoFrames(std::unique_ptr<webrtc::VideoFrame>& output)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_video);
	output = std::move(m_received_video_frame);
}

void MediaSoupMailbox::push_received_audioFrame(std::unique_ptr<SoupRecvAudioFrame> ptr)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_audio);
	m_received_audio_frames.push_back(std::move(ptr));

	// Overflow?
	if (m_received_audio_frames.size() > 256)
		m_received_audio_frames.erase(m_received_audio_frames.begin());
}

void MediaSoupMailbox::pop_receieved_audioFrames(std::vector<std::unique_ptr<SoupRecvAudioFrame>>& output)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_audio);
	m_received_audio_frames.swap(output);
}

void MediaSoupMailbox::assignOutgoingAudioParams(const audio_format audioformat, const speaker_layout speakerLayout, const int bytesPerSample, const int numChannels, const int samples_per_sec)
{
	if (m_obs_bytesPerSample == bytesPerSample && m_obs_numChannels == numChannels && m_obs_samples_per_sec == samples_per_sec && m_obs_audioformat == audioformat && m_obs_speakerLayout == speakerLayout)
		return;

	m_outgoing_audio_data.clear();
	m_outgoing_audio_data.resize(numChannels);
	m_obs_numFrames = 0;

	m_obs_bytesPerSample = bytesPerSample;
	m_obs_numChannels = numChannels;
	m_obs_samples_per_sec = samples_per_sec;
	m_obs_audioformat = audioformat;
	m_obs_speakerLayout = speakerLayout;

	if (m_obs_resampler != nullptr)
		audio_resampler_destroy(m_obs_resampler);

	struct resample_info from;
	struct resample_info to;

	from.samples_per_sec = m_obs_samples_per_sec;
	from.speakers = speakerLayout;
	from.format = audioformat;

	to.samples_per_sec = m_obs_samples_per_sec;
	to.speakers = speakerLayout;
	to.format = MediaSoupTransceiver::GetDefaultAudioFormat();

	m_obs_resampler = audio_resampler_create(&to, &from);
}

void MediaSoupMailbox::push_outgoing_audioFrame(const uint8_t** data, const int frames)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_audio);

	const int framesPer10ms = m_obs_samples_per_sec / 100;

	// Overflow?
	if (m_obs_numFrames > framesPer10ms * 256)
	{
		m_obs_numFrames = 0;

		for (auto& itr : m_outgoing_audio_data)
			itr.clear();
	}

	m_obs_numFrames += frames;

	int channelBufferSize = m_obs_bytesPerSample * frames;

	for (int channel = 0; channel < m_obs_numChannels; ++channel)
		m_outgoing_audio_data[channel].append((char*)data[channel], channelBufferSize);
}

void MediaSoupMailbox::pop_outgoing_audioFrame(std::vector<std::unique_ptr<SoupSendAudioFrame>>& output)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_audio);

	if (m_obs_bytesPerSample == 0 || m_obs_numChannels == 0 || m_obs_samples_per_sec == 0 || m_obs_numFrames == 0)
		return;

	const int framesPer10ms = m_obs_samples_per_sec / 100;

	while (m_obs_numFrames > framesPer10ms)
	{
		m_obs_numFrames -= framesPer10ms;

		std::unique_ptr<SoupSendAudioFrame> ptr = std::make_unique<SoupSendAudioFrame>();
		ptr->numFrames = framesPer10ms;
		ptr->numChannels = m_obs_numChannels;
		ptr->samples_per_sec = m_obs_samples_per_sec;
		ptr->bytesPerSample = sizeof(int16_t);

		// Pluck from the audio buffer and also convert to desired format
		uint8_t** array2d_float_raw = new uint8_t*[m_obs_numChannels];

		for (size_t channel = 0; channel < m_obs_numChannels; ++channel)
		{
			const int bytesFromFloatBuffer = m_obs_bytesPerSample * framesPer10ms;
			
			array2d_float_raw[channel] = new uint8_t[bytesFromFloatBuffer];

			auto& channelBuffer_Float = m_outgoing_audio_data[channel];
			memcpy(array2d_float_raw[channel], channelBuffer_Float.data(), bytesFromFloatBuffer);
			channelBuffer_Float.erase(channelBuffer_Float.begin(), channelBuffer_Float.begin() + bytesFromFloatBuffer);
		}

		uint32_t numFrames = 0;
		uint64_t tOffset = 0;
		uint8_t* array2d_int16_raw[MAX_AV_PLANES];

		if (audio_resampler_resample(m_obs_resampler, array2d_int16_raw, &numFrames, &tOffset, array2d_float_raw, framesPer10ms))
		{
			ptr->audio_data.resize(framesPer10ms * m_obs_numChannels);
			webrtc::Interleave((int16_t**)array2d_int16_raw, ptr->numFrames, ptr->numChannels, ptr->audio_data.data());
		}

		for (size_t channel = 0; channel < m_outgoing_audio_data.size(); ++channel)
			delete[] array2d_float_raw[channel];

		delete[] array2d_float_raw;

		output.push_back(std::move(ptr));
	}
}

void MediaSoupMailbox::push_outgoing_videoFrame(rtc::scoped_refptr<webrtc::I420Buffer> ptr)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_video);
	
	// Overflow?
	if (m_outgoing_video_data.size() > 30)
		m_outgoing_video_data.clear();

	m_outgoing_video_data.push_back(ptr);
}

void MediaSoupMailbox::pop_outgoing_videoFrame(std::vector<rtc::scoped_refptr<webrtc::I420Buffer>>& output)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_video);
	m_outgoing_video_data.swap(output);
}

#endif
