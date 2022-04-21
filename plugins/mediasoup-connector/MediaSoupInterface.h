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

	int getTextureWidth() const { return m_textureWidth; }
	int getTextureHeight() const { return m_textureHeight; }

	MediaSoupTransceiver* getTransceiver() { return m_transceiver.get();  }
	MediaSoupMailbox* getMailboxPtr() { return &m_mailbox; }

	obs_source_t* m_obs_source;
	gs_texture_t* m_obs_scene_texture{ nullptr };

private:
	int m_textureWidth = 0;
	int m_textureHeight = 0;

	std::unique_ptr<MediaSoupTransceiver> m_transceiver;

	MediaSoupMailbox m_mailbox;
};
