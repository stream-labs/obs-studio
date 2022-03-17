#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <RTWorkQ.h>
#include <wrl/implements.h>

#include <util/windows/WinHandle.hpp>
#include <obs.h>

#include <audiopolicy.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>

#include <unordered_map>
#include <tuple>
#include <mutex>
#include <atomic>

class ARtwqAsyncCallback : public IRtwqAsyncCallback {
protected:
	ARtwqAsyncCallback(void *source) : source(source) {}

public:
	STDMETHOD_(ULONG, AddRef)() { return ++refCount; }

	STDMETHOD_(ULONG, Release)() { return --refCount; }

	STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject)
	{
		HRESULT hr = E_NOINTERFACE;

		if (riid == __uuidof(IRtwqAsyncCallback) ||
		    riid == __uuidof(IUnknown)) {
			*ppvObject = this;
			AddRef();
			hr = S_OK;
		} else {
			*ppvObject = NULL;
		}

		return hr;
	}

	STDMETHOD(GetParameters)
	(DWORD *pdwFlags, DWORD *pdwQueue)
	{
		*pdwFlags = 0;
		*pdwQueue = queue_id;
		return S_OK;
	}

	STDMETHOD(Invoke)
	(IRtwqAsyncResult *) override = 0;

	DWORD GetQueueId() const { return queue_id; }
	void SetQueueId(DWORD id) { queue_id = id; }

protected:
	std::atomic<ULONG> refCount = 1;
	void *source;
	DWORD queue_id = 0;
};

class AppAudioSession;
class AppAudioDevices;

typedef std::tuple<std::wstring, std::string, DWORD> AppSessionID;

class AppDevicesCache {
	static long refs;
	static AppDevicesCache *instance;

public:
	Microsoft::WRL::ComPtr<IMMNotificationClient> notify;
	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;

	static AppDevicesCache *getInstance()
	{
		if (!instance) {
			instance = new AppDevicesCache;
			instance->InitCache();
		}
		return instance;
	}

	static void addRef()
	{
		if (refs == 0)
			getInstance();
		refs++;
	}

	static void releaseRef()
	{
		refs--;
		if (refs == 0) {
			delete instance;
			instance = nullptr;
		}
	}

	void InitCache();
	
	const std::vector<AppSessionID> GetSessionList();
	DWORD getPID(std::string session);
	static std::string GetExecutableByPID(DWORD pid);

	AppDevicesCache();
	virtual ~AppDevicesCache();

	std::recursive_mutex cached_mutex;
	std::unordered_map<std::wstring, AppAudioDevices> app_devices;
	std::unordered_map<std::wstring, AppAudioSession> app_sessions;

	void AddSession(Microsoft::WRL::ComPtr<IAudioSessionControl> session);
	void RemoveSession(Microsoft::WRL::ComPtr<IAudioSessionControl> session_control);

	void AddDevice(LPCWSTR device_id);
	void RemoveDevice(LPCWSTR device_id);
};

class WASAPIAppNotify
	: public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, Microsoft::WRL::FtmBase, IMMNotificationClient> 
{

public:
	WASAPIAppNotify(){}

	STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role,LPCWSTR id) { return S_OK; }
	STDMETHODIMP OnDeviceAdded(LPCWSTR device_id) 
	{ 
		AppDevicesCache::getInstance()->AddDevice(device_id);
		return S_OK; 
	}
	STDMETHODIMP OnDeviceRemoved(LPCWSTR device_id) 
	{ 
		AppDevicesCache::getInstance()->RemoveDevice(device_id);
		return S_OK; 
	}
	STDMETHODIMP OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state) 
	{ 
		if(new_state == DEVICE_STATE_ACTIVE) {
			AppDevicesCache::getInstance()->AddDevice(device_id);
		} else {
			AppDevicesCache::getInstance()->RemoveDevice(device_id);
		}
		return S_OK; 
	}
	STDMETHODIMP OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY){return S_OK;}
};

