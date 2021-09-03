#define _CRT_SECURE_NO_WARNINGS

#include <obs-module.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>
#include <vector>
#include <windows.h>

#include "helpers.h"

#define S_CAPTURE_SOURCE_LIST "capture_source_list"
#define S_CAPTURE_SOURCE_PREV "capture_source_prev"

#define S_CAPTURE_MODE "capture_mode"
#define S_CAPTURE_MODE_GAME "capture_mode_game"
#define S_CAPTURE_MODE_MONITOR "capture_mode_monitor"
#define S_CAPTURE_MODE_WINDOW "capture_mode_window"

enum class CAPTURE_MODE:int {
	UNSET = -1,
	GAME = 0,
	MONITOR = 1,
	WINDOW = 2
};

enum class GAME_MODE:int {
	UNSET = -1,
	AUTO = 0,
	FULLSCREEN = 1,
	WINDOW = 2
};

#define S_GAME_CAPTURE_MODE "game_capture_mode"
#define S_GAME_CAPTURE_AUTO "game_capture_mode_autop"
#define S_GAME_CAPTURE_WINDOW "game_capture_mode_window"
#define S_GAME_CAPTURE_FULLSCREEN "game_capture_mode_fullscreen"
#define S_GAME_CAPTURE_AUTO_LIST_FILE   "auto_capture_rules_path"
#define S_GAME_CAPTURE_PLACEHOLDER_IMG  "auto_placeholder_image"
#define S_GAME_CAPTURE_PLACEHOLDER_MSG  "auto_placeholder_message"

#define S_WINDOW_CAPTURE_WINDOW "window_capture_window"
#define S_MONITOR_CAPTURE_ID "monitor_capture_id"

static bool capture_source_update( struct slobs_capture *context, obs_data_t *settings);

struct slobs_capture {
	obs_source_t *source;

	bool initialized;
	bool active;

	CAPTURE_MODE capture_mode = CAPTURE_MODE::UNSET;
	GAME_MODE game_mode = GAME_MODE::UNSET;
	int monitor_id = -1;
	int window;

	obs_source_t *game_capture;
	obs_source_t *window_capture;
	obs_source_t *monitor_capture;
	obs_source_t *current_capture_source;
};

void set_initialized(struct slobs_capture *context, bool new_state) 
{
	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: change from %s to %s ", context->initialized?"true":"false", new_state?"true":"false");
	context->initialized = new_state;
}



static void close_prev_source(struct slobs_capture *context)
{
	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: remove all sources");
	if (context->current_capture_source)
		obs_source_remove_active_child(context->source, context->current_capture_source);
	context->current_capture_source = NULL;
	if (context->window_capture) {
		obs_source_release(context->window_capture);
		context->window_capture = NULL;
	}
	if (context->monitor_capture) {
		obs_source_release(context->monitor_capture);
		context->monitor_capture = NULL;
	}
	if (context->game_capture) {
		obs_source_release(context->game_capture);
		context->game_capture = NULL;
	}
}

static void slobs_capture_init(void *data, obs_data_t *settings)
{
	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: Initialization");
	struct slobs_capture *context = (slobs_capture *)data;

	if (context->initialized)
		return;

	context->capture_mode = CAPTURE_MODE::UNSET;
	context->game_mode = GAME_MODE::UNSET;
	int monitor_id = -1;

	context->game_capture = NULL;
	context->window_capture = NULL;
	context->monitor_capture = NULL;
	context->current_capture_source = NULL;

	set_initialized( context,  true);

	capture_source_update(context, settings);
}

static void slobs_capture_deinit(void *data)
{
	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: Deinitialization");
	struct slobs_capture *context = (slobs_capture *)data;

	context->current_capture_source = NULL;

	close_prev_source(context);

	set_initialized( context,  false);
}

static const char *slobs_capture_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "SLOBS Capture";
}

static void slobs_capture_defaults(obs_data_t *settings) 
{
	obs_data_set_default_string(settings, S_CAPTURE_SOURCE_LIST, "game:0");

	obs_data_set_default_string(settings, S_GAME_CAPTURE_AUTO_LIST_FILE, "");
	obs_data_set_default_string(settings, S_GAME_CAPTURE_PLACEHOLDER_IMG, "");
	obs_data_set_default_string(settings, S_GAME_CAPTURE_PLACEHOLDER_MSG, "Looking for a game to capture");
}

static uint32_t slobs_capture_getwidth(void *data)
{
	struct slobs_capture *context = (slobs_capture *)data;
	if (context->current_capture_source)
		return obs_source_get_width(context->current_capture_source);

	return 0;
}

