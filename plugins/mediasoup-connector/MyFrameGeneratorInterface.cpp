#ifndef _DEBUG

#include "MyFrameGeneratorInterface.h"
#include "MediaSoupTransceiver.h"

MyFrameGeneratorInterface::MyFrameGeneratorInterface(int width, int height, OutputType type, MediaSoupMailbox& mailbox) :
	m_mailbox(mailbox),
	m_width(width),
	m_height(height)
{
	m_lastFrame = webrtc::I420Buffer::Create(width, height);
	webrtc::I420Buffer::SetBlack(m_lastFrame);
}

void MyFrameGeneratorInterface::ChangeResolution(size_t width, size_t height)
{

}
	
webrtc::test::FrameGeneratorInterface::VideoFrameData MyFrameGeneratorInterface::NextFrame() 
{
	std::vector<rtc::scoped_refptr<webrtc::I420Buffer>> frames;
	m_mailbox.pop_outgoing_videoFrame(frames);

	if (!frames.empty())
		m_lastFrame = frames[frames.size() - 1];

	return VideoFrameData(m_lastFrame, absl::nullopt);
}

FrameGeneratorCapturerVideoTrackSource::FrameGeneratorCapturerVideoTrackSource(Config config, webrtc::Clock* clock, bool is_screencast, MediaSoupMailbox& mailbox) :
	VideoTrackSource(false),
	task_queue_factory_(webrtc::CreateDefaultTaskQueueFactory()),
	is_screencast_(is_screencast) 
{
	video_capturer_ = std::make_unique<webrtc::test::FrameGeneratorCapturer>(clock, std::make_unique<MyFrameGeneratorInterface>(config.width, config.height, webrtc::test::FrameGeneratorInterface::OutputType::kI420, mailbox), config.frames_per_second, *task_queue_factory_);
	video_capturer_->Init();
}

FrameGeneratorCapturerVideoTrackSource::~FrameGeneratorCapturerVideoTrackSource()
{
	
}

void FrameGeneratorCapturerVideoTrackSource::Start()
{
	SetState(kLive);
}

void FrameGeneratorCapturerVideoTrackSource::Stop()
{
	SetState(kEnded);
	video_capturer_->Stop();
}

bool FrameGeneratorCapturerVideoTrackSource::is_screencast() const
{
	return is_screencast_;
}

rtc::VideoSourceInterface<webrtc::VideoFrame>* FrameGeneratorCapturerVideoTrackSource::source()
{
	return video_capturer_.get();
}

#endif
