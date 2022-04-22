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
	resetThreadBools();

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

void MediaSoupInterface::resetThreadBools()
{
	m_connectWaiting = false;
	m_produceWaiting = false;
	m_threadInProgress = false;
	m_dataReadyForConnect = false;
	m_dataReadyForProduce = false;
	m_expectingProduceFollowup = false;
}
