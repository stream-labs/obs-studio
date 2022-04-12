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

	void registerOnConnect(std::function<bool(const json& out_dtlsParameters, const std::string& transportId)> func);
	void registerOnProduce(std::function<bool(const std::string& kind, json& rtpParameters, std::string& output_value)> func);
	
	bool loadDevice(json& routerRtpCapabilities, json& output_deviceRtpCapabilities, json& output_deviceSctpCapabilities);	
	bool createReceiver(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, const nlohmann::json& sctpParameters, std::string& output_receiverId);
	bool createSender(const std::string& id, const json& iceParameters, const json& iceCandidates, const json& dtlsParameters, std::string& output_sendId);
	bool createAudioConsumer(const std::string& id, const std::string& producerId, json* rtpParameters);
	bool createVideoConsumer(const std::string& id, const std::string& producerId, json* rtpParameters);
	bool createProducerTracks();
	
	bool uploadAudioReady() const;
	bool uploadVideoReady() const;
	bool downloadAudioReady() const;
	bool downloadVideoReady() const;

	const std::string& getId() const { return m_id; }
	const std::string& getLastError() const { return m_lastErrorMsg; }

	MediaSoupMailbox* getMailboxPtr() { return &m_mailbox; }

	obs_source_t* m_source{ nullptr };
	gs_texture_t* m_texture{ nullptr };

private:
	std::string m_id;
	std::string m_lastErrorMsg;
	std::unique_ptr<MediaSoupTransceiver> m_transceiver;
	MediaSoupMailbox m_mailbox;
};
