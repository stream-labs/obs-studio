
#include "win-wasapi-app.hpp"

#include <initguid.h>
#include <propsys.h>

#include <obs-module.h>
#include <obs.h>
#include <util/platform.h>
#include <util/windows/HRError.hpp>

#include <util/windows/CoTaskMemPtr.hpp>
#include <util/threading.h>
#include <util/util_uint64.h>

#include <cinttypes>


#include <avrt.h>

#include <psapi.h>

using namespace std;
using namespace Microsoft::WRL;

#define OPT_DEVICE_ID "device_id"
#define OPT_USE_DEVICE_TIMING "use_device_timing"

#define OBS_KSAUDIO_SPEAKER_4POINT1 \
	(KSAUDIO_SPEAKER_SURROUND | SPEAKER_LOW_FREQUENCY)

typedef HRESULT(STDAPICALLTYPE *PFN_RtwqUnlockWorkQueue)(DWORD);
typedef HRESULT(STDAPICALLTYPE *PFN_RtwqLockSharedWorkQueue)(PCWSTR usageClass,
							     LONG basePriority,
							     DWORD *taskId,
							     DWORD *id);
typedef HRESULT(STDAPICALLTYPE *PFN_RtwqCreateAsyncResult)(IUnknown *,
							   IRtwqAsyncCallback *,
							   IUnknown *,
							   IRtwqAsyncResult **);
typedef HRESULT(STDAPICALLTYPE *PFN_RtwqPutWorkItem)(DWORD, LONG,
						     IRtwqAsyncResult *);
typedef HRESULT(STDAPICALLTYPE *PFN_RtwqPutWaitingWorkItem)(HANDLE, LONG,
							    IRtwqAsyncResult *,
							    RTWQWORKITEM_KEY *);

class WASAPIAppSource {
	ComPtr<IAudioClient> client;
	ComPtr<IAudioCaptureClient> capture;

	static const int MAX_RETRY_INIT_DEVICE_COUNTER = 3;

	obs_source_t *source;
	std::string selected_session;

	PFN_RtwqUnlockWorkQueue rtwq_unlock_work_queue = NULL;
	PFN_RtwqLockSharedWorkQueue rtwq_lock_shared_work_queue = NULL;
	PFN_RtwqCreateAsyncResult rtwq_create_async_result = NULL;
	PFN_RtwqPutWorkItem rtwq_put_work_item = NULL;
	PFN_RtwqPutWaitingWorkItem rtwq_put_waiting_work_item = NULL;
	bool rtwq_supported = false;
	
	std::atomic<bool> useDeviceTiming = false;

	bool previouslyFailed = false;
	WinHandle reconnectThread;

	class CallbackStartCapture : public ARtwqAsyncCallback {
	public:
		CallbackStartCapture(WASAPIAppSource *source)
			: ARtwqAsyncCallback(source)
		{
		}

		STDMETHOD(Invoke)
		(IRtwqAsyncResult *) override
		{
			((WASAPIAppSource *)source)->OnStartCapture();
			return S_OK;
		}

	} startCapture;
	ComPtr<IRtwqAsyncResult> startCaptureAsyncResult;

	class CallbackSampleReady : public ARtwqAsyncCallback {
	public:
		CallbackSampleReady(WASAPIAppSource *source)
			: ARtwqAsyncCallback(source)
		{
		}

		STDMETHOD(Invoke)
		(IRtwqAsyncResult *) override
		{
			((WASAPIAppSource *)source)->OnSampleReady();
			return S_OK;
		}
	} sampleReady;
	ComPtr<IRtwqAsyncResult> sampleReadyAsyncResult;

	class CallbackRestart : public ARtwqAsyncCallback {
	public:
		CallbackRestart(WASAPIAppSource *source)
			: ARtwqAsyncCallback(source)
		{
		}

		STDMETHOD(Invoke)
		(IRtwqAsyncResult *) override
		{
			((WASAPIAppSource *)source)->OnRestart();
			return S_OK;
		}
	} restart;
	ComPtr<IRtwqAsyncResult> restartAsyncResult;

	WinHandle captureThread;
	WinHandle idleSignal;
	WinHandle stopSignal;
	WinHandle receiveSignal;
	WinHandle restartSignal;
	WinHandle exitSignal;
	WinHandle initSignal;
	DWORD reconnectDuration = 0;
	WinHandle reconnectSignal;
	HANDLE ProcessCallbackHandle = 0;

	speaker_layout speakers;
	audio_format format;
	uint32_t sampleRate;

	static DWORD WINAPI ReconnectThread(LPVOID param);
	static DWORD WINAPI CaptureThread(LPVOID param);

	bool ProcessCaptureData();

