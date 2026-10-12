#include "worker_run_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#define dmx_access _access
#else
#include <unistd.h>
#ifdef __APPLE__
#include <stdint.h>
#endif
#define dmx_access access
#endif

void dmx_worker_config_defaults(DmxWorkerConfig *cfg) {
  memset(cfg, 0, sizeof *cfg);
  cfg->write_mp3 = 1;
  cfg->batch_size = 8;
}

void dmx_worker_live_reset(DmxWorkerLive *live) {
  memset(live, 0, sizeof *live);
  live->exit_code = -1;
}

void dmx_worker_set_msg(char *buf, size_t n, const char *s) {
  if (!buf || n == 0) {
    return;
  }
  snprintf(buf, n, "%s", s ? s : "");
}

int dmx_worker_file_exists(const char *p) {
  return p && p[0] && dmx_access(p, 0) == 0;
}

void dmx_worker_apply_dev_pythonpath(const DmxWorkerConfig *cfg) {
  if (!cfg || !cfg->module_or_empty[0] || !cfg->python_or_worker[0]) {
    return;
  }

  const char *py = cfg->python_or_worker;
  const char *marker = strstr(py, "/.venv/");
#ifdef _WIN32
  if (!marker) {
    marker = strstr(py, "\\.venv\\");
  }
  if (!marker) {
    marker = strstr(py, "\\.venv/");
  }
  if (!marker) {
    marker = strstr(py, "/.venv\\");
  }
#endif
  if (!marker) {
    return;
  }

  size_t n = (size_t)(marker - py);
  if (n == 0 || n >= DMX_PATH_MAX) {
    fprintf(stderr, "dmx_worker: cannot derive PYTHONPATH from %s\n", py);
    return;
  }

  char root[DMX_PATH_MAX];
  memcpy(root, py, n);
  root[n] = '\0';

  char pkg_mlx[DMX_PATH_MAX];
  char pkg_torch[DMX_PATH_MAX];
#ifdef _WIN32
  snprintf(pkg_mlx, sizeof pkg_mlx, "%s\\demucs_mlx", root);
  snprintf(pkg_torch, sizeof pkg_torch, "%s\\demucs_torch", root);
#else
  snprintf(pkg_mlx, sizeof pkg_mlx, "%s/demucs_mlx", root);
  snprintf(pkg_torch, sizeof pkg_torch, "%s/demucs_torch", root);
#endif
  if (!dmx_worker_file_exists(pkg_mlx) && !dmx_worker_file_exists(pkg_torch)) {
    fprintf(stderr,
            "dmx_worker: %s has no demucs_mlx or demucs_torch source; PYTHONPATH not set\n",
            root);
    return;
  }

  const char *old = getenv("PYTHONPATH");
  char combined[DMX_PATH_MAX * 2];
  int wrote;
  if (old && old[0]) {
#ifdef _WIN32
    wrote = snprintf(combined, sizeof combined, "%s;%s", root, old);
#else
    wrote = snprintf(combined, sizeof combined, "%s:%s", root, old);
#endif
  } else {
    wrote = snprintf(combined, sizeof combined, "%s", root);
  }
  if (wrote < 0 || (size_t)wrote >= sizeof combined) {
    fprintf(stderr, "dmx_worker: PYTHONPATH overflow\n");
    return;
  }

#ifdef _WIN32
  if (!SetEnvironmentVariableA("PYTHONPATH", combined)) {
    fprintf(stderr, "dmx_worker: SetEnvironmentVariable PYTHONPATH failed\n");
  }
#else
  if (setenv("PYTHONPATH", combined, 1) != 0) {
    fprintf(stderr, "dmx_worker: setenv PYTHONPATH failed\n");
  }
#endif
}

void dmx_worker_dirname_copy(const char *path, char *out, size_t out_sz) {
  snprintf(out, out_sz, "%s", path);
  char *slash = strrchr(out, '/');
#ifdef _WIN32
  char *bslash = strrchr(out, '\\');
  if (!slash || (bslash && bslash > slash)) {
    slash = bslash;
  }
#endif
  if (slash) {
    *slash = '\0';
  } else {
    snprintf(out, out_sz, ".");
  }
}

