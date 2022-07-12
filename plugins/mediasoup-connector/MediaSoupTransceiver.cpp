#ifndef _DEBUG

#include "MediaSoupTransceiver.h"
#include "MyFrameGeneratorInterface.h"
#include "MyProducerAudioDeviceModule.h"
#include "MediaSoupMailbox.h"
#include "ConnectorFrontApi.h"

#include "api/create_peerconnection_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/audio_device_impl.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "common_audio/include/audio_util.h"
#include "rtc_base/random.h"

#ifdef _WIN32
	#pragma comment(lib, "Secur32.lib")
	#pragma comment(lib, "Winmm.lib")
	#pragma comment(lib, "msdmo.lib")
	#pragma comment(lib, "dmoguids.lib")
	#pragma comment(lib, "wmcodecdspuuid.lib")
	#pragma comment(lib, "strmiids.lib")
#endif

/**
* MediaSoupTransceiver
*/

MediaSoupTransceiver::MediaSoupTransceiver() 
{
	m_producerMailbox = std::make_unique<MediaSoupMailbox>();
}

MediaSoupTransceiver::~MediaSoupTransceiver()
{
	Stop();
}

bool MediaSoupTransceiver::LoadDevice(json& routerRtpCapabilities, json& output_deviceRtpCapabilities, json& output_deviceSctpCapabilities)
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_device != nullptr)
	{
		m_lastErorMsg = "Reciver transport already exists";
		return false;
	}

	m_device = std::make_unique<mediasoupclient::Device>();

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

bool MediaSoupTransceiver::CreateReceiver(const std::string& recvTransportId, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, nlohmann::json* sctpParameters /*= nullptr*/, nlohmann::json* iceServers /*= nullptr*/)
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_recvTransport != nullptr)
	{
		m_lastErorMsg = "Reciver transport already exists";
		return false;
	}

	if (m_device == nullptr)
	{
		m_lastErorMsg = "Device not created";
		return false;
	}

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
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_sendTransport != nullptr)
	{
		m_lastErorMsg = "Send transport already exists";
		return false;
	}

	if (m_device == nullptr)
	{
		m_lastErorMsg = "Device not created";
		return false;
	}

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

bool MediaSoupTransceiver::CreateVideoProducerTrack(const nlohmann::json* ecodings /*= nullptr*/, const nlohmann::json* codecOptions /*= nullptr*/, const nlohmann::json* codec /*= nullptr*/)
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_device == nullptr)
	{
		m_lastErorMsg = "Device not created";
		return false;
	}

	if (m_factory_Producer == nullptr)
	{
		m_lastErorMsg = "Factory not yet created";
		return false;
	}

	if (m_dataProducer_Video != nullptr)
	{
		m_lastErorMsg = "Producer already exists (video)";
		return false;
	}

	if (m_device->CanProduce("video"))
	{
		auto videoTrack = CreateProducerVideoTrack(m_factory_Producer, std::to_string(rtc::CreateRandomId()));

		std::vector<webrtc::RtpEncodingParameters> encodings;

		if (ecodings != nullptr)
		{
			for (auto& itr : *ecodings)
			{
				webrtc::RtpEncodingParameters option;
				option.max_bitrate_bps = itr["maxBitrate"].get<int>();
				option.scale_resolution_down_by = itr["scaleResolutionDownBy"].get<int>();
				encodings.emplace_back(option);
			}
		}
		else
		{
			encodings.emplace_back(webrtc::RtpEncodingParameters{});
			encodings.emplace_back(webrtc::RtpEncodingParameters{});
			encodings.emplace_back(webrtc::RtpEncodingParameters{});
		}

		if (auto ptr = m_sendTransport->Produce(this, videoTrack, &encodings, codecOptions, codec))
		{
			AssignProducer(m_dataProducer_Video, ptr);
		}
		else
		{
			m_lastErorMsg = "MediaSoupTransceiver::CreateVideoProducerTrack - Transport failed to produce video";
			return false;
		}
	}
	else
	{
		m_lastErorMsg = "MediaSoupTransceiver::CreateVideoProducerTrack - Cannot produce video";
		return false;
	}

	return true;
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> MediaSoupTransceiver::CreateProducerVideoTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& /*label*/)
{
	// The factory handles cleanup of this cstyle pointer
	auto videoTrackSource = new rtc::RefCountedObject<FrameGeneratorCapturerVideoTrackSource>(FrameGeneratorCapturerVideoTrackSource::Config(), webrtc::Clock::GetRealTimeClock(), false, *m_producerMailbox.get());
	videoTrackSource->Start();
	
	return factory->CreateVideoTrack(rtc::CreateRandomUuid(), videoTrackSource);
}

