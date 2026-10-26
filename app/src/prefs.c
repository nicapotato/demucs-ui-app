#include "prefs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#endif

#ifndef DMX_PREFS_DIR_NAME
#ifdef _WIN32
#define DMX_PREFS_DIR_NAME "demucs-torch-windows"
#else
#define DMX_PREFS_DIR_NAME "demucs-mlx-c-arm-mac-silicon"
#endif
#endif
#define DMX_PREFS_FILE_NAME "prefs.conf"
#define DMX_PREFS_KEY "OUTPUT_DIR="
#define DMX_PREFS_DEVICE_KEY "STEMS_DEVICE="

static int prefs_dir(char *out, size_t out_sz) {
  if (!out || out_sz < 8) {
    return -1;
  }
#ifdef _WIN32
  const char *appdata = getenv("APPDATA");
  if (!appdata || !appdata[0]) {
    return -1;
  }
  int n = snprintf(out, out_sz, "%s\\%s", appdata, DMX_PREFS_DIR_NAME);
#else
  const char *home = getenv("HOME");
  if (!home || !home[0]) {
    return -1;
  }
  int n = snprintf(out, out_sz, "%s/Library/Application Support/%s", home, DMX_PREFS_DIR_NAME);
#endif
  if (n < 0 || (size_t)n >= out_sz) {
    return -1;
  }
  return 0;
}

static int prefs_path(char *out, size_t out_sz) {
  char dir[1024];
  if (prefs_dir(dir, sizeof dir) != 0) {
    return -1;
  }
  int n = snprintf(out, out_sz, "%s/%s", dir, DMX_PREFS_FILE_NAME);
  if (n < 0 || (size_t)n >= out_sz) {
    return -1;
  }
  return 0;
}

static int ensure_prefs_dir(void) {
  char dir[1024];
  if (prefs_dir(dir, sizeof dir) != 0) {
    return -1;
  }
#ifdef _WIN32
  if (_mkdir(dir) != 0 && errno != EEXIST) {
    return -1;
  }
#else
  if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
    return -1;
  }
#endif
  return 0;
}

static int path_is_dir(const char *path) {
  struct stat st;
  if (!path || !path[0]) {
    return 0;
  }
  if (stat(path, &st) != 0) {
    return 0;
  }
  return S_ISDIR(st.st_mode) ? 1 : 0;
}

static int mkdir_p(const char *path) {
  char buf[2048];
  if (!path || !path[0]) {
    return -1;
  }
  snprintf(buf, sizeof buf, "%s", path);
  size_t n = strlen(buf);
  while (n > 1 && (buf[n - 1] == '/' || buf[n - 1] == '\\')) {
    buf[--n] = '\0';
  }
  for (char *p = buf + 1; *p; ++p) {
    if (*p != '/' && *p != '\\') {
      continue;
    }
    char save = *p;
    *p = '\0';
#ifdef _WIN32
    if (!(buf[0] && buf[1] == ':' && buf[2] == '\0')) {
      if (_mkdir(buf) != 0 && errno != EEXIST) {
        *p = save;
        return -1;
      }
    }
#else
    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
      *p = save;
      return -1;
    }
#endif
    *p = save;
  }
#ifdef _WIN32
  if (_mkdir(buf) != 0 && errno != EEXIST) {
    return -1;
  }
#else
  if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
    return -1;
  }
#endif
  return 0;
}

int dmx_prefs_default_output_dir(char *out, size_t out_sz) {
  if (!out || out_sz == 0) {
    return -1;
  }
  out[0] = '\0';
#ifdef _WIN32
  const char *home = getenv("USERPROFILE");
  if (!home || !home[0]) {
    home = getenv("HOME");
  }
  if (!home || !home[0]) {
    return -1;
  }
  int n = snprintf(out, out_sz, "%s\\Documents\\psarc\\stems", home);
#else
  const char *home = getenv("HOME");
  if (!home || !home[0]) {
    return -1;
  }
  int n = snprintf(out, out_sz, "%s/Documents/psarc/stems", home);
#endif
  if (n < 0 || (size_t)n >= out_sz) {
    out[0] = '\0';
    return -1;
  }
  if (path_is_dir(out)) {
    return 0;
  }
  if (mkdir_p(out) != 0 || !path_is_dir(out)) {
    out[0] = '\0';
    return -1;
  }
  return 0;
}

