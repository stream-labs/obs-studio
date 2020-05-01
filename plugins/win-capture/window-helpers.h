#pragma once

#include <util/dstr.h>

enum window_priority {
	WINDOW_PRIORITY_CLASS,
	WINDOW_PRIORITY_TITLE,
	WINDOW_PRIORITY_EXE,
};

enum WINDOW_MATCH {
	WINDOW_MATCH_EXE   = 0x02,
	WINDOW_MATCH_TITLE = 0x04,
	WINDOW_MATCH_CLASS = 0x08
};

enum WINDOW_MATCH_TYPE {
	WINDOW_MATCH_INCLUDE = 0,
	WINDOW_MATCH_EXCLUDE = 1
};

enum window_search_mode {
	INCLUDE_MINIMIZED,
	EXCLUDE_MINIMIZED,
};

struct game_capture_matching_rule {
	int mask;
	int type;
	int power;
	struct dstr title;
	struct dstr class;
	struct dstr executable;
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
				 char **exe, bool *sli_mode, int *match_mask);

extern HWND find_window(enum window_search_mode mode,
			enum window_priority priority, const char *class,
			const char *title, const char *exe);

extern HWND find_window_one_of(enum window_search_mode mode,
			DARRAY(struct game_capture_matching_rule) * games_whitelist,
			DARRAY(HWND) * checked_windows);

extern HWND find_window_top_level(enum window_search_mode mode,
				  enum window_priority priority,
				  const char *class, const char *title,
				  const char *exe);

extern int window_match_in_rules(HWND window, 
			const DARRAY(struct game_capture_matching_rule) * games_whitelist, 
			int *found_index, int already_matched_power);

int get_rule_match_power(struct game_capture_matching_rule& rule);
