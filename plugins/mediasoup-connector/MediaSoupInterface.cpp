#ifndef _DEBUG

#include "MediaSoupTransceiver.h"
#include "MediaSoupInterface.h"
#include "MediaSoupMailbox.h"

#include <third_party/libyuv/include/libyuv.h>
#include <api/video/i420_buffer.h>

/**
* MediaSoupInterface
*/

MediaSoupInterface::MediaSoupInterface()
{
	m_transceiver = std::make_unique<MediaSoupTransceiver>();
}

MediaSoupInterface::~MediaSoupInterface()
{
	reset();
}

void MediaSoupInterface::reset()
{	
	resetThreadCache();

	if (m_connectionThread != nullptr && m_connectionThread->joinable())
		m_connectionThread->join();
	
	m_threadInProgress = false;
	m_connectWaiting = false;
	m_produceWaiting = false;
	m_expectingProduceFollowup = false;

	m_dataReadyForConnect.clear();
	m_dataReadyForProduce.clear();
	m_produce_params.clear();
	m_connect_params.clear();
	
	m_connectionThread = nullptr;

	m_transceiver = std::make_unique<MediaSoupTransceiver>();
}

void MediaSoupInterface::applyVideoFrameToObsTexture(webrtc::VideoFrame& frame, MediaSoupInterface::ObsSourceInfo& sourceInfo)
{
	// The webrtc image buffer should be in I420 format already, so this is just grabbing a ref ptr to it
	rtc::scoped_refptr<webrtc::I420BufferInterface> i420buffer(frame.video_frame_buffer()->ToI420());

	if (frame.rotation() != webrtc::kVideoRotation_0) 
		i420buffer = webrtc::I420Buffer::Rotate(*i420buffer, frame.rotation());
	
	float extraWidth = float(getHardObsTextureWidth()) / float(i420buffer->width());
	float extraHeight = float(getHardObsTextureHeight()) / float(i420buffer->height());
	float scale = std::min(extraWidth, extraHeight);

	// Scale
	int width = int(float(i420buffer->width()) * scale);
	int height = int(float(i420buffer->height()) * scale);
	i420buffer = rtc::scoped_refptr<webrtc::I420BufferInterface>(i420buffer->Scale(width, height)->ToI420());

	DWORD biBitCount = 32;
	DWORD biSizeImage = i420buffer->width() *  i420buffer->height() * (biBitCount >> 3);
	
	std::unique_ptr<uint8_t[]> abgrBuffer;
	abgrBuffer.reset(new uint8_t[biSizeImage]); 
	libyuv::I420ToABGR(i420buffer->DataY(), i420buffer->StrideY(), i420buffer->DataU(), i420buffer->StrideU(), i420buffer->DataV(), i420buffer->StrideV(), abgrBuffer.get(), i420buffer->width() *  biBitCount / 8, i420buffer->width(), i420buffer->height());
	
	ensureDrawTexture(i420buffer->width(), i420buffer->height(), sourceInfo);
	gs_texture_set_image(sourceInfo.m_obs_scene_texture, abgrBuffer.get(), i420buffer->width()*  4, false);
}

void MediaSoupInterface::ensureDrawTexture(const int w, const int h, MediaSoupInterface::ObsSourceInfo& sourceInfo)
{
	if (sourceInfo.m_textureWidth == w && sourceInfo.m_textureHeight == h && sourceInfo.m_obs_scene_texture != nullptr)
		return;

	sourceInfo.m_textureWidth = w;
	sourceInfo.m_textureHeight = h;

	if (sourceInfo.m_obs_scene_texture != nullptr)
		gs_texture_destroy(sourceInfo.m_obs_scene_texture);

	sourceInfo.m_obs_scene_texture = gs_texture_create(w, h, GS_RGBA, 1, NULL, GS_DYNAMIC);
}

rtc::scoped_refptr<webrtc::I420Buffer> MediaSoupInterface::getProducerFrameBuffer(const int width, const int height)
{
	if (m_producerFrameBuffer == nullptr || m_producerFrameBuffer->width() != width || m_producerFrameBuffer->height() != height)
		m_producerFrameBuffer = webrtc::I420Buffer::Create(width, height);

	return m_producerFrameBuffer;
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
	blog(LOG_DEBUG, "resetThreadCache");
}

void MediaSoupInterface::setProduceParams(const std::string& val)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);
	m_produce_params = val;
	blog(LOG_DEBUG, "setProduceParams %s", val.c_str());
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

	if (m_dataReadyForConnect.empty())
		return false;

	output = m_dataReadyForConnect;
	m_dataReadyForConnect.clear();
	return true;
}

bool MediaSoupInterface::popDataReadyForProduce(std::string& output)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);

	if (m_dataReadyForProduce.empty())
		return false;

	output = m_dataReadyForProduce;
	m_dataReadyForProduce.clear();
	return true;
}

bool MediaSoupInterface::popConnectParams(std::string& output)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);

	if (m_connect_params.empty())
		return false;

	output = m_connect_params;
	m_connect_params.clear();
	return true;
}

bool MediaSoupInterface::popProduceParams(std::string& output)
{
	std::lock_guard<std::mutex> grd(m_dataReadyMtx);

	if (m_produce_params.empty())
	{
		blog(LOG_DEBUG, "popProduceParams false");
		return false;
	}
	
	blog(LOG_DEBUG, "popProduceParams true, %s", m_produce_params.c_str());
	output = m_produce_params;
	m_produce_params.clear();
	return true;
}

#endif
