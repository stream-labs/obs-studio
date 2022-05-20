#include "MediaSoupTransceiver.h"
#include "MyAudioDeviceModule.h"
#include "MyFrameGeneratorInterface.h"

#include "common_audio/include/audio_util.h"
#include "api/create_peerconnection_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "rtc_base/random.h"

#include <obs-module.h>

#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "msdmo.lib")
#pragma comment(lib, "dmoguids.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "strmiids.lib")

/**
* MediaSoupTransceiver
*/

MediaSoupTransceiver::MediaSoupTransceiver(MediaSoupMailbox& mailbox) :
	m_mailbox(mailbox)
{
	// Temporary
	static bool doOnce = false;

	if (!doOnce)
	{
		AllocConsole();
		freopen("conin$", "r", stdin);
		freopen("conout$", "w", stdout);
		freopen("conout$", "w", stderr);
		printf("Debugging Window:\n");
		rtc::LogMessage::SetLogToStderr(true);
		rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
		doOnce = true;
	}

	m_device = std::make_unique<mediasoupclient::Device>();
}

MediaSoupTransceiver::~MediaSoupTransceiver()
{
	Stop();
}

bool MediaSoupTransceiver::LoadDevice(json& routerRtpCapabilities, json& output_deviceRtpCapabilities, json& output_deviceSctpCapabilities)
{
	try
	{
		m_factory_Producer = CreateProducerFactory();

		if (m_factory_Producer == nullptr)
			return false;

		m_factory_Consumer = CreateConsumerFactory();

		if (m_factory_Consumer == nullptr)
			return false;
		
		m_id = std::to_string(rtc::CreateRandomId());
		m_consumerOptions.factory = m_factory_Consumer.get();
		m_producerOptions.factory = m_factory_Producer.get();
		m_device->Load(routerRtpCapabilities, &m_producerOptions);

		output_deviceRtpCapabilities = m_device->GetRtpCapabilities();
		output_deviceSctpCapabilities = m_device->GetSctpCapabilities();
	}
	catch (...)
	{
		m_lastErorMsg = "Failed to load mediasoupclient::Device";
		return false;
	}

	return true;
}

bool MediaSoupTransceiver::CreateReceiver(const std::string& recvTransportId, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, nlohmann::json* sctpParameters /*= nullptr*/, nlohmann::json* iceServers /*= nullptr*/)
{
	try
	{
		m_consumerOptions.config.servers.clear();

		if (iceServers != nullptr)
		{
			blog(LOG_DEBUG, "MediaSoupTransceiver::CreateReceiver - Using iceServers %s", iceServers->dump().c_str());

			for (const auto& iceServerUri : *iceServers)
			{
				webrtc::PeerConnectionInterface::IceServer iceServer;
				iceServer.username = iceServerUri["username"].get<std::string>();
				iceServer.password = iceServerUri["credential"].get<std::string>();
				iceServer.urls = iceServerUri["urls"].get<std::vector<std::string>>();
				m_consumerOptions.config.servers.push_back(iceServer);
			}
		}
		else
		{
			blog(LOG_DEBUG, "MediaSoupTransceiver::CreateReceiver - Not using iceServers");
		}

		if (sctpParameters != nullptr)
		{
			blog(LOG_DEBUG, "MediaSoupTransceiver::CreateReceiver - Using sctpParameters %s", sctpParameters->dump().c_str());
			m_recvTransport = m_device->CreateRecvTransport(this, recvTransportId, iceParameters, iceCandidates, dtlsParameters, *sctpParameters, &m_consumerOptions);
		}
		else
		{
			blog(LOG_DEBUG, "MediaSoupTransceiver::CreateReceiver - Not using sctpParameters");
			m_recvTransport = m_device->CreateRecvTransport(this, recvTransportId, iceParameters, iceCandidates, dtlsParameters, &m_consumerOptions);
		}
	}
	catch (...)
	{
		m_lastErorMsg = "Unable to create the receive transport";
		return false;
	}

	return true;
}

