#include "ui.h"

#include "platform/threads.h"
#include "prefs.h"
#include "tinyfiledialogs.h"
#include "ui_theme.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef _WIN32
#define strtok_r strtok_s
#endif

#ifndef DMX_APP_VERSION
#define DMX_APP_VERSION "0.0.0"
#endif

#define DMX_DLG_RESULT_MAX 8192

#define DMX_UI_MARGIN 20.0f
#define DMX_UI_GAP 12.0f
#define DMX_UI_BTN_H 38.0f
#define DMX_UI_BTN_FS 18
#define DMX_UI_INFO_FS 16
#define DMX_UI_INFO_LINE_H 20.0f
#define DMX_UI_INFO_PAD 10.0f
#define DMX_UI_DROP_H 52.0f
#define DMX_UI_JOBS_HDR_FS 20
#define DMX_UI_TABLE_HEAD_H 26.0f
#define DMX_UI_TABLE_HEAD_FS 16
#define DMX_UI_ROW_H 46.0f
#define DMX_UI_ROW_FS 18
#define DMX_UI_ROW_MSG_FS 14
#define DMX_UI_FOOTER_FS 16
#define DMX_UI_FOOTER_LINE_H 20.0f
#define DMX_UI_FOOTER_LINES 2
#define DMX_UI_WHEEL_STEP 48.0f

typedef enum {
  DMX_DLG_NONE = 0,
  DMX_DLG_FILES,
  DMX_DLG_FOLDER,
} DmxDlgKind;

typedef struct {
  DmxDlgKind kind;
  DmxThread *thread;
  volatile int done;
  char start_dir[DMX_PATH_MAX];
  char result[DMX_DLG_RESULT_MAX];
} DmxDlgState;

enum { DMX_INFO_OUTPUT = 0, DMX_INFO_BACKEND, DMX_INFO_WORKER, DMX_INFO_MODELS, DMX_INFO_DEVICE, DMX_INFO_ROWS };

typedef struct {
  const char *label;
  char value[DMX_PATH_MAX + 128];
  Color col;
  int lines;
  float y;
} DmxInfoRow;

/* Single layout for draw + hit-testing (must stay in sync). */
typedef struct {
  Rectangle backend_chip;
  Rectangle add_btn;
  Rectangle out_btn;
  Rectangle fmt_btn;
  Rectangle dev_btn; /* zero-size unless the ONNX GPU / CPU toggle applies */
  Rectangle start_stop_btn;
  Rectangle info;
  Rectangle out_field;
  float info_value_x;
  float info_value_w;
  DmxInfoRow rows[DMX_INFO_ROWS];
  int n_rows;
  Rectangle drop;
  float jobs_y;
  float table_head_y;
  Rectangle body; /* row viewport, scrollbar excluded */
  Rectangle sbar;
  float max_scroll;
  float footer_y;
} DmxUiLayout;

static DmxDlgState g_dlg;
static float g_scroll;
static int g_sbar_drag;
static int g_follow_index = -1;

static int dlg_busy(void) {
  return g_dlg.kind != DMX_DLG_NONE && g_dlg.thread != NULL && !g_dlg.done;
}

static int point_in(int x, int y, Rectangle r) {
  return r.width > 0 && x >= (int)r.x && x < (int)(r.x + r.width) && y >= (int)r.y && y < (int)(r.y + r.height);
}

static int backend_is_native(const DmxApp *app) { return dmx_worker_is_native(&app->worker_cfg); }

static Color backend_accent(const DmxApp *app) {
  if (!app->worker_cfg.python_or_worker[0]) {
    return DMX_COL_ACCENT_RED;
  }
  return backend_is_native(app) ? DMX_COL_ACCENT_GREEN : DMX_COL_ACCENT_ORANGE;
}

static void upper_copy(char *out, size_t out_sz, const char *s) {
  size_t i = 0;
  for (; s[i] && i + 1 < out_sz; ++i) {
    char c = s[i];
    out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
  }
  out[i] = '\0';
}

static Color job_status_color(DmxJobStatus s) {
  switch (s) {
  case DMX_JOB_QUEUED:
    return DMX_COL_TEXT_FAINT;
  case DMX_JOB_PREPARING:
    return DMX_COL_ACCENT_YELLOW;
  case DMX_JOB_RUNNING:
    return DMX_COL_ACCENT_LIGHT_BLUE;
  case DMX_JOB_DONE:
    return DMX_COL_ACCENT_GREEN;
  case DMX_JOB_ERROR:
    return DMX_COL_ACCENT_RED;
  case DMX_JOB_CANCELLED:
    return DMX_COL_ACCENT_ORANGE;
  }
  return DMX_COL_TEXT;
}