bool MediaSoupTransceiver::CreateAudioProducerTrack()
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_device == nullptr)
	{
		m_lastErorMsg = "Device not created";
		return false;
	}

	if (m_factory_Consumer == nullptr)
	{
		m_lastErorMsg = "Factory not yet created";
		return false;
	}

	if (m_dataProducer_Audio != nullptr)
	{
		m_lastErorMsg = "Producer already exists (audio)";
		return false;
	}

	if (m_device->CanProduce("audio"))
	{
		auto audioTrack = CreateProducerAudioTrack(m_factory_Producer, std::to_string(rtc::CreateRandomId()));

		json codecOptions =
		{
			{ "opusStereo", true },
			{ "opusDtx",	true }
		};

		if (auto ptr = m_sendTransport->Produce(this, audioTrack, nullptr, &codecOptions, nullptr))
		{
			AssignProducer(m_dataProducer_Audio, ptr);
			m_sendingAudio = true;
			m_audioThread = std::thread(&MediaSoupTransceiver::AudioThread, this);
		}
		else
		{
			m_lastErorMsg = "MediaSoupTransceiver::CreateAudioProducerTrack - Transport failed to produce video";
			return false;
		}
	}
	else
	{
		m_lastErorMsg = "Cannot produce audio";
		return false;
	}

	return true;
}

rtc::scoped_refptr<webrtc::AudioTrackInterface> MediaSoupTransceiver::CreateProducerAudioTrack(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, const std::string& label)
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

bool MediaSoupTransceiver::CreateAudioConsumer(const std::string& id, const std::string& producerId, json* rtpParameters, obs_source_t* source)
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_device == nullptr)
	{
		m_lastErorMsg = "Device not created";
		return false;
	}

	mediasoupclient::Consumer* consumer = nullptr;

	try
	{
		consumer = m_recvTransport->Consume(this, id, producerId, "audio", rtpParameters);
	}
	catch (...)
	{
		m_lastErorMsg = "Unable to create the audio consumer";
		return false;
	}

	auto audioSink = std::make_unique<MyAudioSink>();
	audioSink->m_mailbox = std::make_unique<MediaSoupMailbox>();
	audioSink->m_consumerType = MediaSoupTransceiver::ConsumerType::ConsumerAudio;
	audioSink->m_obs_source = source;

	auto trackRaw = consumer->GetTrack();
	dynamic_cast<webrtc::AudioTrackInterface*>(trackRaw)->AddSink(audioSink.get());

	AssignConsumer(id, consumer, std::move(audioSink));
	return true;
}

bool MediaSoupTransceiver::CreateVideoConsumer(const std::string& id, const std::string& producerId, json* rtpParameters)
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_device == nullptr)
	{
		m_lastErorMsg = "Device not created";
		return false;
	}

	mediasoupclient::Consumer* consumer = nullptr;

	try
	{
		consumer = m_recvTransport->Consume(this, id, producerId, "video", rtpParameters);
	}
	catch (...)
	{
		m_lastErorMsg = "Unable to create the video consumer";
		return false;
	}
	
	auto videoSink = std::make_unique<MyVideoSink>();
	videoSink->m_mailbox = std::make_unique<MediaSoupMailbox>();
	videoSink->m_consumerType = MediaSoupTransceiver::ConsumerType::ConsumerAudio;

	auto trackRaw = consumer->GetTrack();
	
	rtc::VideoSinkWants videoSinkWants;
	dynamic_cast<webrtc::VideoTrackInterface*>(trackRaw)->AddOrUpdateSink(videoSink.get(), videoSinkWants);
	
	AssignConsumer(id, consumer, std::move(videoSink));
	return true;
}