bool MediaSoupTransceiver::CreateSender(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, nlohmann::json* iceServers /*= nullptr*/)
{
	try
	{
		m_producerOptions.config.servers.clear();

		if (iceServers != nullptr)
		{
			blog(LOG_DEBUG, "MediaSoupTransceiver::CreateSender - Using iceServers %s", iceServers->dump().c_str());

			for (const auto& iceServerUri : *iceServers)
			{
				webrtc::PeerConnectionInterface::IceServer iceServer;
				iceServer.username = iceServerUri["username"].get<std::string>();
				iceServer.password = iceServerUri["credential"].get<std::string>();
				iceServer.urls = iceServerUri["urls"].get<std::vector<std::string>>();
				m_producerOptions.config.servers.push_back(iceServer);
			}
		}
		else
		{
			blog(LOG_DEBUG, "MediaSoupTransceiver::CreateSender - Not using iceServers");
		}

		m_sendTransport = m_device->CreateSendTransport(this, id, iceParameters, iceCandidates, dtlsParameters, &m_producerOptions);
	}
	catch (...)
	{
		m_lastErorMsg = "Unable to create the send transport";
		return false;
	}

	return true;
}

// Fired for the first Transport::Consume() or Transport::Produce().
// Update the already created remote transport with the local DTLS parameters.
std::future<void> MediaSoupTransceiver::OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters)
{
	std::promise<void> promise;

	if ((m_recvTransport && transport->GetId() == m_recvTransport->GetId()) || (m_sendTransport && transport->GetId() == m_sendTransport->GetId()))
	{
		if (m_onConnect(m_owner, m_id, transport->GetId(), dtlsParameters))
			promise.set_value();
		else
			promise.set_exception(std::make_exception_ptr("OnConnect failed"));

		m_dtlsParameters_local = dtlsParameters;
	}
	else
	{
		promise.set_exception(std::make_exception_ptr((MediaSoupTransceiver::m_lastErorMsg = "Unknown transport requested to connect").c_str()));
	}

	return promise.get_future();
}

// Fired when a producer needs to be created in mediasoup.
// Retrieve the remote producer ID and feed the caller with it.
std::future<std::string> MediaSoupTransceiver::OnProduce(mediasoupclient::SendTransport* transport, const std::string& kind, nlohmann::json rtpParameters, const nlohmann::json& appData)
{
	std::promise<std::string> promise;
	std::string value;

	if (m_onProduce(m_owner, m_id, transport->GetId(), kind, rtpParameters, value))
		promise.set_value(value);
	else
		promise.set_exception(std::make_exception_ptr("OnProduce failed"));

	return promise.get_future();
}

bool MediaSoupTransceiver::CreateVideoProducerTrack()
{
	if (m_factory_Producer == nullptr)
	{
		m_lastErorMsg = "MediaSoupTransceiver::CreateVideoProducerTrack - Factory not yet created";
		return false;
	}

	if (m_uploadVideoReady)
	{
		m_lastErorMsg = "MediaSoupTransceiver::CreateVideoProducerTrack - Already exists";
		return false;
	}

	if (m_device->CanProduce("video"))
	{
		auto videoTrack = CreateVideoTrack(m_factory_Producer, std::to_string(rtc::CreateRandomId()));

		std::vector<webrtc::RtpEncodingParameters> encodings;
		encodings.emplace_back(webrtc::RtpEncodingParameters());
		encodings.emplace_back(webrtc::RtpEncodingParameters());
		encodings.emplace_back(webrtc::RtpEncodingParameters());

		if (auto ptr = m_sendTransport->Produce(this, videoTrack, &encodings, nullptr, nullptr))
		{
			m_uploadVideoReady = true;
			m_dataProducers["video"] = ptr;
		}
	}
	else
	{
		m_lastErorMsg = "MediaSoupTransceiver::CreateVideoProducerTrack - Cannot produce video";
		return false;
	}

	return true;
}

