#include "MediaSoupInterface.h"

/**
* MediaSoupInterface
*/

MediaSoupInterface::MediaSoupInterface()
{
	m_transceiver = std::make_unique<MediaSoupTransceiver>(m_mailbox);
	m_transceiver->m_owner = this;
}

MediaSoupInterface::~MediaSoupInterface()
{
	resetThreadCache();

	if (m_connectionThread != nullptr && m_connectionThread->joinable())
		m_connectionThread->join();

	if (m_obs_scene_texture != nullptr)
		gs_texture_destroy(m_obs_scene_texture);
}

void MediaSoupInterface::initDrawTexture(const int w, const int h)
{
	if (m_transceiver == nullptr)
		return;

	if (m_textureWidth == w && m_textureHeight == h && m_obs_scene_texture != nullptr)
		return;

	m_textureWidth = w;
	m_textureHeight = h;

	if (m_obs_scene_texture != nullptr)
		gs_texture_destroy(m_obs_scene_texture);

	m_obs_scene_texture = gs_texture_create(w, h, GS_RGBA, 1, NULL, GS_DYNAMIC);
}

void MediaSoupInterface::joinWaitingThread()
{
	if (m_connectionThread != nullptr && m_connectionThread->joinable())
		m_connectionThread->join();

	m_connectionThread = nullptr;
}

void MediaSoupInterface::resetThreadCache()
{
	m_connectWaiting = false;
	m_produceWaiting = false;
	m_threadInProgress = false;
	m_expectingProduceFollowup = false;
	m_dataReadyForConnect.clear();
	m_dataReadyForProduce.clear();
	m_produce_params.clear();
	m_connect_params.clear();
}

void MediaSoupInterface::setProduceParams(const std::string& val)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	m_produce_params = val;
}

void MediaSoupInterface::setConnectParams(const std::string& val)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	m_connect_params = val;
}

void MediaSoupInterface::setDataReadyForConnect(const std::string& val)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	m_dataReadyForConnect = val;
}

void MediaSoupInterface::setDataReadyForProduce(const std::string& val)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	m_dataReadyForProduce = val;
}

bool MediaSoupInterface::popDataReadyForConnect(std::string& output)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	output = m_dataReadyForConnect;
	m_dataReadyForConnect.clear();
}

bool MediaSoupInterface::popDataReadyForProduce(std::string& output)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	output = m_dataReadyForProduce;
	m_dataReadyForProduce.clear();
}

bool MediaSoupInterface::popConnectParams(std::string& output)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	output = m_connect_params;
	m_connect_params.clear();
}

bool MediaSoupInterface::popProduceParams(std::string& output)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	output = m_produce_params;
	m_produce_params.clear();
}