	void Start();
	void Stop();

	static ComPtr<IAudioClient> InitClient(enum speaker_layout &speakers,
					       enum audio_format &format,
					       uint32_t &sampleRate, DWORD target_PID);
	static void InitFormat(const WAVEFORMATEX *wfex,
			       enum speaker_layout &speakers,
			       enum audio_format &format, uint32_t &sampleRate);
	static ComPtr<IAudioCaptureClient> InitCapture(IAudioClient *client,
						       HANDLE receiveSignal);

	void Initialize();
	bool TryInitialize();
	void UpdateSettings(obs_data_t *settings);

public:
	WASAPIAppSource(obs_data_t *settings, obs_source_t *source_);
	virtual ~WASAPIAppSource();

	void Update(obs_data_t *settings);

	void OnStartCapture();
	void OnSampleReady();
	void OnRestart();
	void OnProcessClose();
	void StopWaitingProcessClose();
};


AppAudioDevices::AppAudioDevices(ComPtr<IMMDevice> new_device, std::wstring new_device_id):device(new_device), device_id(new_device_id) 
{
	HRESULT hr;

	hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**) manager.ReleaseAndGetAddressOf());
	if (FAILED(hr))
		throw "Failed to activate a device";

	hr = manager->RegisterSessionNotification(&notification_client);
	if (FAILED(hr))
		throw "Failed to register device notification reciever";

	hr = manager->GetSessionEnumerator(enumerator.ReleaseAndGetAddressOf());
	if (FAILED(hr))
		throw "Failed to enumerate device sessions";

	int sessions = 0;
	hr = enumerator->GetCount(&sessions);
	if (FAILED(hr))
		throw "Failed to get count of device sessions";

	for (int i = 0; i < sessions; ++i) {
		ComPtr<IAudioSessionControl> session;
		hr = enumerator->GetSession(i, session.ReleaseAndGetAddressOf());
		if (FAILED(hr))
			throw "Failed to get session from enumeration";

		AudioSessionState session_state;
		hr = session->GetState(&session_state);
		if (FAILED(hr))
			throw "Failed to get sessions state";

		if (session_state != AudioSessionStateExpired) {
			session->AddRef();
			AppDevicesCache::getInstance()->AddSession(session);
		}
	}
}

AppAudioDevices::~AppAudioDevices() {
	blog(LOG_DEBUG, "[WASAPIAPPSOURCE] remove app device: id %s", std::string(device_id.begin(), device_id.end()).c_str());
	manager->UnregisterSessionNotification(&notification_client);
}

AppAudioSession::AppAudioSession(ComPtr<IAudioSessionControl> new_session_control, DWORD new_PID, std::string new_executable)
	: session_control(new_session_control),
	  PID(new_PID),
	  executable(new_executable)
{
	HRESULT hr;

	session_control->RegisterAudioSessionNotification(&notification_client);
	notification_client.SetSessionControl(session_control);
}

AppAudioSession::~AppAudioSession()
{
	blog(LOG_DEBUG, "[WASAPIAPPSOURCE] app device session removed");
	session_control->UnregisterAudioSessionNotification(
		&notification_client);
}

long AppDevicesCache::refs = 0;
AppDevicesCache* AppDevicesCache::instance = nullptr;

DWORD AppDevicesCache::getPID(std::string session)
{
	DWORD pid = 0;
	std::lock_guard<std::recursive_mutex> lk(cached_mutex);
	auto app_session = app_sessions.find(std::wstring(session.begin(), session.end()));
	if(app_session != app_sessions.end())
		pid = app_session->second.PID;

	return pid;
}

std::string AppDevicesCache::GetExecutableByPID(DWORD pid)
{
	WinHandle exe_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid);
	if (!exe_handle)
		return "";

	wchar_t exe_name_buf[MAX_PATH] = {0};
	DWORD length = GetProcessImageFileName(exe_handle, exe_name_buf, MAX_PATH - 1);
	auto exe_num_chars = WideCharToMultiByte(CP_UTF8, 0, exe_name_buf, -1, NULL, 0, NULL, NULL);
	std::string executable_path(exe_num_chars - 1, '0');
	WideCharToMultiByte(CP_UTF8, 0, exe_name_buf, -1, &executable_path[0], exe_num_chars, NULL, NULL);
	std::string executable = executable_path.substr(executable_path.find_last_of("\\") + 1);

	return executable;
}