bool MediaSoupTransceiver::CreateAudioProducerTrack()
{
	if (m_device->CanProduce("audio"))
	{
		auto audioTrack = CreateAudioTrack(m_factory_Producer, std::to_string(rtc::CreateRandomId()));

		json codecOptions =
		{
			{ "opusStereo", true },
			{ "opusDtx",	true }
		};

		if (auto ptr = m_sendTransport->Produce(this, audioTrack, nullptr, &codecOptions, nullptr))
		{
			m_uploadAudioReady = true;
			m_dataProducers["audio"] = ptr;
			m_sendingAudio = true;
			m_audioThread = std::thread(&MediaSoupTransceiver::AudioThread, this);
		}
	}
	else
	{
		m_lastErorMsg = "MediaSoupTransceiver::CreateAudioProducerTrack - Cannot produce audio";
		return false;
	}

	return true;
}

bool MediaSoupTransceiver::CreateAudioConsumer(const std::string& id, const std::string& producerId, json* rtpParameters)
{
	try
	{
		m_dataConsumers["audio"] = m_recvTransport->Consume(this, id, producerId, "audio", rtpParameters);
	}
	catch (...)
	{
		m_lastErorMsg = "Unable to create the audio consumer";
		return false;
	}
	
	m_MyAudioSink = std::make_unique<MyAudioSink>();
	m_MyAudioSink->m_mailbox = &m_mailbox;

	auto ptr = m_dataConsumers["audio"]->GetTrack();
	auto track = dynamic_cast<webrtc::AudioTrackInterface*>(ptr);

	track->AddSink(m_MyAudioSink.get());
	return m_downloadAudioReady = true;
}

bool MediaSoupTransceiver::CreateVideoConsumer(const std::string& id, const std::string& producerId, json* rtpParameters)
{
	try
	{
		m_dataConsumers["video"] = m_recvTransport->Consume(this, id, producerId, "video", rtpParameters);
	}
	catch (...)
	{
		m_lastErorMsg = "Unable to create the video consumer";
		return false;
	}
	
	m_MyVideoSink = std::make_unique<MyVideoSink>();
	m_MyVideoSink->m_mailbox = &m_mailbox;

	auto ptr = m_dataConsumers["video"]->GetTrack();
	auto track = dynamic_cast<webrtc::VideoTrackInterface*>(ptr);
	
	rtc::VideoSinkWants videoSinkWants;
	track->AddOrUpdateSink(m_MyVideoSink.get(), videoSinkWants);
	return m_downloadVideoReady = true;
}

void MediaSoupTransceiver::AudioThread()
{
	webrtc::Random random_generator_(1);

	while (m_sendingAudio)
	{
		std::vector<std::unique_ptr<MediaSoupMailbox::SoupSendAudioFrame>> frames;
		m_mailbox.pop_outgoing_audioFrame(frames);

		if (!frames.empty())
		{
			uint32_t unused = 0;

			for (auto& itr : frames)
				m_MyProducerAudioDeviceModule->PlayData(itr->audio_data.data(), itr->numFrames, itr->bytesPerSample, itr->numChannels, itr->samples_per_sec, 0, 0, 0, false, unused);			
		}
		else
		{
			using namespace std::chrono;
			std::this_thread::sleep_for(1ms);
		}
	}
}

