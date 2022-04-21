// -- Initialization

1. Get 'rotuerRtpCapabilities' from the webserver
	DEMO API: GET 'https://v3demo.mediasoup.org:4443/rooms/{roomId}'

2. Create C++ plugin and fill OBS Setting 'room', 'rotuerRtpCapabilities'
	- Backend assigns values to OBS Setting 

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
	Receive json body from webserver -> Update OBS Setting 'send_transport_response' with json body

2. Upon signal from backend, finalize connection to 'audio' or 'video' with supplied json
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{senderId}/producers'
							json{	{ 'kind',          'audio'       },
								{ 'rtpParameters', rtpParameters }}.dump()); 
	C++ Plugin needs to know of success or fail, boolean

// -- Receiving A/V

1. Register a receive transport using 'deviceSctpCapabilities'
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/{clientId}/transports' 
							json{	{ 'type',    'webrtc'				},
								{ 'rtcpMux', true				},
								{ 'sctpCapabilities', deviceSctpCapabilities	}}.dump());
	Receive json body from webserver -> Update OBS Setting 'receive_transport_response' with json body

2. Register a video consumer that targets the track ID of your choice
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{receiverId}/consume?producerId={trackId}'
							json{	{ 'rtpCapabilities', deviceRtpCapabilities }}
	Receive json body from webserver -> Update OBS Setting 'video_consumer_response' with json body
	
3. Upon signal from backend, finalize connection to receive transport using { 'dtlsParameters', 'transportId' }
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/connect'
							json{	{ 'dtlsParameters', out_dtlsParameters }}
	C++ Plugin needs to know of success or fail, boolean

4. Audio consumption is done identically, but instead at/after step (2) replace 'video' to 'audio' in json and settings namings

// -- Side node

During steps that state 'Inform C++ Plugin of success or fail', the C++ plugin is in a state waiting for an answer so that it can proceede.



