void AppDevicesCache::AddSession(ComPtr<IAudioSessionControl> session_control)
{
	HRESULT hr;
	ComPtr<IAudioSessionControl2> session_control2;

	hr = session_control->QueryInterface( __uuidof(IAudioSessionControl2), (void**)session_control2.ReleaseAndGetAddressOf());
	
	LPWSTR pwszID = NULL;
	hr = session_control2->GetSessionIdentifier(&pwszID);
	std::wstring session_id = std::wstring(pwszID);

	DWORD PID;
	hr = session_control2->GetProcessId(&PID);
	if (FAILED(hr))
		return;

	if (!PID)
		return;
	std::string executable = GetExecutableByPID(PID);

	blog(LOG_DEBUG, "[WASAPIAPPSOURCE] app device session: pid %d exe %s id %s", PID, executable.c_str(), std::string(session_id.begin(), session_id.end()).c_str());
 	
	std::lock_guard<std::recursive_mutex> lk(cached_mutex);
	app_sessions.try_emplace(session_id, session_control, PID, executable);
}

void AppDevicesCache::RemoveSession(ComPtr<IAudioSessionControl> session_control)
{
	HRESULT hr;
	LPWSTR pwszID = NULL;
	ComPtr<IAudioSessionControl2> session_control2;

	hr = session_control->QueryInterface( __uuidof(IAudioSessionControl2), (void**)session_control2.ReleaseAndGetAddressOf());
	if (FAILED(hr)) 
		return;

	hr = session_control2->GetSessionIdentifier(&pwszID);
	if (FAILED(hr)) 
		return;
	
	std::wstring session_id = std::wstring(pwszID);

	std::lock_guard<std::recursive_mutex> lk(cached_mutex);
	app_sessions.erase(session_id);

	blog(LOG_DEBUG, "[WASAPIAPPSOURCE] remove session: id %s", std::string(session_id.begin(), session_id.end()).c_str());
}

void AppDevicesCache::AddDevice(LPCWSTR device_id)
{
	std::lock_guard<std::recursive_mutex> lk(cached_mutex);
	if(app_devices.count(device_id) == 1)
		return;

	ComPtr<IMMDevice> device;
	HRESULT hr = enumerator->GetDevice(device_id, device.ReleaseAndGetAddressOf());
	if (FAILED(hr)) 
		return;

	ComPtr<IMMEndpoint> endpoint;
	hr = device->QueryInterface( __uuidof(IMMEndpoint), (void**)endpoint.ReleaseAndGetAddressOf());
	if (FAILED(hr)) 
		return;

	EDataFlow data_flow;
	hr = endpoint->GetDataFlow(&data_flow);
	if (!FAILED(hr) && data_flow == eRender )
		app_devices.try_emplace(device_id, device, device_id);
}

void AppDevicesCache::RemoveDevice(LPCWSTR device_id)
{
	std::lock_guard<std::recursive_mutex> lk(cached_mutex);
	app_devices.erase(device_id);
}

AppDevicesCache::AppDevicesCache() 
{
	notify = Microsoft::WRL::Make<WASAPINotify>();
	if (!notify)
		throw "Could not create WASAPINotify";
		
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
				CLSCTX_ALL,
				IID_PPV_ARGS(enumerator.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
		throw HRError("Failed to create enumerator", hr);

	hr = enumerator->RegisterEndpointNotificationCallback(notify.Get());
	if (FAILED(hr))
		throw HRError("Failed to register endpoint callback", hr);
}

AppDevicesCache ::~AppDevicesCache()
{
	enumerator->UnregisterEndpointNotificationCallback(notify.Get());
}

void AppDevicesCache::InitCache() {
	HRESULT hr;
	ComPtr<IMMDeviceCollection> collection;

	hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.ReleaseAndGetAddressOf());
	if (FAILED(hr)) {
		enumerator->UnregisterEndpointNotificationCallback(notify.Get());
		throw HRError("Failed to get a list of audio endpoints", hr);
	}

	UINT count = 0;
	collection->GetCount(&count);
	
	for (UINT i = 0; i < count; ++i) {
		ComPtr<IMMDevice> device;
		collection->Item(i, device.ReleaseAndGetAddressOf());

		LPWSTR pwszID = NULL;
		hr = device->GetId(&pwszID);
		if (FAILED(hr))
			throw "Failed to get a device name";

		std::wstring device_id = std::wstring(pwszID);
		
		std::string name = std::string(device_id.begin(), device_id.end());
		blog(LOG_DEBUG, "[WASAPIAPPSOURCE] app device name %s", name.c_str());
		
		std::lock_guard<std::recursive_mutex> lk(cached_mutex);
		app_devices.try_emplace(device_id, device, device_id);
	}
}

typedef std::tuple<std::wstring, std::string> AppSessionID;

