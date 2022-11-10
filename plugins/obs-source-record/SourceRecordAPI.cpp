#include "SourceRecordContext.h"
#include "SourceRecordAPI.h"
#include "nlohmann/json.hpp"

#include <string>

using namespace nlohmann;

/*
"path": directory where the file goes
"filename": name of the file, be sure to append the .format such as .flv etc
"audio_track": 1 to MAX_AUDIO_MIXES
"audio_source": name of the source where audio comes from 
"encoder": from api_get_available_encoders
*/

/*static*/
void SourceRecordAPI::api_start(void *data, calldata_t *cd)
{
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "api_start %s", input.c_str());
	
	context->m_outputMode = SourceRecordContext::OutputMode::OUTPUT_MODE_RECORDING;
	context->refresh();
}

/*static*/
void SourceRecordAPI::api_stop(void *data, calldata_t *cd)
{
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "api_stop %s", input.c_str());
	
	context->m_outputMode = SourceRecordContext::OutputMode::OUTPUT_MODE_RECORDING;
	context->refresh();
}

/*static*/
void SourceRecordAPI::api_get_available_formats(void *data, calldata_t *cd)
{
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "api_get_available_formats %s", input.c_str());

	json formats;
	
	formats.push_back("flv");
	formats.push_back("mp4");
	formats.push_back("mov");
	formats.push_back("mkv");
	formats.push_back("m3u8");
	formats.push_back("ts");
	
	calldata_set_string(cd, "output", formats.dump().c_str());
}

/*static*/
void SourceRecordAPI::api_get_available_encoders(void *data, calldata_t *cd)
{
	SourceRecordContext *context = reinterpret_cast<SourceRecordContext *>(data);
	std::string input = calldata_string(cd, "input");	
	blog(LOG_DEBUG, "api_get_available_encoders %s", input.c_str());

	json formats;
	
	if (SourceRecordContext::is_encoder_available("obs_qsv11"))
		formats.push_back("qsv");
	if (SourceRecordContext::is_encoder_available("ffmpeg_nvenc"))
		formats.push_back("nvenc");
	if (SourceRecordContext::is_encoder_available("amd_amf_h264"))
		formats.push_back("amd");
	
	const char *enc_id = NULL;
	size_t i = 0;
	while (obs_enum_encoder_types(i++, &enc_id)) {
		if (obs_get_encoder_type(enc_id) != OBS_ENCODER_VIDEO)
			continue;
		const uint32_t caps = obs_get_encoder_caps(enc_id);
		if ((caps & (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL)) != 0)
			continue;
		formats.push_back(enc_id);
	}
	
	calldata_set_string(cd, "output", formats.dump().c_str());
}
