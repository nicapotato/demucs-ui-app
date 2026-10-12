#ifndef DMX_WORKER_RUN_H
#define DMX_WORKER_RUN_H

#include "job_queue.h"

#include <stddef.h>

typedef struct {
  char python_or_worker[DMX_PATH_MAX];
  char module_or_empty[64]; /* "demucs_mlx.separate" when using python -m */
  char model_cache_dir[DMX_PATH_MAX];
  char model_name[64];
  int write_mp3;
  int batch_size;
} DmxWorkerConfig;

typedef struct {
  volatile int running;
  volatile int done;
  volatile int cancel_requested;
  volatile float progress_pct;
  char status_msg[DMX_MSG_MAX];
  char error_msg[DMX_MSG_MAX];
  char device[16]; /* "cuda" / "cpu" from worker status JSON; empty until known */
  int exit_code;
  int pid;
  void *os_handle; /* Windows process HANDLE; unused on POSIX */
} DmxWorkerLive;

void dmx_worker_config_defaults(DmxWorkerConfig *cfg);
void dmx_worker_live_reset(DmxWorkerLive *live);

/* Resolve worker binary / python. Returns 0 on success. */
int dmx_worker_resolve(DmxWorkerConfig *cfg, char *err, size_t err_sz);

/* Native mlx-c worker: sibling mlx.metallib, no python -m. */
int dmx_worker_is_mlx_c(const DmxWorkerConfig *cfg);

/* Native C/CUDA worker (demucs_cuda_worker.exe). */
int dmx_worker_is_cuda_c(const DmxWorkerConfig *cfg);

/* Either native worker: htdemucs_6s only, no python -m. */
int dmx_worker_is_native(const DmxWorkerConfig *cfg);
const char *dmx_worker_native_label(const DmxWorkerConfig *cfg);

/* Spawn worker for one job. Updates live asynchronously via monitor thread.
 * Call from a background thread; blocks until process exits. */
int dmx_worker_run_job(const DmxWorkerConfig *cfg, DmxJob *job, DmxWorkerLive *live);

/* Request cancel of the currently running worker pid. */
void dmx_worker_request_cancel(DmxWorkerLive *live);

#endif
