#include "MediaSoupClients.h"
#include "mediasoup-connector-funcs.h"

#include "temp_httpclass.h"

const bool g_debugging = false;
const std::string g_baseServerUrl = "https://v3demo.mediasoup.org:4443";

static void getRoomFromConsole(obs_data_t* settings, obs_source_t* source)
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

	printf("%s...\n", obs_source_get_name(source));

	std::string roomId;
	printf("Enter Room: ");
	std::cin >> roomId;

	obs_data_set_string(settings, "room", roomId.c_str());
	obs_source_update(source, settings);
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

static void emulate_frontend_finalize_produce(obs_data_t* settings, obs_source_t* source, std::string produce_params)
{
	// hack...
	strReplaceAll(produce_params, "\\\"", "\"");
	strReplaceAll(produce_params, ":\"{", ":{");
	strReplaceAll(produce_params, "\"}\"}", "\"}}");

	const std::string roomId = obs_data_get_string(settings, "room");

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
	func_produce_result(source, &cd);
}

static std::string emulate_frontend_finalize_connect(obs_data_t* settings, obs_source_t* source, std::string connect_params)
{
	// hack...
	strReplaceAll(connect_params, "\\\"", "\"");
	strReplaceAll(connect_params, ":\"{", ":{");
	strReplaceAll(connect_params, "\"}\"}", "\"}}");
	
	const std::string roomId = obs_data_get_string(settings, "room");

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
	func_connect_result(source, &cd);

	if (auto str = calldata_string(&cd, "output"))
		return str;

	return "";
}

static bool emulate_frontend_join_lobby(obs_data_t* settings, obs_source_t* source)
{
	const std::string roomId = obs_data_get_string(settings, "room");
	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (soupClient == nullptr)
		return false;

	const std::string version = mediasoupclient::Version();
	const std::string clientId = soupClient->getTransceiver()->GetId();
	const std::string deviceRtpCapabilities_Raw = soupClient->getTransceiver()->GetRtpCapabilities();
	const std::string deviceSctpCapabilities_Raw =  soupClient->getTransceiver()->GetSctpCapabilities();

	json deviceRtpCapabilities;
	json deviceSctpCapabilities;

	try
	{
		deviceRtpCapabilities = json::parse(deviceRtpCapabilities_Raw);
		deviceSctpCapabilities = json::parse(deviceSctpCapabilities_Raw);
	}
	catch (...)
	{
		return false;
	}

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
				obs_data_set_string(settings, "consume_audio_trackId", itr["id"].get<std::string>().c_str());
			else if (kind == "video")
				obs_data_set_string(settings, "consume_video_trackId", itr["id"].get<std::string>().c_str());
		}
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_query_rotuerRtpCapabilities(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP
	const std::string roomId = obs_data_get_string(settings, "room");
	WSHTTPGenericRequestToStream(g_baseServerUrl + "/rooms/" + roomId, "GET", "", &httpOut, 2000, strResponse);

	if (httpOut != 200)
		return nullptr;

	try
	{		
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", json::parse(strResponse).dump().c_str());
		func_routerRtpCapabilities(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

static bool emulate_frontend_register_send_transport(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register send transport
	const std::string roomId = obs_data_get_string(settings, "room");
	auto soupClient = sMediaSoupClients->getInterface(roomId);
	const std::string clientId = soupClient->getTransceiver()->GetId();
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
		func_send_transport_response(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static bool emulate_frontend_register_receive_transport(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string roomId = obs_data_get_string(settings, "room");
	auto soupClient = sMediaSoupClients->getInterface(roomId);
	const std::string clientId = soupClient->getTransceiver()->GetId();
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
		func_receive_transport_response(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static std::string emulate_frontend_create_video_consumer(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string roomId = obs_data_get_string(settings, "room");
	auto soupClient = sMediaSoupClients->getInterface(roomId);
	const std::string clientId = soupClient->getTransceiver()->GetId();
	const std::string receiverId = soupClient->getTransceiver()->GetReceiverId();
	const std::string consume_video_trackId = obs_data_get_string(settings, "consume_video_trackId");
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
		func_video_consumer_response(source, &cd);

		if (auto str = calldata_string(&cd, "output"))
			return str;

		return "";
	}
	catch (...)
	{

	}

	return "";
}

static bool emulate_frontend_create_audio_consumer(obs_data_t* settings, obs_source_t* source)
{
	DWORD httpOut = 0;
	std::string strResponse;

	// HTTP - Register receive transport
	const std::string roomId = obs_data_get_string(settings, "room");
	auto soupClient = sMediaSoupClients->getInterface(roomId);
	const std::string clientId = soupClient->getTransceiver()->GetId();
	const std::string receiverId = soupClient->getTransceiver()->GetReceiverId();
	const std::string consume_audio_trackId = obs_data_get_string(settings, "consume_audio_trackId");
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
		func_audio_consumer_response(source, &cd);
		return true;
	}
	catch (...)
	{
		return false;
	}

	return true;
}

static std::string emulte_frontend_create_audio_producer(obs_data_t* settings, obs_source_t* source)
{
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "input", " ");
	func_create_audio_producer(source, &cd);	

	if (auto str = calldata_string(&cd, "output"))
		return str;

	return "";
}

static std::string emulte_frontend_create_video_producer(obs_data_t* settings, obs_source_t* source)
{
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "input", " ");
	func_create_video_producer(source, &cd);

	if (auto str = calldata_string(&cd, "output"))
		return str;

	return "";
}

static void initDebugging(obs_data_t* settings, obs_source_t* source)
{
	{
		getRoomFromConsole(settings, source);

		emulate_frontend_query_rotuerRtpCapabilities(settings, source);

		//obs_data_set_string(settings, "consume_audio_trackId"
		//obs_data_set_string(settings, "consume_video_trackId"
		emulate_frontend_join_lobby(settings, source);

		emulate_frontend_register_send_transport(settings, source);

		std::string connect_params = emulte_frontend_create_audio_producer(settings, source);

		std::string produce_params = emulate_frontend_finalize_connect(settings, source, connect_params);

		emulate_frontend_finalize_produce(settings, source, produce_params);

		produce_params = emulte_frontend_create_video_producer(settings, source);

		emulate_frontend_finalize_produce(settings, source, produce_params);

		emulate_frontend_register_receive_transport(settings, source);

		connect_params = emulate_frontend_create_video_consumer(settings, source);

		emulate_frontend_finalize_connect(settings, source, connect_params);

		emulate_frontend_create_audio_consumer(settings, source);
	}

	// debbuging stop
	/*{
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "input", " ");
		func_stop_sender(source, &cd);
		
		emulate_frontend_register_send_transport(settings, source);

		std::string connect_params = emulte_frontend_create_audio_producer(settings, source);

		std::string produce_params = emulate_frontend_finalize_connect(settings, source, connect_params);

		emulate_frontend_finalize_produce(settings, source, produce_params);

		produce_params = emulte_frontend_create_video_producer(settings, source);

		emulate_frontend_finalize_produce(settings, source, produce_params);
	}*/
}
