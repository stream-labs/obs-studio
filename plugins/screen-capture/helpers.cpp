#define PSAPI_VERSION 1
#include <obs.h>
#include <util/dstr.h>

#include <dwmapi.h>
#include <psapi.h>
#include <windows.h>
#include "helpers.h"


static inline void encode_dstr(struct dstr *str)
{
	dstr_replace(str, "#", "#22");
	dstr_replace(str, ":", "#3A");
}

static HMODULE kernel32(void)
{
	static HMODULE kernel32_handle = NULL;
	if (!kernel32_handle)
		kernel32_handle = GetModuleHandleA("kernel32");
	return kernel32_handle;
}

static inline HANDLE open_process(DWORD desired_access, bool inherit_handle,
				  DWORD process_id)
{
	typedef HANDLE(WINAPI * PFN_OpenProcess)(DWORD, BOOL, DWORD);
	static PFN_OpenProcess open_process_proc = NULL;
	if (!open_process_proc)
		open_process_proc = (PFN_OpenProcess)get_obfuscated_func(
			kernel32(), "B}caZyah`~q", 0x2D5BEBAF6DDULL);

	return open_process_proc(desired_access, inherit_handle, process_id);
}

bool get_window_exe(struct dstr *name, HWND window)
{
	wchar_t wname[MAX_PATH];
	struct dstr temp = {0};
	bool success = false;
	HANDLE process = NULL;
	char *slash;
	DWORD id;

	GetWindowThreadProcessId(window, &id);
	if (id == GetCurrentProcessId())
		return false;

	process = open_process(PROCESS_QUERY_LIMITED_INFORMATION, false, id);
	if (!process)
		goto fail;

	if (!GetProcessImageFileNameW(process, wname, MAX_PATH))
		goto fail;

	dstr_from_wcs(&temp, wname);
	slash = strrchr(temp.array, '\\');
	if (!slash)
		goto fail;

	dstr_copy(name, slash + 1);
	success = true;

fail:
	if (!success)
		dstr_copy(name, "unknown");

	dstr_free(&temp);
	CloseHandle(process);
	return true;
}

void get_window_title(struct dstr *name, HWND hwnd)
{
	wchar_t *temp;
	int len;

	len = GetWindowTextLengthW(hwnd);
	if (!len)
		return;

	temp = (wchar_t *)malloc(sizeof(wchar_t) * (len + 1));
	if (GetWindowTextW(hwnd, temp, len + 1))
		dstr_from_wcs(name, temp);
	free(temp);
}

void get_window_class(struct dstr *classname, HWND hwnd)
{
	wchar_t temp[256];

	temp[0] = 0;
	if (GetClassNameW(hwnd, temp, sizeof(temp) / sizeof(wchar_t)))
		dstr_from_wcs(classname, temp);
}

void get_captured_window_line(HWND hwnd, struct dstr * window_line)
{
	struct dstr classname = {0};
	struct dstr title = {0};
	struct dstr exe = {0};

	if (!get_window_exe(&exe, hwnd))
		return;

	get_window_title(&title, hwnd);

	get_window_class(&classname, hwnd);

	encode_dstr(&title);
	encode_dstr(&classname);
	encode_dstr(&exe);

	dstr_cat_dstr(window_line, &title);
	dstr_cat(window_line, ":");
	dstr_cat_dstr(window_line, &classname);
	dstr_cat(window_line, ":");
	dstr_cat_dstr(window_line, &exe);

	dstr_free(&classname);
	dstr_free(&title);
	dstr_free(&exe);
}

#ifdef _MSC_VER
#pragma warning(disable : 4152) /* casting func ptr to void */
#endif

#include <stdbool.h>

#define LOWER_HALFBYTE(x) ((x)&0xF)
#define UPPER_HALFBYTE(x) (((x) >> 4) & 0xF)

static void deobfuscate_str(char *str, uint64_t val)
{
	uint8_t *dec_val = (uint8_t *)&val;
	int i = 0;

	while (*str != 0) {
		int pos = i / 2;
		bool bottom = (i % 2) == 0;
		uint8_t *ch = (uint8_t *)str;
		uint8_t xor = bottom ? LOWER_HALFBYTE(dec_val[pos])
				     : UPPER_HALFBYTE(dec_val[pos]);

		*ch ^= xor;

		if (++i == sizeof(uint64_t) * 2)
			i = 0;

		str++;
	}
}

void *get_obfuscated_func(HMODULE module, const char *str, uint64_t val)
{
	char new_name[128];
	strcpy(new_name, str);
	deobfuscate_str(new_name, val);
	return GetProcAddress(module, new_name);
}
