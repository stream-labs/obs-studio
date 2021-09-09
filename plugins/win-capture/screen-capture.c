#include <stdlib.h>
#include <util/dstr.h>
#include <util/threading.h>
#include "dc-capture.h"
#include "window-helpers.h"
#include "../../libobs/util/platform.h"

#define S_CAPTURE_SOURCE_LIST "capture_source_list"

#define CAPTURE_MODE_UNSET -1
#define CAPTURE_MODE_GAME 0
#define CAPTURE_MODE_MONITOR 1
#define CAPTURE_MODE_WINDOW 2

#define GAME_MODE_UNSET -1
#define GAME_MODE_AUTO 0
#define GAME_MODE_FULLSCREEN 1
#define GAME_MODE_WINDOW 2

#define S_CAPTURE_COURSOR "capture_cursor"
#define S_CAPTURE_WINDOW "capture_window_line"

#define S_GC_AUTO_LIST_FILE "auto_capture_rules_path"
#define S_GC_PLACEHOLDER_IMG "auto_placeholder_image"
#define S_GC_PLACEHOLDER_MSG "auto_placeholder_message"

static bool capture_source_update(struct screen_capture *context,
				  obs_data_t *settings);

struct screen_capture {
	obs_source_t *source;

	bool initialized;
	bool active;

	int capture_mode;
	int game_mode;
	int monitor_id;

	obs_source_t *game_capture;
	obs_source_t *window_capture;
	obs_source_t *monitor_capture;
	obs_source_t *current_capture_source;
};

void set_initialized(struct screen_capture *context, bool new_state)
{
	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: change from %s to %s ",
	     context->initialized ? "true" : "false",
	     new_state ? "true" : "false");
	context->initialized = new_state;
}

static void close_prev_source(struct screen_capture *context)
{
	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: remove all sources");
	if (context->current_capture_source)
		obs_source_remove_active_child(context->source,
					       context->current_capture_source);
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
static void scs_update_window_mode_line(struct screen_capture *contex, obs_data_t *settings)
{
	const char * window_line = obs_data_get_string(settings, S_CAPTURE_WINDOW);
	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: prev window line %s", window_line);
	if (window_line != 0 && strlen(window_line) > 0) {
		char *class = NULL;
		char *title = NULL;
		char *executable = NULL;

		build_window_strings(window_line, &class, &title, &executable);	
		if (executable != NULL && strlen(executable) > 0) {
			blog(LOG_DEBUG, "[SCREEN_CAPTURE]: prev exe name %s", executable);
			HWND hwnd = find_window_top_level(INCLUDE_MINIMIZED, WINDOW_PRIORITY_CLASS, class, title, executable );
			if (hwnd != NULL) {
				char mode_line[32]= {0};
				sprintf(mode_line, "window:%d:1", hwnd);
				obs_data_set_string(settings, S_CAPTURE_SOURCE_LIST, mode_line);
				blog(LOG_DEBUG, "[SCREEN_CAPTURE]: new mode line %s", mode_line);
			}
		}
	}
}
static void scs_init(void *data, obs_data_t *settings)
{
	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: Initialization");
	struct screen_capture *context = data;

	if (context->initialized)
		return;

	context->capture_mode = CAPTURE_MODE_UNSET;
	context->game_mode = GAME_MODE_UNSET;
	context->monitor_id = -1;

	context->game_capture = NULL;
	context->window_capture = NULL;
	context->monitor_capture = NULL;
	context->current_capture_source = NULL;

	set_initialized(context, true);

	capture_source_update(context, settings);
}

static void scs_deinit(void *data)
{
	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: Deinitialization");
	struct screen_capture *context = data;

	context->current_capture_source = NULL;

	close_prev_source(context);

	set_initialized(context, false);
}

static const char *scs_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Screen Capture";
}

static void scs_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_CAPTURE_WINDOW, "");
	obs_data_set_default_string(settings, S_CAPTURE_SOURCE_LIST, "unset:0");

	obs_data_set_default_string(settings, S_GC_AUTO_LIST_FILE, "");
	obs_data_set_default_string(settings, S_GC_PLACEHOLDER_IMG, "");
	obs_data_set_default_string(settings, S_GC_PLACEHOLDER_MSG,
				    "Looking for a game to capture");
}

static uint32_t scs_getwidth(void *data)
{
	struct screen_capture *context = data;
	if (context->current_capture_source)
		return obs_source_get_width(context->current_capture_source);

	return 0;
}

static uint32_t scs_getheight(void *data)
{
	struct screen_capture *context = data;
	if (context->current_capture_source)
		return obs_source_get_height(context->current_capture_source);

	return 0;
}

static void scs_show(void *data) {}

static void scs_hide(void *data) {}

