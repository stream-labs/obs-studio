#pragma once

#include <util/dstr.h>

enum window_priority {
	WINDOW_PRIORITY_NON = -1,
	WINDOW_PRIORITY_CLASS = 0,
	WINDOW_PRIORITY_TITLE = 1,
	WINDOW_PRIORITY_EXE = 2,

	WINDOW_PRIORITY_EXE_ONLY = 20,
	WINDOW_PRIORITY_NOT_EXE_ONLY = 21,
	WINDOW_PRIORITY_CLASS_ONLY = 22,
	WINDOW_PRIORITY_NOT_CLASS = 23,
	WINDOW_PRIORITY_TITLE_ONLY = 24,
	WINDOW_PRIORITY_NOT_TITLE = 25,
	WINDOW_PRIORITY_EXE_CLASS = 26,
	WINDOW_PRIORITY_NOT_EXE_CLASS = 27,
	WINDOW_PRIORITY_EXE_TITLE = 28,
	WINDOW_PRIORITY_NOT_EXE_TITLE = 29,
	WINDOW_PRIORITY_EXE_CLASS_TITLE = 30,
	WINDOW_PRIORITY_NOT_EXE_CLASS_TITLE = 31,
};

enum window_search_mode {
	INCLUDE_MINIMIZED,
	EXCLUDE_MINIMIZED,
};

struct game_capture_picking_info {
	struct dstr title;
	struct dstr class;
	struct dstr executable;
	enum window_priority priority;
	bool sli_mode;
};

extern bool get_window_exe(struct dstr *name, HWND window);
extern void get_window_title(struct dstr *name, HWND hwnd);
extern void get_window_class(struct dstr *class, HWND hwnd);
extern bool is_uwp_window(HWND hwnd);
extern HWND get_uwp_actual_window(HWND parent);

extern void get_captured_window_line(HWND hwnd, struct dstr * window_line);

typedef bool (*add_window_cb)(const char *title, const char *class,
			      const char *exe);

extern void fill_window_list(obs_property_t *p, enum window_search_mode mode,
			     add_window_cb callback);

extern void build_window_strings(const char *str, char **class, char **title,
				 char **exe, bool *sli_mode, int *priority);

extern HWND find_window(enum window_search_mode mode,
			enum window_priority priority, const char *class,
			const char *title, const char *exe);

extern HWND find_window_one_of(enum window_search_mode mode,
			DARRAY(struct game_capture_picking_info) * games_whitelist,
			DARRAY(HWND) * checked_windows);

extern enum window_priority window_rating_by_list(HWND window, 
			const DARRAY(struct game_capture_picking_info) * games_whitelist, int *found_index, enum window_priority had_priority);

extern HWND find_window_top_level(enum window_search_mode mode,
				  enum window_priority priority,
				  const char *class, const char *title,
				  const char *exe);