int dmx_worker_exe_path(char *out, size_t out_sz) {
  if (!out || out_sz == 0) {
    return -1;
  }
  out[0] = '\0';
#ifdef _WIN32
  wchar_t wexe[DMX_PATH_MAX];
  DWORD n = GetModuleFileNameW(NULL, wexe, (DWORD)(sizeof wexe / sizeof wexe[0]));
  if (n == 0 || n >= (DWORD)(sizeof wexe / sizeof wexe[0])) {
    return -1;
  }
  int need = WideCharToMultiByte(CP_UTF8, 0, wexe, -1, out, (int)out_sz, NULL, NULL);
  return need > 0 ? 0 : -1;
#elif defined(__APPLE__)
  uint32_t sz = (uint32_t)out_sz;
  extern int _NSGetExecutablePath(char *buf, uint32_t *bufsize);
  if (_NSGetExecutablePath(out, &sz) != 0) {
    out[0] = '\0';
    return -1;
  }
  return 0;
#else
  ssize_t n = readlink("/proc/self/exe", out, out_sz - 1);
  if (n <= 0) {
    out[0] = '\0';
    return -1;
  }
  out[n] = '\0';
  return 0;
#endif
}

static int try_path(DmxWorkerConfig *cfg, const char *path, const char *module) {
  if (!dmx_worker_file_exists(path)) {
    return 0;
  }
  snprintf(cfg->python_or_worker, sizeof cfg->python_or_worker, "%s", path);
  if (module && module[0]) {
    snprintf(cfg->module_or_empty, sizeof cfg->module_or_empty, "%s", module);
  } else {
    cfg->module_or_empty[0] = '\0';
  }
  return 1;
}

int dmx_worker_is_mlx_c(const DmxWorkerConfig *cfg) {
  if (!cfg || cfg->module_or_empty[0] || !cfg->python_or_worker[0]) {
    return 0;
  }
  char dir[DMX_PATH_MAX];
  dmx_worker_dirname_copy(cfg->python_or_worker, dir, sizeof dir);
  char metal[DMX_PATH_MAX];
#ifdef _WIN32
  snprintf(metal, sizeof metal, "%s\\mlx.metallib", dir);
#else
  snprintf(metal, sizeof metal, "%s/mlx.metallib", dir);
#endif
  return dmx_worker_file_exists(metal);
}

int dmx_worker_is_cuda_c(const DmxWorkerConfig *cfg) {
  if (!cfg || cfg->module_or_empty[0] || !cfg->python_or_worker[0]) {
    return 0;
  }
  const char *base = cfg->python_or_worker;
  for (const char *p = cfg->python_or_worker; *p; p++) {
    if (*p == '/' || *p == '\\') {
      base = p + 1;
    }
  }
  return strncmp(base, "demucs_cuda_worker", 18) == 0;
}

int dmx_worker_is_native(const DmxWorkerConfig *cfg) {
  return dmx_worker_is_mlx_c(cfg) || dmx_worker_is_cuda_c(cfg);
}

static int worker_is_torch(const DmxWorkerConfig *cfg) {
  if (strstr(cfg->module_or_empty, "torch")) {
    return 1;
  }
  const char *base = cfg->python_or_worker;
  for (const char *p = cfg->python_or_worker; *p; p++) {
    if (*p == '/' || *p == '\\') {
      base = p + 1;
    }
  }
  return strncmp(base, "demucs_torch", 12) == 0;
}

const char *dmx_worker_backend_label(const DmxWorkerConfig *cfg) {
  if (!cfg || !cfg->python_or_worker[0]) {
    return "none";
  }
  if (dmx_worker_is_mlx_c(cfg)) {
    return "mlx-c";
  }
  if (dmx_worker_is_cuda_c(cfg)) {
    return "cuda-c";
  }
  return worker_is_torch(cfg) ? "torch-py" : "mlx-py";
}

const char *dmx_worker_backend_detail(const DmxWorkerConfig *cfg) {
  if (!cfg || !cfg->python_or_worker[0]) {
    return "no worker found";
  }
  if (dmx_worker_is_mlx_c(cfg)) {
    return "native C + Metal";
  }
  if (dmx_worker_is_cuda_c(cfg)) {
    return "native C + CUDA";
  }
  const int dev = cfg->module_or_empty[0] != '\0';
  if (worker_is_torch(cfg)) {
    return dev ? "PyTorch via python -m" : "frozen PyTorch worker";
  }
  return dev ? "MLX Python via python -m" : "frozen MLX Python worker";
}

