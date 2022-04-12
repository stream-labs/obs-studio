#include "MediaSoupInterface.h"

class MediaSoupClients
{
public:
	MediaSoupInterface* registerSoupInterface(const std::string& roomId)
	{
		std::lock_guard<std::mutex> grd(m_soupMutex);

		if (g_soupClients[roomId])
			g_soupClients[roomId]->unloadSelf();

		g_soupClients[roomId] = std::make_unique<MediaSoupInterface>();
		return g_soupClients[roomId].get();
	}

	MediaSoupInterface* getSupInterface(const std::string& roomId)
	{
		std::lock_guard<std::mutex> grd(m_soupMutex);
		return g_soupClients[roomId].get();
	}

public:
	static MediaSoupClients* instance()
	{
		static MediaSoupClients c;
		return &c;
	}

private:
	std::mutex m_soupMutex;
	std::map<std::string, std::unique_ptr<MediaSoupInterface>> g_soupClients;
};

#define sMedaSoupClients MediaSoupClients::instance()
