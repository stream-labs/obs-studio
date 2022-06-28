#pragma once

#include "rtc_base/logging.h"

#include <obs-module.h>

class MyLogSink : public rtc::LogSink
{
public:
	~MyLogSink();

	static MyLogSink& instance()
	{
		static MyLogSink s;
		return s;
	}

protected:
	void OnLogMessage(const std::string& message) final;
	void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity) final;

private:
	MyLogSink();
};