const std::vector<AppSessionID> AppDevicesCache::GetSessionList()
{
	std::vector<AppSessionID> ret;
	std::lock_guard<std::recursive_mutex> lk(cached_mutex);

	for(const auto & app_session : app_sessions) {
		ret.push_back(AppSessionID(app_session.first, app_session.second.executable));
	}
	return ret;
}

WASAPIAppSource::WASAPIAppSource(obs_data_t *settings, obs_source_t *source_)
	: source(source_),
	  startCapture(this),
	  sampleReady(this),
	  restart(this)
{
	HRESULT hr;
	UpdateSettings(settings);

	blog(LOG_INFO, "[WASAPIAppSource][%08X] WASAPI Source constructor", this);

	idleSignal = CreateEvent(nullptr, true, false, nullptr);
	if (!idleSignal.Valid())
		throw "Could not create idle signal";

	stopSignal = CreateEvent(nullptr, true, false, nullptr);
	if (!stopSignal.Valid())
		throw "Could not create stop signal";

	receiveSignal = CreateEvent(nullptr, false, false, nullptr);
	if (!receiveSignal.Valid())
		throw "Could not create receive signal";

	restartSignal = CreateEvent(nullptr, true, false, nullptr);
	if (!restartSignal.Valid())
		throw "Could not create restart signal";

	exitSignal = CreateEvent(nullptr, true, false, nullptr);
	if (!exitSignal.Valid())
		throw "Could not create exit signal";

	initSignal = CreateEvent(nullptr, false, false, nullptr);
	if (!initSignal.Valid())
		throw "Could not create init signal";

	reconnectSignal = CreateEvent(nullptr, false, false, nullptr);
	if (!reconnectSignal.Valid())
		throw "Could not create reconnect signal";

	reconnectThread = CreateThread(
		nullptr, 0, WASAPIAppSource::ReconnectThread, this, 0, nullptr);
	if (!reconnectThread.Valid())
		throw "Failed to create reconnect thread";



	/* OBS will already load DLL on startup if it exists */
	const HMODULE rtwq_module = GetModuleHandle(L"RTWorkQ.dll");
	rtwq_supported = rtwq_module != NULL;
	if (rtwq_supported) {
		rtwq_unlock_work_queue =
			(PFN_RtwqUnlockWorkQueue)GetProcAddress(
				rtwq_module, "RtwqUnlockWorkQueue");
		rtwq_lock_shared_work_queue =
			(PFN_RtwqLockSharedWorkQueue)GetProcAddress(
				rtwq_module, "RtwqLockSharedWorkQueue");
		rtwq_create_async_result =
			(PFN_RtwqCreateAsyncResult)GetProcAddress(
				rtwq_module, "RtwqCreateAsyncResult");
		rtwq_put_work_item = (PFN_RtwqPutWorkItem)GetProcAddress(
			rtwq_module, "RtwqPutWorkItem");
		rtwq_put_waiting_work_item =
			(PFN_RtwqPutWaitingWorkItem)GetProcAddress(
				rtwq_module, "RtwqPutWaitingWorkItem");

		hr = rtwq_create_async_result(nullptr, &startCapture, nullptr,
					      &startCaptureAsyncResult);
		if (FAILED(hr)) {
			throw HRError(
				"Could not create startCaptureAsyncResult", hr);
		}

		hr = rtwq_create_async_result(nullptr, &sampleReady, nullptr,
					      &sampleReadyAsyncResult);
		if (FAILED(hr)) {
			throw HRError("Could not create sampleReadyAsyncResult",
				      hr);
		}

		hr = rtwq_create_async_result(nullptr, &restart, nullptr,
					      &restartAsyncResult);
		if (FAILED(hr)) {
			throw HRError("Could not create restartAsyncResult",
				      hr);
		}

		DWORD taskId = 0;
		DWORD id = 0;
		LPWSTR MMCSS_Capture = L"Capture";
		hr = rtwq_lock_shared_work_queue(MMCSS_Capture, 0, &taskId, &id);
		blog(LOG_INFO,"[WASAPIApp] rtwq_lock_shared_work_queue CaptureGame hr %d", hr);
		if (FAILED(hr)) {
			throw HRError("Call to RtwqLockSharedWorkQueue failed", hr);
		}

		startCapture.SetQueueId(id);
		sampleReady.SetQueueId(id);
		restart.SetQueueId(id);
	} else {
		captureThread = CreateThread(nullptr, 0,
					     WASAPIAppSource::CaptureThread, this,
					     0, nullptr);
		if (!captureThread.Valid()) {
			throw "Failed to create capture thread";
		}
	}
	
	AppDevicesCache::addRef();

	Start();
}

void WASAPIAppSource::Start()
{
	if (rtwq_supported) {
		rtwq_put_work_item(startCapture.GetQueueId(), 0,
				   startCaptureAsyncResult.Get());
	} else {
		SetEvent(initSignal);
	}
}