// Fired for the first Transport::Consume() or Transport::Produce().
// Update the already created remote transport with the local DTLS parameters.
std::future<void> MediaSoupTransceiver::OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters)
{
	std::promise<void> promise;

	if ((m_recvTransport && transport->GetId() == m_recvTransport->GetId()) || (m_sendTransport && transport->GetId() == m_sendTransport->GetId()))
	{
		if (ConnectorFrontApiHelper::onConnect(m_id, transport->GetId(), dtlsParameters))
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

	if (ConnectorFrontApiHelper::onProduce(m_id, transport->GetId(), kind, rtpParameters, value))
		promise.set_value(value);
	else
		promise.set_exception(std::make_exception_ptr("OnProduce failed"));

	return promise.get_future();
}

std::future<std::string> MediaSoupTransceiver::OnProduceData(mediasoupclient::SendTransport* sendTransport, const nlohmann::json& sctpStreamParameters,  const std::string& label, const std::string& protocol, const nlohmann::json& appData)
{ 
	std::promise<std::string> promise; 
	promise.set_value(""); 
	return promise.get_future(); 
};

void MediaSoupTransceiver::AudioThread()
{
	webrtc::Random random_generator_(1);

	while (m_sendingAudio)
	{
		std::vector<std::unique_ptr<MediaSoupMailbox::SoupSendAudioFrame>> frames;
		m_producerMailbox->pop_outgoing_audioFrame(frames);

		if (!frames.empty())
		{
			uint32_t unused = 0;

			for (auto& itr : frames)
				m_MyProducerAudioDeviceModule->PlayData(itr->audio_data.data(), itr->numFrames, itr->bytesPerSample, itr->numChannels, itr->samples_per_sec, 0, 0, 0, false, unused);			
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

void MediaSoupTransceiver::StopReceiveTransport()
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	if (m_recvTransport)
		m_recvTransport->Close();

	{
		std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);

		// Cleanup the consumers attached to it
		for (auto& itr : m_dataConsumers)
		{
			if (itr.second.first != nullptr)
				itr.second.first->Close();

			delete itr.second.first;
		}
	}

	while (ReceiverConnected())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	delete m_recvTransport;
	m_recvTransport = nullptr;

	{
		// The sinks are in here too
		std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);
		m_dataConsumers.clear();
	}
}

void MediaSoupTransceiver::StopSendTransport()
{	
	std::lock_guard<std::mutex> grd(m_externalMutex);

	m_sendingAudio = false;

	if (m_audioThread.joinable())
		m_audioThread.join();

	if (m_sendTransport)
		m_sendTransport->Close();

	{		
		std::lock_guard<std::recursive_mutex> grd(m_producerMutex);

		// Cleanup the producers attached to it
		if (m_dataProducer_Audio != nullptr)
		{
			m_dataProducer_Audio->Close();
			delete m_dataProducer_Audio;
			m_dataProducer_Audio = nullptr;
		}

		if (m_dataProducer_Video != nullptr)
		{
			m_dataProducer_Video->Close();
			delete m_dataProducer_Video;
			m_dataProducer_Video = nullptr;
		}
	}

	while (SenderConnected())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	delete m_sendTransport;
	m_sendTransport = nullptr;

	//m_factory_Producer = nullptr;
	//m_networkThread_Producer = nullptr;
	//m_signalingThread_Producer = nullptr;
	//m_workerThread_Producer = nullptr;
}
                                                                            
void MediaSoupTransceiver::Stop()
{
	std::lock_guard<std::mutex> grd(m_externalMutex);

	m_sendingAudio = false;

	if (m_audioThread.joinable())
		m_audioThread.join();

	//if (m_videoTrackSource)
	//	m_videoTrackSource->Stop();

	if (m_recvTransport)
		m_recvTransport->Close();

	if (m_sendTransport)
		m_sendTransport->Close();

	{
		std::lock_guard<std::recursive_mutex> grd(m_producerMutex);

		if (m_dataProducer_Audio != nullptr)
		{
			m_dataProducer_Audio->Close();
			delete m_dataProducer_Audio;
			m_dataProducer_Audio = nullptr;
		}

		if (m_dataProducer_Video != nullptr)
		{
			m_dataProducer_Video->Close();
			delete m_dataProducer_Video;
			m_dataProducer_Video = nullptr;
		}
	}

	{
		std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);

		for (auto& itr : m_dataConsumers)
		{
			if (itr.second.first != nullptr)
				itr.second.first->Close();

			delete itr.second.first;
		}
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

	{		
		// The sinks are in here too
		std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);
		m_dataConsumers.clear();
	}
}
bool MediaSoupTransceiver::SenderCreated()
{
	return m_sendTransport != nullptr;
}

bool MediaSoupTransceiver::ReceiverCreated()
{
	return m_recvTransport != nullptr;
}

bool MediaSoupTransceiver::SenderConnected()
{
	if (!SenderCreated())
		return false;

	return GetConnectionState(m_sendTransport) == "completed";
}

bool MediaSoupTransceiver::ReceiverConnected()
{
	if (!ReceiverCreated())
		return false;
	
	return GetConnectionState(m_recvTransport) == "completed";
}

const std::string MediaSoupTransceiver::GetSenderId()
{
	if (!SenderCreated())
		return "";

	return m_sendTransport->GetId();
}

const std::string MediaSoupTransceiver::GetReceiverId()
{
	if (!ReceiverCreated())
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
	std::lock_guard<std::mutex> grd(m_stateMutex);
	auto itr = m_connectionState.find(transport);

	if (itr != m_connectionState.end())
		return itr->second;

	return "";
}

void MediaSoupTransceiver::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
	std::lock_guard<std::mutex> grd(m_stateMutex);
	m_connectionState[transport] = connectionState;
}

void MediaSoupTransceiver::AssignProducer(mediasoupclient::Producer*& ref, mediasoupclient::Producer* value)
{
	if (value == nullptr)
	{
		blog(LOG_ERROR, "MediaSoupTransceiver::AssignProducer - Nullptr");
		return;
	}

	std::lock_guard<std::recursive_mutex> grd(m_producerMutex);
	ref = value;
}

