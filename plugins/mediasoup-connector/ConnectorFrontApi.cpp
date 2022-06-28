#ifndef _DEBUG

#include "ConnectorFrontApi.h"
#include "MediaSoupClients.h"

void ConnectorFrontApi::func_load_device(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");

	blog(LOG_DEBUG, "func_load_device %s", input.c_str());

	ConnectorFrontApiHelper::createInterfaceObject(settings, (obs_source_t*)source, room, input, cd);

	obs_data_release(settings);
}

void ConnectorFrontApi::func_change_playback_volume(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_change_playback_volume %s", input.c_str());

	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->SetSpeakerVolume(uint16_t(atoi(input.c_str())));
	}
	else
	{
		blog(LOG_ERROR, "%s func_change_playback_device but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

void ConnectorFrontApi::func_change_playback_device(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_change_playback_device %s", input.c_str());

	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->SetPlayoutDevice(uint16_t(atoi(input.c_str())));
	}
	else
	{
		blog(LOG_ERROR, "%s func_change_playback_device but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

void ConnectorFrontApi::func_toggle_direct_audio_broadcast(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_toggle_direct_audio_broadcast %s", input.c_str());

	if (auto soupClient = sMediaSoupClients->getInterface(room))
		soupClient->setBoolDirectAudioBroadcast(input == "true");
	else
		blog(LOG_ERROR, "%s func_toggle_direct_audio_broadcast but can't find room", obs_module_description());

	obs_data_release(settings);
}

void ConnectorFrontApi::func_stop_receiver(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_stop_receiver %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->StopReceiver();
	}
	else
	{
		blog(LOG_ERROR, "%s func_stop_receiver but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

void ConnectorFrontApi::func_stop_sender(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_stop_sender %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->StopSender();
	}
	else
	{
		blog(LOG_ERROR, "%s func_stop_sender but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

void ConnectorFrontApi::func_stop_consumer(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_stop_consumer %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		soupClient->getTransceiver()->StopConsumerByProducerId(input);
	}
	else
	{
		blog(LOG_ERROR, "%s func_stop_consumer but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

void ConnectorFrontApi::func_connect_result(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_connect_result %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		if (!soupClient->isThreadInProgress() || !soupClient->isConnectWaiting())
		{
			blog(LOG_ERROR, "%s func_connect_result but thread is not in good state", obs_module_description());
		}
		else
		{
			soupClient->setDataReadyForConnect(input);

			if (soupClient->isExpectingProduceFollowup())
			{
				while (!soupClient->isProduceWaiting() && soupClient->isThreadInProgress())
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else
			{
				soupClient->joinWaitingThread();
				soupClient->resetThreadCache();
			}

			std::string params;

			if (soupClient->popProduceParams(params))
			{
				json output;
				output["produce_params"] = params;
				calldata_set_string(cd, "output", output.dump().c_str());
			}
		}
	}
	else
	{
		blog(LOG_ERROR, "%s func_connect_result but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

void ConnectorFrontApi::func_produce_result(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_produce_result %s", input.c_str());
	
	if (auto soupClient = sMediaSoupClients->getInterface(room))
	{
		if (!soupClient->isThreadInProgress() || !soupClient->isProduceWaiting())
		{
			blog(LOG_ERROR, "%s func_produce_result but thread is not in good state", obs_module_description());
			return;
		}

		soupClient->setDataReadyForProduce(input);
		soupClient->joinWaitingThread();
		soupClient->resetThreadCache();
	}
	else
	{
		blog(LOG_ERROR, "%s func_produce_result but can't find room", obs_module_description());
	}

	obs_data_release(settings);
}

void ConnectorFrontApi::func_create_send_transport(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_create_send_transport %s", input.c_str());
	
	ConnectorFrontApiHelper::createSender(settings, (obs_source_t*)source, room, input, cd);

	obs_data_release(settings);
}

void ConnectorFrontApi::func_create_audio_producer(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_create_audio_producer %s", input.c_str());
	
	ConnectorFrontApiHelper::createAudioProducerTrack(room, cd, input);

	obs_data_release(settings);
}

void ConnectorFrontApi::func_create_video_producer(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_create_video_producer %s", input.c_str());
	
	ConnectorFrontApiHelper::createVideoProducerTrack(room, cd, input);

	obs_data_release(settings);
}

void ConnectorFrontApi::func_create_receive_transport(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_create_receive_transport %s", input.c_str());
	
	ConnectorFrontApiHelper::createReceiver(settings, (obs_source_t*)source, room, input, cd);

	obs_data_release(settings);
}

void ConnectorFrontApi::func_video_consumer_response(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_video_consumer_response %s", input.c_str());
	
	ConnectorFrontApiHelper::createConsumer(settings, (obs_source_t*)source, room, input, "video", cd);

	obs_data_release(settings);
}

void ConnectorFrontApi::func_audio_consumer_response(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");
	
	blog(LOG_DEBUG, "func_audio_consumer_response %s", input.c_str());
	
	ConnectorFrontApiHelper::createConsumer(settings, (obs_source_t*)source, room, input, "audio", cd);

	obs_data_release(settings);
}

void ConnectorFrontApi::func_get_playback_devices(void* data, calldata_t* cd)
{
	obs_source_t* source = static_cast<obs_source_t*>(data);
	obs_data_t* settings = obs_source_get_settings(source);
	std::string input = calldata_string(cd, "input");
	std::string room = obs_data_get_string(settings, "room");

	blog(LOG_DEBUG, "func_get_playback_devices %s", input.c_str());

	if (auto soupClient = sMediaSoupClients->getInterface(obs_data_get_string(settings, "room")))
	{
		std::map<int16_t, std::string> devices;
		soupClient->getTransceiver()->GetPlayoutDevices(devices);

		if (!devices.empty())
		{
			try
			{
				json devicesJson;

				for (auto& itr : devices)
				{
					json blob
					{
						{ "id", std::to_string(itr.first)	},
						{ "name", itr.second			}
					};

					devicesJson.push_back(blob);
				}

				calldata_set_string(cd, "output", devicesJson.dump().c_str());
			}
			catch (...)
			{
				
			}
		}
	}

	obs_data_release(settings);		
}

/***
* Private
*/

bool ConnectorFrontApiHelper::createReceiver(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, calldata_t* cd)
{
	blog(LOG_DEBUG, "createReceiver start");

	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createReceiver but !transceiverCreated", obs_module_description());
		return false;
	}

	if (soupClient->getTransceiver()->ReceiverCreated())
	{
		blog(LOG_WARNING, "%s createReceiver already ready", obs_module_description());
		return false;
	}

	try
	{
		// lib - Create receiver
		auto response = json::parse(params);

		json sctpParameters;
		try { sctpParameters = response["sctpParameters"]; } catch (...) { }
		
		json iceServers;
		try { iceServers = response["iceServers"]; } catch (...) { }

		if (!soupClient->getTransceiver()->CreateReceiver(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], sctpParameters.empty() ? nullptr : &sctpParameters, iceServers.empty() ? nullptr : &iceServers))
			return false;
	}
	catch (...)
	{
		blog(LOG_ERROR, "%s createReceiver exception", obs_module_description());
		return false;
	}
	
	json output;
	output["receiverId"] = soupClient->getTransceiver()->GetReceiverId().c_str();
	calldata_set_string(cd, "output", output.dump().c_str());
	return true;
}

bool ConnectorFrontApiHelper::createSender(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, calldata_t* cd)
{
	blog(LOG_DEBUG, "createSender start");

	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createSender but !transceiverCreated", obs_module_description());
		return false;
	}

	if (soupClient->getTransceiver()->SenderCreated())
	{
		blog(LOG_WARNING, "%s createSender already ready", obs_module_description());
		return false;
	}

	try
	{
		// lib - Create sender
		auto response = json::parse(params.c_str());
		
		json iceServers;
		try { iceServers = response["iceServers"]; } catch (...) { }

		if (!soupClient->getTransceiver()->CreateSender(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], iceServers.empty() ? nullptr : &iceServers))
		{
			blog(LOG_ERROR, "%s createSender CreateSender failed, error '%s'", obs_module_description(), soupClient->getTransceiver()->PopLastError().c_str());
			return false;
		}
	}
	catch (...)
	{
		blog(LOG_ERROR, "%s createSender exception", obs_module_description());
		return false;
	}

	json output;
	output["senderId"] = soupClient->getTransceiver()->GetSenderId();
	calldata_set_string(cd, "output", output.dump().c_str());
	return true;
}

// Creating a producer will do ::OnConnect ::Consume in same stack that does CreateAudioProducerTrack (or video)
// Those must not return until the front end can do its job, and so we delegate the task into a background thread, entering a waiting state
// Sadly this is a bit of a balancing act between the backend and frontend, not sure how else to handle this annoying scenario
bool ConnectorFrontApiHelper::createConsumer(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& params, const std::string& kind, calldata_t* cd)
{
	blog(LOG_DEBUG, "createConsumer start");

	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createConsumer but !transceiverCreated", obs_module_description());
		return false;
	}
	
	if (!soupClient->getTransceiver()->ReceiverCreated())
	{
		blog(LOG_WARNING, "%s createConsumer but receiver not ready", obs_module_description());
		return false;
	}

	if (kind == "audio" && soupClient->getTransceiver()->DownloadAudioReady())
	{
		blog(LOG_WARNING, "%s createConsumer audio but DownloadAudioReady", obs_module_description());
		return false;
	}

	if (kind == "video" && soupClient->getTransceiver()->DownloadVideoReady())
	{
		blog(LOG_WARNING, "%s createConsumer video but DownloadVideoReady", obs_module_description());
		return false;
	}
	
	auto func = [](MediaSoupInterface* soupClient, const std::string params, const std::string kind)
	{
		try
		{
			auto response = json::parse(params);
			auto id = response["id"].get<std::string>();
			auto producerId = response["producerId"].get<std::string>();
			auto rtpParam = response["rtpParameters"].get<json>();

			if (kind == "audio")
				soupClient->getTransceiver()->CreateAudioConsumer(id, producerId, &rtpParam);

			if (kind == "video")
				soupClient->getTransceiver()->CreateVideoConsumer(id, producerId, &rtpParam);
		}
		catch (...)
		{
			blog(LOG_ERROR, "%s createVideoConsumer exception", obs_module_description());
		}

		soupClient->setThreadIsProgress(false);
	};

	// Connect handshake is not needed more than once on the transport
	if (soupClient->getTransceiver()->DownloadAudioReady() || soupClient->getTransceiver()->DownloadVideoReady())
	{
		// In which case, just do on this thread
		func(soupClient.get(), params, kind);
		return soupClient->getTransceiver()->PopLastError().empty();
	}
	else
	{
		if (soupClient->isThreadInProgress())
		{
			blog(LOG_WARNING, "%s createConsumer '%s' but already a thread in progress", obs_module_description(), kind.c_str());
			return false;
		}

		soupClient->setThreadIsProgress(true);
		std::unique_ptr<std::thread> thr = std::make_unique<std::thread>(func, soupClient.get(), params, kind);
	
		while (true)
		{
			if (!soupClient->isThreadInProgress())
				break;

			if (soupClient->isConnectWaiting())
				break;

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (!soupClient->isThreadInProgress())
			thr->join();
		else
			soupClient->setConnectionThread(std::move(thr));
		
		std::string params;

		// Sent to the frontend
		if (soupClient->popConnectParams(params))
		{
			json output;
			output["connect_params"] = params;
			calldata_set_string(cd, "output", output.dump().c_str());
		}
		else
		{
			blog(LOG_ERROR, "createConsumer was expecting to return connect_params but did not");
		}

		return soupClient->isThreadInProgress();
	}
}

// Same logic as creating a consumer
bool ConnectorFrontApiHelper::createProducerTrack(std::shared_ptr<MediaSoupInterface> soupClient, const std::string& kind, calldata_t* cd, const std::string& input)
{
	blog(LOG_DEBUG, "createProducerTrack start");

	if (soupClient->isThreadInProgress())
	{
		blog(LOG_WARNING, "%s createProducerTrack but already a thread in progress", obs_module_description());
		return false;
	}
	
	if (!soupClient->getTransceiver()->SenderCreated())
	{
		blog(LOG_WARNING, "%s createProducerTrack but sender not ready", obs_module_description());
		return false;
	}

	if (kind == "audio" && soupClient->getTransceiver()->UploadAudioReady())
	{
		blog(LOG_WARNING, "%s createProducerTrack audio but UploadAudioReady", obs_module_description());
		return false;
	}

	if (kind == "video" && soupClient->getTransceiver()->UploadVideoReady())
	{
		blog(LOG_WARNING, "%s createProducerTrack video but UploadVideoReady", obs_module_description());
		return false;
	}

	auto func = [](MediaSoupInterface* soupClient, const std::string kind, const std::string input)
	{
		try
		{
			if (kind == "audio")
			{
				soupClient->getTransceiver()->CreateAudioProducerTrack();
			}
			else if (kind == "video")
			{			
				json jsonInput;
				json ecodings;
				json codecOptions;
				json codec;

				try { jsonInput = json::parse(input); } catch (...) { }
				try { ecodings = jsonInput["encodings"]; } catch (...) {}
				try { codecOptions = jsonInput["codecOptions"]; } catch (...) { }
				try { codec = jsonInput["codec"]; } catch (...) { }

				soupClient->getTransceiver()->CreateVideoProducerTrack(ecodings.empty() ? nullptr : &ecodings, codecOptions.empty() ? nullptr : &codecOptions, codec.empty() ? nullptr : &codec);
			}
			else
			{
				blog(LOG_ERROR, "%s createProducerTrack unexpected kind %s", obs_module_description(), kind.c_str());
			}
		}
		catch (...)
		{
			blog(LOG_ERROR, "%s createProducerTrack exception %s", obs_module_description(), soupClient->getTransceiver()->PopLastError().c_str());
		}

		soupClient->setThreadIsProgress(false);
	};
	
	soupClient->setThreadIsProgress(true);
	soupClient->setExpectingProduceFollowup(true);
	std::unique_ptr<std::thread> thr = std::make_unique<std::thread>(func, soupClient.get(), kind, input);

	while (true)
	{
		if (!soupClient->isThreadInProgress())
			break;
		
		if (soupClient->isConnectWaiting())
			break;

		if (soupClient->isProduceWaiting())
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (!soupClient->isThreadInProgress())
		thr->join();
	else
		soupClient->setConnectionThread(std::move(thr));

	std::string params;

	// Sent to the frontend
	if (soupClient->popConnectParams(params))
	{
		json output;
		output["connect_params"] = params;
		calldata_set_string(cd, "output", output.dump().c_str());
	}
	else if (soupClient->popProduceParams(params))
	{
		json output;
		output["produce_params"] = params;
		calldata_set_string(cd, "output", output.dump().c_str());
	}

	return soupClient->isThreadInProgress();
}

bool ConnectorFrontApiHelper::createAudioProducerTrack(const std::string& roomId, calldata_t* cd, const std::string& input)
{
	blog(LOG_DEBUG, "createAudioProducerTrack start");

	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createAudioProducerTrack but !transceiverCreated", obs_module_description());
		return false;
	}

	if (!soupClient->getTransceiver()->SenderCreated())
	{
		blog(LOG_WARNING, "%s createAudioProducerTrack but not senderReady", obs_module_description());
		return false;
	}

	if (soupClient->getTransceiver()->UploadAudioReady())
	{
		blog(LOG_WARNING, "%s createAudioProducerTrack but already UploadAudioReady=true", obs_module_description());
		return false;
	}

	return createProducerTrack(soupClient, "audio", cd, input);
}

bool ConnectorFrontApiHelper::createVideoProducerTrack(const std::string& roomId, calldata_t* cd, const std::string& input)
{
	blog(LOG_DEBUG, "createVideoProducerTrack start");

	auto soupClient = sMediaSoupClients->getInterface(roomId);

	if (!soupClient)
	{
		blog(LOG_WARNING, "%s createVideoProducerTrack but !transceiverCreated", obs_module_description());
		return false;
	}

	if (!soupClient->getTransceiver()->SenderCreated())
	{
		blog(LOG_WARNING, "%s createVideoProducerTrack but not senderReady", obs_module_description());
		return false;
	}

	if (soupClient->getTransceiver()->UploadVideoReady())
	{
		blog(LOG_WARNING, "%s createVideoProducerTrack but already UploadVideoReady=true", obs_module_description());
		return false;
	}

	return createProducerTrack(soupClient, "video", cd, input);
}

void ConnectorFrontApiHelper::createInterfaceObject(obs_data_t* settings, obs_source_t* source, const std::string& roomId, const std::string& routerRtpCapabilities_Raw, calldata_t* cd)
{
	blog(LOG_DEBUG, "createInterfaceObject start");

	if (roomId.empty())
	{
		blog(LOG_WARNING, "%s createInterfaceObject but roomId empty", obs_module_description());
		return;
	}

	if (routerRtpCapabilities_Raw.empty())
	{
		blog(LOG_WARNING, "%s createInterfaceObject but routerRtpCapabilities_Raw empty", obs_module_description());
		return;
	}

	auto soupClient = std::make_shared<MediaSoupInterface>();
	soupClient->m_obs_source = source;

	json rotuerRtpCapabilities;
	json deviceRtpCapabilities;
	json deviceSctpCapabilities;

	try
	{
		rotuerRtpCapabilities = json::parse(routerRtpCapabilities_Raw);
	}
	catch (...)
	{
		blog(LOG_ERROR, "msoup_create json error parsing routerRtpCapabilities_Raw %s", routerRtpCapabilities_Raw.c_str());
		return;
	}

	// lib - Create device
	if (!soupClient->getTransceiver()->LoadDevice(rotuerRtpCapabilities, deviceRtpCapabilities, deviceSctpCapabilities))
	{
		blog(LOG_ERROR, "msoup_create LoadDevice failed error = '%s'", soupClient->getTransceiver()->PopLastError().c_str());
		return;
	}

	sMediaSoupClients->registerInterface(roomId, soupClient);

	// Callback function
	auto onProduceFunc = [](MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const std::string& kind, const json& rtpParameters, std::string& output_value)
	{
		auto settings = obs_source_get_settings(soupClient->m_obs_source);

		if (settings == nullptr)
			return false;
		
		json data;
		data["clientId"] = clientId;
		data["transportId"] = transportId;
		data["rtpParameters"] = rtpParameters;
		data["kind"] = kind;
		soupClient->setProduceParams(data.dump());
		soupClient->setProduceIsWaiting(true);

		std::string dataReady;

		while (soupClient->isProduceWaiting() && soupClient->isThreadInProgress() && !soupClient->popDataReadyForProduce(dataReady))
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		soupClient->setProduceIsWaiting(false);

		if (!dataReady.empty() || soupClient->popDataReadyForProduce(dataReady))
			return dataReady == "true";

		return false;
	};
	
	// Callback function
	auto onConnectFunc = [](MediaSoupInterface* soupClient, const std::string& clientId, const std::string& transportId, const json& dtlsParameters)
	{
		auto settings = obs_source_get_settings(soupClient->m_obs_source);

		if (settings == nullptr)
			return false;
		
		json data;
		data["clientId"] = clientId;
		data["transportId"] = transportId;
		data["dtlsParameters"] = dtlsParameters;
		soupClient->setConnectParams(data.dump());
		soupClient->setConnectIsWaiting(true);

		std::string dataReady;

		while (soupClient->isConnectWaiting() && soupClient->isThreadInProgress() && !soupClient->popDataReadyForConnect(dataReady))
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		soupClient->setConnectIsWaiting(false);

		if (!dataReady.empty() || soupClient->popDataReadyForConnect(dataReady))
			return dataReady == "true";

		return false;
	};

	soupClient->getTransceiver()->RegisterOnProduce(onProduceFunc);	
	soupClient->getTransceiver()->RegisterOnConnect(onConnectFunc);

	json output;
	output["deviceRtpCapabilities"] = deviceRtpCapabilities;
	output["deviceSctpCapabilities"] = deviceSctpCapabilities;
	output["version"] = mediasoupclient::Version();
	output["clientId"] = soupClient->getTransceiver()->GetId();
	calldata_set_string(cd, "output", output.dump().c_str());
}

#endif
