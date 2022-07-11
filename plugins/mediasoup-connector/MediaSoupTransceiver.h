#pragma once

#include "Device.hpp"
#include "Logger.hpp"

#include <json.hpp>
#include <atomic>
#include <media-io\audio-io.h>
#include <media-io\audio-resampler.h>
#include <util\platform.h>

#include "api/video/i420_buffer.h"

namespace mediasoupclient
{
	void Initialize();     // NOLINT(readability-identifier-naming)
	void Cleanup();        // NOLINT(readability-identifier-naming)
	std::string Version(); // NOLINT(readability-identifier-naming)
} // namespace mediasoupclient

using json = nlohmann::json;

class MediaSoupMailbox;
class MediaSoupInterface;
class MediaSoupTransceiver;
class MyProducerAudioDeviceModule;
class FrameGeneratorCapturerVideoTrackSource;

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
	enum ConsumerType
	{
		ConsumerError,
		ConsumerAudio,
		ConsumerVideo,
	};

public:
	MediaSoupTransceiver();
	~MediaSoupTransceiver();
	
	bool LoadDevice(json& routerRtpCapabilities, json& output_deviceRtpCapabilities, json& outpudet_viceSctpCapabilities);
	bool CreateReceiver(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, nlohmann::json* sctpParameters = nullptr, nlohmann::json* iceServers = nullptr);
	bool CreateSender(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, nlohmann::json* iceServers = nullptr);
	bool CreateAudioConsumer(const std::string& id, const std::string& producerId, json* rtpParameters);
	bool CreateVideoConsumer(const std::string& id, const std::string& producerId, json* rtpParameters);
	bool CreateVideoProducerTrack(const nlohmann::json* ebcodings = nullptr, const nlohmann::json* codecOptions = nullptr,  const nlohmann::json* codec = nullptr);
	bool CreateAudioProducerTrack();

	bool AudioProducerReady();
	bool VideoProducerReady();
	bool ConsumerReady(const std::string& id);
	bool ConsumerReadyAtLeastOne();

	bool SenderCreated();
	bool ReceiverCreated();
	bool SenderConnected();
	bool ReceiverConnected();

	void StopReceiveTransport();
	void StopSendTransport();
	void StopConsumerById(const std::string& id);
	void StopConsumerByProducerId(const std::string& id);

	MediaSoupMailbox* GetConsumerMailbox(const std::string& id);
	MediaSoupMailbox* GetProducerMailbox() { return m_producerMailbox.get(); }
	mediasoupclient::Device* GetDevice() const { return m_device.get(); }

	const std::string GetSenderId();
	const std::string GetReceiverId();
	const std::string PopLastError();
	const std::string& GetId() const { return m_id; }

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

	rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateProducerVideoTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& /*label*/);
	rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateProducerAudioTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& /*label*/);

	json m_dtlsParameters_local;
	
	std::string m_id;
	std::string m_myBroadcasterId;
	std::string m_mediasoupVersion;
	std::string m_lastErorMsg;
	std::thread m_audioThread;
	std::atomic<bool> m_sendingAudio{ false };
	
	std::mutex m_stateMutex;
	std::mutex m_externalMutex;

	std::unique_ptr<mediasoupclient::Device> m_device;

	mediasoupclient::RecvTransport* m_recvTransport{ nullptr };
	mediasoupclient::SendTransport* m_sendTransport{ nullptr };

	std::map<mediasoupclient::Transport*, std::string> m_connectionState;

// Sinks
private:
	class GenericSink
	{
	public:
		virtual ~GenericSink() {}
		ConsumerType m_consumerType;
		std::unique_ptr<MediaSoupMailbox> m_mailbox;
	};

	class MyAudioSink : public webrtc::AudioTrackSinkInterface, public GenericSink
	{
	public:
		void OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels, size_t number_of_frames, absl::optional<int64_t> absolute_capture_timestamp_ms) override;
	};

	class MyVideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame>, public GenericSink
	{
	public:
		void OnFrame(const webrtc::VideoFrame& video_frame) override;
	};

	rtc::scoped_refptr<MyProducerAudioDeviceModule> m_MyProducerAudioDeviceModule;
	rtc::scoped_refptr<webrtc::AudioDeviceModule> m_DefaultDeviceCore;
	std::unique_ptr<webrtc::TaskQueueFactory> m_DefaultDeviceCore_TaskQueue;
	std::unique_ptr<MediaSoupMailbox> m_producerMailbox;

// Producer
private:
	// MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
	// Use plain pointers in order to avoid threads being destructed before tracks.
	std::unique_ptr<rtc::Thread> m_networkThread_Producer{ nullptr };
	std::unique_ptr<rtc::Thread> m_signalingThread_Producer{ nullptr };
	std::unique_ptr<rtc::Thread> m_workerThread_Producer{ nullptr };
	
	mediasoupclient::PeerConnection::Options m_producerOptions;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_factory_Producer;

	// Producers
	mediasoupclient::Producer* m_dataProducer_Audio{ nullptr };
	mediasoupclient::Producer* m_dataProducer_Video{ nullptr };


// Consumer
private:
	std::unique_ptr<rtc::Thread> m_networkThread_Consumer{ nullptr };
	std::unique_ptr<rtc::Thread> m_signalingThread_Consumer{ nullptr };
	std::unique_ptr<rtc::Thread> m_workerThread_Consumer{ nullptr };
	
	mediasoupclient::PeerConnection::Options m_consumerOptions;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_factory_Consumer;
	
	// id, Consumers
	std::map<std::string, std::pair<mediasoupclient::Consumer*, std::unique_ptr<GenericSink>>> m_dataConsumers;

// Thread safe assignment
private:
	void AssignProducer(mediasoupclient::Producer*& ref, mediasoupclient::Producer* value);
	void AssignConsumer(const std::string& id, mediasoupclient::Consumer* value, std::unique_ptr<GenericSink> sink);

	std::recursive_mutex m_consumerMutex;
	std::recursive_mutex m_producerMutex;
};