std::future<std::string> MediaSoupTransceiver::OnProduceData(mediasoupclient::SendTransport* sendTransport, const nlohmann::json& sctpStreamParameters,  const std::string& label, const std::string& protocol, const nlohmann::json& appData)
{ 
	std::promise<std::string> promise; 
	promise.set_value(""); 
	return promise.get_future(); 
};

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> MediaSoupTransceiver::CreateProducerFactory()
{
	m_networkThread_Producer = rtc::Thread::CreateWithSocketServer();
	m_signalingThread_Producer = rtc::Thread::Create();
	m_workerThread_Producer = rtc::Thread::Create();

	m_networkThread_Producer->SetName("MSTproducer_netthread", nullptr);
	m_signalingThread_Producer->SetName("MSTproducer_sigthread", nullptr);
	m_workerThread_Producer->SetName("MSTproducer_workthread", nullptr);

	if (!m_networkThread_Producer->Start() || !m_signalingThread_Producer->Start() || !m_workerThread_Producer->Start())
	{
		blog(LOG_ERROR, "MediaSoupTransceiver::CreateProducerFactory - webrtc thread start errored");
		return nullptr;
	}
	
	m_MyProducerAudioDeviceModule = new rtc::RefCountedObject<MyProducerAudioDeviceModule>{};
	
	auto factory = webrtc::CreatePeerConnectionFactory(
		m_networkThread_Producer.get(),
		m_workerThread_Producer.get(),
		m_signalingThread_Producer.get(),
		m_MyProducerAudioDeviceModule,
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		webrtc::CreateBuiltinVideoEncoderFactory(),
		webrtc::CreateBuiltinVideoDecoderFactory(),
		nullptr /*audio_mixer*/,
		nullptr /*audio_processing*/);

	if (!factory)
	{
		blog(LOG_ERROR, "MediaSoupTransceiver::CreateProducerFactory - webrtc error ocurred creating peerconnection factory");
		return nullptr;
	}

	return factory;
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> MediaSoupTransceiver::CreateConsumerFactory()
{
	m_networkThread_Consumer = rtc::Thread::CreateWithSocketServer();
	m_signalingThread_Consumer = rtc::Thread::Create();
	m_workerThread_Consumer = rtc::Thread::Create();

	m_networkThread_Consumer->SetName("MSTconsumer_netthread", nullptr);
	m_signalingThread_Consumer->SetName("MSTconsumer_sigthread", nullptr);
	m_workerThread_Consumer->SetName("MSTconsumerr_workthread", nullptr);

	if (!m_networkThread_Consumer->Start() || !m_signalingThread_Consumer->Start() || !m_workerThread_Consumer->Start())
	{
		blog(LOG_ERROR, "MediaSoupTransceiver::CreateConsumerFactory - webrtc thread start errored");
		return nullptr;
	}

	std::thread thr([&]()
		{
			m_DefaultDeviceCore_TaskQueue = webrtc::CreateDefaultTaskQueueFactory();
			m_DefaultDeviceCore = webrtc::AudioDeviceModule::Create(webrtc::AudioDeviceModule::kPlatformDefaultAudio, m_DefaultDeviceCore_TaskQueue.get());
		});

	thr.join();

	auto factory = webrtc::CreatePeerConnectionFactory(
		m_networkThread_Producer.get(),
		m_workerThread_Producer.get(),
		m_signalingThread_Producer.get(),
		m_DefaultDeviceCore,
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		webrtc::CreateBuiltinVideoEncoderFactory(),
		webrtc::CreateBuiltinVideoDecoderFactory(),
		nullptr /*audio_mixer*/,
		nullptr /*audio_processing*/);

	if (!factory)
	{
		blog(LOG_ERROR, "MediaSoupTransceiver::CreateFactory - webrtc error ocurred creating peerconnection factory");
		return nullptr;
	}

	return factory;
}

void MediaSoupTransceiver::SetSpeakerVolume(const uint32_t volume)
{
	if (m_DefaultDeviceCore != nullptr)
		m_DefaultDeviceCore->SetSpeakerVolume(volume);
}

void MediaSoupTransceiver::SetPlayoutDevice(const uint16_t id)
{
	if (m_DefaultDeviceCore != nullptr)
		m_DefaultDeviceCore->SetPlayoutDevice(id);
}

void MediaSoupTransceiver::GetPlayoutDevices(std::map<int16_t, std::string>& output)
{
	output.clear();

	if (m_DefaultDeviceCore == nullptr)
		return;

	int16_t numDevices = m_DefaultDeviceCore->PlayoutDevices();

	for (int16_t i = 0; i < numDevices; ++i)
	{
		char name[128];
		char guid[128];
		m_DefaultDeviceCore->PlayoutDeviceName(uint16_t(i), name, guid);
		output[i] = name;
	}
}

rtc::scoped_refptr<webrtc::AudioTrackInterface> MediaSoupTransceiver::CreateAudioTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& label)
{
	cricket::AudioOptions options;
	options.highpass_filter = true;
	options.auto_gain_control = false;
	options.noise_suppression = true;
	options.echo_cancellation = false;
	options.residual_echo_detector = false;
	options.experimental_agc = false;
	options.experimental_ns = false;
	options.typing_detection = false;

	rtc::scoped_refptr<webrtc::AudioSourceInterface> source = factory->CreateAudioSource(options);
	return factory->CreateAudioTrack(label, source);
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> MediaSoupTransceiver::CreateVideoTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& /*label*/)
{
	m_videoTrackSource = new rtc::RefCountedObject<FrameGeneratorCapturerVideoTrackSource>(FrameGeneratorCapturerVideoTrackSource::Config(), webrtc::Clock::GetRealTimeClock(), false, m_mailbox);
	m_videoTrackSource->Start();
	
	return factory->CreateVideoTrack(rtc::CreateRandomUuid(), m_videoTrackSource);
}