int dmx_worker_resolve(DmxWorkerConfig *cfg, char *err, size_t err_sz) {
  if (!cfg) {
    return -1;
  }

#ifdef _WIN32
  const char *env_cuda_worker = getenv("DEMUCS_CUDA_WORKER");
  const char *env_torch_worker = getenv("DEMUCS_TORCH_WORKER");
  const char *env_torch_python = getenv("DEMUCS_TORCH_PYTHON");
#endif
  const char *env_mlx_worker = getenv("DEMUCS_MLX_WORKER");
  const char *env_mlx_python = getenv("DEMUCS_MLX_PYTHON");

#ifdef _WIN32
  if (try_path(cfg, env_cuda_worker, NULL)) {
    /* native C/CUDA worker */
  } else if (try_path(cfg, env_torch_worker, NULL)) {
    /* frozen torch worker */
  } else if (try_path(cfg, env_mlx_worker, NULL)) {
    /* frozen mlx worker (unexpected on Windows) */
  } else if (try_path(cfg, env_torch_python, "demucs_torch.separate")) {
    /* dev python -m */
  } else if (try_path(cfg, env_mlx_python, "demucs_mlx.separate")) {
    /* fallback */
  } else {
    char exe[DMX_PATH_MAX];
    char exe_dir[DMX_PATH_MAX];
    char candidate[DMX_PATH_MAX];
    if (dmx_worker_exe_path(exe, sizeof exe) != 0) {
      exe[0] = '\0';
    }
    dmx_worker_dirname_copy(exe, exe_dir, sizeof exe_dir);

    const char *worker_names[] = {"demucs_cuda_worker.exe", "demucs_torch_worker.exe"};
    for (size_t w = 0; w < sizeof worker_names / sizeof worker_names[0]; w++) {
      snprintf(candidate, sizeof candidate, "%s\\worker\\%s", exe_dir, worker_names[w]);
      if (try_path(cfg, candidate, NULL)) {
        break;
      }
      snprintf(candidate, sizeof candidate, "%s\\%s", exe_dir, worker_names[w]);
      if (try_path(cfg, candidate, NULL)) {
        break;
      }
    }
    if (!dmx_worker_file_exists(cfg->python_or_worker)) {
      snprintf(candidate, sizeof candidate,
               "%s\\..\\..\\workers\\demucs-windows\\.venv\\Scripts\\python.exe", exe_dir);
      if (!try_path(cfg, candidate, "demucs_torch.separate")) {
        snprintf(err, err_sz, "No demucs worker found. Set DEMUCS_CUDA_WORKER, DEMUCS_TORCH_WORKER or DEMUCS_TORCH_PYTHON.");
        return -1;
      }
    }
  }
#else
  if (try_path(cfg, env_mlx_worker, NULL)) {
    /* frozen mlx worker */
  } else if (try_path(cfg, env_mlx_python, "demucs_mlx.separate")) {
    /* dev python -m */
  } else {
    char exe[DMX_PATH_MAX];
    char exe_dir[DMX_PATH_MAX];
    char candidate[DMX_PATH_MAX];
    if (dmx_worker_exe_path(exe, sizeof exe) != 0) {
      exe[0] = '\0';
    }
    dmx_worker_dirname_copy(exe, exe_dir, sizeof exe_dir);

    snprintf(candidate, sizeof candidate, "%s/../Resources/worker/demucs_mlx_worker", exe_dir);
    if (!try_path(cfg, candidate, NULL)) {
      snprintf(candidate, sizeof candidate, "%s/worker/demucs_mlx_worker", exe_dir);
      if (!try_path(cfg, candidate, NULL)) {
        snprintf(candidate, sizeof candidate, "%s/demucs_mlx_worker", exe_dir);
        try_path(cfg, candidate, NULL);
      }
    }
    if (!dmx_worker_file_exists(cfg->python_or_worker)) {
      snprintf(candidate, sizeof candidate,
               "%s/../../workers/demucs-mac-silicon/.venv/bin/python", exe_dir);
      if (!try_path(cfg, candidate, "demucs_mlx.separate")) {
        snprintf(err, err_sz, "No demucs worker found. Set DEMUCS_MLX_PYTHON or DEMUCS_MLX_WORKER.");
        return -1;
      }
    }
  }
#endif

  const char *env_torch_cache = getenv("DEMUCS_TORCH_CACHE");
  const char *env_mlx_cache = getenv("DEMUCS_MLX_CACHE");
  if (env_torch_cache && env_torch_cache[0]) {
    snprintf(cfg->model_cache_dir, sizeof cfg->model_cache_dir, "%s", env_torch_cache);
  } else if (env_mlx_cache && env_mlx_cache[0]) {
    snprintf(cfg->model_cache_dir, sizeof cfg->model_cache_dir, "%s", env_mlx_cache);
  } else {
    char exe[DMX_PATH_MAX];
    char exe_dir[DMX_PATH_MAX];
    char candidate[DMX_PATH_MAX];
    if (dmx_worker_exe_path(exe, sizeof exe) != 0) {
      exe[0] = '\0';
    }
    dmx_worker_dirname_copy(exe, exe_dir, sizeof exe_dir);
#ifdef _WIN32
    snprintf(candidate, sizeof candidate, "%s\\models", exe_dir);
#else
    snprintf(candidate, sizeof candidate, "%s/../Resources/models", exe_dir);
    if (!dmx_worker_file_exists(candidate)) {
      snprintf(candidate, sizeof candidate, "%s/models", exe_dir);
    }
#endif
    if (dmx_worker_file_exists(candidate)) {
      snprintf(cfg->model_cache_dir, sizeof cfg->model_cache_dir, "%s", candidate);
    } else {
#ifdef _WIN32
      const char *appdata = getenv("LOCALAPPDATA");
      if (appdata && appdata[0]) {
        snprintf(cfg->model_cache_dir, sizeof cfg->model_cache_dir, "%s\\demucs-cuda-windows\\models",
                 appdata);
      }
#else
      const char *home = getenv("HOME");
      if (home) {
        snprintf(cfg->model_cache_dir, sizeof cfg->model_cache_dir,
                 "%s/.cache/demucs-mlx-arm-mac-silicon", home);
      }
#endif
    }
  }

  return 0;
}

