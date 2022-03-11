#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-wasapi", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows WASAPI audio input/output sources";
}

void RegisterWASAPIInput();
void RegisterWASAPIOutput();
void RegisterWASAPIApp();
void patchMediaCrash();

bool obs_module_load(void)
{
	RegisterWASAPIInput();
	RegisterWASAPIOutput();
	RegisterWASAPIApp();
	patchMediaCrash();
	return true;
}
