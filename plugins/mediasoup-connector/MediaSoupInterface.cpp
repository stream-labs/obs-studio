#include "MediaSoupInterface.h"

/**
* MediaSoupInterface
*/

MediaSoupInterface::MediaSoupInterface()
{

}

MediaSoupInterface::~MediaSoupInterface()
{

}

void MediaSoupInterface::initDrawTexture(const int w, const int h)
{
	if (m_transceiver == nullptr)
		return;

	if (m_texture != nullptr)
		gs_texture_destroy(m_texture);

	m_texture = gs_texture_create(w, h, GS_RGBA, 1, NULL, GS_DYNAMIC);
}

bool MediaSoupInterface::loadDevice(json& routerRtpCapabilities, json& output_deviceRtpCapabilities, json& output_deviceSctpCapabilities)
{
	m_id = std::to_string(rtc::CreateRandomId());
	m_transceiver = std::make_unique<MediaSoupTransceiver>(m_mailbox);
	
	if (!m_transceiver->LoadDevice(routerRtpCapabilities, output_deviceRtpCapabilities, output_deviceSctpCapabilities))
	{
		m_lastErrorMsg = m_transceiver->GetLastError();
		m_transceiver = nullptr;
		return false;
	}

	return true;
}

bool MediaSoupInterface::createSender(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, std::string& output_sendId)
{
	if (!m_transceiver->CreateSender(id, iceParameters, iceCandidates, dtlsParameters, output_sendId))
	{
		m_lastErrorMsg = m_transceiver->GetLastError();
		m_transceiver = nullptr;
		return false;
	}

	return true;
}

bool MediaSoupInterface::createReceiver(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, const nlohmann::json& sctpParameters, std::string& output_receiverId)
{
	if (!m_transceiver->CreateReceiver(id, iceParameters, iceCandidates, dtlsParameters, sctpParameters, output_receiverId))
	{
		m_lastErrorMsg = m_transceiver->GetLastError();
		m_transceiver = nullptr;
		return false;
	}

	return true;
}

void MediaSoupInterface::registerOnConnect(std::function<bool(const json& out_dtlsParameters, const std::string& transportId)> func)
{
	m_transceiver->RegisterOnConnect(func);
}

void MediaSoupInterface::registerOnProduce(std::function<bool(const std::string& kind, json& rtpParameters, std::string& output_value)> func)
{
	m_transceiver->RegisterOnProduce(func);
}

bool MediaSoupInterface::createAudioConsumer(const std::string& id, const std::string& producerId, json* rtpParameters)
{
	if (!m_transceiver->CreateAudioConsumer(id, producerId, rtpParameters))
	{
		m_lastErrorMsg = m_transceiver->GetLastError();
		m_transceiver = nullptr;
		return false;
	}

	return true;
}

bool MediaSoupInterface::createVideoConsumer(const std::string& id, const std::string& producerId, json* rtpParameters)
{
	if (!m_transceiver->CreateVideoConsumer(id, producerId, rtpParameters))
	{
		m_lastErrorMsg = m_transceiver->GetLastError();
		m_transceiver = nullptr;
		return false;
	}

	return true;
}

bool MediaSoupInterface::createProducerTracks()
{
	if (!m_transceiver->CreateProducerTracks())
	{
		m_lastErrorMsg = m_transceiver->GetLastError();
		m_transceiver = nullptr;
		return false;
	}

	return true;
}

bool MediaSoupInterface::uploadAudioReady() const
{
	return m_transceiver && m_transceiver->UploadAudioReady();
}

bool MediaSoupInterface::uploadVideoReady() const
{
	return m_transceiver && m_transceiver->UploadVideoReady();
}

bool MediaSoupInterface::downloadAudioReady() const
{
	return m_transceiver && m_transceiver->DownloadAudioReady();
}

bool MediaSoupInterface::downloadVideoReady() const
{
	return m_transceiver && m_transceiver->DownloadVideoReady();
}
