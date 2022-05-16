// func_routerRtpCapabilities(string input, string output)
// func_send_transport_response(string input, string output)
// func_receive_transport_response(string input, string output)
// func_video_consumer_response(string input, string output)
// func_video_consumer_response(string input, string output)
// func_create_audio_producer(string input, string output)
// func_create_video_producer(string input, string output)
// func_produce_result(string input, string output)
// func_connect_result(string input, string output)
// func_stop_receiver(string input, string output)
// func_stop_sender(string input, string output)
// func_stop_consumer(string input, string output)
// func_change_playback_volume(string input, string output)
// func_get_playback_devices(string input, string output)
// func_change_playback_device(string input, string output)

// -- Initialization

1. Get 'routerRtpCapabilities' from the webserver
	DEMO API: GET 'https://v3demo.mediasoup.org:4443/rooms/{roomId}'

2. Create C++ plugin, apply OBS Setting 'room', then callHandler('func_routerRtpCapabilities', json{rotuerRtpCapabilities}.string)
	Output:	- "deviceRtpCapabilities"
		- "deviceSctpCapabilities"
		- "version"
		- "clientId"
		
3. Join lobby with parameters from OBS Settings
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/{roomId}/broadcasters'
							json{		{ "id",           clientId	},
									{ "displayName", "broadcaster"	},
									{ "device",
										{
											{ "name",    "libmediasoupclient"	},
											{ "version", version			}
										}
									},
									{ "rtpCapabilities", deviceRtpCapabilities	}
								}.dump());

// -- Sending A/V

1. Register send transport
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/{clientId}/transports'
							json{	{ 'type',    'webrtc' },
								{ 'rtcpMux', true     }}.dump());	

2. callHandler('func_send_transport_response', json{received body}.string)
						// "id"
						// "iceParameters"
						// "iceCandidates"
						// "dtlsParameters"

2. callHandler('func_create_audio_producer', '')
	Output: 'connect_params' // has info needed to finalize the transport connectio

3. Finalize transport connection 
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/connect'
							json{	{ 'dtlsParameters', dtlsParameters }}

4. callHandler('func_connect_result', 'true' or 'false') // Failure is unexpected, start over at (1) after plugin knows of the failure
	Output: 'produce_params' // has info needed to create producer on webserver

5. Register audio producer on webserver
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{senderId}/producers'
							json{	{ 'kind',          'audio'       },
								{ 'rtpParameters', rtpParameters }}.dump()); 

6. callHandler('func_produce_result', 'true' or 'false') // Failure is unexpected, start over at (1) after letting the plugin know of the failure

7. callHandler('func_create_video_producer', '')
	Output: 'produce_params' // has info needed to finalize the transport connectio
	// The transport is already connected, so that's why we're not repeating (3) for the video producer

8. Create video producer on the webserver
	// Identically as step (5) but replace 'audio' with 'video'

// -- Receiving A/V

1. Register receive transport
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/{clientId}/transports'
							json{	{ "type",    "webrtc" },
								{ "rtcpMux", true     }}.dump());

2. callHandler('func_receive_transport_response', json{received body}.string)
						// "id"
						// "iceParameters"
						// "iceCandidates"
						// "dtlsParameters"
						// "sctpParameters"

3. Register video consumer (or audio)
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/consume?producerId={video_track_id}'
							json{	{ "type",    "webrtc" },
								{ "rtcpMux", true     }}.dump());

4. callHandler('func_video_consumer_response', json{received body}.string) // (or 'audio_$')
	Output: 'connect_params' // has info needed to finalize the transport connectio

5. Finalize transport connection 
	DEMO API: POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/connect'
							json{{ 'dtlsParameters', dtlsParameters }}

6. callHandler('func_connect_result', 'true' or 'false') // Failure is unexpected, start over at (1) after plugin knows of the failure

7. For audio/video the steps are the same, but finalizing connection to transprot only happens once, at the time of the first consumer creation
	NOTE: After audio starts from the audio consumer, 'func_get_playback_devices' is ready // Output: { id: string, name: string },{ id: string, name: string }

// -- Restarting either the Send or Receive transport

- callHandler(func_stop_receiver, '')
- callHandler(func_stop_sender, '')

// -- Changing video/audio consumer to target a different track

1. callHandler('func_stop_consumer', 'ID') // string ID of the consumer, not json

2. Do step (3) in /* Receiving A/V */ section
	- Finalizing transport connection again is not needed, hence why you can (5) and (6) If youve already done them once

// -- Changing audio playback device / volume

- Volume: Update OBS Setting string 0-100 'func_change_playback_volume', string not an integer
- Device:
	There is already values assigned to OBS Setting 'playback_devices' when the plugin is created
	If you null that value out and then perform update on settings, the backend will re-fill it with an updated list

	NOTE: webrtc picks a device on its own and starts playing, the id of the chosen device isnt specified anywhere so I cant provide it to you
		// It picks the users's "default" device without reporting back to me what that is, I could maybe change this to default to the first device in the list so that we know the id of the device playing, TBD

- Changing playback device
	callHandler('func_change_playback_device', 'ID') // string ID of the device