static void fill_info_rows(const DmxApp *app, DmxUiLayout *L) {
  const int out_ok = app->output_dir_accepted && app->output_dir[0];
  const DmxWorkerConfig *cfg = &app->worker_cfg;
  L->n_rows = 0;

  DmxInfoRow *r = &L->rows[L->n_rows++];
  r->label = "OUTPUT";
  if (out_ok) {
    snprintf(r->value, sizeof r->value, "%s", app->output_dir);
    r->col = DMX_COL_TEXT;
  } else {
    snprintf(r->value, sizeof r->value, "Not set - click here to pick a stem output folder");
    r->col = DMX_COL_ACCENT_RED;
  }

  r = &L->rows[L->n_rows++];
  r->label = "BACKEND";
  char up[32];
  upper_copy(up, sizeof up, dmx_worker_backend_label(cfg));
  if (dmx_worker_is_onnx(cfg) && app->device[0]) {
    snprintf(r->value, sizeof r->value, "%s - %s (%s) - %s (6 stems)", up, dmx_worker_backend_detail(cfg),
             dmx_worker_device_label(app->device), DMX_MODEL_NAME);
  } else {
    snprintf(r->value, sizeof r->value, "%s - %s - %s (6 stems)", up, dmx_worker_backend_detail(cfg),
             DMX_MODEL_NAME);
  }
  r->col = backend_accent(app);

  r = &L->rows[L->n_rows++];
  r->label = "WORKER";
  if (cfg->python_or_worker[0]) {
    if (cfg->module_or_empty[0]) {
      snprintf(r->value, sizeof r->value, "%s -m %s", cfg->python_or_worker, cfg->module_or_empty);
    } else {
      snprintf(r->value, sizeof r->value, "%s", cfg->python_or_worker);
    }
    r->col = DMX_COL_TEXT_FAINT;
  } else {
    snprintf(r->value, sizeof r->value, "%s", app->status_line[0] ? app->status_line : "No worker found");
    r->col = DMX_COL_ACCENT_RED;
  }

  if (cfg->model_cache_dir[0]) {
    r = &L->rows[L->n_rows++];
    r->label = "MODELS";
    snprintf(r->value, sizeof r->value, "%s", cfg->model_cache_dir);
    r->col = DMX_COL_TEXT_FAINT;
  }

  if (app->gpu_error) {
    r = &L->rows[L->n_rows++];
    r->label = "DEVICE";
    snprintf(r->value, sizeof r->value, "GPU failed%s%s - switch Device to CPU to run on the CPU (see console)",
             app->gpu[0] ? " on " : "", app->gpu);
    r->col = DMX_COL_ACCENT_RED;
  } else if (app->device[0]) {
    r = &L->rows[L->n_rows++];
    r->label = "DEVICE";
    if (app->gpu_failed) {
      snprintf(r->value, sizeof r->value, "CPU (GPU inference failed - see console; running slower on CPU)");
      r->col = DMX_COL_ACCENT_ORANGE;
    } else if (strcmp(app->device, "cpu") == 0 && dmx_worker_gpu_selectable(cfg) && app->use_cpu) {
      snprintf(r->value, sizeof r->value, "CPU (selected - switch Device to GPU for speed)");
      r->col = DMX_COL_ACCENT_ORANGE;
    } else if (strcmp(app->device, "cpu") == 0) {
#if defined(TARGET_CPU_ARM64) && TARGET_CPU_ARM64
      snprintf(r->value, sizeof r->value, "CPU (slow - MLX on Apple Silicon recommended)");
#elif defined(__APPLE__)
      snprintf(r->value, sizeof r->value, "CPU (slow)");
#else
      snprintf(r->value, sizeof r->value, "CPU (slow - NVIDIA CUDA recommended)");
#endif
      r->col = DMX_COL_ACCENT_ORANGE;
    } else if (app->gpu[0]) {
      snprintf(r->value, sizeof r->value, "%s - %s", dmx_worker_device_label(app->device), app->gpu);
      r->col = DMX_COL_ACCENT_GREEN;
    } else {
      snprintf(r->value, sizeof r->value, "%s", dmx_worker_device_label(app->device));
      r->col = DMX_COL_TEXT_FAINT;
    }
  }
}

static const char *fmt_label(const DmxApp *app) { return app->write_mp3 ? "Format: MP3" : "Format: WAV"; }
static const char *dev_label(const DmxApp *app) { return app->use_cpu ? "Device: CPU" : "Device: GPU"; }