void WASAPIAppSource::Stop()
{
	SetEvent(stopSignal);

	blog(LOG_INFO, "WASAPIApp: Device Terminated");

	if (rtwq_supported)
		SetEvent(receiveSignal);

	WaitForSingleObject(idleSignal, INFINITE);

	SetEvent(exitSignal);

	WaitForSingleObject(reconnectThread, INFINITE);

	if (rtwq_supported)
		rtwq_unlock_work_queue(sampleReady.GetQueueId());
	else
		WaitForSingleObject(captureThread, INFINITE);
}

WASAPIAppSource::~WASAPIAppSource()
{
	StopWaitingProcessClose();

	Stop();
	AppDevicesCache::releaseRef();
}

void WASAPIAppSource::UpdateSettings(obs_data_t *settings)
{
	selected_session = obs_data_get_string(settings, OPT_DEVICE_ID);
	useDeviceTiming = obs_data_get_bool(settings, OPT_USE_DEVICE_TIMING);

	blog(LOG_INFO,"[WASAPIApp] update settings, selected session %s", selected_session.c_str());
}

void WASAPIAppSource::Update(obs_data_t *settings)
{
	UpdateSettings(settings);

	SetEvent(restartSignal);
}

#define BUFFER_TIME_100NS (5 * 10000000)

ComPtr<IAudioClient> WASAPIAppSource::InitClient(enum speaker_layout &speakers,
					      enum audio_format &format,
					      uint32_t &sampleRate, DWORD target_PID)
{
	AUDIOCLIENT_ACTIVATION_PARAMS params = {};
	params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
	params.ProcessLoopbackParams.TargetProcessId = target_PID;
	params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
	
	PROPVARIANT propvariant = {0};
	propvariant.vt = VT_BLOB;
	propvariant.blob.cbSize = sizeof(params);
	propvariant.blob.pBlobData = (BYTE *)&params;
 
	ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;
	WASAPIAppSourceActivatedNotify activated;
	
	HRESULT hr = ActivateAudioInterfaceAsync(
		VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
		&propvariant, &activated, &async_op);
	if (FAILED(hr)) {
		throw HRError("Failed activate interface", hr);
	}

	WaitForSingleObject(activated.signal, INFINITE);

	if(FAILED(activated.hr)) 
		throw HRError("Failed to set event handle on interface activation", activated.hr);
	

	ComPtr<IAudioClient> temp_client = activated.getClient();

	obs_audio_info info;
	obs_get_audio_info(&info);

	WAVEFORMATEX wfex;
	wfex.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	wfex.nChannels = info.speakers;
	wfex.nSamplesPerSec = info.samples_per_sec;

	wfex.nBlockAlign = wfex.nChannels * sizeof(float);
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;
	wfex.wBitsPerSample = CHAR_BIT * sizeof(float);
	wfex.cbSize = 0;

	InitFormat(&wfex, speakers, format, sampleRate);
	
	temp_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			BUFFER_TIME_100NS, 0, &wfex, NULL);
	blog(LOG_DEBUG, "[WASAPIAppSource] Initialization of app audio capture client finished");
	return temp_client;
}

ComPtr<IAudioCaptureClient> WASAPIAppSource::InitCapture(IAudioClient *client,
						      HANDLE receiveSignal)
{
	ComPtr<IAudioCaptureClient> capture;

	HRESULT res = client->GetService(IID_PPV_ARGS(capture.ReleaseAndGetAddressOf()));
	if (FAILED(res))
		throw HRError("Failed to create capture context", res);

	res = client->SetEventHandle(receiveSignal);
	if (FAILED(res))
		throw HRError("Failed to set event handle", res);

	res = client->Start();
	if (FAILED(res))
		throw HRError("Failed to start capture client", res);

	return capture;
}

static speaker_layout ConvertSpeakerLayout(DWORD layout, WORD channels)
{
	switch (layout) {
	case KSAUDIO_SPEAKER_2POINT1:
		return SPEAKERS_2POINT1;
	case KSAUDIO_SPEAKER_SURROUND:
		return SPEAKERS_4POINT0;
	case OBS_KSAUDIO_SPEAKER_4POINT1:
		return SPEAKERS_4POINT1;
	case KSAUDIO_SPEAKER_5POINT1_SURROUND:
		return SPEAKERS_5POINT1;
	case KSAUDIO_SPEAKER_7POINT1_SURROUND:
		return SPEAKERS_7POINT1;
	}

	return (speaker_layout)channels;
}

void WASAPIAppSource::InitFormat(const WAVEFORMATEX *wfex,
			      enum speaker_layout &speakers,
			      enum audio_format &format, uint32_t &sampleRate)
{
	DWORD layout = 0;

