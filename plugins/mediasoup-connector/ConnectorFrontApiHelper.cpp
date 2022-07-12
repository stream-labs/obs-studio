#ifndef _DEBUG

#include "ConnectorFrontApi.h"
#include "MediaSoupInterface.h"
#include "MediaSoupMailbox.h"

bool ConnectorFrontApiHelper::createReceiver(const std::string& params, calldata_t* cd)
{
	blog(LOG_DEBUG, "createReceiver start");

	if (MediaSoupInterface::instance().getTransceiver()->ReceiverCreated())
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

		if (MediaSoupInterface::instance().getTransceiver()->CreateReceiver(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], sctpParameters.empty() ? nullptr : &sctpParameters, iceServers.empty() ? nullptr : &iceServers))
			return false;
	}
	catch (...)
	{
		blog(LOG_ERROR, "%s createReceiver exception", obs_module_description());
		return false;
	}
	
	json output;
	output["receiverId"] = MediaSoupInterface::instance().getTransceiver()->GetReceiverId().c_str();
	calldata_set_string(cd, "output", output.dump().c_str());
	return true;
}

bool ConnectorFrontApiHelper::createSender(const std::string& params, calldata_t* cd)
{
	blog(LOG_DEBUG, "createSender start");

	if (MediaSoupInterface::instance().getTransceiver()->SenderCreated())
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

		if (!MediaSoupInterface::instance().getTransceiver()->CreateSender(response["id"].get<std::string>(), response["iceParameters"], response["iceCandidates"], response["dtlsParameters"], iceServers.empty() ? nullptr : &iceServers))
		{
			blog(LOG_ERROR, "%s createSender CreateSender failed, error '%s'", obs_module_description(), MediaSoupInterface::instance().getTransceiver()->PopLastError().c_str());
			return false;
		}
	}
	catch (...)
	{
		blog(LOG_ERROR, "%s createSender exception", obs_module_description());
		return false;
	}

	json output;
	output["senderId"] = MediaSoupInterface::instance().getTransceiver()->GetSenderId();
	calldata_set_string(cd, "output", output.dump().c_str());
	return true;
}

