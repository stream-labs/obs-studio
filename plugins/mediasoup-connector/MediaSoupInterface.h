#pragma once

#include "MediaSoupTransceiver.h"

#include <obs.h>
#include <mutex>

/**
* MediaSoupInterface
*/

class MediaSoupInterface
{
public:
	MediaSoupInterface();
	~MediaSoupInterface();
	
	void joinWaitingThread();
	void resetThreadCache();
	void applyVideoFrameToObsTexture(webrtc::VideoFrame& frame);
	void setDataReadyForConnect(const std::string& val);
	void setDataReadyForProduce(const std::string& val);
	void setProduceParams(const std::string& val);
	void setConnectParams(const std::string& val);
	void setConnectIsWaiting(const bool v) { m_connectWaiting = v;  }
	void setProduceIsWaiting(const bool v) { m_produceWaiting = v;  }
	void setThreadIsProgress(const bool v) { m_threadInProgress = v;  }
	void setExpectingProduceFollowup(const bool v) { m_expectingProduceFollowup = v; }
	void setConnectionThread(std::unique_ptr<std::thread> thr) { m_connectionThread = std::move(thr); }

	int getTextureWidth() const { return m_textureWidth; }
	int getTextureHeight() const { return m_textureHeight; }
	
	bool popDataReadyForConnect(std::string& output);
	bool popDataReadyForProduce(std::string& output);
	bool popConnectParams(std::string& output);
	bool popProduceParams(std::string& output);
	bool isThreadInProgress() const { return m_threadInProgress; }
	bool isConnectWaiting() const { return m_connectWaiting; }
	bool isProduceWaiting() const { return m_produceWaiting; }
	bool isExpectingProduceFollowup() { return m_expectingProduceFollowup; }
	
	static int getHardObsTextureWidth() { return 1280; }
	static int getHardObsTextureHeight() { return 720; }

	MediaSoupTransceiver* getTransceiver() { return m_transceiver.get();  }
	MediaSoupMailbox* getMailboxPtr() { return &m_mailbox; }

	obs_source_t* m_obs_source{ nullptr };
	gs_texture_t* m_obs_scene_texture{ nullptr };

private:
	void initDrawTexture(const int width, const int height);

	int m_textureWidth = 0;
	int m_textureHeight = 0;
	
	bool m_threadInProgress{ false };
	bool m_connectWaiting{ false };
	bool m_produceWaiting{ false };
	bool m_expectingProduceFollowup{ false };

	std::mutex m_dataReadyMtx;
	std::string m_dataReadyForConnect;
	std::string m_dataReadyForProduce;
	std::string m_produce_params;
	std::string m_connect_params;

	std::unique_ptr<MediaSoupTransceiver> m_transceiver;
	std::unique_ptr<std::thread> m_connectionThread;

	MediaSoupMailbox m_mailbox;
};
