#ifndef DMX_JOB_QUEUE_H
#define DMX_JOB_QUEUE_H

#include <stddef.h>

#define DMX_PATH_MAX 1024
#define DMX_JOB_MAX 64
#define DMX_MSG_MAX 512

typedef enum {
  DMX_JOB_QUEUED = 0,
  DMX_JOB_PREPARING,
  DMX_JOB_RUNNING,
  DMX_JOB_DONE,
  DMX_JOB_ERROR,
  DMX_JOB_CANCELLED,
} DmxJobStatus;

typedef struct {
  char input_path[DMX_PATH_MAX];
  char display_name[256];
  char track_name[256]; /* output subfolder name */
  char prepared_audio[DMX_PATH_MAX]; /* wav fed to worker; may equal input */
  int is_psarc;
  int owns_temp_audio; /* unlink prepared_audio when done */
  DmxJobStatus status;
  float progress_pct;
  char message[DMX_MSG_MAX];
  char out_dir[DMX_PATH_MAX];
  int remove_when_done; /* drop this row after the worker thread finishes */
  double started_at;    /* GetTime() when the worker thread was spawned; 0 = never ran */
  double elapsed_s;     /* set when the worker thread is joined; while running, now - started_at */
} DmxJob;

typedef struct {
  DmxJob jobs[DMX_JOB_MAX];
  int slot_used[DMX_JOB_MAX];
  int order[DMX_JOB_MAX]; /* display order; values are stable slot indexes */
  int count;
  int active_slot; /* stable slot of the running job, or -1 */
} DmxJobQueue;

void dmx_queue_init(DmxJobQueue *q);
int dmx_queue_add(DmxJobQueue *q, const char *path);
/* Next DMX_JOB_QUEUED job in display order. *out_slot is the stable slot. */
DmxJob *dmx_queue_next_queued(DmxJobQueue *q, int *out_slot);
const DmxJob *dmx_queue_at(const DmxJobQueue *q, int row);
DmxJob *dmx_queue_at_mut(DmxJobQueue *q, int row);
int dmx_queue_active_row(const DmxJobQueue *q);
/* Swap with the neighbor. delta is -1 or +1. Returns 0, or -1 if it cannot move. */
int dmx_queue_move(DmxJobQueue *q, int row, int delta);
/* 0 = removed now, 1 = active job marked to drop after cancel, -1 = bad row. */
int dmx_queue_remove(DmxJobQueue *q, int row);
void dmx_queue_drop_slot(DmxJobQueue *q, int slot);
const char *dmx_job_status_label(DmxJobStatus s);
/* "0:42", "12:05", "1:02:03"; empty for negative. */
void dmx_format_duration(double seconds, char *out, size_t out_sz);

#endif
