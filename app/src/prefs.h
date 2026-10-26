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

/* Steam Rocksmith2014 game root (not its dlc/ subfolder) when installed:
 *   macOS:  ~/Library/Application Support/Steam/steamapps/common/Rocksmith2014
 *   Windows: %ProgramFiles(x86)%\Steam\steamapps\common\Rocksmith2014
 * Returns 0 when the folder exists. */
int dmx_prefs_rocksmith_dir(char *out, size_t out_sz);

/* Persist output dir. Returns 0 on success. */
int dmx_prefs_save_output_dir(const char *dir);

/* ONNX stems device choice (STEMS_DEVICE=gpu|cpu). Load returns 0 when saved. */
int dmx_prefs_load_use_cpu(int *out);
int dmx_prefs_save_use_cpu(int use_cpu);

#endif
