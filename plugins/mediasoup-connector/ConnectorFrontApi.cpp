#ifndef _DEBUG

#include "ConnectorFrontApi.h"
#include "MediaSoupInterface.h"
#include "MediaSoupMailbox.h"

void ConnectorFrontApi::func_load_device(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");
	blog(LOG_DEBUG, "func_load_device %s", input.c_str());
	ConnectorFrontApiHelper::createInterfaceObject(input, cd);
}

void ConnectorFrontApi::func_stop_receiver(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_stop_receiver %s", input.c_str());	
	MediaSoupInterface::instance().getTransceiver()->StopReceiveTransport();
	
	auto sourceInfo = static_cast<MediaSoupInterface::ObsSourceInfo*>(data);
	sourceInfo->m_consumer_audio.clear();
	sourceInfo->m_consumer_video.clear();
}

void ConnectorFrontApi::func_stop_sender(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_stop_sender %s", input.c_str());	
	MediaSoupInterface::instance().getTransceiver()->StopSendTransport();
}

void ConnectorFrontApi::func_stop_consumer(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_stop_consumer %s", input.c_str());
	std::string destroyedId = MediaSoupInterface::instance().getTransceiver()->StopConsumerByProducerId(input);
	auto sourceInfo = static_cast<MediaSoupInterface::ObsSourceInfo*>(data);

	if (destroyedId == sourceInfo->m_consumer_audio)
		sourceInfo->m_consumer_audio.clear();
	else if (destroyedId == sourceInfo->m_consumer_video)
		sourceInfo->m_consumer_video.clear();
}

void ConnectorFrontApi::func_connect_result(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_connect_result %s", input.c_str());
	
	if (!MediaSoupInterface::instance().isThreadInProgress() || !MediaSoupInterface::instance().isConnectWaiting())
	{
		blog(LOG_ERROR, "%s func_connect_result but thread is not in good state", obs_module_description());
	}
	else
	{
		MediaSoupInterface::instance().setDataReadyForConnect(input);

		if (MediaSoupInterface::instance().isExpectingProduceFollowup())
		{
			while (!MediaSoupInterface::instance().isProduceWaiting() && MediaSoupInterface::instance().isThreadInProgress())
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else
		{
			MediaSoupInterface::instance().joinWaitingThread();
			MediaSoupInterface::instance().resetThreadCache();
		}

		std::string params;

		if (MediaSoupInterface::instance().popProduceParams(params))
		{
			json output;
			output["produce_params"] = params;
			calldata_set_string(cd, "output", output.dump().c_str());
		}
	}
}

void ConnectorFrontApi::func_produce_result(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");
	
	blog(LOG_DEBUG, "func_produce_result %s", input.c_str());
	
	if (!MediaSoupInterface::instance().isThreadInProgress() || !MediaSoupInterface::instance().isProduceWaiting())
	{
		blog(LOG_ERROR, "%s func_produce_result but thread is not in good state", obs_module_description());
		return;
	}

	MediaSoupInterface::instance().setDataReadyForProduce(input);
	MediaSoupInterface::instance().joinWaitingThread();
	MediaSoupInterface::instance().resetThreadCache();
}

void ConnectorFrontApi::func_create_send_transport(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_create_send_transport %s", input.c_str());	
	ConnectorFrontApiHelper::createSender(input, cd);
}

void ConnectorFrontApi::func_create_audio_producer(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_create_audio_producer %s", input.c_str());	
	ConnectorFrontApiHelper::createAudioProducerTrack(cd, input);
}

void ConnectorFrontApi::func_create_video_producer(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_create_video_producer %s", input.c_str());	
	ConnectorFrontApiHelper::createVideoProducerTrack(cd, input);
}

void ConnectorFrontApi::func_create_receive_transport(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");
	blog(LOG_DEBUG, "func_create_receive_transport %s", input.c_str());	
	ConnectorFrontApiHelper::createReceiver(input, cd);
}

void ConnectorFrontApi::func_video_consumer_response(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_video_consumer_response %s", input.c_str());	
	ConnectorFrontApiHelper::createConsumer(*static_cast<MediaSoupInterface::ObsSourceInfo*>(data), input, "video", cd);
}

void ConnectorFrontApi::func_audio_consumer_response(void* data, calldata_t* cd)
{
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "func_audio_consumer_response %s", input.c_str());	
	ConnectorFrontApiHelper::createConsumer(*static_cast<MediaSoupInterface::ObsSourceInfo*>(data), input, "audio", cd);
}

#endif