static void scs_destroy(void *data)
{
	struct screen_capture *context = data;

	scs_deinit(data);
	bfree(context);
}

static void scs_activate(void *data)
{
	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: activated ");
}

static void scs_deactivate(void *data)
{
	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: deactivated ");
}

static void scs_render(void *data, gs_effect_t *effect)
{
	struct screen_capture *context = data;
	if (context->initialized) {
		if (context->current_capture_source) {
			obs_source_video_render(
				context->current_capture_source);
		}
	}
	UNUSED_PARAMETER(effect);
}

static void scs_tick(void *data, float seconds)
{
	struct screen_capture *context = data;
	if (context->initialized) {
		if (context->current_capture_source) {
			obs_source_video_tick(context->current_capture_source,
					      seconds);
		}
	}
}

static void switch_to_game_capture_mode(struct screen_capture *context)
{
	close_prev_source(context);

	obs_data_t *settings = obs_source_get_settings(context->source);
	obs_data_t *game_capture_settings = obs_data_create();

	switch (context->game_mode) {
	case GAME_MODE_AUTO:
		obs_data_set_string(game_capture_settings, "capture_mode",
				    "auto");
		obs_data_set_string(game_capture_settings, S_GC_AUTO_LIST_FILE,
				    obs_data_get_string(settings,
							S_GC_AUTO_LIST_FILE));
		obs_data_set_string(game_capture_settings, S_GC_PLACEHOLDER_IMG,
				    obs_data_get_string(settings,
							S_GC_PLACEHOLDER_IMG));
		obs_data_set_string(game_capture_settings, S_GC_PLACEHOLDER_MSG,
				    obs_data_get_string(settings,
							S_GC_PLACEHOLDER_MSG));

		obs_data_set_string(game_capture_settings, S_CAPTURE_COURSOR,
				    obs_data_get_string(settings,
							S_CAPTURE_COURSOR));

		break;

	case GAME_MODE_FULLSCREEN:
		obs_data_set_string(game_capture_settings, "capture_mode",
				    "any_fullscreen");
		break;
	}

	context->game_capture = obs_source_create_private(
		"game_capture", "screen_capture_game_capture",
		game_capture_settings);
	obs_source_add_active_child(context->source, context->game_capture);

	obs_data_release(game_capture_settings);
	obs_data_release(settings);
	context->current_capture_source = context->game_capture;
}

static void switch_to_monitor_capture_mode(struct screen_capture *context)
{
	close_prev_source(context);

	obs_data_t *settings = obs_source_get_settings(context->source);
	obs_data_t *monitor_settings = obs_data_create();

	obs_data_set_int(monitor_settings, "monitor", context->monitor_id);
	obs_data_set_int(monitor_settings, "method", 0);
	obs_data_set_int(monitor_settings, "monitor_wgc", 0);
	obs_data_set_bool(monitor_settings, S_CAPTURE_COURSOR,
			  obs_data_get_string(settings, S_CAPTURE_COURSOR));

	context->monitor_capture = obs_source_create_private(
		"monitor_capture", "screen_capture_monitor_capture",
		monitor_settings);

	obs_source_add_active_child(context->source, context->monitor_capture);
	obs_data_release(monitor_settings);
	obs_data_release(settings);
	context->current_capture_source = context->monitor_capture;
}

static void switch_to_window_capture_mode(struct screen_capture *context)
{
	close_prev_source(context);

	obs_data_t *settings = obs_source_get_settings(context->source);
	obs_data_t *window_settings = obs_data_create();

	obs_data_set_int(window_settings, "method", 0);
	obs_data_set_string(window_settings, "window",
			  obs_data_get_string(settings, S_CAPTURE_WINDOW));
	obs_data_set_bool(window_settings, S_CAPTURE_COURSOR,
			  obs_data_get_string(settings, S_CAPTURE_COURSOR));

	context->window_capture = obs_source_create_private(
		"window_capture", "screen_capture_window_capture",
		window_settings);

	obs_source_add_active_child(context->source, context->window_capture);
	obs_data_release(window_settings);
	obs_data_release(settings);

	context->current_capture_source = context->window_capture;
}