static void copy_json_string(const char *line, const char *key, char *out, size_t out_sz) {
  if (!out || out_sz == 0) {
    return;
  }
  out[0] = '\0';
  const char *p = strstr(line, key);
  if (!p) {
    return;
  }
  p = strchr(p, ':');
  if (!p) {
    return;
  }
  p++;
  while (*p == ' ' || *p == '"') {
    p++;
  }
  size_t i = 0;
  while (*p && *p != '"' && *p != ',' && *p != '}' && i + 1 < out_sz) {
    out[i++] = *p++;
  }
  out[i] = '\0';
}

void dmx_worker_handle_json_line(DmxWorkerLive *live, const char *line) {
  const char *ev = strstr(line, "\"event\"");
  if (!ev) {
    return;
  }
  if (strstr(line, "\"separating\"")) {
    const char *pct = strstr(line, "\"pct\"");
    if (pct) {
      pct = strchr(pct, ':');
      if (pct) {
        live->progress_pct = (float)atof(pct + 1);
      }
    }
    dmx_worker_set_msg(live->status_msg, sizeof live->status_msg, "Separating...");
  } else if (strstr(line, "\"loading\"")) {
    dmx_worker_set_msg(live->status_msg, sizeof live->status_msg, "Loading audio...");
  } else if (strstr(line, "\"writing\"")) {
    dmx_worker_set_msg(live->status_msg, sizeof live->status_msg, "Writing stems...");
  } else if (strstr(line, "\"done\"")) {
    live->progress_pct = 100.0f;
    dmx_worker_set_msg(live->status_msg, sizeof live->status_msg, "Done");
  } else if (strstr(line, "\"error\"")) {
    const char *msg = strstr(line, "\"message\"");
    if (msg) {
      msg = strchr(msg, ':');
      if (msg) {
        msg++;
        while (*msg == ' ' || *msg == '"') {
          msg++;
        }
        char tmp[DMX_MSG_MAX];
        size_t i = 0;
        while (*msg && *msg != '"' && i + 1 < sizeof tmp) {
          tmp[i++] = *msg++;
        }
        tmp[i] = '\0';
        dmx_worker_set_msg(live->error_msg, sizeof live->error_msg, tmp);
      }
    }
  } else if (strstr(line, "\"status\"")) {
    char device[32];
    copy_json_string(line, "\"device\"", device, sizeof device);
    if (device[0]) {
      snprintf(live->device, sizeof live->device, "%s", device);
    }
    if (strstr(line, "loading_model")) {
      if (live->device[0]) {
        snprintf(live->status_msg, sizeof live->status_msg, "Loading model (%s)...", live->device);
      } else {
        dmx_worker_set_msg(live->status_msg, sizeof live->status_msg, "Loading model...");
      }
    } else if (strstr(line, "model_ready")) {
      if (strcmp(live->device, "cpu") == 0) {
        dmx_worker_set_msg(live->status_msg, sizeof live->status_msg,
                           "Model ready (CPU — a 4-minute track may take about a minute)");
      } else if (live->device[0]) {
        snprintf(live->status_msg, sizeof live->status_msg, "Model ready (%s)", live->device);
      } else {
        dmx_worker_set_msg(live->status_msg, sizeof live->status_msg, "Model ready");
      }
    }
  }
}
