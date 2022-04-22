#include "MediaSoupTransceiver.h"

#include <obs.h>

/**
* MediaSoupInterface
*/

class MediaSoupInterface
{
public:
	MediaSoupInterface();
	~MediaSoupInterface();
	
	void initDrawTexture(const int width, const int height);
	void joinWaitingThread();
	void resetThreadBools();
	void setConnectIsWaiting(const bool v) { m_connectWaiting = v;  }
	void setProduceIsWaiting(const bool v) { m_produceWaiting = v;  }
	void setThreadIsProgress(const bool v) { m_threadInProgress = v;  }
	void setIsDataReadyForConnect(const bool v) { m_dataReadyForConnect = v;  }
	void setIsDataReadyForProduce(const bool v) { m_dataReadyForProduce = v;  }
	void setExpectingProduceFollowup(const bool v) { m_expectingProduceFollowup = v; }

	int getTextureWidth() const { return m_textureWidth; }
	int getTextureHeight() const { return m_textureHeight; }
	
	bool isDataReadyForConnect() const { return m_dataReadyForConnect; }
	bool isDataReadyForProduce() const { return m_dataReadyForProduce; }
	bool isThreadInProgress() const { return m_threadInProgress; }
	bool isConnectWaiting() const { return m_connectWaiting; }
	bool isProduceWaiting() const { return m_produceWaiting; }
	bool isExpectingProduceFollowup() { return m_expectingProduceFollowup; }

	void setConnectionThread(std::unique_ptr<std::thread> thr) { m_connectionThread = std::move(thr); }

	MediaSoupTransceiver* getTransceiver() { return m_transceiver.get();  }
	MediaSoupMailbox* getMailboxPtr() { return &m_mailbox; }

	obs_source_t* m_obs_source;
	gs_texture_t* m_obs_scene_texture{ nullptr };

private:
	int m_textureWidth = 0;
	int m_textureHeight = 0;
	
	bool m_dataReadyForConnect{ false };
	bool m_dataReadyForProduce{ false };
	bool m_threadInProgress{ false };
	bool m_connectWaiting{ false };
	bool m_produceWaiting{ false };
	bool m_expectingProduceFollowup{ false };

	std::unique_ptr<MediaSoupTransceiver> m_transceiver;
	std::unique_ptr<std::thread> m_connectionThread;

	MediaSoupMailbox m_mailbox;
};
