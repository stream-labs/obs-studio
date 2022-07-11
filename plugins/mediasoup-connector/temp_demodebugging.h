#include "MediaSoupInterface.h"
#include "ConnectorFrontApi.h"

#include "temp_httpclass.h"

bool g_debugging = false;
std::string g_baseServerUrl = "https://v3demo.mediasoup.org:4443";
std::string roomId;
std::string consume_video_trackId;
std::string consume_audio_trackId;

static void getRoomFromConsole(MediaSoupInterface::ObsSourceInfo* source)
{
	if (!g_debugging)
		return;

	static bool consoleAlloc = false;
	
	if (!consoleAlloc)
	{
		consoleAlloc = true;
		AllocConsole();
		freopen("conin$","r", stdin);
		freopen("conout$","w", stdout);
		freopen("conout$","w", stderr);
		printf("Debugging Window\n\n");
	}

	//printf("Enter Room: ");
	//std::cin >> roomId;
	roomId = "bsmosdvu";
}

static void strReplaceAll(std::string &str, const std::string& from, const std::string& to)
{
	size_t start_pos = 0;

	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

static void emulate_frontend_finalize_produce(MediaSoupInterface::ObsSourceInfo* source, std::string produce_params)
{
	// hack...
	strReplaceAll(produce_params, "\\\"", "\"");
	strReplaceAll(produce_params, ":\"{", ":{");
	strReplaceAll(produce_params, "\"}\"}", "\"}}");

	DWORD httpOut = 0;
	std::string strResponse;

	try
	{
		json produce_paramsJson = json::parse(produce_params);
		json data = produce_paramsJson["produce_params"].get<json>();
		
		WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + data["clientId"].get<std::string>() + "/transports/" + data["transportId"].get<std::string>() +"/producers", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
			json{	{ "kind",          data["kind"].get<std::string>()	},
				{ "rtpParameters", data["rtpParameters"].get<json>()	}}.dump());
	}
	catch (...)
	{
		
	}

	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "input", "true");
	ConnectorFrontApi::func_produce_result(source, &cd);
}

static std::string emulate_frontend_finalize_connect(MediaSoupInterface::ObsSourceInfo* source, std::string connect_params)
{
	// hack...
	strReplaceAll(connect_params, "\\\"", "\"");
	strReplaceAll(connect_params, ":\"{", ":{");
	strReplaceAll(connect_params, "\"}\"}", "\"}}");
	
	DWORD httpOut = 0;
	std::string strResponse;

	try
	{
		json connect_paramsJson = json::parse(connect_params);
		json data = connect_paramsJson["connect_params"].get<json>();

		WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + data["clientId"].get<std::string>() + "/transports/" + data["transportId"].get<std::string>() + "/connect", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
			json{ { "dtlsParameters", data["dtlsParameters"].get<json>() } }.dump());
	}
	catch (...)
	{
		
	}

	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "input", "true");
	ConnectorFrontApi::func_connect_result(source, &cd);

	if (auto str = calldata_string(&cd, "output"))
		return str;

	return "";
}

