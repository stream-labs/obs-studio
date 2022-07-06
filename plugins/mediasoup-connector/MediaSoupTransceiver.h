#pragma once

#include "Device.hpp"
#include "Logger.hpp"

#include <json.hpp>
#include <atomic>
#include <media-io\audio-io.h>
#include <media-io\audio-resampler.h>
#include <util\platform.h>

#include "api/video/i420_buffer.h"
#include "modules/audio_device/win/audio_device_core_win.h"

namespace mediasoupclient
{
	void Initialize();     // NOLINT(readability-identifier-naming)
	void Cleanup();        // NOLINT(readability-identifier-naming)
	std::string Version(); // NOLINT(readability-identifier-naming)
} // namespace mediasoupclient

using json = nlohmann::json;

class MediaSoupMailbox;
class MediaSoupTransceiver;
class MyProducerAudioDeviceModule;
class FrameGeneratorCapturerVideoTrackSource;
class MediaSoupInterface;

/**
* MediaSoupTransceiver
*/

class MediaSoupTransceiver : public
                    mediasoupclient::RecvTransport::Listener,
                    mediasoupclient::SendTransport::Listener,
                    mediasoupclient::Consumer::Listener,
                    mediasoupclient::Producer::Listener
{
public:
	MediaSoupTransceiver(MediaSoupMailbox& mailbox);
	~MediaSoupTransceiver();
	
	bool LoadDevice(json& routerRtpCapabilities, json& output_deviceRtpCapabilities, json& outpudet_viceSctpCapabilities);
	bool CreateReceiver(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, nlohmann::json* sctpParameters = nullptr, nlohmann::json* iceServers = nullptr);
	bool CreateSender(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, nlohmann::json* iceServers = nullptr);
	bool CreateAudioConsumer(const std::string& id, const std::string& producerId, json* rtpParameters);
	bool CreateVideoConsumer(const std::string& id, const std::string& producerId, json* rtpParameters);
	bool CreateVideoProducerTrack(const nlohmann::json* ebcodings = nullptr, const nlohmann::json* codecOptions = nullptr,  const nlohmann::json* codec = nullptr);
	bool CreateAudioProducerTrack();

	bool UploadAudioReady() const;
	bool UploadVideoReady() const;
	bool DownloadAudioReady() const;
	bool DownloadVideoReady() const;

	bool SenderCreated();
	bool ReceiverCreated();
	bool SenderConnected();
	bool ReceiverConnected();

	void RegisterOnConnect(std::function<bool(MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const json& dtlsParameters)> func) { m_onConnect = func; }
	void RegisterOnProduce(std::function<bool(MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const std::string& kind, const json& rtpParameters, std::string& output_value)> func) { m_onProduce = func; }

	void StopReceiver();
	void StopSender();
	void StopConsumerByProducerId(const std::string& id);
	void SetSpeakerVolume(const uint32_t volume);
	void GetPlayoutDevices(std::map<int16_t, std::string>& output);
	void SetPlayoutDevice(const uint16_t id);
	
	const std::string GetRtpCapabilities();
	const std::string GetSctpCapabilities();
	const std::string GetSenderId();
	const std::string GetReceiverId();
	const std::string PopLastError();
	const std::string& GetId() const { return m_id; }
	
	MediaSoupInterface* m_owner{ nullptr };

	static audio_format GetDefaultAudioFormat() { return AUDIO_FORMAT_16BIT_PLANAR; }

public:
	// SendTransport
	// RecvTransport
	std::future<void> OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters) override;
	void OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState) override;

public:
	// SendTransport
	std::future<std::string> OnProduceData(mediasoupclient::SendTransport* sendTransport, const nlohmann::json& sctpStreamParameters, const std::string& label, const std::string& protocol, const nlohmann::json& appData) override;
	std::future<std::string> OnProduce(mediasoupclient::SendTransport* /*transport*/, const std::string& kind, nlohmann::json rtpParameters, const nlohmann::json& appData) override;

public:
	// Producer
	// Consumer
	void OnTransportClose(mediasoupclient::Producer* producer) override;
	void OnTransportClose(mediasoupclient::Consumer* dataConsumer) override;

private:
	void Stop();
	void AudioThread();

	std::string GetConnectionState(mediasoupclient::Transport* transport);

	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> CreateProducerFactory();
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> CreateConsumerFactory();

	rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& /*label*/);
	rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& /*label*/);

	json m_dtlsParameters_local;
	
	std::string m_id;
	std::string m_myBroadcasterId;
	std::string m_mediasoupVersion;
	std::string m_lastErorMsg;
	std::thread m_audioThread;
	std::atomic<bool> m_sendingAudio{ false };
	
	std::map<std::string, std::string> m_hostProducerIds;
	std::map<std::string, mediasoupclient::Consumer*> m_dataConsumers;
	std::map<std::string, mediasoupclient::Producer*> m_dataProducers;
	
	std::function<bool(MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const json& dtlsParameters)> m_onConnect;
	std::function<bool(MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const std::string& kind, const json& rtpParameters, std::string& output_value)> m_onProduce;
	
	std::unique_ptr<mediasoupclient::Device> m_device;

	mediasoupclient::RecvTransport* m_recvTransport{ nullptr };
	mediasoupclient::SendTransport* m_sendTransport{ nullptr };

	std::mutex m_cstateMutex;
	std::map<mediasoupclient::Transport*, std::string> m_connectionState;