	if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wfex;
		layout = ext->dwChannelMask;
	}

	speakers = ConvertSpeakerLayout(layout, wfex->nChannels);
	format = AUDIO_FORMAT_FLOAT;
	sampleRate = wfex->nSamplesPerSec;
}

VOID CALLBACK WaitOrTimerCallback(_In_  PVOID lpParameter, _In_  BOOLEAN TimerOrWaitFired)
{
	blog(LOG_DEBUG, "[WASAPIAppSource] Callback for a process state.");
	if(lpParameter != nullptr) {
		WASAPIAppSource *gs = (WASAPIAppSource*)lpParameter;
		gs->OnProcessClose();
	}
	return;
}

void WASAPIAppSource::Initialize()
{
	ResetEvent(receiveSignal);
	DWORD process_pid = AppDevicesCache::getInstance()->getPID(selected_session);
	if(!process_pid)
		throw HRError("Selected source PID is 0", 0);

	blog(LOG_DEBUG, "[WASAPIAppSource] Initialize app audio capture for PID %d", process_pid);
	
	StopWaitingProcessClose();
	WinHandle hProcHandle = OpenProcess(SYNCHRONIZE, FALSE, process_pid);
	if (hProcHandle) {
		RegisterWaitForSingleObject(&ProcessCallbackHandle, hProcHandle , WaitOrTimerCallback, this, INFINITE, WT_EXECUTEONLYONCE);
	} else {
		throw HRError("Selected source PID is not alive", 0);
	}

	ComPtr<IAudioClient> temp_client = InitClient(speakers, format, sampleRate, process_pid);
	
	ComPtr<IAudioCaptureClient> temp_capture = InitCapture(temp_client.Get(), receiveSignal);

	client = std::move(temp_client);
	capture = std::move(temp_capture);
	
	blog(LOG_DEBUG, "[WASAPIAppSource] Initialize before setting queue");
	if (rtwq_supported) {
		HRESULT hr = rtwq_put_waiting_work_item(
			receiveSignal, 0, sampleReadyAsyncResult.Get(), nullptr);
		if (FAILED(hr)) {
			capture.Reset();
			client.Reset();
			throw HRError("RtwqPutWaitingWorkItem failed", hr);
		}

		hr = rtwq_put_waiting_work_item(restartSignal, 0,
						restartAsyncResult.Get(), nullptr);
		if (FAILED(hr)) {
			capture.Reset();
			client.Reset();
			throw HRError("RtwqPutWaitingWorkItem failed", hr);
		}
	}

}

bool WASAPIAppSource::TryInitialize()
{
	bool success = false;
	try {
		Initialize();
		success = true;
	} catch (HRError &error) {
		{
			blog(LOG_WARNING,
			     "[WASAPIAppSource::TryInitialize]: %s: %lX",
			     error.str, error.hr);
		}
	}

	previouslyFailed = !success;
	return success;
}

DWORD WINAPI WASAPIAppSource::ReconnectThread(LPVOID param)
{
	os_set_thread_name("win-wasapi-app: reconnect thread");

	WASAPIAppSource *source = (WASAPIAppSource *)param;

	const HANDLE sigs[] = {
		source->exitSignal,
		source->reconnectSignal,
	};

	bool exit = false;
	while (!exit) {
		const DWORD ret = WaitForMultipleObjects(_countof(sigs), sigs,
							 false, INFINITE);
		switch (ret) {
		case WAIT_OBJECT_0:
			exit = true;
			break;
		default:
			assert(ret == (WAIT_OBJECT_0 + 1));
			if (source->reconnectDuration > 0) {
				WaitForSingleObject(source->stopSignal,
						    source->reconnectDuration);
			}
			source->Start();
		}
	}

	return 0;
}

bool WASAPIAppSource::ProcessCaptureData()
{
	HRESULT res;
	LPBYTE buffer;
	UINT32 frames;
	DWORD flags;
	UINT64 pos, ts;
	UINT captureSize = 0;

	while (true) {
		res = capture->GetNextPacketSize(&captureSize);
		if (FAILED(res)) {
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				blog(LOG_WARNING,
				     "[WASAPIAppSource::ProcessCaptureData]"
				     " capture->GetNextPacketSize"
				     " failed: %lX",
				     res);
			return false;
		}

		if (!captureSize)
			break;

		res = capture->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
		if (FAILED(res)) {
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				blog(LOG_WARNING,
				     "[WASAPIAppSource::ProcessCaptureData]"
				     " capture->GetBuffer"
				     " failed: %lX",
				     res);
			return false;
		}

		obs_source_audio data = {};
		data.data[0] = (const uint8_t *)buffer;
		data.frames = (uint32_t)frames;
		data.speakers = speakers;
		data.samples_per_sec = sampleRate;
		data.format = format;
		data.timestamp = useDeviceTiming ? ts * 100 : os_gettime_ns();

		if (!useDeviceTiming)
			data.timestamp -= util_mul_div64(frames, 1000000000ULL,
							 sampleRate);

		obs_source_output_audio(source, &data);

		capture->ReleaseBuffer(frames);
	}

	return true;
}

