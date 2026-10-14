#ifndef DMX_PREFS_H
#define DMX_PREFS_H

#include <stddef.h>

/* Tiny per-user prefs:
 *   macOS: ~/Library/Application Support/demucs-mlx-arm-mac-silicon/prefs.conf
 *   Windows: %APPDATA%\demucs-cuda-windows\prefs.conf
 */

/* Returns 0 on success with a non-empty path written to out. */
int dmx_prefs_load_output_dir(char *out, size_t out_sz);

/* Shared Rock N Roller stem root when the user has not picked a folder:
 *   macOS:  ~/Documents/psarc/stems
 *   Windows: %USERPROFILE%\Documents\psarc\stems
 * Creates the directory if missing. */
int dmx_prefs_default_output_dir(char *out, size_t out_sz);

/* Persist output dir. Returns 0 on success. */
int dmx_prefs_save_output_dir(const char *dir);

#endif
