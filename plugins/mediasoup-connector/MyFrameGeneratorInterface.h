#pragma once

#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_periodic_video_track_source.h"
#include "pc/test/frame_generator_capturer_video_track_source.h"
#include "api/video/i420_buffer.h"

class MediaSoupMailbox;

class MyFrameGeneratorInterface : public webrtc::test::FrameGeneratorInterface
{
public:
	MyFrameGeneratorInterface(int width, int height, OutputType type, MediaSoupMailbox& mailbox);

	void ChangeResolution(size_t width, size_t height) override;
	
	VideoFrameData NextFrame() override;

private:
	const int m_width;
	const int m_height;

	MediaSoupMailbox& m_mailbox;
	rtc::scoped_refptr<webrtc::I420Buffer> m_lastFrame;
};

class FrameGeneratorCapturerVideoTrackSource : public webrtc::VideoTrackSource
{
public:
	static const int kDefaultFramesPerSecond = 30;
	static const int kDefaultWidth = 640;
	static const int kDefaultHeight = 480;
	static const int kNumSquaresGenerated = 50;

	struct Config
	{
		int frames_per_second = kDefaultFramesPerSecond;
		int width = kDefaultWidth;
		int height = kDefaultHeight;
	};

	FrameGeneratorCapturerVideoTrackSource(Config config, webrtc::Clock* clock, bool is_screencast, MediaSoupMailbox& mailbox);
	~FrameGeneratorCapturerVideoTrackSource();

	void Start();
	void Stop();

	bool is_screencast() const override;

protected:
	rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override;

private:
	const std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
	std::unique_ptr<webrtc::test::FrameGeneratorCapturer> video_capturer_;
	const bool is_screencast_;
};