#define RECONNECT_INTERVAL 3000

DWORD WINAPI WASAPIAppSource::CaptureThread(LPVOID param)
{
	os_set_thread_name("win-wasapi-app: capture thread");

	const HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	const bool com_initialized = SUCCEEDED(hr);
	if (!com_initialized) {
		blog(LOG_ERROR,
		     "[WASAPIAppSource::CaptureThread]"
		     " CoInitializeEx failed: 0x%08X",
		     hr);
	}

	DWORD unused = 0;
	const HANDLE handle = AvSetMmThreadCharacteristics(L"Audio", &unused);

	WASAPIAppSource *source = (WASAPIAppSource *)param;

	const HANDLE inactive_sigs[] = {
		source->exitSignal,
		source->stopSignal,
		source->initSignal,
	};

	const HANDLE active_sigs[] = {
		source->exitSignal,
		source->stopSignal,
		source->receiveSignal,
		source->restartSignal,
	};

	DWORD sig_count = _countof(inactive_sigs);
	const HANDLE *sigs = inactive_sigs;

	bool exit = false;
	while (!exit) {
		bool idle = false;
		bool stop = false;
		bool reconnect = false;
		do {
			/* Windows 7 does not seem to wake up for LOOPBACK */
			const DWORD dwMilliseconds = (sigs == active_sigs)
							     ? 10
							     : INFINITE;

			const DWORD ret = WaitForMultipleObjects(
				sig_count, sigs, false, dwMilliseconds);
			switch (ret) {
			case WAIT_OBJECT_0: {
				exit = true;
				stop = true;
				idle = true;
				break;
			}

			case WAIT_OBJECT_0 + 1:
				stop = true;
				idle = true;
				break;

			case WAIT_OBJECT_0 + 2:
			case WAIT_TIMEOUT:
				if (sigs == inactive_sigs) {
					assert(ret != WAIT_TIMEOUT);

					if (source->TryInitialize()) {
						sig_count =
							_countof(active_sigs);
						sigs = active_sigs;
					} else {
						blog(LOG_INFO,
						     "[WASAPIAppSource]: Device failed to start");
						stop = true;
						reconnect = true;
						source->reconnectDuration =
							RECONNECT_INTERVAL;
					}
				} else {
					stop = !source->ProcessCaptureData();
					if (stop) {
						blog(LOG_INFO,
						     "[WASAPIAppSource] Audio Source invalidated.  Retrying" );
						stop = true;
						reconnect = true;
						source->reconnectDuration =
							RECONNECT_INTERVAL;
					}
				}
				break;

			default:
				assert(sigs == active_sigs);
				assert(ret == WAIT_OBJECT_0 + 3);
				stop = true;
				reconnect = true;
				source->reconnectDuration = 0;
				ResetEvent(source->restartSignal);
			}
		} while (!stop);

		sig_count = _countof(inactive_sigs);
		sigs = inactive_sigs;

		if (source->client) {
			source->client->Stop();

			source->capture.Reset();
			source->client.Reset();
		}

		if (idle) {
			SetEvent(source->idleSignal);
		} else if (reconnect) {
			blog(LOG_INFO, "Device  invalidated.  Retrying" );
			SetEvent(source->reconnectSignal);
		}
	}

	if (handle)
		AvRevertMmThreadCharacteristics(handle);

	if (com_initialized)
		CoUninitialize();

	return 0;
}

void WASAPIAppSource::OnStartCapture()
{
	const DWORD ret = WaitForSingleObject(stopSignal, 0);
	switch (ret) {
	case WAIT_OBJECT_0:
		SetEvent(idleSignal);
		break;

	default:
		assert(ret == WAIT_TIMEOUT);

		if (!TryInitialize()) {
			blog(LOG_INFO, "[WASAPIAppSource] Device failed to start");
			reconnectDuration = RECONNECT_INTERVAL;
			SetEvent(reconnectSignal);
		}
	}
}

