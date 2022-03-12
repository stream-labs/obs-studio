#pragma once

#include <obs-properties.h>
#include <util/c99defs.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

enum window_priority {
	WINDOW_PRIORITY_CLASS,
	WINDOW_PRIORITY_TITLE,
	WINDOW_PRIORITY_EXE,
};

enum window_search_mode {
	INCLUDE_MINIMIZED,
	EXCLUDE_MINIMIZED,
};

enum window_match_mask {
	WINDOW_MATCH_EXE = 0x01,
	WINDOW_MATCH_TITLE = 0x02,
	WINDOW_MATCH_CLASS = 0x04,
};

enum window_match_type {
	WINDOW_MATCH_CAPTURE = 0x10,
	WINDOW_MATCH_IGNORE = 0x20
};

struct game_capture_matching_rule {
	int power;
	int mask;
	enum window_match_type type;
	struct dstr title;
	struct dstr winclass;
	struct dstr executable;
};

EXPORT bool ms_get_window_exe(struct dstr *name, HWND window);
EXPORT void ms_get_window_title(struct dstr *name, HWND hwnd);
EXPORT void ms_get_window_class(struct dstr *window_class, HWND hwnd);
EXPORT bool ms_is_uwp_window(HWND hwnd);
EXPORT HWND ms_get_uwp_actual_window(HWND parent);

EXPORT void get_captured_window_line(HWND hwnd, struct dstr * window_line);

typedef bool (*add_window_cb)(const char *title, const char *window_class,
			      const char *exe);

EXPORT void ms_fill_window_list(obs_property_t *p, enum window_search_mode mode,
				add_window_cb callback);

EXPORT void ms_build_window_strings(const char *str, char **window_class,
				    char **title, char **exe);

EXPORT HWND ms_find_window(enum window_search_mode mode,
			   enum window_priority priority,
			   const char *window_class, const char *title,
			   const char *exe);
			   
EXPORT HWND ms_find_window_top_level(enum window_search_mode mode,
				     enum window_priority priority,
				     const char *window_class,
				     const char *title, const char *exe);


EXPORT int get_rule_match_power(struct game_capture_matching_rule* rule);

EXPORT HWND next_window(HWND window, enum window_search_mode mode, HWND *parent,
			bool use_findwindowex);
EXPORT HWND first_window(enum window_search_mode mode, HWND *parent,
			 bool *use_findwindowex);
#ifdef __cplusplus
}
#endif