static bool capture_source_update(struct screen_capture *context,
				  obs_data_t *settings)
{
	const char *capture_source_string =
		obs_data_get_string(settings, S_CAPTURE_SOURCE_LIST);

	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: settings updated string %s",
	     capture_source_string);

	char **strlist;
	strlist = strlist_split(capture_source_string, ':', true);
	char *mode = strlist[0];
	char *option = strlist[1];

	if (astrcmpi(mode, "game") == 0) {
		context->capture_mode = CAPTURE_MODE_GAME;
		if (astrcmpi(option, "1") == 0) {
			context->game_mode = GAME_MODE_AUTO;
		} else if (astrcmpi(option, "2") == 0) {
			context->game_mode = GAME_MODE_FULLSCREEN;
		}
		obs_data_set_string(settings, S_CAPTURE_WINDOW, "");
	} else if (astrcmpi(mode, "monitor") == 0) {
		context->capture_mode = CAPTURE_MODE_MONITOR;
		context->monitor_id = atoi(option);
		obs_data_set_string(settings, S_CAPTURE_WINDOW, "");
	} else if (astrcmpi(mode, "window") == 0) {
		context->capture_mode = CAPTURE_MODE_WINDOW;
		
		HWND hwnd = atoi(option);
		struct dstr window_line = {0};
		get_captured_window_line(hwnd, &window_line);
		blog(LOG_DEBUG, "[SCREEN_CAPTURE]: window mode %d %s", hwnd, window_line.array);

		if (window_line.array != 0 && strcmp(window_line.array, "::unknown") != 0) {
			obs_data_set_string(settings, S_CAPTURE_WINDOW,
					window_line.array);
		} else {
			scs_update_window_mode_line(context, settings);
		}
	} else {
		blog(LOG_DEBUG, "[SCREEN_CAPTURE]: mode unrecognized, try to check if we remember some window");
		scs_update_window_mode_line(context, settings);
	}
	strlist_free(strlist);

	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: switch to mode %d",
	     context->capture_mode);

	close_prev_source(context);

	switch (context->capture_mode) {
	case CAPTURE_MODE_GAME:
		switch_to_game_capture_mode(context);
		break;
	case CAPTURE_MODE_MONITOR:
		switch_to_monitor_capture_mode(context);
		break;
	case CAPTURE_MODE_WINDOW:
		switch_to_window_capture_mode(context);
		break;
	};

	return true;
}

static void scs_update(void *data, obs_data_t *settings)
{
	struct screen_capture *context = data;
	blog(LOG_DEBUG, "[SCREEN_CAPTURE] Update called ");

	if (context->initialized) {
		capture_source_update(context, settings);
	}
}

static void *scs_create(obs_data_t *settings, obs_source_t *source)
{
	struct screen_capture *context = bzalloc(sizeof(struct screen_capture));
	context->source = source;

	set_initialized(context, false);

	scs_init(context, settings);
	return context;
}

static void scs_enum_active_sources(void *data, obs_source_enum_proc_t cb,
				    void *props)
{
	struct screen_capture *context = data;
	if (context->current_capture_source)
		cb(context->source, context->current_capture_source, props);
}

static void scs_enum_sources(void *data, obs_source_enum_proc_t cb, void *props)
{
	struct screen_capture *context = data;
	if (context->current_capture_source)
		cb(context->source, context->current_capture_source, props);
}

static bool capture_source_changed(obs_properties_t *props, obs_property_t *p,
				   obs_data_t *settings)
{
	struct screen_capture *context = obs_properties_get_param(props);

	if (!context) {
		blog(LOG_DEBUG,
		     "[SCREEN_CAPTURE]: failed to get context on settings change callback");
		return false;
	}

	blog(LOG_DEBUG, "[SCREEN_CAPTURE]: settings change callback");
	return capture_source_update(context, settings);
}

static obs_properties_t *scs_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_properties_set_param(props, data, NULL);

	p = obs_properties_add_capture(props, S_CAPTURE_SOURCE_LIST,
				       S_CAPTURE_SOURCE_LIST);
	obs_property_set_modified_callback(p, capture_source_changed);

	p = obs_properties_add_bool(props, S_CAPTURE_COURSOR,
				    S_CAPTURE_COURSOR);

	p = obs_properties_add_text(props, S_CAPTURE_WINDOW, S_CAPTURE_WINDOW,
				    OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);

	p = obs_properties_add_text(props, S_GC_AUTO_LIST_FILE,
				    S_GC_AUTO_LIST_FILE, OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);
	p = obs_properties_add_text(props, S_GC_PLACEHOLDER_IMG,
				    S_GC_PLACEHOLDER_IMG, OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);
	p = obs_properties_add_text(props, S_GC_PLACEHOLDER_MSG,
				    S_GC_PLACEHOLDER_MSG, OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);

	return props;
}

struct obs_source_info screen_capture_info = {
	.id = "screen_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = scs_get_name,
	.create = scs_create,
	.destroy = scs_destroy,
	.activate = scs_activate,
	.deactivate = scs_deactivate,
	.update = scs_update,
	.get_defaults = scs_defaults,
	.show = scs_show,
	.hide = scs_hide,
	.enum_active_sources = scs_enum_active_sources,
	.enum_all_sources = scs_enum_sources,
	.get_width = scs_getwidth,
	.get_height = scs_getheight,
	.video_render = scs_render,
	.video_tick = scs_tick,
	.get_properties = scs_properties,
};
