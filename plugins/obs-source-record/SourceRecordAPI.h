#pragma once

#include <callback\calldata.h>

class SourceRecordAPI {
public:
	static void api_start(void *data, calldata_t *cd);
	static void api_stop(void *data, calldata_t *cd);
	static void api_get_available_formats(void *data, calldata_t *cd);
	static void api_get_available_encoders(void *data, calldata_t *cd);
};
