#include "job_queue.h"

#include <stdio.h>
#include <string.h>

void dmx_queue_init(DmxJobQueue *q) {
  memset(q, 0, sizeof *q);
  q->active_slot = -1;
}

static void basename_no_ext(const char *path, char *out, size_t out_sz, const char *drop_ext) {
  const char *base = path;
  for (const char *p = path; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      base = p + 1;
    }
  }
  snprintf(out, out_sz, "%s", base);
  size_t n = strlen(out);
  size_t el = strlen(drop_ext);
  if (n > el) {
    for (size_t i = 0; i < el; ++i) {
      char a = out[n - el + i];
      char b = drop_ext[i];
      if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
      }
      if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
      }
      if (a != b) {
        return;
      }
    }
    out[n - el] = '\0';
  }
}

int dmx_queue_add(DmxJobQueue *q, const char *path) {
  if (!q || !path || !path[0] || q->count >= DMX_JOB_MAX) {
    return -1;
  }
  int slot = -1;
  for (int i = 0; i < DMX_JOB_MAX; ++i) {
    if (!q->slot_used[i]) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    return -1;
  }
  DmxJob *j = &q->jobs[slot];
  memset(j, 0, sizeof *j);
  q->slot_used[slot] = 1;
  q->order[q->count++] = slot;
  snprintf(j->input_path, sizeof j->input_path, "%s", path);

  const char *dot = strrchr(path, '.');
  j->is_psarc = (dot && (strcmp(dot, ".psarc") == 0 || strcmp(dot, ".PSARC") == 0));
  if (j->is_psarc) {
    basename_no_ext(path, j->track_name, sizeof j->track_name, ".psarc");
  } else {
    basename_no_ext(path, j->track_name, sizeof j->track_name, "");
    /* strip last extension */
    char *d = strrchr(j->track_name, '.');
    if (d) {
      *d = '\0';
    }
  }
  snprintf(j->display_name, sizeof j->display_name, "%s", j->track_name);
  j->status = DMX_JOB_QUEUED;
  snprintf(j->message, sizeof j->message, "Queued");
  return 0;
}

DmxJob *dmx_queue_next_queued(DmxJobQueue *q, int *out_slot) {
  if (!q) {
    return NULL;
  }
  for (int row = 0; row < q->count; ++row) {
    int slot = q->order[row];
    if (q->jobs[slot].status == DMX_JOB_QUEUED && !q->jobs[slot].remove_when_done) {
      if (out_slot) {
        *out_slot = slot;
      }
      return &q->jobs[slot];
    }
  }
  return NULL;
}

const DmxJob *dmx_queue_at(const DmxJobQueue *q, int row) {
  if (!q || row < 0 || row >= q->count) {
    return NULL;
  }
  return &q->jobs[q->order[row]];
}

DmxJob *dmx_queue_at_mut(DmxJobQueue *q, int row) { return (DmxJob *)dmx_queue_at(q, row); }

int dmx_queue_active_row(const DmxJobQueue *q) {
  if (!q || q->active_slot < 0) {
    return -1;
  }
  for (int row = 0; row < q->count; ++row) {
    if (q->order[row] == q->active_slot) {
      return row;
    }
  }
  return -1;
}

int dmx_queue_move(DmxJobQueue *q, int row, int delta) {
  if (!q || (delta != -1 && delta != 1)) {
    return -1;
  }
  int dst = row + delta;
  if (row < 0 || row >= q->count || dst < 0 || dst >= q->count) {
    return -1;
  }
  int tmp = q->order[row];
  q->order[row] = q->order[dst];
  q->order[dst] = tmp;
  return 0;
}

void dmx_queue_drop_slot(DmxJobQueue *q, int slot) {
  if (!q || slot < 0 || slot >= DMX_JOB_MAX || !q->slot_used[slot]) {
    return;
  }
  int row = -1;
  for (int i = 0; i < q->count; ++i) {
    if (q->order[i] == slot) {
      row = i;
      break;
    }
  }
  if (row >= 0) {
    memmove(&q->order[row], &q->order[row + 1], (size_t)(q->count - row - 1) * sizeof q->order[0]);
    q->count--;
  }
  if (q->active_slot == slot) {
    q->active_slot = -1;
  }
  q->slot_used[slot] = 0;
  memset(&q->jobs[slot], 0, sizeof q->jobs[slot]);
}

int dmx_queue_remove(DmxJobQueue *q, int row) {
  if (!q || row < 0 || row >= q->count) {
    return -1;
  }
  int slot = q->order[row];
  if (slot == q->active_slot) {
    q->jobs[slot].remove_when_done = 1;
    return 1;
  }
  dmx_queue_drop_slot(q, slot);
  return 0;
}

void dmx_format_duration(double seconds, char *out, size_t out_sz) {
  if (!out || out_sz == 0) {
    return;
  }
  if (seconds < 0.0) {
    out[0] = '\0';
    return;
  }
  long t = (long)(seconds + 0.5);
  long h = t / 3600;
  long m = (t / 60) % 60;
  long s = t % 60;
  if (h > 0) {
    snprintf(out, out_sz, "%ld:%02ld:%02ld", h, m, s);
  } else {
    snprintf(out, out_sz, "%ld:%02ld", m, s);
  }
}

const char *dmx_job_status_label(DmxJobStatus s) {
  switch (s) {
  case DMX_JOB_QUEUED:
    return "Queued";
  case DMX_JOB_PREPARING:
    return "Preparing";
  case DMX_JOB_RUNNING:
    return "Running";
  case DMX_JOB_DONE:
    return "Done";
  case DMX_JOB_ERROR:
    return "Error";
  case DMX_JOB_CANCELLED:
    return "Cancelled";
  }
  return "?";
}