static DmxUiLayout dmx_ui_layout(const DmxApp *app, int screen_w, int screen_h) {
  DmxUiLayout L;
  memset(&L, 0, sizeof L);
  const float m = DMX_UI_MARGIN;
  const float right = (float)screen_w - m;

  /* Header: logo / title on the left, backend chip on the right. */
  const float header_top = 16.0f;
  float header_bottom = header_top + 40.0f + 6.0f + 16.0f;
  {
    char up[32];
    upper_copy(up, sizeof up, dmx_worker_backend_label(&app->worker_cfg));
    float vw = (float)dmx_ui_measure_header(up, 20);
    float lw = (float)MeasureText("BACKEND", 10);
    float cw = fmaxf(vw, lw) + 28.0f;
    L.backend_chip = (Rectangle){right - cw, header_top, cw, 52.0f};
    if (L.backend_chip.y + L.backend_chip.height > header_bottom) {
      header_bottom = L.backend_chip.y + L.backend_chip.height;
    }
  }

  /* Toolbar: actions left, Start/Stop right. */
  float y = header_bottom + DMX_UI_GAP;
  {
    float ss_w = (float)MeasureText("Start", DMX_UI_BTN_FS) + 64.0f;
    float w_add = (float)MeasureText("Add files...", DMX_UI_BTN_FS) + 36.0f;
    float w_out = (float)MeasureText("Output folder...", DMX_UI_BTN_FS) + 36.0f;
    float w_fmt = (float)MeasureText("Format: MP3", DMX_UI_BTN_FS) + 36.0f;
    const int has_dev = dmx_worker_gpu_selectable(&app->worker_cfg);
    float w_dev = has_dev ? (float)MeasureText("Device: GPU", DMX_UI_BTN_FS) + 36.0f : 0.0f;
    float avail = right - m - ss_w - DMX_UI_GAP * (has_dev ? 4.0f : 3.0f);
    float natural = w_add + w_out + w_fmt + w_dev;
    if (natural > avail && avail > 0.0f) {
      float k = avail / natural;
      w_add *= k;
      w_out *= k;
      w_fmt *= k;
      w_dev *= k;
    }
    float x = m;
    L.add_btn = (Rectangle){x, y, w_add, DMX_UI_BTN_H};
    x += w_add + DMX_UI_GAP;
    L.out_btn = (Rectangle){x, y, w_out, DMX_UI_BTN_H};
    x += w_out + DMX_UI_GAP;
    L.fmt_btn = (Rectangle){x, y, w_fmt, DMX_UI_BTN_H};
    x += w_fmt + DMX_UI_GAP;
    L.dev_btn = (Rectangle){x, y, w_dev, has_dev ? DMX_UI_BTN_H : 0.0f};
    L.start_stop_btn = (Rectangle){right - ss_w, y, ss_w, DMX_UI_BTN_H};
  }
  y += DMX_UI_BTN_H + DMX_UI_GAP;

  /* Info panel: label column + wrapped values (long paths break at '/'). */
  fill_info_rows(app, &L);
  {
    float label_w = (float)dmx_ui_measure_header("BACKEND", DMX_UI_INFO_FS) + 18.0f;
    L.info_value_x = m + DMX_UI_INFO_PAD + label_w;
    L.info_value_w = right - DMX_UI_INFO_PAD - L.info_value_x;
    float ry = y + DMX_UI_INFO_PAD;
    for (int i = 0; i < L.n_rows; ++i) {
      DmxInfoRow *r = &L.rows[i];
      r->y = ry;
      r->lines = dmx_ui_text_wrap(r->value, 0, 0, L.info_value_w, DMX_UI_INFO_FS, DMX_UI_INFO_LINE_H, r->col, 0,
                                  false);
      if (r->lines < 1) {
        r->lines = 1;
      }
      float rh = (float)r->lines * DMX_UI_INFO_LINE_H;
      if (i == DMX_INFO_OUTPUT) {
        L.out_field = (Rectangle){m + 4.0f, ry - 6.0f, right - m - 8.0f, rh + 10.0f};
        ry += 8.0f;
        r->y += 1.0f;
      }
      ry += rh + 6.0f;
    }
    L.info = (Rectangle){m, y, right - m, ry - y + DMX_UI_INFO_PAD - 6.0f};
  }
  y += L.info.height + DMX_UI_GAP;

  L.drop = (Rectangle){m, y, right - m, DMX_UI_DROP_H};
  y += DMX_UI_DROP_H + DMX_UI_GAP + 4.0f;

  L.jobs_y = y;
  y += (float)DMX_UI_JOBS_HDR_FS + 8.0f;
  L.table_head_y = y;
  y += DMX_UI_TABLE_HEAD_H;

  const float footer_h = DMX_UI_FOOTER_LINE_H * (float)DMX_UI_FOOTER_LINES + 12.0f;
  L.footer_y = (float)screen_h - footer_h;
  float body_h = L.footer_y - 8.0f - y;
  if (body_h < DMX_UI_ROW_H) {
    body_h = DMX_UI_ROW_H;
  }
  L.body = (Rectangle){m, y, right - m - DMX_SBAR_W, body_h};
  L.sbar = (Rectangle){L.body.x + L.body.width, y, DMX_SBAR_W, body_h};
  L.max_scroll = (float)app->queue.count * DMX_UI_ROW_H - body_h;
  if (L.max_scroll < 0.0f) {
    L.max_scroll = 0.0f;
  }
  return L;
}

static Rectangle sbar_thumb(const DmxUiLayout *L, int n_rows) {
  float content_h = (float)n_rows * DMX_UI_ROW_H;
  float h = L->body.height;
  float th = content_h > 0.0f ? h * (h / content_h) : h;
  if (th < 20.0f) {
    th = 20.0f;
  }
  if (th > h) {
    th = h;
  }
  float travel = h - th;
  float ty = (L->max_scroll > 0.0f) ? (g_scroll / L->max_scroll) * travel : 0.0f;
  return (Rectangle){L->sbar.x + 2.0f, L->sbar.y + ty, DMX_SBAR_W - 4.0f, th};
}

static void clamp_scroll(const DmxUiLayout *L) {
  if (g_scroll > L->max_scroll) {
    g_scroll = L->max_scroll;
  }
  if (g_scroll < 0.0f) {
    g_scroll = 0.0f;
  }
}

static void update_scroll(const DmxApp *app, const DmxUiLayout *L) {
  Vector2 mp = GetMousePosition();
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f && (CheckCollisionPointRec(mp, L->body) || CheckCollisionPointRec(mp, L->sbar))) {
    g_scroll -= wheel * DMX_UI_WHEEL_STEP;
  }
  if (g_sbar_drag) {
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      g_sbar_drag = 0;
    } else if (L->max_scroll > 0.0f) {
      Rectangle th = sbar_thumb(L, app->queue.count);
      float travel = L->body.height - th.height;
      if (travel < 1.0f) {
        travel = 1.0f;
      }
      g_scroll = ((mp.y - L->body.y - th.height * 0.5f) / travel) * L->max_scroll;
    }
  }

  /* Keep the job that just started in view. */
  int active = dmx_queue_active_row(&app->queue);
  if (active != g_follow_index) {
    g_follow_index = active;
    if (active >= 0) {
      float top = (float)active * DMX_UI_ROW_H;
      if (top < g_scroll) {
        g_scroll = top;
      } else if (top + DMX_UI_ROW_H > g_scroll + L->body.height) {
        g_scroll = top + DMX_UI_ROW_H - L->body.height;
      }
    }
  }
  clamp_scroll(L);
}