static uint32_t slobs_capture_getheight(void *data)
{
	struct slobs_capture *context = (slobs_capture *)data;
	if (context->current_capture_source)
		return obs_source_get_height(context->current_capture_source);

	return 0;
}

static void slobs_capture_show(void *data)
{
}

static void slobs_capture_hide(void *data)
{
}

static void slobs_capture_destroy(void *data)
{
	struct slobs_capture *context = (slobs_capture *)data;

	slobs_capture_deinit(data);
	bfree(context);
}

static void slobs_capture_activate(void *data)
{
	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: activated ");
}

static void slobs_capture_deactivate(void *data)
{
	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: deactivated ");
}

static void slobs_capture_render(void *data, gs_effect_t *effect)
{
	struct slobs_capture *context = (slobs_capture *)data;
	if (context->initialized) 
	{
		if (context->current_capture_source) {
			obs_source_video_render(context->current_capture_source);
		}
	}
	UNUSED_PARAMETER(effect);
}

static void slobs_capture_tick(void *data, float seconds)
{
	struct slobs_capture *context = (slobs_capture *)data;
	if (context->initialized) 
	{
 		if (context->current_capture_source) {
			obs_source_video_tick(context->current_capture_source, seconds);
		}
	}
}

static void switch_to_game_capture_mode(struct slobs_capture *context)
{
	close_prev_source(context);
 
	obs_data_t *settings = obs_source_get_settings(context->source);
	obs_data_t *game_capture_settings = obs_data_create();

	switch (context->game_mode) {
		case GAME_MODE::AUTO:
		obs_data_set_string(game_capture_settings, "capture_mode", "auto");
		obs_data_set_string(game_capture_settings, S_GAME_CAPTURE_AUTO_LIST_FILE, obs_data_get_string(settings, S_GAME_CAPTURE_AUTO_LIST_FILE));
		obs_data_set_string(game_capture_settings, S_GAME_CAPTURE_PLACEHOLDER_IMG, obs_data_get_string(settings, S_GAME_CAPTURE_PLACEHOLDER_IMG));
		obs_data_set_string(game_capture_settings, S_GAME_CAPTURE_PLACEHOLDER_MSG, obs_data_get_string(settings, S_GAME_CAPTURE_PLACEHOLDER_MSG));
		break;

		case GAME_MODE::FULLSCREEN:
		obs_data_set_string(game_capture_settings, "capture_mode", "any_fullscreen");
		break;
	} 

	context->game_capture = obs_source_create_private("game_capture", "slobs_capture_game_capture", game_capture_settings);
	obs_source_add_active_child(context->source, context->game_capture);

	obs_data_release(game_capture_settings);	
	obs_data_release(settings);
	context->current_capture_source = context->game_capture;
}

static void switch_to_monitor_capture_mode(struct slobs_capture *context)
{
	close_prev_source(context);

	obs_data_t *settings = obs_data_create();
	
	obs_data_set_int(settings, "monitor", context->monitor_id);
	obs_data_set_int(settings, "method", 0);
	obs_data_set_int(settings, "monitor_wgc", 0);
	obs_data_set_bool(settings, "capture_cursor", false);
	
	context->monitor_capture = obs_source_create_private("monitor_capture", "slobs_capture_monitor_capture", settings);
	
	obs_source_add_active_child(context->source, context->monitor_capture);
	obs_data_release(settings);

	context->current_capture_source = context->monitor_capture;
}

static void switch_to_window_capture_mode(struct slobs_capture *context)
{
	close_prev_source(context);

	obs_data_t *settings = obs_data_create();

	struct dstr window_line = {0};
	get_captured_window_line((HWND)context->window, &window_line);

	obs_data_set_string(settings, "window", window_line.array);
	obs_data_set_int(settings, "method", 0);
	
	context->window_capture = obs_source_create_private("window_capture", "slobs_capture_window_capture", settings);

	obs_source_add_active_child(context->source, context->window_capture);
	obs_data_release(settings);

	context->current_capture_source = context->window_capture;
}