static bool emulate_frontend_join_lobby(MediaSoupInterface::ObsSourceInfo* source)
{
	const std::string version = mediasoupclient::Version();
	const std::string clientId = MediaSoupInterface::instance().getTransceiver()->GetId();

	json deviceRtpCapabilities = MediaSoupInterface::instance().getTransceiver()->GetDevice()->GetRtpCapabilities();
	json deviceSctpCapabilities = MediaSoupInterface::instance().getTransceiver()->GetDevice()->GetSctpCapabilities();

	DWORD httpOut = 0;
	std::string strResponse;

	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "id",          clientId				},
			{ "displayName", "broadcaster"				},
			{ "device",
				{
					{ "name",    "libmediasoupclient"       },
					{ "version", version			}
				}
			},
			{ "rtpCapabilities", deviceRtpCapabilities	}}.dump());

	if (httpOut != 200)
		return false;

	try
	{
		auto roomInfo = json::parse(strResponse);
	
		if (roomInfo["peers"].empty())
			return true;
	
		if (roomInfo["peers"].begin().value().empty())
			return true;
	
		auto itrlist = roomInfo["peers"].begin().value()["producers"];
	
		for (auto& itr : itrlist)
		{
			auto kind = itr["kind"].get<std::string>();
	
			if (kind == "audio")
				consume_audio_trackId = itr["id"].get<std::string>().c_str();
			else if (kind == "video")
				consume_video_trackId = itr["id"].get<std::string>().c_str();
		}
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_query_rotuerRtpCapabilities(MediaSoupInterface::ObsSourceInfo* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId, "GET", "", &httpOut, 2000, strResponse);

	if (httpOut != 200)
		return nullptr;

	try
	{		
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", json::parse(strResponse).dump().c_str());
		ConnectorFrontApi::func_load_device(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

static bool emulate_frontend_register_send_transport(MediaSoupInterface::ObsSourceInfo* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register send transport
	const std::string clientId = MediaSoupInterface::instance().getTransceiver()->GetId();
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return false;
	
	try
	{
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", json::parse(strResponse).dump().c_str());
		ConnectorFrontApi::func_create_send_transport(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_register_receive_transport(MediaSoupInterface::ObsSourceInfo* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string clientId = MediaSoupInterface::instance().getTransceiver()->GetId();
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports", "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return false;
	
	try
	{
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", json::parse(strResponse).dump().c_str());
		ConnectorFrontApi::func_create_receive_transport(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static std::string emulate_frontend_create_video_consumer(MediaSoupInterface::ObsSourceInfo* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string clientId = MediaSoupInterface::instance().getTransceiver()->GetId();
	const std::string receiverId = MediaSoupInterface::instance().getTransceiver()->GetReceiverId();
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports/" + receiverId + "/consume?producerId=" + consume_video_trackId, "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return "";
	
	try
	{
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", json::parse(strResponse).dump().c_str());
		ConnectorFrontApi::func_video_consumer_response(source, &cd);

		if (auto str = calldata_string(&cd, "output"))
			return str;

		return "";
	}
	catch (...)
	{

	}

	return "";
}

static bool emulate_frontend_create_audio_consumer(MediaSoupInterface::ObsSourceInfo* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string clientId = MediaSoupInterface::instance().getTransceiver()->GetId();
	const std::string receiverId = MediaSoupInterface::instance().getTransceiver()->GetReceiverId();
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId + "/broadcasters/" + clientId + "/transports/" + receiverId + "/consume?producerId=" + consume_audio_trackId, "POST", "Content-Type: application/json", &httpOut, 2000, strResponse,
		json{	{ "type",    "webrtc" },
			{ "rtcpMux", true     }}.dump());

	if (httpOut != 200)
		return false;
	
	try
	{
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", json::parse(strResponse).dump().c_str());
		ConnectorFrontApi::func_audio_consumer_response(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static std::string emulte_frontend_create_audio_producer(MediaSoupInterface::ObsSourceInfo* source)
{
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "input", " ");
	ConnectorFrontApi::func_create_audio_producer(source, &cd);	

	if (auto str = calldata_string(&cd, "output"))
		return str;

	return "";
}

static std::string emulte_frontend_create_video_producer(MediaSoupInterface::ObsSourceInfo* source)
{
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "input", " ");
	ConnectorFrontApi::func_create_video_producer(source, &cd);

	if (auto str = calldata_string(&cd, "output"))
		return str;

	return "";
}

static void initDebugging(MediaSoupInterface::ObsSourceInfo* source)
{
	{
		getRoomFromConsole(source);
		
		emulate_frontend_query_rotuerRtpCapabilities(source);
		
		emulate_frontend_join_lobby(source);
		
		emulate_frontend_register_send_transport(source);
		
		std::string connect_params = emulte_frontend_create_audio_producer(source);
		
		std::string produce_params = emulate_frontend_finalize_connect(source, connect_params);
		
		emulate_frontend_finalize_produce(source, produce_params);
		
		produce_params = emulte_frontend_create_video_producer(source);
		
		emulate_frontend_finalize_produce(source, produce_params);
		
		emulate_frontend_register_receive_transport(source);
		
		connect_params = emulate_frontend_create_video_consumer(source);
		
		emulate_frontend_finalize_connect(source, connect_params);
		
		emulate_frontend_create_audio_consumer(source);

		
	}

	// debbuging stop
	/*{
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", " ");
		func_stop_sender(source, &cd);
		
		emulate_frontend_register_send_transport(source);

		std::string connect_params = emulte_frontend_create_audio_producer(source);

		std::string produce_params = emulate_frontend_finalize_connect(source, connect_params);

		emulate_frontend_finalize_produce(source, produce_params);

		produce_params = emulte_frontend_create_video_producer(source);

		emulate_frontend_finalize_produce(source, produce_params);
	}*/
}