int dmx_prefs_rocksmith_dir(char *out, size_t out_sz) {
  if (!out || out_sz == 0) {
    return -1;
  }
  out[0] = '\0';
#ifdef _WIN32
  const char *vars[] = {"ProgramFiles(x86)", "ProgramFiles", NULL};
  for (int i = 0; vars[i]; i++) {
    const char *base = getenv(vars[i]);
    if (!base || !base[0]) {
      continue;
    }
    int n = snprintf(out, out_sz, "%s\\Steam\\steamapps\\common\\Rocksmith2014", base);
    if (n > 0 && (size_t)n < out_sz && path_is_dir(out)) {
      return 0;
    }
  }
#else
  const char *home = getenv("HOME");
  if (home && home[0]) {
    int n = snprintf(out, out_sz, "%s/Library/Application Support/Steam/steamapps/common/Rocksmith2014", home);
    if (n > 0 && (size_t)n < out_sz && path_is_dir(out)) {
      return 0;
    }
  }
#endif
  out[0] = '\0';
  return -1;
}

/* Value of "KEY=" (key includes the '='). Returns 0 when found and non-empty. */
static int prefs_get(const char *key, char *out, size_t out_sz) {
  if (!out || out_sz == 0) {
    return -1;
  }
  out[0] = '\0';

  char path[1024];
  if (prefs_path(path, sizeof path) != 0) {
    return -1;
  }

  FILE *f = fopen(path, "r");
  if (!f) {
    return -1;
  }

  char line[2048];
  int found = 0;
  while (fgets(line, sizeof line, f)) {
    if (strncmp(line, key, strlen(key)) != 0) {
      continue;
    }
    char *v = line + strlen(key);
    while (*v == ' ' || *v == '\t') {
      ++v;
    }
    size_t n = strlen(v);
    while (n > 0 && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ' || v[n - 1] == '\t')) {
      v[--n] = '\0';
    }
    if (n == 0 || n >= out_sz) {
      break;
    }
    memcpy(out, v, n + 1);
    found = 1;
    break;
  }
  fclose(f);
  return found ? 0 : -1;
}

/* Set "KEY=value", keeping every other line of prefs.conf. */
static int prefs_set(const char *key, const char *value) {
  if (ensure_prefs_dir() != 0) {
    return -1;
  }

  char path[1024];
  if (prefs_path(path, sizeof path) != 0) {
    return -1;
  }

  char keep[8192];
  size_t used = 0;
  keep[0] = '\0';
  FILE *f = fopen(path, "r");
  if (f) {
    char line[2048];
    while (fgets(line, sizeof line, f)) {
      size_t n = strlen(line);
      if (strncmp(line, key, strlen(key)) == 0 || n == 0 || used + n + 2 >= sizeof keep) {
        continue;
      }
      memcpy(keep + used, line, n + 1);
      used += n;
      if (keep[used - 1] != '\n') {
        keep[used++] = '\n';
        keep[used] = '\0';
      }
    }
    fclose(f);
  }

  f = fopen(path, "w");
  if (!f) {
    return -1;
  }
  fprintf(f, "%s%s%s\n", keep, key, value);
  fclose(f);
  return 0;
}

int dmx_prefs_load_output_dir(char *out, size_t out_sz) {
  if (prefs_get(DMX_PREFS_KEY, out, out_sz) != 0 || !path_is_dir(out)) {
    if (out && out_sz) {
      out[0] = '\0';
    }
    return -1;
  }
  return 0;
}

int dmx_prefs_save_output_dir(const char *dir) {
  if (!dir || !dir[0]) {
    return -1;
  }
  return prefs_set(DMX_PREFS_KEY, dir);
}

int dmx_prefs_load_use_cpu(int *out) {
  char v[16];
  if (!out || prefs_get(DMX_PREFS_DEVICE_KEY, v, sizeof v) != 0) {
    return -1;
  }
  if (strcmp(v, "cpu") == 0) {
    *out = 1;
  } else if (strcmp(v, "gpu") == 0) {
    *out = 0;
  } else {
    return -1;
  }
  return 0;
}

int dmx_prefs_save_use_cpu(int use_cpu) { return prefs_set(DMX_PREFS_DEVICE_KEY, use_cpu ? "cpu" : "gpu"); }