class AppAudioDevicesNotificationClient
	: public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, Microsoft::WRL::FtmBase, IAudioSessionNotification> 
{
public:
	AppAudioDevicesNotificationClient() {};

	STDMETHOD(OnSessionCreated)(IAudioSessionControl *session)
	{
		session->AddRef();
		AppDevicesCache::getInstance()->AddSession(session);
		return S_OK;
	}
};

class AppAudioSessionNotificationClient 
	: public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, Microsoft::WRL::FtmBase,
			      IAudioSessionEvents> {

 	Microsoft::WRL::ComPtr<IAudioSessionControl> session_control;
public:
	AppAudioSessionNotificationClient() {}
	
	void SetSessionControl(Microsoft::WRL::ComPtr<IAudioSessionControl> new_session_control) {
		session_control = new_session_control;
	}

	STDMETHOD(OnChannelVolumeChanged)
	(DWORD channel_count, float *new_channel_volume_array,DWORD changed_channel, LPCGUID event_context) { return S_OK; }

	STDMETHOD(OnDisplayNameChanged)
	(LPCWSTR new_display_name, LPCGUID event_context) { return S_OK; }

	STDMETHOD(OnGroupingParamChanged)
	(LPCGUID new_grouping_param, LPCGUID event_context) { return S_OK; }

	STDMETHOD(OnIconPathChanged)
	(LPCWSTR new_icon_path, LPCGUID event_context) { return S_OK; }

	STDMETHOD(OnSessionDisconnected)(AudioSessionDisconnectReason reason)
	{
		AppDevicesCache::getInstance()->RemoveSession(session_control);
		return S_OK;
	}

	STDMETHOD(OnSimpleVolumeChanged)
	(float new_volume, BOOL new_mute, LPCGUID event_context) { return S_OK; }

	STDMETHOD(OnStateChanged)(AudioSessionState new_state)
	{
		if (new_state == AudioSessionStateExpired) {
			AppDevicesCache::getInstance()->RemoveSession(session_control);
		}

		return S_OK;
	}	
};

class AppAudioSession
{
	Microsoft::WRL::ComPtr<IAudioSessionControl> session_control;
	AppAudioSessionNotificationClient notification_client;

public:
	DWORD PID;
	std::string executable;

	AppAudioSession(Microsoft::WRL::ComPtr<IAudioSessionControl> new_session_control, DWORD new_PID, std::string new_executable);
	virtual ~AppAudioSession();
};


class AppAudioDevices 
{
private:
	std::wstring device_id;
	Microsoft::WRL::ComPtr<IMMDevice> device;
	
	AppAudioDevicesNotificationClient notification_client;

	Microsoft::WRL::ComPtr<IAudioSessionManager2> manager;
	Microsoft::WRL::ComPtr<IAudioSessionEnumerator> enumerator;
public:
	AppAudioDevices(Microsoft::WRL::ComPtr<IMMDevice> new_device, std::wstring new_device_id);

	virtual ~AppAudioDevices();
};


class WASAPIAppSourceActivatedNotify
	: public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, Microsoft::WRL::FtmBase,
			      IActivateAudioInterfaceCompletionHandler> {
	
public:
	WASAPIAppSourceActivatedNotify() {
		signal = CreateEvent(nullptr, false, false, nullptr);
	}

	Microsoft::WRL::ComPtr<IAudioClient> client;
	WinHandle signal;
	HRESULT hr = E_FAIL;

	Microsoft::WRL::ComPtr<IAudioClient> getClient(){
		return client.Get();
	};

	STDMETHOD(ActivateCompleted)
	(IActivateAudioInterfaceAsyncOperation *operation)
	{
		operation->GetActivateResult(&hr, (IUnknown**) client.ReleaseAndGetAddressOf());
		SetEvent(signal);
		return S_OK;
	}
};

void fill_apps_list(obs_property_t *p, enum window_search_mode mode);
