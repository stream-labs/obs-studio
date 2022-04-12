#pragma once

#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_device/include/audio_device_default.h"
#include "rtc_base/event.h"

class MyProducerAudioDeviceModule : public webrtc::webrtc_impl::AudioDeviceModuleDefault<webrtc::AudioDeviceModule> 
{
public:
    MyProducerAudioDeviceModule() : 
        audio_callback_(nullptr),
        rendering_(false),
        capturing_(false) 
    {

    }

    ~MyProducerAudioDeviceModule() override 
    {
        StopPlayout();
        StopRecording();
    }

    int32_t Init() override
    {
        return 0;
    }

    int32_t RegisterAudioCallback(webrtc::AudioTransport* callback) override 
    {
        webrtc::MutexLock lock(&lock_);
        RTC_DCHECK(callback || audio_callback_);
        audio_callback_ = callback;
        return 0;
    }

    int32_t StartPlayout() override 
    {
        webrtc::MutexLock lock(&lock_);
        rendering_ = true;
        return 0;
    }

    int32_t StopPlayout() override 
    {
        webrtc::MutexLock lock(&lock_);
        rendering_ = false;
        return 0;
    }

    int32_t StartRecording() override 
    {
        webrtc::MutexLock lock(&lock_);
        capturing_ = true;
        return 0;
    }

    int32_t StopRecording() override 
    {
        webrtc::MutexLock lock(&lock_);
        capturing_ = false;
        return 0;
    }

    bool Playing() const override 
    {
        webrtc::MutexLock lock(&lock_);
        return rendering_;
    }

    bool Recording() const override 
    {
        webrtc::MutexLock lock(&lock_);
        return capturing_;
    }

public:
    void PlayData(const void* audioSamples,
                  const size_t nSamples,
                  const size_t nBytesPerSample,
                  const size_t nChannels,
                  const uint32_t samples_per_sec,
                  const uint32_t total_delay_ms,
                  const int32_t clockDrift,
                  const uint32_t currentMicLevel,
                  const bool keyPressed,
                  uint32_t& newMicLevel)
    {
	if (audio_callback_ == nullptr)
		return;
	
	audio_callback_->RecordedDataIsAvailable(audioSamples, nSamples, nBytesPerSample, nChannels, samples_per_sec, total_delay_ms, clockDrift, currentMicLevel, keyPressed, newMicLevel);
    }

private:
    mutable webrtc::Mutex lock_;

    rtc::Event done_rendering_;
    rtc::Event done_capturing_;

    bool rendering_ RTC_GUARDED_BY(lock_);
    bool capturing_ RTC_GUARDED_BY(lock_);

    webrtc::AudioTransport* audio_callback_{ nullptr }; RTC_GUARDED_BY(lock_);
};
