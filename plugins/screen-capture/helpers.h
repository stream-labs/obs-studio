extern bool get_window_exe(struct dstr *name, HWND window);
extern void get_window_title(struct dstr *name, HWND hwnd);
extern void get_window_class(struct dstr *classname, HWND hwnd);
extern void get_captured_window_line(HWND hwnd, struct dstr * window_line);

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* this is a workaround to A/Vs going crazy whenever certain functions (such as
 * OpenProcess) are used */
extern void *get_obfuscated_func(HMODULE module, const char *str, uint64_t val);

#ifdef __cplusplus
}
#endif