// Creating a producer will do ::OnConnect ::Consume in same stack that does CreateAudioProducerTrack (or video)
// Those must not return until the front end can do its job, and so we delegate the task into a background thread, entering a waiting state
// Sadly this is a bit of a balancing act between the backend and frontend, not sure how else to handle this annoying scenario
bool ConnectorFrontApiHelper::createConsumer(MediaSoupInterface::ObsSourceInfo& obsSourceInfo, const std::string& params, const std::string& kind, calldata_t* cd)
{
	blog(LOG_DEBUG, "createConsumer start");

	if (!MediaSoupInterface::instance().getTransceiver()->ReceiverCreated())
	{
		blog(LOG_WARNING, "%s createConsumer but receiver not ready", obs_module_description());
		return false;
	}

	json params_parsed;
	std::string inputId;

	try
	{	params_parsed = json::parse(params);
		inputId = params_parsed["id"].get<std::string>();
	}
	catch (...)
	{
		blog(LOG_WARNING, "%s createConsumer audio but already ready", obs_module_description());
		return false;
	}

	if (MediaSoupInterface::instance().getTransceiver()->ConsumerReady(inputId))
	{
		blog(LOG_WARNING, "%s createConsumer but already exists", obs_module_description());
		return false;
	}

	if (kind == "video")
		obsSourceInfo.m_consumer_video = inputId;
	else if (kind == "audio")
		obsSourceInfo.m_consumer_audio = inputId;
	
	auto func = [](const json params_parsed, const std::string kind, obs_source_t* source)
	{
		try
		{
			auto id = params_parsed["id"].get<std::string>();
			auto producerId = params_parsed["producerId"].get<std::string>();
			auto rtpParam = params_parsed["rtpParameters"].get<json>();

			if (kind == "audio")
				MediaSoupInterface::instance().getTransceiver()->CreateAudioConsumer(id, producerId, &rtpParam, source);

			if (kind == "video")
				MediaSoupInterface::instance().getTransceiver()->CreateVideoConsumer(id, producerId, &rtpParam);
		}
		catch (...)
		{
			blog(LOG_ERROR, "%s createVideoConsumer exception", obs_module_description());
		}

		MediaSoupInterface::instance().setThreadIsProgress(false);
	};

	// Connect handshake on the receive transport is not needed more than once on the transport
	if (MediaSoupInterface::instance().getTransceiver()->ConsumerReadyAtLeastOne())
	{
		// In which case, just do on this thread
		func(params_parsed, kind, obsSourceInfo.m_obs_source);
		return MediaSoupInterface::instance().getTransceiver()->PopLastError().empty();
	}
	else
	{
		if (MediaSoupInterface::instance().isThreadInProgress())
		{
			blog(LOG_WARNING, "%s createConsumer '%s' but already a thread in progress", obs_module_description(), kind.c_str());
			return false;
		}

		MediaSoupInterface::instance().setThreadIsProgress(true);
		std::unique_ptr<std::thread> thr = std::make_unique<std::thread>(func, params_parsed, kind, obsSourceInfo.m_obs_source);
		auto timeStart = std::clock();

		while (MediaSoupInterface::instance().isThreadInProgress() && !MediaSoupInterface::instance().isConnectWaiting())
		{
			// Timeout
			if (std::clock() - getWaitTimeoutDuration() > timeStart)
			{
				MediaSoupInterface::instance().setThreadIsProgress(false);
				MediaSoupInterface::instance().resetThreadCache();
				thr->join();
				blog(LOG_ERROR, "%s createConsumer timed out waiting, we waited %dms", obs_module_description(), getWaitTimeoutDuration());
				return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (!MediaSoupInterface::instance().isThreadInProgress())
			thr->join();
		else
			MediaSoupInterface::instance().setConnectionThread(std::move(thr));
		
		std::string params;

		// Sent to the frontend
		if (MediaSoupInterface::instance().popConnectParams(params))
		{
			json output;
			output["connect_params"] = params;
			calldata_set_string(cd, "output", output.dump().c_str());
		}
		else
		{
			blog(LOG_ERROR, "createConsumer was expecting to return connect_params but did not");
		}

		return MediaSoupInterface::instance().isThreadInProgress();
	}
}

// Same logic as creating a consumer
bool ConnectorFrontApiHelper::createProducerTrack(const std::string& kind, calldata_t* cd, const std::string& input)
{
	blog(LOG_DEBUG, "createProducerTrack start");

	if (MediaSoupInterface::instance().isThreadInProgress())
	{
		blog(LOG_WARNING, "%s createProducerTrack but already a thread in progress", obs_module_description());
		return false;
	}
	
	if (!MediaSoupInterface::instance().getTransceiver()->SenderCreated())
	{
		blog(LOG_WARNING, "%s createProducerTrack but sender not ready", obs_module_description());
		return false;
	}

	if (kind == "audio" && MediaSoupInterface::instance().getTransceiver()->AudioProducerReady())
	{
		blog(LOG_WARNING, "%s createProducerTrack audio but UploadAudioReady", obs_module_description());
		return false;
	}

	if (kind == "video" && MediaSoupInterface::instance().getTransceiver()->VideoProducerReady())
	{
		blog(LOG_WARNING, "%s createProducerTrack video but UploadVideoReady", obs_module_description());
		return false;
	}

	auto func = [](const std::string kind, const std::string input)
	{
		try
		{
			if (kind == "audio")
			{
				MediaSoupInterface::instance().getTransceiver()->CreateAudioProducerTrack();
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

				MediaSoupInterface::instance().getTransceiver()->CreateVideoProducerTrack(ecodings.empty() ? nullptr : &ecodings, codecOptions.empty() ? nullptr : &codecOptions, codec.empty() ? nullptr : &codec);
			}
			else
			{
				blog(LOG_ERROR, "%s createProducerTrack unexpected kind %s", obs_module_description(), kind.c_str());
			}
		}
		catch (...)
		{
			blog(LOG_ERROR, "%s createProducerTrack exception %s", obs_module_description(), MediaSoupInterface::instance().getTransceiver()->PopLastError().c_str());
		}

		MediaSoupInterface::instance().setThreadIsProgress(false);
	};
	
	MediaSoupInterface::instance().setThreadIsProgress(true);
	MediaSoupInterface::instance().setExpectingProduceFollowup(true);
	std::unique_ptr<std::thread> thr = std::make_unique<std::thread>(func, kind, input);
	auto timeStart = std::clock();

	while (MediaSoupInterface::instance().isThreadInProgress() && !MediaSoupInterface::instance().isConnectWaiting() && !MediaSoupInterface::instance().isProduceWaiting())
	{
		// Timeout
		if (std::clock() - getWaitTimeoutDuration() > timeStart)
		{
			MediaSoupInterface::instance().setThreadIsProgress(false);
			MediaSoupInterface::instance().joinWaitingThread();
			thr->join();
			blog(LOG_ERROR, "%s createProducerTrack timed out waiting, we waited %dms", obs_module_description(), getWaitTimeoutDuration());
			return false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (!MediaSoupInterface::instance().isThreadInProgress())
		thr->join();
	else
		MediaSoupInterface::instance().setConnectionThread(std::move(thr));

	std::string params;

	// Sent to the frontend
	if (MediaSoupInterface::instance().popConnectParams(params))
	{
		json output;
		output["connect_params"] = params;
		calldata_set_string(cd, "output", output.dump().c_str());
	}
	else if (MediaSoupInterface::instance().popProduceParams(params))
	{
		json output;
		output["produce_params"] = params;
		calldata_set_string(cd, "output", output.dump().c_str());
	}

	return MediaSoupInterface::instance().isThreadInProgress();
}

bool ConnectorFrontApiHelper::createAudioProducerTrack(calldata_t* cd, const std::string& input)
{
	blog(LOG_DEBUG, "createAudioProducerTrack start");

	if (!MediaSoupInterface::instance().getTransceiver()->SenderCreated())
	{
		blog(LOG_WARNING, "%s createAudioProducerTrack but not senderReady", obs_module_description());
		return false;
	}

	if (MediaSoupInterface::instance().getTransceiver()->AudioProducerReady())
	{
		blog(LOG_WARNING, "%s createAudioProducerTrack but AudioProducerReady", obs_module_description());
		return false;
	}

	return createProducerTrack("audio", cd, input);
}

bool ConnectorFrontApiHelper::createVideoProducerTrack(calldata_t* cd, const std::string& input)
{
	blog(LOG_DEBUG, "createVideoProducerTrack start");

	if (!MediaSoupInterface::instance().getTransceiver()->SenderCreated())
	{
		blog(LOG_WARNING, "%s createVideoProducerTrack but not senderReady", obs_module_description());
		return false;
	}

	if (MediaSoupInterface::instance().getTransceiver()->VideoProducerReady())
	{
		blog(LOG_WARNING, "%s createVideoProducerTrack but already UploadVideoReady=true", obs_module_description());
		return false;
	}

	return createProducerTrack("video", cd, input);
}

bool ConnectorFrontApiHelper::onConnect(const std::string& clientId, const std::string& transportId, const json& dtlsParameters)
{
	json data;
	data["clientId"] = clientId;
	data["transportId"] = transportId;
	data["dtlsParameters"] = dtlsParameters;
	MediaSoupInterface::instance().setConnectParams(data.dump());
	MediaSoupInterface::instance().setConnectIsWaiting(true);
	
	std::string dataReady;
	
	while (MediaSoupInterface::instance().isConnectWaiting() && MediaSoupInterface::instance().isThreadInProgress() && !MediaSoupInterface::instance().popDataReadyForConnect(dataReady))
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	
	MediaSoupInterface::instance().setConnectIsWaiting(false);
	
	if (!dataReady.empty() || MediaSoupInterface::instance().popDataReadyForConnect(dataReady))
		return dataReady == "true";
	
	return false;
}

bool ConnectorFrontApiHelper::onProduce(const std::string& clientId, const std::string& transportId, const std::string& kind, const json& rtpParameters, std::string& output_value)
{
	json data;
	data["clientId"] = clientId;
	data["transportId"] = transportId;
	data["rtpParameters"] = rtpParameters;
	data["kind"] = kind;
	MediaSoupInterface::instance().setProduceParams(data.dump());
	MediaSoupInterface::instance().setProduceIsWaiting(true);
	
	std::string dataReady;
	
	while (MediaSoupInterface::instance().isProduceWaiting() && MediaSoupInterface::instance().isThreadInProgress() && !MediaSoupInterface::instance().popDataReadyForProduce(dataReady))
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	
	MediaSoupInterface::instance().setProduceIsWaiting(false);
	
	if (!dataReady.empty() || MediaSoupInterface::instance().popDataReadyForProduce(dataReady))
		return dataReady == "true";
	
	return false;
}

void ConnectorFrontApiHelper::createInterfaceObject(const std::string& routerRtpCapabilities_Raw, calldata_t* cd)
{
	blog(LOG_DEBUG, "createInterfaceObject start");

	if (routerRtpCapabilities_Raw.empty())
	{
		blog(LOG_WARNING, "%s createInterfaceObject but routerRtpCapabilities_Raw empty", obs_module_description());
		return;
	}

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
	if (!MediaSoupInterface::instance().getTransceiver()->LoadDevice(rotuerRtpCapabilities, deviceRtpCapabilities, deviceSctpCapabilities))
	{
		blog(LOG_ERROR, "msoup_create LoadDevice failed error = '%s'", MediaSoupInterface::instance().getTransceiver()->PopLastError().c_str());
		return;
	}

	json output;
	output["deviceRtpCapabilities"] = deviceRtpCapabilities;
	output["deviceSctpCapabilities"] = deviceSctpCapabilities;
	output["version"] = mediasoupclient::Version();
	output["clientId"] = MediaSoupInterface::instance().getTransceiver()->GetId();
	calldata_set_string(cd, "output", output.dump().c_str());
}

#endif
