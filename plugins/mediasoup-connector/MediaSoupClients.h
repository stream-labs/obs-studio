#include "MediaSoupInterface.h"

class MedaSoupClients
{
public:
	void unregisterInterface(const std::string& roomId)
	{
		if (roomId.empty())
			return;

		std::lock_guard<std::recursive_mutex> grd(m_soupMutex);
		g_soupClients.erase(roomId);
	}

	std::shared_ptr<MediaSoupInterface> registerInterface(const std::string& roomId, std::shared_ptr<MediaSoupInterface> ptr)
	{
		std::lock_guard<std::recursive_mutex> grd(m_soupMutex);
		g_soupClients[roomId] = ptr;
		return ptr;
	}

	std::shared_ptr<MediaSoupInterface> getInterface(const std::string& roomId)
	{
		if (roomId.empty())
			return nullptr;

		std::lock_guard<std::recursive_mutex> grd(m_soupMutex);
		return g_soupClients[roomId];
	}

public:
	static MedaSoupClients* instance()
	{
		static MedaSoupClients c;
		return &c;
	}

private:
	std::recursive_mutex m_soupMutex;
	std::map<std::string, std::shared_ptr<MediaSoupInterface>> g_soupClients;
};

#define sMediaSoupClients MedaSoupClients::instance()
