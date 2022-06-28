#pragma once

#include "MediaSoupInterface.h"

class MedaSoupClients
{
public:
	void unregisterInterface(const std::string& roomId)
	{
		if (roomId.empty())
			return;

		std::lock_guard<std::mutex> grd(m_soupMutex);
		g_soupClients.erase(roomId);
	}

	void registerInterface(const std::string& roomId, std::shared_ptr<MediaSoupInterface> ptr)
	{
		if (roomId.empty())
			return;

		std::lock_guard<std::mutex> grd(m_soupMutex);
		g_soupClients[roomId] = ptr;
	}

	std::shared_ptr<MediaSoupInterface> getInterface(const std::string& roomId)
	{
		if (roomId.empty())
			return nullptr;

		std::lock_guard<std::mutex> grd(m_soupMutex);
		return g_soupClients[roomId];
	}

public:
	static MedaSoupClients* instance()
	{
		static MedaSoupClients c;
		return &c;
	}

private:
	std::mutex m_soupMutex;
	std::map<std::string, std::shared_ptr<MediaSoupInterface>> g_soupClients;
};

#define sMediaSoupClients MedaSoupClients::instance()