void MediaSoupTransceiver::AssignConsumer(const std::string& id, mediasoupclient::Consumer* value, std::unique_ptr<GenericSink> sink)
{
	if (id.empty())
	{
		blog(LOG_ERROR, "MediaSoupTransceiver::AssignConsumer - Empty ID");
		return;
	}

	std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);
	StopConsumerById(id);
	m_dataConsumers[id] = { value, std::move(sink) };
}

MediaSoupMailbox* MediaSoupTransceiver::GetConsumerMailbox(const std::string& id)
{
	std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);

	auto itr = m_dataConsumers.find(id);

	if (itr != m_dataConsumers.end())
	{
		if (itr->second.first != nullptr)
			return itr->second.second->m_mailbox.get();
	}

	return nullptr;
}

void MediaSoupTransceiver::StopConsumerById(const std::string& id)
{
	std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);

	auto itr = m_dataConsumers.find(id);

	if (itr != m_dataConsumers.end())
	{
		if (itr->second.first != nullptr)
			itr->second.first->Close();

		delete itr->second.first;
		itr = m_dataConsumers.erase(itr);
	}
}

std::string MediaSoupTransceiver::StopConsumerByProducerId(const std::string& id)
{
	std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);
	std::string result;

	for (auto itr = m_dataConsumers.begin(); itr != m_dataConsumers.end();)
	{
		// [] initialized an itr at some point leaving empty values
		if (itr->second.first == nullptr)
		{
			itr = m_dataConsumers.erase(itr);
			continue;
		}

		if (itr->second.first->GetProducerId() == id)
		{
			result = itr->second.first->GetId();
			itr->second.first->Close();
			delete itr->second.first;
			itr = m_dataConsumers.erase(itr);
		}
		else
		{
			++itr;
		}
	}

	return result;
}

// Webrtc's own threads can do this at any time for any number of reasons
// It seems to destroy the object for us, so we're just keeping ourselves up to date with it
void MediaSoupTransceiver::OnTransportClose(mediasoupclient::Consumer* consumer)
{
	std::lock_guard<std::recursive_mutex> grd(m_consumerMutex);

	auto itr = m_dataConsumers.begin();

	while (itr != m_dataConsumers.end())
	{
		if (itr->second.first != nullptr && itr->second.first->GetId() == consumer->GetId())
		{
			m_dataConsumers.erase(itr);
			return;
		}
		else
		{
			++itr;
		}
	}
}

// Webrtc's own threads can do this at any time for any number of reasons
// It seems to destroy the object for us, so we're just keeping ourselves up to date with it
void MediaSoupTransceiver::OnTransportClose(mediasoupclient::Producer* producer)
{
	std::lock_guard<std::recursive_mutex> grd(m_producerMutex);

	if (m_dataProducer_Audio == producer)
		m_dataProducer_Audio = nullptr;

	if (m_dataProducer_Video == producer)
		m_dataProducer_Video = nullptr;
}

bool MediaSoupTransceiver::AudioProducerReady()
{
	return SenderConnected() && m_dataProducer_Audio != nullptr;
}

bool MediaSoupTransceiver::VideoProducerReady()
{
	return SenderConnected() && m_dataProducer_Video != nullptr;
}

bool MediaSoupTransceiver::ConsumerReady(const std::string& id)
{
	return SenderConnected() && m_dataConsumers[id].first != nullptr;
}

bool MediaSoupTransceiver::ConsumerReadyAtLeastOne()
{
	if (!SenderConnected())
		return false;

	for (auto& itr : m_dataConsumers)
	{
		if (itr.second.first != nullptr)
			return true;
	}

	return false;
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
	size_t number_of_bytes = 0;
	
	switch (bits_per_sample)
	{
	case 8: number_of_bytes = number_of_channels * number_of_frames * sizeof(int8_t); break;
	case 16: number_of_bytes = number_of_channels * number_of_frames * sizeof(int16_t); break;
	case 32: number_of_bytes = number_of_channels * number_of_frames * sizeof(int32_t); break;
	default: return;
	}

	// Output to source
	obs_source_audio sdata;
	sdata.data[0] = (uint8_t*)audio_data;
	sdata.frames = uint32_t(number_of_frames);
	sdata.speakers = static_cast<speaker_layout>(number_of_channels);
	sdata.samples_per_sec = sample_rate;
	sdata.format = MediaSoupTransceiver::GetDefaultAudioFormat();
	
	if (absolute_capture_timestamp_ms.has_value())
		sdata.timestamp = absolute_capture_timestamp_ms.value();

	obs_source_output_audio(m_obs_source, &sdata);

	// Workaround to mute webrtc's default playback
	memset((void*)audio_data, 0, number_of_bytes);
}

#endif