static bool capture_source_update( struct slobs_capture *context, obs_data_t *settings)
{
	const char * capture_source_string =  obs_data_get_string(settings, S_CAPTURE_SOURCE_LIST);
	const char * capture_source_prev =  obs_data_get_string(settings, S_CAPTURE_SOURCE_PREV);

	if( astrcmpi(capture_source_prev, capture_source_string) == 0 ) {
		return true;
	} else {
		obs_data_set_string(settings, S_CAPTURE_SOURCE_PREV, capture_source_string);
	}

	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: settings updated string %s",
	     capture_source_string);
	
	char **strlist;
	strlist = strlist_split(capture_source_string, ':', true);
	char* mode = strlist[0];
	char* option = strlist[1];
	
	if (astrcmpi(mode, "game") == 0) {
		context->capture_mode = CAPTURE_MODE::GAME;
		if (astrcmpi(option, "1") == 0) { 
			context->game_mode = GAME_MODE::AUTO;
		} else if (astrcmpi(option, "2") == 0) { 
			context->game_mode = GAME_MODE::FULLSCREEN;
		}
	} else if (astrcmpi(mode, "monitor") == 0) {
		context->capture_mode = CAPTURE_MODE::MONITOR;
		context->monitor_id = atoi(option);
	} else if (astrcmpi(mode, "window") == 0) {
		context->capture_mode = CAPTURE_MODE::WINDOW;
		context->window = atoi(option);
	}
	strlist_free(strlist);

	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: switch to mode %d", context->capture_mode);

	switch (context->capture_mode) {
	case CAPTURE_MODE::GAME:
		switch_to_game_capture_mode(context);
		break;
	case CAPTURE_MODE::MONITOR:
		switch_to_monitor_capture_mode(context);
		break;
	case CAPTURE_MODE::WINDOW:
		switch_to_window_capture_mode(context);
		break;
	};

	return true;
}

static void slobs_capture_update(void *data, obs_data_t *settings)
{
	struct slobs_capture *context = (slobs_capture *)data;
	blog(LOG_DEBUG, "[SLOBS_CAPTURE] Update called ");

	if (context->initialized) {
		capture_source_update(context, settings);
	}
}

static void *slobs_capture_create(obs_data_t *settings, obs_source_t *source)
{
	struct slobs_capture *context =
		(slobs_capture *)bzalloc(sizeof(slobs_capture));
	context->source = source;

	set_initialized( context,  false);
	
	slobs_capture_init(context, settings);
	return context;
}

static void slobs_capture_enum_active_sources(void *data, obs_source_enum_proc_t cb, void *props)
{
	struct slobs_capture *context = (struct slobs_capture *)data;
	if (context->current_capture_source)
		cb(context->source, context->current_capture_source, props);
}

static void slobs_capture_enum_sources(void *data, obs_source_enum_proc_t cb, void *props)
{
	struct slobs_capture *context = (struct slobs_capture *)data;
	if (context->current_capture_source)
		cb(context->source, context->current_capture_source, props);
}

static bool capture_source_changed(obs_properties_t *props, obs_property_t *p,
				   obs_data_t *settings)
{
	struct slobs_capture *context = (struct slobs_capture *)obs_properties_get_param(props);

	if (!context) {
		blog(LOG_DEBUG, "[SLOBS_CAPTURE]: failed to get context on settings change callback");
		return false;
	}
	
	blog(LOG_DEBUG, "[SLOBS_CAPTURE]: settings change callback");
	return capture_source_update(context, settings);
}

static obs_properties_t *slobs_capture_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_properties_set_param(props, data, NULL);
 
	p = obs_properties_add_capture(props, S_CAPTURE_SOURCE_LIST, S_CAPTURE_SOURCE_LIST);
	obs_property_set_modified_callback(p, capture_source_changed);

	p = obs_properties_add_text(props, S_CAPTURE_SOURCE_PREV, S_CAPTURE_SOURCE_PREV, OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);

	p = obs_properties_add_text(props, S_GAME_CAPTURE_AUTO_LIST_FILE, S_GAME_CAPTURE_AUTO_LIST_FILE, OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);
	p = obs_properties_add_text(props, S_GAME_CAPTURE_PLACEHOLDER_IMG, S_GAME_CAPTURE_PLACEHOLDER_IMG, OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);
	p = obs_properties_add_text(props, S_GAME_CAPTURE_PLACEHOLDER_MSG, S_GAME_CAPTURE_PLACEHOLDER_MSG, OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);
	
	return props;
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("slobs-capture", "en-US")

bool obs_module_load(void)
{
	obs_source_info info = {};
	info.id = "slobs_capture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			    OBS_SOURCE_DO_NOT_DUPLICATE,
	info.get_name = slobs_capture_get_name;
	info.create = slobs_capture_create;
	info.destroy = slobs_capture_destroy;
	info.activate = slobs_capture_activate;
	info.deactivate = slobs_capture_deactivate;
	info.update = slobs_capture_update;
	info.get_defaults = slobs_capture_defaults;
	info.show = slobs_capture_show;
	info.hide = slobs_capture_hide;
	info.enum_active_sources = slobs_capture_enum_active_sources,
	info.enum_all_sources = slobs_capture_enum_sources,
	info.get_width = slobs_capture_getwidth;
	info.get_height = slobs_capture_getheight;
	info.video_render = slobs_capture_render;
	info.video_tick = slobs_capture_tick;
	info.get_properties = slobs_capture_properties;
	obs_register_source(&info);

	return true;
}
