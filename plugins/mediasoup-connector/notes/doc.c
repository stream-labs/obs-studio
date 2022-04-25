// -- Initialization

1. Get 'rotuerRtpCapabilities' from the webserver
	DEMO API: GET 'https://v3demo.mediasoup.org:4443/rooms/{roomId}'

2. Create C++ plugin with OBS Setting 'room', 'rotuerRtpCapabilities'
	Backend will assign more values to OBS Setting 

3. Join lobby with parameters from OBS Settings
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/{roomId}/broadcasters'
							json{	{ "id",         clientId	},
								{ "displayName", "broadcaster"	},
								{ "device",
									{
										{ "name",    "libmediasoupclient"       },
										{ "version", version 			}
									}
								},
								{ "rtpCapabilities", deviceRtpCapabilities	}}.dump());

// -- Sending A/V

1. Register send transport
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/{clientId}/transports'
							json{	{ 'type',    'webrtc' },
								{ 'rtcpMux', true     }}.dump());	

2. Update OBS Setting 'send_transport_response' with received body

2. Update OBS Setting 'create_audio_producer' to 'true'
	The plugin does some work and assigns 'connect_params' json blob that has info needed to finalize the transport connection

3. Finalize transport connection 
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/connect'
							json{	{ 'dtlsParameters', dtlsParameters }}

4. Update OBS Settings 'connect_result' to 'true' or 'false' depending on success // Failure is unexpected, start over at (1) after plugin knows of the failure
	The plugin does some work and assigns 'produce_params' that has info needed to create producer on webserver

5. Register audio producer on webserver
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{senderId}/producers'
							json{	{ 'kind',          'audio'       },
								{ 'rtpParameters', rtpParameters }}.dump()); 

7. Update OBS Settings 'produce_result' to 'true' or 'false' depending on success // Failure is unexpected, start over at (1) after letting the plugin know of the failure

8. Update OBS Settings 'create_video_producer' to 'true' in OBS Setting
	The plugin does some work and assigns 'produce_params' json blob that has info needed to finalize the transport connection
	// The transport is already connectedso that's why we're not repeating (3) for the video producer

9. Create video producer on the webserver
	// Identically as step (4) but replace 'audio' with 'video'

// -- Receiving A/V

1. Register receive transport
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/{clientId}/transports'
							json{	{ "type",    "webrtc" },
								{ "rtcpMux", true     }}.dump());

2. Update OBS Setting 'receive_transport_response' with received body
	
3. Register video consumer (or audio)
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/consume?producerId={video_track_id}'
							json{	{ "type",    "webrtc" },
								{ "rtcpMux", true     }}.dump());

4. Update OBS Setting 'video_consumer_response' (or 'audio_$') with received body 
	The plugin does some work and assigns 'connect_params' json blob that has info needed to finalize the transport connection

5. Finalize transport connection 
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/connect'
							json{	{ 'dtlsParameters', dtlsParameters }}

6. For audio/video the steps are the same, but finalizing connection to transprot only happens once, at the time of the first consumer creation














