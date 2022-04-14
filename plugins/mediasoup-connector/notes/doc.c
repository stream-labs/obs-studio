// -- Initialization

1. Get 'rotuerRtpCapabilities' json blob from the webserver
	Demo api provides this via GET from 'https://v3demo.mediasoup.org:4443/rooms/{roomId}'

2. Wait for confirmation from C++ Plugin to initialize with 'rotuerRtpCapabilities'
	- C++ Plugin sends back to frontend  { 'clientId', 'deviceRtpCapabilities', 'deviceSctpCapabilities', 'version' }

3. Join lobby using 'clientId' and 'deviceRtpCapabilities', receive back 'peers' which has a list of their audio/video track IDs
	Demo api does this via POST 'https://v3demo.mediasoup.org:4443/rooms/{roomId}/broadcasters'
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
	Demo api does this via POST 'https://v3demo.mediasoup.org:4443/rooms/{clientId}/transports'
							json{	{ 'type',    'webrtc' },
								{ 'rtcpMux', true     }}.dump());	
	Receive { 'id', 'iceParameters', 'iceCandidates' 'dtlsParameters' } from webserver
	Send { 'id', 'iceParameters', 'iceCandidates' 'dtlsParameters' } to C++ Plugin

2. Wait for confirmation from C++ Plugin to intialize sender 
	- C++ Plugin sends back to frontend {'clientId', 'transportId', 'rtpParameters'} for video, and {'clientId', 'transportId', 'rtpParameters'} for audio

3. Finalize connection to send transport for AUDIO using corrosponding 'rtpParameters' (video is the same, but with 'video' instead at 'kind')
	Demo api does this via POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{senderId}/producers'
							json{	{ 'kind',          'audio'       },
								{ 'rtpParameters', rtpParameters }}.dump()); 
	Inform C++ Plugin of success or fail, boolean


4. Finalize connection to send transport for VIDEO using corrosponding 'rtpParameters'
	Demo api does this via POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{senderId}/producers'
						json{	{ 'kind',          'video'       },
							{ 'rtpParameters', rtpParameters }}.dump()); 
	Inform C++ Plugin of success or fail, boolean

// -- Receiving A/V

1. Register a receive transport using 'deviceSctpCapabilities'
	Demo api does this via POST 'https://v3demo.mediasoup.org:4443/rooms/{clientId}/transports' 
							json{	{ 'type',    'webrtc'				},
								{ 'rtcpMux', true				},
								{ 'sctpCapabilities', deviceSctpCapabilities	}}.dump());
	Receive {'id', 'iceParameters', 'iceCandidates' 'dtlsParameters', 'sctpParameters'} from webserver
	Send {'id', 'iceParameters', 'iceCandidates' 'dtlsParameters', 'sctpParameters'} to C++ Plugin

2. Wait for confirmation from C++ Plugin to intialize receiver
	C++ Plugin sends back to frontend 'receiverId'

3. Register a video consumer that targets the track ID of your choice
	Demo api does this via POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{receiverId}/consume?producerId={trackId}'
							json{	{ 'rtpCapabilities', deviceRtpCapabilities }}
	Receive { 'id', 'producerId', 'rtpParam' } from webserver
	Send { 'video', 'id', 'iceParameters', 'iceCandidates' 'dtlsParameters' } to C++ Plugin
	Receive { 'dtlsParameters', 'transportId' } from C++ Plugin
	
4. Finalize connection to video consumer using { 'dtlsParameters', 'transportId' }
	Demo api does this via POST 'https://v3demo.mediasoup.org:4443/rooms/broadcasters/{clientId}/transports/{transportId}/connect'
							json{	{ 'dtlsParameters', out_dtlsParameters }}
	Inform C++ Plugin of success or fail, boolean

5. Audio consumption is done identically, but instead during step (1) replace 'video' to 'audio' in this json { 'video', 'id', 'iceParameters', 'iceCandidates' 'dtlsParameters' } 

// -- Side node

During steps that state 'Inform C++ Plugin of success or fail', the C++ plugin is in a state waiting for an answer so that it can proceede.



















