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
	struct ObsSourceInfo
	{
		obs_source_t* m_obs_source{ nullptr };
		gs_texture_t* m_obs_scene_texture{ nullptr };
		std::string m_consumer_audio;
		std::string m_consumer_video;
		int m_textureWidth = 0;
		int m_textureHeight = 0;
	};

public:
	void reset();
	void joinWaitingThread();
	void resetThreadCache();
	void setDataReadyForConnect(const std::string& val);
	void setDataReadyForProduce(const std::string& val);
	void setProduceParams(const std::string& val);
	void setConnectParams(const std::string& val);
	void setConnectIsWaiting(const bool v) { m_connectWaiting = v;  }
	void setProduceIsWaiting(const bool v) { m_produceWaiting = v;  }
	void setThreadIsProgress(const bool v) { m_threadInProgress = v;  }
	void setExpectingProduceFollowup(const bool v) { m_expectingProduceFollowup = v; }
	void setConnectionThread(std::unique_ptr<std::thread> thr) { m_connectionThread = std::move(thr); }
	
	static void applyVideoFrameToObsTexture(webrtc::VideoFrame& frame, ObsSourceInfo& sourceInfo);
	static void ensureDrawTexture(const int width, const int height, ObsSourceInfo& sourceInfo);

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

	MediaSoupTransceiver* getTransceiver() { return m_transceiver.get(); }

	rtc::scoped_refptr<webrtc::I420Buffer> getProducerFrameBuffer(const int width, const int height);

	std::atomic<int> m_sourceCounter = 0;

private:	
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
	
	rtc::scoped_refptr<webrtc::I420Buffer> m_producerFrameBuffer;

public:
	static MediaSoupInterface& instance()
	{
		static MediaSoupInterface s;
		return s;
	}

private:
	MediaSoupInterface();
	~MediaSoupInterface();
	
};
