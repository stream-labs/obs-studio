#include <obs-module.h>
#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-dshow", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows DirectShow source/encoder";
}

extern void RegisterDShowSource();
extern void RegisterDShowEncoders();

bool obs_module_load(void)
{
    char *config_dir = obs_module_config_path(nullptr);
    if (config_dir) {
        os_mkdirs(config_dir);
        bfree(config_dir);
    }

	RegisterDShowSource();
	RegisterDShowEncoders();
	return true;
}