private:
	std::atomic<bool> m_uploadAudioReady{ false };
	std::atomic<bool> m_uploadVideoReady{ false };
	std::atomic<bool> m_downloadAudioReady{ false };
	std::atomic<bool> m_downloadVideoReady{ false };

	MediaSoupMailbox& m_mailbox;

private:
	class MyAudioSink : public webrtc::AudioTrackSinkInterface
	{
	public:
		void OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels, size_t number_of_frames, absl::optional<int64_t> absolute_capture_timestamp_ms) override;
		MediaSoupMailbox* m_mailbox{ nullptr };
	};

	class MyVideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame>
	{
	public:
		void OnFrame(const webrtc::VideoFrame& video_frame) override;
		MediaSoupMailbox* m_mailbox{ nullptr };
	};
	
	std::unique_ptr<MyAudioSink> m_MyAudioSink;
	std::unique_ptr<MyVideoSink> m_MyVideoSink;

private:
	rtc::scoped_refptr<MyProducerAudioDeviceModule> m_MyProducerAudioDeviceModule;
	rtc::scoped_refptr<webrtc::AudioDeviceModule> m_DefaultDeviceCore;
	std::unique_ptr<webrtc::TaskQueueFactory> m_DefaultDeviceCore_TaskQueue;
	FrameGeneratorCapturerVideoTrackSource* m_videoTrackSource{ nullptr };

// Producer
private:
	// MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
	// Use plain pointers in order to avoid threads being destructed before tracks.
	std::unique_ptr<rtc::Thread> m_networkThread_Producer{ nullptr };
	std::unique_ptr<rtc::Thread> m_signalingThread_Producer{ nullptr };
	std::unique_ptr<rtc::Thread> m_workerThread_Producer{ nullptr };

	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_factory_Producer;
	
	mediasoupclient::PeerConnection::Options m_producerOptions;

// Consumer
private:
	std::unique_ptr<rtc::Thread> m_networkThread_Consumer{ nullptr };
	std::unique_ptr<rtc::Thread> m_signalingThread_Consumer{ nullptr };
	std::unique_ptr<rtc::Thread> m_workerThread_Consumer{ nullptr };

	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_factory_Consumer;
	
	mediasoupclient::PeerConnection::Options m_consumerOptions;
};

/**
* MediaSoupMailbox
*/

class MediaSoupMailbox
{
public:
	struct SoupRecvAudioFrame
	{
		std::vector<BYTE> audio_data;
		int bits_per_sample = 0;
		int sample_rate = 0;
		size_t number_of_channels = 0;
		size_t number_of_frames = 0;
		int64_t absolute_capture_timestamp_ms = 0;
		uint64_t timestamp = os_gettime_ns();
	};

	// 10ms frame
	struct SoupSendAudioFrame
	{
		std::vector<int16_t> audio_data;
		int numFrames = 0;
		int numChannels = 0;
		int bytesPerSample = 0;
		int samples_per_sec = 0;
	};
public:
	~MediaSoupMailbox();

	// todo: used for if the streamer wants to broadcast audio data directly
	//	webrtc has to be playing audio to a device, but if that isn't the device being captured by obs then we have to broadcast the data with this plugin
	void setAcceptingIncomingAudio(const bool v) { m_acceptingIncomingAudio = v; }
	bool isAcceptingIncomingAudio() const { return m_acceptingIncomingAudio; }

public:
	// Receive
	void push_received_videoFrame(std::unique_ptr<webrtc::VideoFrame> ptr);
	void push_received_audioFrame(std::unique_ptr<SoupRecvAudioFrame> ptr);

	void pop_receieved_videoFrames(std::unique_ptr<webrtc::VideoFrame>& output);
	void pop_receieved_audioFrames(std::vector<std::unique_ptr<SoupRecvAudioFrame>>& output);

public:
	// Outgoing
	void push_outgoing_videoFrame(rtc::scoped_refptr<webrtc::I420Buffer>);
	void pop_outgoing_videoFrame(std::vector<rtc::scoped_refptr<webrtc::I420Buffer>>& output);

	void push_outgoing_audioFrame(const uint8_t** data, const int frames);
	void pop_outgoing_audioFrame(std::vector<std::unique_ptr<SoupSendAudioFrame>>& output);

	void assignOutgoingAudioParams(const audio_format audioformat, const speaker_layout speakerLayout, const int bytesPerSample, const int numChannels, const int samples_per_sec);

private:
	// Receive
	std::mutex m_mtx_received_video;
	std::mutex m_mtx_received_audio;
	
	std::unique_ptr<webrtc::VideoFrame> m_received_video_frame;
	std::vector<std::unique_ptr<SoupRecvAudioFrame>> m_received_audio_frames;

private:
	// Outgoing
	std::mutex m_mtx_outgoing_video;
	std::mutex m_mtx_outgoing_audio;

	std::vector<rtc::scoped_refptr<webrtc::I420Buffer>> m_outgoing_video_data;
	std::vector<std::string> m_outgoing_audio_data;

	int m_obs_bytesPerSample = 0;
	int m_obs_numChannels = 0;
	int m_obs_samples_per_sec = 0;
	int m_obs_numFrames = 0;
	audio_format m_obs_audioformat = AUDIO_FORMAT_UNKNOWN;
	speaker_layout m_obs_speakerLayout = SPEAKERS_UNKNOWN;
	audio_resampler_t* m_obs_resampler = nullptr;

	bool m_acceptingIncomingAudio = true;
};