void WASAPIAppSource::OnSampleReady()
{
	bool stop = false;
	bool reconnect = false;

	if (!ProcessCaptureData()) {
		stop = true;
		reconnect = true;
		reconnectDuration = RECONNECT_INTERVAL;
	}

	if (WaitForSingleObject(restartSignal, 0) == WAIT_OBJECT_0) {
		stop = true;
		reconnect = true;
		reconnectDuration = 0;

		ResetEvent(restartSignal);
		rtwq_put_waiting_work_item(restartSignal, 0, restartAsyncResult.Get(),
					   nullptr);
	}

	if (WaitForSingleObject(stopSignal, 0) == WAIT_OBJECT_0) {
		stop = true;
		reconnect = false;
	}

	if (!stop) {
		if (FAILED(rtwq_put_waiting_work_item(receiveSignal, 0,
						      sampleReadyAsyncResult.Get(),
						      nullptr))) {
			blog(LOG_ERROR,
			     "Could not requeue sample receive work");
			stop = true;
			reconnect = true;
			reconnectDuration = RECONNECT_INTERVAL;
		}
	}

	if (stop) {
		blog(LOG_DEBUG, "[WASAPIAppSource] Stop inside OnSampleReady");
		client->Stop();

		capture.Reset();
		client.Reset();

		if (reconnect) {
			blog(LOG_INFO, "Device invalidated.  Retrying");
			SetEvent(reconnectSignal);
		} else {
			SetEvent(idleSignal);
		}
	}
}

void WASAPIAppSource::OnRestart()
{
	SetEvent(receiveSignal);
}

void WASAPIAppSource::StopWaitingProcessClose()
{
	if(ProcessCallbackHandle) {
		UnregisterWait(ProcessCallbackHandle);
		ProcessCallbackHandle =NULL;
	}	
}

void WASAPIAppSource::OnProcessClose()
{
	StopWaitingProcessClose();
	SetEvent(restartSignal);
}

/* ------------------------------------------------------------------------- */

static const char *GetWASAPIAppName(void *)
{
	return obs_module_text("AppAudio");
}
 
static void GetWASAPIAppDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, OPT_DEVICE_ID, "None");
	obs_data_set_default_bool(settings, OPT_USE_DEVICE_TIMING, true);
}

static void *CreateWASAPIAppSource(obs_data_t *settings, obs_source_t *source)
{
	try {
		return new WASAPIAppSource(settings, source);
	} catch (const char *error) {
		blog(LOG_ERROR, "[WASAPIAppSource][CreateWASAPIAppSource] Catch %s", error);
	}

	return nullptr;
}

static void *CreateWASAPIApp(obs_data_t *settings, obs_source_t *source)
{
	return CreateWASAPIAppSource(settings, source);
}

static void DestroyWASAPIAppSource(void *obj)
{
	delete static_cast<WASAPIAppSource *>(obj);
}

bool UpdateWASAPIAppSourceCallback(obs_properties_t *props, obs_property_t *p,
			      obs_data_t *settings)
{
	void *wc = obs_properties_get_param(props);
	if(wc) {
		static_cast<WASAPIAppSource *>(wc)->Update(settings);
		return true;
	}
	return false;
}

static void UpdateWASAPIAppSource(void *obj, obs_data_t *settings)
{
	static_cast<WASAPIAppSource *>(obj)->Update(settings);
}

static obs_properties_t *GetWASAPIAppProperties()
{
}

static obs_properties_t *GetWASAPIAppProperties(void * data)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_set_param(props, data, NULL);

	obs_property_t *device_prop = obs_properties_add_list(props, OPT_DEVICE_ID, obs_module_text("Process"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(device_prop, "None", "None");

	auto apps_list = AppDevicesCache::getInstance()->GetSessionList();
	for (const auto&  audio_app : apps_list){
		auto wsession_id = std::get<0>(audio_app);
		std::string session_id = std::string(wsession_id.begin(), wsession_id.end());
		obs_property_list_add_string(device_prop, std::get<1>(audio_app).c_str(), session_id.c_str());
		blog(LOG_ERROR, "[WASAPIAppSource] add to a settings list %s %s", std::get<1>(audio_app).c_str(), session_id.c_str());
	}

	obs_properties_add_bool(props, OPT_USE_DEVICE_TIMING, obs_module_text("UseDeviceTiming"));

	return props;
}

void RegisterWASAPIApp()
{
	obs_source_info info = {};
	info.id = "wasapi_app_capture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = GetWASAPIAppName;
	info.create = CreateWASAPIApp;
	info.destroy = DestroyWASAPIAppSource;
	info.update = UpdateWASAPIAppSource;
	info.get_defaults = GetWASAPIAppDefaults;
	info.get_properties = GetWASAPIAppProperties;
	info.icon_type = OBS_ICON_TYPE_AUDIO_INPUT;
	obs_register_source(&info);
}