void MediaSoupTransceiver::StopConsumerById(const std::string& id)
{
	for (auto& itr : m_dataConsumers)
	{
		if (itr.second->GetId() == id)
		{
			itr.second->Close();
			delete itr.second;

			if (itr.first == "video")
				m_videoTrackSource = nullptr;

			break;
		}
	}
}

void MediaSoupTransceiver::StopReceiver()
{
	if (m_recvTransport)
		m_recvTransport->Close();

	for (auto& itr : m_dataConsumers)
	{
		itr.second->Close();
		delete itr.second;
	}
	
	delete m_recvTransport;
	m_recvTransport = nullptr;
	m_factory_Consumer = nullptr;

	m_networkThread_Consumer = nullptr;
	m_signalingThread_Consumer = nullptr;
	m_workerThread_Consumer = nullptr;

	m_videoTrackSource = nullptr;
}

void MediaSoupTransceiver::StopSender()
{	
	m_sendingAudio = false;

	if (m_audioThread.joinable())
		m_audioThread.join();

	if (m_sendTransport)
		m_sendTransport->Close();

	for (auto& itr : m_dataProducers)
	{
		itr.second->Close();
		delete itr.second;
	}

	delete m_sendTransport;
	m_sendTransport = nullptr;
	m_factory_Producer = nullptr;

	m_networkThread_Producer = nullptr;
	m_signalingThread_Producer = nullptr;
	m_workerThread_Producer = nullptr;
}
                                                                            
void MediaSoupTransceiver::Stop()
{
	m_sendingAudio = false;

	if (m_audioThread.joinable())
		m_audioThread.join();

	//if (m_videoTrackSource)
	//	m_videoTrackSource->Stop();

	if (m_recvTransport)
		m_recvTransport->Close();

	if (m_sendTransport)
		m_sendTransport->Close();

	for (auto& itr : m_dataProducers)
	{
		itr.second->Close();
		delete itr.second;
	}

	for (auto& itr : m_dataConsumers)
	{
		itr.second->Close();
		delete itr.second;
	}

	delete m_recvTransport;
	delete m_sendTransport;

	m_device = nullptr;
	m_factory_Producer = nullptr;
	m_factory_Consumer = nullptr;

	m_networkThread_Producer = nullptr;
	m_signalingThread_Producer = nullptr;
	m_workerThread_Producer = nullptr;

	m_networkThread_Consumer = nullptr;
	m_signalingThread_Consumer = nullptr;
	m_workerThread_Consumer = nullptr;
}

bool MediaSoupTransceiver::SenderReady()
{
	if (GetConnectionState(m_sendTransport) == "failed")
		return false;

	return m_sendTransport != nullptr;
}

bool MediaSoupTransceiver::ReceiverReady()
{
	if (GetConnectionState(m_recvTransport) == "failed")
		return false;

	return m_recvTransport != nullptr;
}

const std::string MediaSoupTransceiver::GetSenderId()
{
	if (!SenderReady())
		return "";

	return m_sendTransport->GetId();
}

const std::string MediaSoupTransceiver::GetReceiverId()
{
	if (!ReceiverReady())
		return "";

	return m_recvTransport->GetId();
}

const std::string MediaSoupTransceiver::PopLastError()
{
	std::string ret = m_lastErorMsg;
	m_lastErorMsg.clear();
	return ret;
}

std::string MediaSoupTransceiver::GetConnectionState(mediasoupclient::Transport* transport)
{
	std::lock_guard<std::mutex> grd(m_cstateMutex);
	auto itr = m_connectionState.find(transport);

	if (itr != m_connectionState.end())
		return itr->second;

	return "";
}

