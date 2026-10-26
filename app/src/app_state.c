#include "app_state.h"

#include "prefs.h"
#include "psarc_input.h"

#include "rocksmith_psarc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <direct.h>
#endif

typedef struct {
  DmxApp *app;
  int job_slot;
} DmxWorkerArgs;

static int is_supported_path(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot) {
    return 0;
  }
  static const char *exts[] = {".mp3", ".wav", ".ogg", ".flac", ".m4a", ".aiff", ".aif", ".psarc", NULL};
  for (int i = 0; exts[i]; ++i) {
    const char *a = dot;
    const char *b = exts[i];
    int ok = 1;
    while (*a && *b) {
      char ca = *a;
      char cb = *b;
      if (ca >= 'A' && ca <= 'Z') {
        ca = (char)(ca - 'A' + 'a');
      }
      if (ca != cb) {
        ok = 0;
        break;
      }
      ++a;
      ++b;
    }
    if (ok && *a == '\0' && *b == '\0') {
      return 1;
    }
  }
  return 0;
}

#ifdef __APPLE__
static int path_is_wav(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot) {
    return 0;
  }
  return strcmp(dot, ".wav") == 0 || strcmp(dot, ".WAV") == 0;
}

static int afconvert_to_44100_wav(const char *src, char *out, size_t out_sz, char *err, size_t err_sz,
                                  DmxWorkerLive *live) {
  char *tmp_err = NULL;
  if (rs_temp_make_empty_with_suffix(".wav", out, out_sz, &tmp_err) != 0) {
    snprintf(err, err_sz, "%s", tmp_err ? tmp_err : "temp wav failed");
    free(tmp_err);
    return -1;
  }
  pid_t pid = fork();
  if (pid < 0) {
    remove(out);
    snprintf(err, err_sz, "fork afconvert failed");
    return -1;
  }
  if (pid == 0) {
    execl("/usr/bin/afconvert", "afconvert", "-f", "WAVE", "-d", "LEI16@44100", "-c", "2", src, out,
          (char *)NULL);
    _exit(127);
  }
  if (live) {
    live->pid = (int)pid;
  }
  int status = 0;
  for (;;) {
    pid_t w = waitpid(pid, &status, WNOHANG);
    if (w == pid) {
      break;
    }
    if (w < 0) {
      if (live) {
        live->pid = 0;
      }
      remove(out);
      snprintf(err, err_sz, "afconvert failed for %s (C worker needs WAV/MP3/M4A/AIFF)", src);
      return -1;
    }
    if (live && live->cancel_requested) {
      kill(pid, SIGTERM);
      usleep(200000);
      if (waitpid(pid, &status, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
      }
      if (live) {
        live->pid = 0;
      }
      remove(out);
      snprintf(err, err_sz, "Cancelled");
      return -1;
    }
    usleep(50000);
  }
  if (live) {
    live->pid = 0;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    remove(out);
    snprintf(err, err_sz, "afconvert failed for %s (C worker needs WAV/MP3/M4A/AIFF)", src);
    return -1;
  }
  return 0;
}
#endif

int dmx_app_status_count(const DmxApp *app, DmxJobStatus status) {
  if (!app) {
    return 0;
  }
  int n = 0;
  for (int row = 0; row < app->queue.count; ++row) {
    const DmxJob *j = dmx_queue_at(&app->queue, row);
    if (j && j->status == status) {
      ++n;
    }
  }
  return n;
}

int dmx_app_queued_count(const DmxApp *app) { return dmx_app_status_count(app, DMX_JOB_QUEUED); }

void dmx_app_init(DmxApp *app) {
  memset(app, 0, sizeof *app);
  dmx_queue_init(&app->queue);
  dmx_worker_config_defaults(&app->worker_cfg);
  dmx_worker_live_reset(&app->live);
  app->write_mp3 = 1;
  app->output_dir[0] = '\0';
  app->output_dir_accepted = 0;
  app->processing_enabled = 0;

  char err[512];
  if (dmx_worker_resolve(&app->worker_cfg, err, sizeof err) != 0) {
    snprintf(app->status_line, sizeof app->status_line, "%s", err);
  } else if (dmx_worker_is_native(&app->worker_cfg)) {
    snprintf(app->status_line, sizeof app->status_line, "%s worker (%s). Add files, pick output, Start",
             dmx_worker_backend_label(&app->worker_cfg), DMX_MODEL_NAME);
  } else {
    snprintf(app->status_line, sizeof app->status_line,
             "Add files, pick an output folder, then press Start");
  }

  /* Last picked folder, else the shared Rock N Roller stems root. */
  char saved[DMX_PATH_MAX];
  if (dmx_prefs_load_output_dir(saved, sizeof saved) == 0) {
    snprintf(app->output_dir, sizeof app->output_dir, "%s", saved);
    app->output_dir_accepted = 1;
    snprintf(app->status_line, sizeof app->status_line, "Output restored -> %s", saved);
  } else if (dmx_prefs_default_output_dir(saved, sizeof saved) == 0) {
    snprintf(app->output_dir, sizeof app->output_dir, "%s", saved);
    app->output_dir_accepted = 1;
    snprintf(app->status_line, sizeof app->status_line, "Output -> %s", saved);
  }

  int use_cpu = 0;
  if (dmx_prefs_load_use_cpu(&use_cpu) == 0) {
    app->use_cpu = use_cpu;
  }

  dmx_app_apply_settings(app);
}

static Image dmx_load_image_branding(const char *name) {
  Image img = LoadImage(name);
  if (img.data == NULL) {
    img = LoadImage(TextFormat("%sresources/%s", GetApplicationDirectory(), name));
  }
  if (img.data == NULL) {
    img = LoadImage(TextFormat("resources/%s", name));
  }
  return img;
}

void dmx_app_load_branding(DmxApp *app) {
  if (!app) {
    return;
  }

  Image logo = dmx_load_image_branding("demuc-mlx-app-logo-export-1000.png");
  if (logo.data != NULL) {
    app->logo_tex = LoadTextureFromImage(logo);
    SetTextureFilter(app->logo_tex, TEXTURE_FILTER_POINT);
    app->logo_loaded = 1;
    UnloadImage(logo);
  }

  Image win_icon = dmx_load_image_branding("demucs-mlx-app-initials.png");
  if (win_icon.data != NULL) {
    SetWindowIcon(win_icon);
    UnloadImage(win_icon);
  }
}

void dmx_app_shutdown(DmxApp *app) {
  if (!app) {
    return;
  }
  if (app->live.running) {
    dmx_worker_request_cancel(&app->live);
  }
  if (app->worker_thread) {
    dmx_thread_join(app->worker_thread);
    app->worker_thread = NULL;
  }
  if (app->logo_loaded) {
    UnloadTexture(app->logo_tex);
    app->logo_loaded = 0;
  }
}

void dmx_app_apply_settings(DmxApp *app) {
  app->worker_cfg.write_mp3 = app->write_mp3;
  app->worker_cfg.use_cpu = app->use_cpu;
}

void dmx_app_set_use_cpu(DmxApp *app, int use_cpu) {
  app->use_cpu = use_cpu ? 1 : 0;
  app->gpu_error = 0;
  dmx_app_apply_settings(app);
  const char *which = app->use_cpu ? "CPU" : "GPU";
  if (dmx_prefs_save_use_cpu(app->use_cpu) != 0) {
    snprintf(app->status_line, sizeof app->status_line, "Device: %s for the next job (could not save prefs)",
             which);
  } else {
    snprintf(app->status_line, sizeof app->status_line, "Device: %s for the next job", which);
  }
}

void dmx_app_set_output_dir(DmxApp *app, const char *dir) {
  if (!app || !dir || !dir[0]) {
    return;
  }
  snprintf(app->output_dir, sizeof app->output_dir, "%s", dir);
  app->output_dir_accepted = 1;
  if (dmx_prefs_save_output_dir(dir) != 0) {
    snprintf(app->status_line, sizeof app->status_line,
             "Output set (could not save prefs): %s", dir);
    return;
  }
  int q = dmx_app_queued_count(app);
  if (q > 0 && !app->processing_enabled) {
    snprintf(app->status_line, sizeof app->status_line, "Output set - %d queued, press Start", q);
  } else {
    snprintf(app->status_line, sizeof app->status_line, "Output -> %s", dir);
  }
}

void dmx_app_add_path(DmxApp *app, const char *path) {
  if (!app || !path || !path[0]) {
    return;
  }
  if (!is_supported_path(path)) {
    snprintf(app->status_line, sizeof app->status_line, "Unsupported: %s", path);
    return;
  }
  if (dmx_queue_add(&app->queue, path) != 0) {
    snprintf(app->status_line, sizeof app->status_line, "Queue full");
    return;
  }
  int q = dmx_app_queued_count(app);
  if (!app->output_dir_accepted || !app->output_dir[0]) {
    snprintf(app->status_line, sizeof app->status_line,
             "Queued %d file(s) - pick output folder, then Start", q);
  } else if (!app->processing_enabled) {
    snprintf(app->status_line, sizeof app->status_line, "Queued %d file(s) - press Start", q);
  } else {
    snprintf(app->status_line, sizeof app->status_line, "Added %s", path);
  }
}

void dmx_app_start(DmxApp *app) {
  if (!app) {
    return;
  }
  if (!app->output_dir_accepted || !app->output_dir[0]) {
    snprintf(app->status_line, sizeof app->status_line, "Pick an output folder first");
    return;
  }
  if (dmx_app_queued_count(app) == 0 && !app->worker_thread) {
    snprintf(app->status_line, sizeof app->status_line, "Add files before pressing Start");
    return;
  }
  app->processing_enabled = 1;
  app->cancel_pressed = 0;
  snprintf(app->status_line, sizeof app->status_line, "Processing...");
}

void dmx_app_stop(DmxApp *app) {
  if (!app) {
    return;
  }
  app->processing_enabled = 0;
  app->cancel_pressed = 1;
  dmx_worker_request_cancel(&app->live);
  snprintf(app->status_line, sizeof app->status_line, "Stopped");
}

void dmx_app_request_cancel(DmxApp *app) {
  dmx_app_stop(app);
}

void dmx_app_move_job(DmxApp *app, int row, int delta) {
  if (!app) {
    return;
  }
  dmx_queue_move(&app->queue, row, delta);
}

void dmx_app_remove_job(DmxApp *app, int row) {
  if (!app) {
    return;
  }
  int rc = dmx_queue_remove(&app->queue, row);
  if (rc < 0) {
    return;
  }
  if (rc == 1) {
    DmxJob *j = dmx_queue_at_mut(&app->queue, row);
    if (j) {
      snprintf(j->message, sizeof j->message, "Cancelling...");
    }
    dmx_worker_request_cancel(&app->live);
    snprintf(app->status_line, sizeof app->status_line, "Cancelling...");
    return;
  }
  int q = dmx_app_queued_count(app);
  if (q > 0 && !app->processing_enabled) {
    snprintf(app->status_line, sizeof app->status_line, "Removed - %d still queued", q);
  } else {
    snprintf(app->status_line, sizeof app->status_line, "Removed");
  }
}

static void cleanup_temp(DmxJob *job) {
  if (job && job->owns_temp_audio && job->prepared_audio[0]) {
    remove(job->prepared_audio);
    job->prepared_audio[0] = '\0';
    job->owns_temp_audio = 0;
  }
}

static void discard_partial_stems(const DmxJob *job) {
  static const char *stems[] = {"drums", "bass", "other", "vocals", "guitar", "piano"};
  static const char *exts[] = {"mp3", "wav"};
  char path[DMX_PATH_MAX];
  if (!job || !job->out_dir[0] || !job->track_name[0]) {
    return;
  }
  for (size_t s = 0; s < sizeof stems / sizeof stems[0]; ++s) {
    for (size_t e = 0; e < sizeof exts / sizeof exts[0]; ++e) {
      int n = snprintf(path, sizeof path, "%s/%s/%s.%s", job->out_dir, job->track_name, stems[s], exts[e]);
      if (n > 0 && (size_t)n < sizeof path) {
        remove(path);
      }
    }
  }
  int n = snprintf(path, sizeof path, "%s/%s", job->out_dir, job->track_name);
  if (n > 0 && (size_t)n < sizeof path) {
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
  }
}

static void mark_cancelled(DmxApp *app, DmxJob *job, int discard_stems) {
  job->status = DMX_JOB_CANCELLED;
  snprintf(job->message, sizeof job->message, "Cancelled");
  if (discard_stems) {
    discard_partial_stems(job);
  }
  cleanup_temp(job);
  snprintf(app->live.status_msg, sizeof app->live.status_msg, "Cancelled");
  app->live.error_msg[0] = '\0';
  app->live.running = 0;
  app->live.done = 1;
}

static int cancel_requested(const DmxApp *app) { return app->live.cancel_requested; }

static void *worker_thread_main(void *arg) {
  DmxWorkerArgs *wa = (DmxWorkerArgs *)arg;
  DmxApp *app = wa->app;
  DmxJob *job = &app->queue.jobs[wa->job_slot];

  if (cancel_requested(app)) {
    mark_cancelled(app, job, 0);
    free(wa);
    return NULL;
  }

  snprintf(job->out_dir, sizeof job->out_dir, "%s", app->output_dir);
  job->status = DMX_JOB_PREPARING;
  if (!job->remove_when_done) {
    snprintf(job->message, sizeof job->message, "Preparing...");
  }
  snprintf(app->live.status_msg, sizeof app->live.status_msg, "Preparing...");

  if (job->is_psarc) {
    char *errmsg = NULL;
    int rc = dmx_psarc_extract_audio(job->input_path, job->prepared_audio, sizeof job->prepared_audio, &errmsg);
    if (cancel_requested(app)) {
      if (rc == 0) {
        job->owns_temp_audio = 1;
      }
      free(errmsg);
      mark_cancelled(app, job, 0);
      free(wa);
      return NULL;
    }
    if (rc != 0) {
      job->status = DMX_JOB_ERROR;
      snprintf(job->message, sizeof job->message, "%s", errmsg ? errmsg : "psarc extract failed");
      snprintf(app->live.error_msg, sizeof app->live.error_msg, "%s", job->message);
      free(errmsg);
      app->live.done = 1;
      app->live.running = 0;
      free(wa);
      return NULL;
    }
    job->owns_temp_audio = 1;
    free(errmsg);
  } else {
    snprintf(job->prepared_audio, sizeof job->prepared_audio, "%s", job->input_path);
    job->owns_temp_audio = 0;
  }

#ifdef __APPLE__
  if ((dmx_worker_is_mlx_c(&app->worker_cfg) || dmx_worker_is_onnx(&app->worker_cfg)) &&
      !path_is_wav(job->prepared_audio)) {
    char converted[DMX_PATH_MAX];
    char conv_err[512];
    if (afconvert_to_44100_wav(job->prepared_audio, converted, sizeof converted, conv_err, sizeof conv_err,
                               &app->live) != 0) {
      if (cancel_requested(app)) {
        mark_cancelled(app, job, 0);
      } else {
        job->status = DMX_JOB_ERROR;
        snprintf(job->message, sizeof job->message, "%s", conv_err);
        snprintf(app->live.error_msg, sizeof app->live.error_msg, "%s", conv_err);
        cleanup_temp(job);
        app->live.done = 1;
        app->live.running = 0;
      }
      free(wa);
      return NULL;
    }
    if (job->owns_temp_audio) {
      remove(job->prepared_audio);
    }
    snprintf(job->prepared_audio, sizeof job->prepared_audio, "%s", converted);
    job->owns_temp_audio = 1;
  }
#endif

  if (cancel_requested(app)) {
    mark_cancelled(app, job, 0);
    free(wa);
    return NULL;
  }

  job->status = DMX_JOB_RUNNING;
  if (!job->remove_when_done) {
    snprintf(job->message, sizeof job->message, "Running...");
  }
  dmx_worker_run_job(&app->worker_cfg, job, &app->live);

  if (cancel_requested(app)) {
    mark_cancelled(app, job, 1);
  } else if (app->live.exit_code == 0) {
    job->status = DMX_JOB_DONE;
    job->progress_pct = 100.0f;
    snprintf(job->message, sizeof job->message, "Wrote stems to %s/%s", job->out_dir, job->track_name);
    cleanup_temp(job);
  } else {
    job->status = DMX_JOB_ERROR;
    snprintf(job->message, sizeof job->message, "%s",
             app->live.error_msg[0] ? app->live.error_msg : "Worker failed");
    fprintf(stderr, "demucs: job failed (exit %d) for %s: %s\n", app->live.exit_code,
            job->input_path, job->message);
    fflush(stderr);
    cleanup_temp(job);
  }

  free(wa);
  return NULL;
}

void dmx_app_tick(DmxApp *app) {
  dmx_app_apply_settings(app);

  /* Sync live progress into the running slot. Reordering does not move that slot. */
  if (app->queue.active_slot >= 0 && app->queue.active_slot < DMX_JOB_MAX &&
      app->queue.slot_used[app->queue.active_slot]) {
    DmxJob *j = &app->queue.jobs[app->queue.active_slot];
    if (j->status == DMX_JOB_RUNNING || j->status == DMX_JOB_PREPARING) {
      if (j->remove_when_done || app->live.cancel_requested) {
        snprintf(j->message, sizeof j->message, "Cancelling...");
      } else {
        j->progress_pct = app->live.progress_pct;
        if (app->live.status_msg[0]) {
          snprintf(j->message, sizeof j->message, "%s", app->live.status_msg);
        }
      }
    }
  }
  if (app->live.device[0]) {
    snprintf(app->device, sizeof app->device, "%s", app->live.device);
    snprintf(app->gpu, sizeof app->gpu, "%s", app->live.gpu);
    app->gpu_failed = app->live.gpu_failed;
  }

  /* Join finished worker */
  if (app->worker_thread && app->live.done && !app->live.running) {
    int slot = app->queue.active_slot;
    int drop = slot >= 0 && slot < DMX_JOB_MAX && app->queue.slot_used[slot] &&
               app->queue.jobs[slot].remove_when_done;
    dmx_thread_join(app->worker_thread);
    if (slot >= 0 && slot < DMX_JOB_MAX && app->queue.slot_used[slot]) {
      DmxJob *fj = &app->queue.jobs[slot];
      if (fj->started_at > 0.0) {
        fj->elapsed_s = GetTime() - fj->started_at;
        fprintf(stderr, "demucs: %s %s in %.1fs\n", fj->display_name, dmx_job_status_label(fj->status),
                fj->elapsed_s);
      }
    }
    app->worker_thread = NULL;
    app->queue.active_slot = -1;
    app->cancel_pressed = 0;
    if (app->live.gpu_error) {
      /* Every queued job would hit the same GPU failure. The user picks CPU and restarts. */
      app->gpu_error = 1;
      app->processing_enabled = 0;
      snprintf(app->status_line, sizeof app->status_line,
               "GPU failed - stopped. Switch Device to CPU and press Start to run on the CPU.");
      fprintf(stderr, "demucs: GPU failed; queue stopped. Switch Device to CPU to run on the CPU.\n");
      fflush(stderr);
    } else if (app->live.exit_code == 0) {
      app->gpu_error = 0;
    }
    dmx_worker_live_reset(&app->live);
    if (drop) {
      dmx_queue_drop_slot(&app->queue, slot);
    }
  }

  /* Start next job only when user pressed Start and output folder is set. */
  if (!app->worker_thread) {
    if (!app->processing_enabled) {
      return;
    }
    if (!app->output_dir_accepted || !app->output_dir[0]) {
      return;
    }
    int idx = -1;
    DmxJob *next = dmx_queue_next_queued(&app->queue, &idx);
    if (next) {
      DmxWorkerArgs *wa = (DmxWorkerArgs *)calloc(1, sizeof *wa);
      if (!wa) {
        return;
      }
      wa->app = app;
      wa->job_slot = idx;
      app->queue.active_slot = idx;
      dmx_worker_live_reset(&app->live);
      next->started_at = GetTime();
      next->elapsed_s = 0.0;
      if (dmx_thread_spawn(&app->worker_thread, worker_thread_main, wa) != 0) {
        free(wa);
        next->status = DMX_JOB_ERROR;
        snprintf(next->message, sizeof next->message, "Failed to spawn worker thread");
        app->queue.active_slot = -1;
        app->worker_thread = NULL;
      }
    } else {
      app->processing_enabled = 0;
      if (app->queue.count == 0) {
        snprintf(app->status_line, sizeof app->status_line, "Queue empty");
        return;
      }
      int done = dmx_app_status_count(app, DMX_JOB_DONE);
      int failed = dmx_app_status_count(app, DMX_JOB_ERROR) + dmx_app_status_count(app, DMX_JOB_CANCELLED);
      if (failed > 0) {
        snprintf(app->status_line, sizeof app->status_line, "All jobs finished: %d done, %d failed or cancelled",
                 done, failed);
      } else {
        snprintf(app->status_line, sizeof app->status_line, "All jobs finished: %d done -> %s", done,
                 app->output_dir);
      }
    }
  }
}