static void strip_trailing_crlf(char *s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

static void *dlg_files_thread(void *arg) {
  DmxDlgState *d = (DmxDlgState *)arg;
  const char *filters[] = {"*.mp3", "*.wav", "*.ogg", "*.flac", "*.m4a", "*.psarc"};
  const char *path = tinyfd_openFileDialog("Add audio or PSARC", d->start_dir, 6, filters, "Audio / PSARC", 1);
  d->result[0] = '\0';
  if (path && path[0]) {
    snprintf(d->result, sizeof d->result, "%s", path);
  }
  d->done = 1;
  return NULL;
}

static void *dlg_folder_thread(void *arg) {
  DmxDlgState *d = (DmxDlgState *)arg;
  const char *start = d->start_dir[0] ? d->start_dir : NULL;
  const char *dir = tinyfd_selectFolderDialog("Stem output folder", start);
  d->result[0] = '\0';
  if (dir && dir[0]) {
    snprintf(d->result, sizeof d->result, "%s", dir);
    strip_trailing_crlf(d->result);
  }
  d->done = 1;
  return NULL;
}

static int begin_dialog(DmxApp *app, DmxDlgKind kind) {
  if (dlg_busy()) {
    snprintf(app->status_line, sizeof app->status_line, "A file dialog is already open");
    return 0;
  }
  if (g_dlg.thread) {
    dmx_thread_join(g_dlg.thread);
    g_dlg.thread = NULL;
  }

  memset(&g_dlg, 0, sizeof g_dlg);
  g_dlg.kind = kind;
  g_dlg.done = 0;
  g_dlg.result[0] = '\0';

  if (kind == DMX_DLG_FOLDER) {
    if (app->output_dir[0]) {
      snprintf(g_dlg.start_dir, sizeof g_dlg.start_dir, "%s", app->output_dir);
    } else {
      char def[DMX_PATH_MAX];
      if (dmx_prefs_default_output_dir(def, sizeof def) == 0) {
        snprintf(g_dlg.start_dir, sizeof g_dlg.start_dir, "%s", def);
      }
    }
  } else {
    char rs[DMX_PATH_MAX];
    if (dmx_prefs_rocksmith_dir(rs, sizeof rs) == 0) {
      /* tinyfd drops the last path component of the default; keep the root with a trailing separator. */
#ifdef _WIN32
      snprintf(g_dlg.start_dir, sizeof g_dlg.start_dir, "%s\\", rs);
#else
      snprintf(g_dlg.start_dir, sizeof g_dlg.start_dir, "%s/", rs);
#endif
    }
  }

  void *(*fn)(void *) = (kind == DMX_DLG_FILES) ? dlg_files_thread : dlg_folder_thread;
  if (dmx_thread_spawn(&g_dlg.thread, fn, &g_dlg) != 0) {
    g_dlg.kind = DMX_DLG_NONE;
    g_dlg.thread = NULL;
    snprintf(app->status_line, sizeof app->status_line, "Could not open file dialog");
    return 0;
  }

  /* Keep a normal arrow over the GLFW window while Finder is open. */
  SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  snprintf(app->status_line, sizeof app->status_line,
           kind == DMX_DLG_FILES ? "Choose files in the dialog..." : "Choose output folder in the dialog...");
  return 1;
}

void dmx_ui_poll(DmxApp *app) {
  if (!app || g_dlg.kind == DMX_DLG_NONE || !g_dlg.thread || !g_dlg.done) {
    return;
  }

  DmxDlgKind kind = g_dlg.kind;
  /* Join first so result writes from the dialog thread are fully visible. */
  dmx_thread_join(g_dlg.thread);
  g_dlg.thread = NULL;

  char result[DMX_DLG_RESULT_MAX];
  snprintf(result, sizeof result, "%s", g_dlg.result);
  g_dlg.kind = DMX_DLG_NONE;
  g_dlg.done = 0;
  g_dlg.result[0] = '\0';

  if (kind == DMX_DLG_FILES) {
    if (!result[0]) {
      snprintf(app->status_line, sizeof app->status_line, "No files selected");
      return;
    }
    char *save = NULL;
    char *tok = strtok_r(result, "|", &save);
    while (tok) {
      dmx_app_add_path(app, tok);
      tok = strtok_r(NULL, "|", &save);
    }
    return;
  }

  if (kind == DMX_DLG_FOLDER) {
    if (result[0]) {
      dmx_app_set_output_dir(app, result);
    } else {
      snprintf(app->status_line, sizeof app->status_line, "Output folder not selected");
    }
  }
}

void dmx_ui_shutdown(void) {
  if (g_dlg.thread) {
    /* Dialog may still be open; join waits until user dismisses it. */
    dmx_thread_join(g_dlg.thread);
    g_dlg.thread = NULL;
  }
  memset(&g_dlg, 0, sizeof g_dlg);
}

static void draw_header(const DmxApp *app, const DmxUiLayout *L) {
#ifdef _WIN32
  const char *title = "Demucs CUDA Windows";
  const char *subtitle = "Windows stem separator (NVIDIA CUDA recommended)";
#elif defined(__APPLE__)
#if defined(TARGET_CPU_ARM64) && TARGET_CPU_ARM64
  const char *title = "Demucs MLX ARM Mac Silicon";
  const char *subtitle = "Apple Silicon stem separator (MLX recommended)";
#else
  const char *title = "Demucs";
  const char *subtitle = "macOS stem separator";
#endif
#else
  const char *title = "Demucs";
  const char *subtitle = "Linux stem separator (NVIDIA CUDA recommended)";
#endif
  const float m = DMX_UI_MARGIN;
  if (app->logo_loaded && app->logo_tex.id > 0) {
    const float target_h = 40.0f;
    float scale = target_h / (float)app->logo_tex.height;
    Rectangle src = {0, 0, (float)app->logo_tex.width, (float)app->logo_tex.height};
    Rectangle dst = {m, 16, (float)app->logo_tex.width * scale, target_h};
    DrawTexturePro(app->logo_tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    dmx_ui_draw_text_fit((int)m, 62, (int)(L->backend_chip.x - m - 12.0f), title, 16, DMX_COL_TEXT_MUTED);
  } else {
    dmx_ui_draw_header((int)m, 18, title, 30, DMX_COL_TEXT);
    dmx_ui_draw_text_fit((int)m, 58, (int)(L->backend_chip.x - m - 12.0f), subtitle, 16, DMX_COL_TEXT_MUTED);
  }

  Color accent = backend_accent(app);
  Rectangle c = L->backend_chip;
  DrawRectangleRec(c, DMX_COL_FIELD);
  DrawRectangleLinesEx(c, DMX_BORDER_PX, accent);
  DrawText("BACKEND", (int)(c.x + 14.0f), (int)(c.y + 9.0f), 10, DMX_COL_TEXT_MUTED);
  char up[32];
  upper_copy(up, sizeof up, dmx_worker_backend_label(&app->worker_cfg));
  dmx_ui_draw_header((int)(c.x + 14.0f), (int)(c.y + 24.0f), up, 20, accent);
}

static int draw_info(const DmxApp *app, const DmxUiLayout *L, int enabled) {
  dmx_ui_draw_panel(L->info, false);
  const int out_ok = app->output_dir_accepted && app->output_dir[0];
  int hover = enabled && dmx_ui_hovered(L->out_field);
  Color border = out_ok ? Fade(DMX_COL_ACCENT_GREEN, hover ? 1.0f : 0.7f) : DMX_COL_ACCENT_RED;
  DrawRectangleRec(L->out_field, dmx_ui_hover_lift(DMX_COL_FIELD, hover));
  DrawRectangleLinesEx(L->out_field, DMX_BORDER_PX, border);

  for (int i = 0; i < L->n_rows; ++i) {
    const DmxInfoRow *r = &L->rows[i];
    dmx_ui_draw_header((int)(DMX_UI_MARGIN + DMX_UI_INFO_PAD), (int)r->y, r->label, DMX_UI_INFO_FS,
                       i == DMX_INFO_OUTPUT ? border : DMX_COL_TEXT_PLACEHOLDER);
    dmx_ui_text_wrap(r->value, L->info_value_x, r->y, L->info_value_w, DMX_UI_INFO_FS, DMX_UI_INFO_LINE_H, r->col,
                     0, true);
  }
  return hover;
}

static void draw_drop_zone(const DmxApp *app, const DmxUiLayout *L, int busy) {
  const int out_ok = app->output_dir_accepted && app->output_dir[0];
  DrawRectangleRec(L->drop, DMX_COL_PANEL);
  DrawRectangleLinesEx(L->drop, DMX_BORDER_PX, Fade(DMX_COL_ACCENT_BLUE, 0.7f));
#ifdef _WIN32
  const char *busy_hint = "File dialog open - finish or cancel in the picker";
#else
  const char *busy_hint = "File dialog open - finish or cancel in Finder";
#endif
  const char *hint = busy ? busy_hint
                          : (out_ok ? "Drop MP3 / WAV / OGG / FLAC / PSARC (_p or _m) here, then press Start"
                                    : "Drop files here - pick an output folder and press Start");
  const int fs = 18;
  int max_w = (int)L->drop.width - 24;
  if (!busy && MeasureText(hint, fs) > max_w) {
    hint = out_ok ? "Drop audio or PSARC files here" : "Drop files here, then pick an output folder";
  }
  int tw = MeasureText(hint, fs);
  int tx = tw < max_w ? (int)(L->drop.x + (L->drop.width - (float)tw) * 0.5f) : (int)L->drop.x + 12;
  dmx_ui_draw_text_fit(tx, (int)(L->drop.y + (L->drop.height - (float)fs) * 0.5f), max_w, hint, fs,
                       DMX_COL_TEXT_MUTED);
}

typedef struct {
  float x_idx, x_name, x_status, x_prog, x_act;
  float w_idx, w_name, w_status, w_prog, w_act;
} DmxTableCols;

typedef struct {
  Rectangle up, down, dismiss;
} DmxRowActs;

static DmxTableCols table_cols(const DmxUiLayout *L) {
  DmxTableCols c;
  float w = L->body.width;
  c.w_idx = 36.0f;
  c.w_act = 84.0f;
  c.w_status = 100.0f;
  c.w_prog = fminf(180.0f, fmaxf(96.0f, w * 0.20f));
  c.w_name = w - c.w_idx - c.w_status - c.w_prog - c.w_act;
  if (c.w_name < 48.0f) {
    c.w_prog -= 48.0f - c.w_name;
    if (c.w_prog < 64.0f) {
      c.w_prog = 64.0f;
    }
    c.w_name = w - c.w_idx - c.w_status - c.w_prog - c.w_act;
  }
  c.x_idx = L->body.x;
  c.x_name = c.x_idx + c.w_idx;
  c.x_status = c.x_name + c.w_name;
  c.x_prog = c.x_status + c.w_status;
  c.x_act = c.x_prog + c.w_prog;
  return c;
}

static DmxRowActs row_acts(const DmxTableCols *c, float ry) {
  DmxRowActs a;
  const float bw = 24.0f;
  float by = ry + (DMX_UI_ROW_H - bw) * 0.5f;
  float x = c->x_act + 2.0f;
  a.up = (Rectangle){x, by, bw, bw};
  a.down = (Rectangle){x + bw + 4.0f, by, bw, bw};
  a.dismiss = (Rectangle){x + (bw + 4.0f) * 2.0f, by, bw, bw};
  return a;
}

static void draw_chevron(Rectangle r, int down, Color col) {
  float cx = r.x + r.width * 0.5f;
  float left = r.x + 7.0f;
  float right = r.x + r.width - 7.0f;
  float y0 = down ? r.y + 8.0f : r.y + r.height - 8.0f;
  float y1 = down ? r.y + r.height - 8.0f : r.y + 8.0f;
  DrawLineEx((Vector2){left, y0}, (Vector2){cx, y1}, 2.0f, col);
  DrawLineEx((Vector2){cx, y1}, (Vector2){right, y0}, 2.0f, col);
}

static void draw_x_mark(Rectangle r, Color col) {
  float p = 7.0f;
  DrawLineEx((Vector2){r.x + p, r.y + p}, (Vector2){r.x + r.width - p, r.y + r.height - p}, 2.0f, col);
  DrawLineEx((Vector2){r.x + r.width - p, r.y + p}, (Vector2){r.x + p, r.y + r.height - p}, 2.0f, col);
}

/* kind: 0 up, 1 down, 2 remove. Returns 1 when the enabled button is hovered. */
static int draw_row_btn(Rectangle r, int kind, int enabled) {
  int hover = enabled && dmx_ui_hovered(r);
  Color fill = enabled ? dmx_ui_hover_lift(DMX_COL_PANEL_RAISED, hover) : DMX_COL_FIELD;
  Color border = !enabled ? Fade(DMX_COL_BORDER, 0.18f) : (hover ? DMX_COL_HILIGHT : Fade(DMX_COL_BORDER, 0.45f));
  Color icon = !enabled ? DMX_COL_BORDER_DIM : (kind == 2 ? (hover ? WHITE : DMX_COL_ACCENT_RED) : DMX_COL_TEXT);
  DrawRectangleRec(r, fill);
  DrawRectangleLinesEx(r, 1.0f, border);
  if (kind == 2) {
    draw_x_mark(r, icon);
  } else {
    draw_chevron(r, kind == 1, icon);
  }
  return hover;
}

/* Returns hovered row index or -1. *action_hover is 1 when the pointer is on a row button. */
static int draw_jobs_table(const DmxApp *app, const DmxUiLayout *L, int *action_hover) {
  const float m = DMX_UI_MARGIN;
  dmx_ui_draw_header((int)m, (int)L->jobs_y, "JOBS", DMX_UI_JOBS_HDR_FS, DMX_COL_TEXT);
  {
    int running = dmx_app_status_count(app, DMX_JOB_RUNNING) + dmx_app_status_count(app, DMX_JOB_PREPARING);
    int failed = dmx_app_status_count(app, DMX_JOB_ERROR) + dmx_app_status_count(app, DMX_JOB_CANCELLED);
    const char *counts = TextFormat("%d queued   %d running   %d done   %d failed", dmx_app_queued_count(app),
                                    running, dmx_app_status_count(app, DMX_JOB_DONE), failed);
    int hx = (int)m + dmx_ui_measure_header("JOBS", DMX_UI_JOBS_HDR_FS) + 16;
    dmx_ui_draw_text_fit(hx, (int)L->jobs_y + 3, (int)((float)GetScreenWidth() - m - (float)hx), counts, 16,
                         DMX_COL_TEXT_FAINT);
  }

  DmxTableCols c = table_cols(L);
  const int hy = (int)(L->table_head_y + (DMX_UI_TABLE_HEAD_H - (float)DMX_UI_TABLE_HEAD_FS) * 0.5f);
  DrawRectangleRec((Rectangle){m, L->table_head_y, L->body.width + DMX_SBAR_W, DMX_UI_TABLE_HEAD_H},
                   DMX_COL_PANEL_RAISED);
  dmx_ui_draw_header((int)c.x_idx + 8, hy, "#", DMX_UI_TABLE_HEAD_FS, DMX_COL_TEXT_MUTED);
  dmx_ui_draw_header((int)c.x_name + 4, hy, "FILE", DMX_UI_TABLE_HEAD_FS, DMX_COL_TEXT_MUTED);
  dmx_ui_draw_header((int)c.x_status + 4, hy, "STATUS", DMX_UI_TABLE_HEAD_FS, DMX_COL_TEXT_MUTED);
  dmx_ui_draw_header((int)c.x_prog + 4, hy, "PROGRESS", DMX_UI_TABLE_HEAD_FS, DMX_COL_TEXT_MUTED);

  const Rectangle body = L->body;
  DrawRectangleRec((Rectangle){body.x, body.y, body.width + DMX_SBAR_W, body.height}, DMX_COL_BG);
  Vector2 mp = GetMousePosition();
  int hovered = -1;
  if (action_hover) {
    *action_hover = 0;
  }
  int active_row = dmx_queue_active_row(&app->queue);

  BeginScissorMode((int)body.x, (int)body.y, (int)body.width, (int)body.height);
  if (app->queue.count == 0) {
    dmx_ui_draw_text_fit((int)body.x + 14, (int)body.y + 14, (int)body.width - 28,
                         "No jobs yet - add files or drop them above", 18, DMX_COL_TEXT_PLACEHOLDER);
  }
  int first = (int)(g_scroll / DMX_UI_ROW_H);
  int last = first + (int)ceilf(body.height / DMX_UI_ROW_H) + 1;
  if (last > app->queue.count) {
    last = app->queue.count;
  }
  for (int i = first < 0 ? 0 : first; i < last; ++i) {
    const DmxJob *j = dmx_queue_at(&app->queue, i);
    if (!j) {
      continue;
    }
    float ry = body.y + (float)i * DMX_UI_ROW_H - g_scroll;
    Rectangle row = {body.x, ry, body.width, DMX_UI_ROW_H};
    int is_active = (i == active_row);
    int is_hover = !g_sbar_drag && CheckCollisionPointRec(mp, body) && CheckCollisionPointRec(mp, row);
    if (is_hover) {
      hovered = i;
    }
    int odd = (i & 1) != 0;
    Color bg = odd ? DMX_COL_ROW_ODD : DMX_COL_ROW_EVEN;
    if (is_active) {
      bg = odd ? (Color){64, 12, 15, 255} : (Color){54, 10, 12, 255};
    } else if (is_hover) {
      bg = dmx_ui_hover_lift(bg, true);
    }
    DrawRectangleRec(row, bg);
    if (is_active) {
      DrawRectangleLinesEx(row, DMX_BORDER_PX, Fade(DMX_COL_HILIGHT, 0.9f));
    } else if (is_hover) {
      DrawRectangleLinesEx(row, DMX_BORDER_PX, Fade(DMX_COL_HILIGHT, 0.45f));
    }

    Color tc = (is_active || is_hover) ? WHITE : DMX_COL_TEXT;
    const int y1 = (int)ry + 6;
    const int y2 = (int)ry + 28;
    DrawText(TextFormat("%d", i + 1), (int)c.x_idx + 8, y1, DMX_UI_ROW_FS, DMX_COL_TEXT_PLACEHOLDER);
    dmx_ui_draw_text_fit((int)c.x_name + 4, y1, (int)c.w_name - 16, j->display_name, DMX_UI_ROW_FS, tc);
    const int timed = j->started_at > 0.0;
    dmx_ui_draw_text_fit((int)c.x_name + 4, y2, (int)(timed ? c.w_name : c.w_name + c.w_status) - 8, j->message,
                         DMX_UI_ROW_MSG_FS,
                         j->status == DMX_JOB_ERROR ? DMX_COL_ACCENT_RED : DMX_COL_TEXT_PLACEHOLDER);
    dmx_ui_draw_header((int)c.x_status + 4, y1, dmx_job_status_label(j->status), 16, job_status_color(j->status));
    if (timed) {
      const int live_t = j->status == DMX_JOB_RUNNING || j->status == DMX_JOB_PREPARING;
      char t[24];
      dmx_format_duration(live_t ? GetTime() - j->started_at : j->elapsed_s, t, sizeof t);
      dmx_ui_draw_text_fit((int)c.x_status + 4, y2, (int)c.w_status - 8, t, DMX_UI_ROW_MSG_FS,
                           live_t ? DMX_COL_ACCENT_BLUE : DMX_COL_TEXT_PLACEHOLDER);
    }

    const char *pct = TextFormat("%3.0f%%", (double)j->progress_pct);
    int pw = MeasureText("100%", 16);
    Rectangle bar = {c.x_prog + 4.0f, ry + (DMX_UI_ROW_H - 14.0f) * 0.5f, c.w_prog - (float)pw - 20.0f, 14.0f};
    if (bar.width > 12.0f) {
      DrawRectangleRec(bar, DMX_COL_FIELD);
      float fill = (bar.width - 4.0f) * fminf(1.0f, fmaxf(0.0f, j->progress_pct / 100.0f));
      if (fill > 0.0f) {
        Color fc = j->status == DMX_JOB_DONE ? DMX_COL_ACCENT_GREEN : DMX_COL_ACCENT_BLUE;
        DrawRectangleRec((Rectangle){bar.x + 2.0f, bar.y + 2.0f, fill, bar.height - 4.0f}, fc);
      }
      DrawRectangleLinesEx(bar, 1.0f, Fade(DMX_COL_BORDER, 0.4f));
    }
    DrawText(pct, (int)(bar.x + bar.width + 8.0f), (int)(ry + (DMX_UI_ROW_H - 16.0f) * 0.5f), 16, tc);

    DmxRowActs acts = row_acts(&c, ry);
    int up_h = draw_row_btn(acts.up, 0, i > 0);
    int down_h = draw_row_btn(acts.down, 1, i + 1 < app->queue.count);
    int x_h = draw_row_btn(acts.dismiss, 2, 1);
    if (action_hover && (up_h || down_h || x_h)) {
      *action_hover = 1;
    }
  }
  EndScissorMode();

  DrawRectangleLinesEx((Rectangle){body.x, body.y, body.width, body.height}, DMX_BORDER_PX,
                       Fade(DMX_COL_BORDER, 0.4f));

  /* Scrollbar track is always drawn so the table edge stays put; thumb only when needed. */
  int hov_track = CheckCollisionPointRec(mp, L->sbar);
  DrawRectangleRec(L->sbar, DMX_COL_PANEL);
  DrawRectangleLinesEx(L->sbar, 1.0f, Fade(DMX_COL_BORDER, hov_track ? 0.6f : 0.3f));
  if (L->max_scroll > 0.0f) {
    Rectangle th = sbar_thumb(L, app->queue.count);
    int hot = g_sbar_drag || CheckCollisionPointRec(mp, th);
    DrawRectangleRec(th, hot ? DMX_COL_HILIGHT : Fade(DMX_COL_HILIGHT, 0.85f));
    if (hot) {
      DrawRectangleLinesEx(th, 1.0f, WHITE);
    }
  }
  return hovered;
}

void dmx_ui_draw(DmxApp *app, int screen_w, int screen_h) {
  ClearBackground(DMX_COL_BG);

  DmxUiLayout L = dmx_ui_layout(app, screen_w, screen_h);
  update_scroll(app, &L);

  const int out_ok = app->output_dir_accepted && app->output_dir[0];
  const int queued = dmx_app_queued_count(app);
  const int start_ready = out_ok && queued > 0;
  const int busy = dlg_busy();

  draw_header(app, &L);

  int any_hover = 0;
  any_hover |= dmx_ui_draw_btn(L.add_btn, "Add files...", DMX_UI_BTN_FS, true, !busy, DMX_COL_ACCENT_BLUE);
  any_hover |= dmx_ui_draw_btn(L.out_btn, "Output folder...", DMX_UI_BTN_FS, !out_ok, !busy, DMX_COL_ACCENT_YELLOW);
  any_hover |= dmx_ui_draw_btn(L.fmt_btn, fmt_label(app), DMX_UI_BTN_FS, false, 1, DMX_COL_HILIGHT);
  if (L.dev_btn.width > 0.0f) {
    any_hover |= dmx_ui_draw_btn(L.dev_btn, dev_label(app), DMX_UI_BTN_FS, app->gpu_error, 1,
                                 app->gpu_error ? DMX_COL_ACCENT_RED : DMX_COL_HILIGHT);
  }
  if (app->processing_enabled) {
    any_hover |= dmx_ui_draw_btn(L.start_stop_btn, "Stop", DMX_UI_BTN_FS, true, 1, DMX_COL_ACCENT_RED);
  } else {
    any_hover |=
        dmx_ui_draw_btn(L.start_stop_btn, "Start", DMX_UI_BTN_FS, start_ready, 1, DMX_COL_ACCENT_DARK_GREEN);
  }

  any_hover |= draw_info(app, &L, !busy);
  draw_drop_zone(app, &L, busy);
  int action_hover = 0;
  int hovered_row = draw_jobs_table(app, &L, &action_hover);
  any_hover |= action_hover;
  if (L.max_scroll > 0.0f && dmx_ui_hovered(L.sbar)) {
    any_hover = 1;
  }

  if (busy) {
    /* Main loop is alive; force a normal arrow (no beach-ball wait cursor). */
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  } else {
    SetMouseCursor(any_hover ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
  }

  /* Footer: hovered job details, else the app status line. */
  const char *ver = TextFormat("v%s", DMX_APP_VERSION);
  int vw = MeasureText(ver, 14);
  char footer[DMX_PATH_MAX + DMX_MSG_MAX + 8];
  Color fc = DMX_COL_TEXT_MUTED;
  const DmxJob *hovered_job = hovered_row >= 0 ? dmx_queue_at(&app->queue, hovered_row) : NULL;
  if (hovered_job) {
    snprintf(footer, sizeof footer, "%s - %s", hovered_job->input_path, hovered_job->message);
    fc = hovered_job->status == DMX_JOB_ERROR ? DMX_COL_ACCENT_RED : DMX_COL_TEXT_FAINT;
  } else {
    snprintf(footer, sizeof footer, "%s", app->status_line);
  }
  DrawRectangle(0, (int)L.footer_y - 4, screen_w, screen_h - (int)L.footer_y + 4, DMX_COL_PANEL);
  DrawRectangle(0, (int)L.footer_y - 4, screen_w, (int)DMX_BORDER_PX, Fade(DMX_COL_BORDER, 0.25f));
  dmx_ui_text_wrap(footer, DMX_UI_MARGIN, L.footer_y + 4.0f, (float)screen_w - DMX_UI_MARGIN * 2.0f - (float)vw - 16.0f,
                   DMX_UI_FOOTER_FS, DMX_UI_FOOTER_LINE_H, fc, DMX_UI_FOOTER_LINES, true);
  DrawText(ver, screen_w - vw - (int)DMX_UI_MARGIN, (int)L.footer_y + 6, 14, DMX_COL_TEXT_PLACEHOLDER);
}

int dmx_ui_handle_click(DmxApp *app, int x, int y, int screen_w, int screen_h) {
  DmxUiLayout L = dmx_ui_layout(app, screen_w, screen_h);

  if (point_in(x, y, L.add_btn)) {
    if (!dlg_busy()) {
      begin_dialog(app, DMX_DLG_FILES);
    }
    return 1;
  }
  if (point_in(x, y, L.out_btn) || point_in(x, y, L.out_field)) {
    if (!dlg_busy()) {
      begin_dialog(app, DMX_DLG_FOLDER);
    }
    return 1;
  }
  if (point_in(x, y, L.fmt_btn)) {
    app->write_mp3 = !app->write_mp3;
    dmx_app_apply_settings(app);
    return 1;
  }
  if (L.dev_btn.width > 0.0f && point_in(x, y, L.dev_btn)) {
    dmx_app_set_use_cpu(app, !app->use_cpu);
    return 1;
  }
  if (point_in(x, y, L.body)) {
    float rel = (float)y - L.body.y + g_scroll;
    int row = (int)(rel / DMX_UI_ROW_H);
    if (row >= 0 && row < app->queue.count) {
      float ry = L.body.y + (float)row * DMX_UI_ROW_H - g_scroll;
      DmxTableCols cols = table_cols(&L);
      DmxRowActs acts = row_acts(&cols, ry);
      if (point_in(x, y, acts.up)) {
        dmx_app_move_job(app, row, -1);
        return 1;
      }
      if (point_in(x, y, acts.down)) {
        dmx_app_move_job(app, row, 1);
        return 1;
      }
      if (point_in(x, y, acts.dismiss)) {
        dmx_app_remove_job(app, row);
        return 1;
      }
    }
  }
  if (point_in(x, y, L.start_stop_btn)) {
    if (app->processing_enabled) {
      dmx_app_stop(app);
    } else {
      dmx_app_start(app);
    }
    return 1;
  }
  if (L.max_scroll > 0.0f && point_in(x, y, L.sbar)) {
    Rectangle th = sbar_thumb(&L, app->queue.count);
    if (point_in(x, y, th)) {
      g_sbar_drag = 1;
    } else if ((float)y < th.y) {
      g_scroll -= L.body.height * 0.85f;
    } else {
      g_scroll += L.body.height * 0.85f;
    }
    clamp_scroll(&L);
    return 1;
  }
  return 0;
}