void MediaSoupTransceiver::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
	if (connectionState == "failed")
	{
		if (m_recvTransport == transport)
		{
			m_downloadAudioReady = false;
			m_downloadVideoReady = false;
		}

		if (m_sendTransport == transport)
		{
			m_uploadAudioReady = false;
			m_uploadVideoReady = false;
		}
	}

	std::lock_guard<std::mutex> grd(m_cstateMutex);
	m_connectionState[transport] = connectionState;
}

void MediaSoupTransceiver::OnTransportClose(mediasoupclient::Consumer* consumer)
{
	auto itr = m_dataConsumers.begin();

	while (itr != m_dataConsumers.end())
	{
		if (itr->second->GetId() == consumer->GetId())
		{
			if (itr->first == "video")
				m_videoTrackSource = nullptr;

			m_dataConsumers.erase(itr);
			return;
		}
		else
		{
			++itr;
		}
	}
}

void MediaSoupTransceiver::OnTransportClose(mediasoupclient::Producer* producer)
{
	auto itr = m_dataProducers.begin();

	while (itr != m_dataProducers.end())
	{
		if (itr->second->GetId() == producer->GetId())
		{
			m_dataProducers.erase(itr);
			return;
		}
		else
		{
			++itr;
		}
	}
}

bool MediaSoupTransceiver::UploadAudioReady() const
{
	return m_uploadAudioReady;
}

bool MediaSoupTransceiver::UploadVideoReady() const
{
	return m_uploadVideoReady;
}

bool MediaSoupTransceiver::DownloadAudioReady() const
{
	return m_downloadAudioReady;
}

bool MediaSoupTransceiver::DownloadVideoReady() const
{
	return m_downloadVideoReady;
}

/**
* Sinks 
*/

void MediaSoupTransceiver::MyVideoSink::OnFrame(const webrtc::VideoFrame& video_frame)
{
	// copy
	m_mailbox->push_received_videoFrame(std::make_unique<webrtc::VideoFrame>(video_frame));
}

void MediaSoupTransceiver::MyAudioSink::OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels, size_t number_of_frames, absl::optional<int64_t> absolute_capture_timestamp_ms)
{
	//std::unique_ptr<MediaSoupMailbox::SoupRecvAudioFrame> frame = std::make_unique<MediaSoupMailbox::SoupRecvAudioFrame>();
	//frame->bits_per_sample = bits_per_sample;
	//frame->sample_rate = sample_rate;
	//frame->number_of_channels = number_of_channels;
	//frame->number_of_frames = number_of_frames;
	//
	//if (absolute_capture_timestamp_ms.has_value())
	//	frame->absolute_capture_timestamp_ms = absolute_capture_timestamp_ms.value();
	//
	//size_t number_of_bytes = 0;
	//
	//switch (bits_per_sample)
	//{
	//case 8: number_of_bytes = number_of_channels * number_of_frames * sizeof(int8_t); break;
	//case 16: number_of_bytes = number_of_channels * number_of_frames * sizeof(int16_t); break;
	//case 32: number_of_bytes = number_of_channels * number_of_frames * sizeof(int32_t); break;
	//default: return;
	//}
	//
	//frame->audio_data.resize(number_of_bytes);
	//memcpy(frame->audio_data.data(), audio_data, number_of_bytes);
	//
	//m_mailbox->push_received_audioFrame(std::move(frame));
}

/**
* MediaSoupMailbox
*/

MediaSoupMailbox::~MediaSoupMailbox()
{

}

void MediaSoupMailbox::push_received_videoFrame(std::unique_ptr<webrtc::VideoFrame> ptr)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_video);

	// Overflow?
	if (m_received_video_frames.size() > 30)
		m_received_video_frames.erase(m_received_video_frames.begin());

	m_received_video_frames.push_back(std::move(ptr));
}

void MediaSoupMailbox::pop_receieved_videoFrames(std::vector<std::unique_ptr<webrtc::VideoFrame>>& output)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_video);
	m_received_video_frames.swap(output);
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
	to.format = AUDIO_FORMAT_16BIT_PLANAR;

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
