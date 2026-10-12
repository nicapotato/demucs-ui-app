#include "ui_theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define DMX_UI_LINE_MAX 1200

Color dmx_ui_hover_lift(Color base, bool hovered) {
  if (!hovered) {
    return base;
  }
  return (Color){(unsigned char)fminf(255.0f, (float)base.r + 12.0f),
                 (unsigned char)fminf(255.0f, (float)base.g + 12.0f),
                 (unsigned char)fminf(255.0f, (float)base.b + 14.0f), base.a};
}

bool dmx_ui_hovered(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

void dmx_ui_draw_panel(Rectangle r, bool raised) {
  DrawRectangleRec(r, raised ? DMX_COL_PANEL_RAISED : DMX_COL_PANEL);
  DrawRectangleLinesEx(r, DMX_BORDER_PX, Fade(DMX_COL_BORDER, raised ? 0.9f : 0.55f));
}

void dmx_ui_draw_field(Rectangle r, Color border) {
  DrawRectangleRec(r, DMX_COL_FIELD);
  DrawRectangleLinesEx(r, DMX_BORDER_PX, border);
}

static Color scale_rgb(Color c, int pct) {
  return (Color){(unsigned char)((int)c.r * pct / 100), (unsigned char)((int)c.g * pct / 100),
                 (unsigned char)((int)c.b * pct / 100), c.a};
}

bool dmx_ui_draw_btn(Rectangle r, const char *label, int fs, bool active, bool enabled, Color accent) {
  bool hovered = enabled && dmx_ui_hovered(r);
  bool held = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  Rectangle face = r;
  if (r.height > DMX_BTN_LIP_PX + 4.0f) {
    face.height = r.height - DMX_BTN_LIP_PX;
    if (held) {
      face.y += DMX_BTN_LIP_PX;
    }
  }

  Color fill;
  Color border;
  if (!enabled) {
    fill = (Color){10, 11, 14, 200};
    border = Fade(DMX_COL_BORDER, 0.18f);
  } else if (active) {
    fill = dmx_ui_hover_lift(accent, hovered && !held);
    if (held) {
      fill = scale_rgb(fill, 85);
    }
    border = Fade(accent, hovered ? 1.0f : 0.9f);
  } else {
    fill = dmx_ui_hover_lift(DMX_COL_PANEL_RAISED, hovered && !held);
    if (held) {
      fill = scale_rgb(fill, 80);
    }
    border = hovered ? Fade(accent, 0.9f) : Fade(DMX_COL_BORDER, 0.4f);
  }

  if (face.height < r.height) {
    Color lip = enabled ? scale_rgb(fill, 40) : (Color){6, 7, 9, 220};
    DrawRectangle((int)r.x, (int)(r.y + r.height - DMX_BTN_LIP_PX), (int)r.width, (int)DMX_BTN_LIP_PX, lip);
  }
  DrawRectangleRec(face, fill);
  DrawRectangleLinesEx(face, DMX_BORDER_PX, border);

  if (label && label[0]) {
    int use_fs = fs;
    const int max_tw = (int)face.width - 8;
    while (use_fs > 10 && MeasureText(label, use_fs) > max_tw) {
      use_fs--;
    }
    int tw = MeasureText(label, use_fs);
    int tx = (int)(face.x + (face.width - (float)tw) * 0.5f);
    int ty = (int)(face.y + (face.height - (float)use_fs) * 0.5f);
    Color tc = DMX_COL_TEXT;
    if (!enabled) {
      tc = DMX_COL_TEXT_PLACEHOLDER;
    } else if (active) {
      int lum = (int)accent.r + (int)accent.g + (int)accent.b;
      tc = lum >= 540 ? (Color){10, 11, 14, 255} : WHITE;
    } else if (hovered) {
      tc = WHITE;
    }
    DrawText(label, tx, ty, use_fs, tc);
  }
  return hovered;
}

void dmx_ui_draw_header(int x, int y, const char *text, int fs, Color col) {
  DrawText(text, x + 1, y, fs, col);
  DrawText(text, x, y, fs, col);
}

int dmx_ui_measure_header(const char *text, int fs) { return MeasureText(text, fs) + 1; }

static int measure_prefix(const char *text, size_t n, int fs, char *buf) {
  memcpy(buf, text, n);
  buf[n] = '\0';
  return MeasureText(buf, fs);
}

/* Largest k <= n with text[0..k) no wider than max_w (at least 1 when n > 0). */
static size_t fit_prefix(const char *text, size_t n, int fs, float max_w, char *buf) {
  if ((float)measure_prefix(text, n, fs, buf) <= max_w) {
    return n;
  }
  size_t lo = 1;
  size_t hi = n;
  while (lo < hi) {
    size_t mid = (lo + hi + 1) / 2;
    if ((float)measure_prefix(text, mid, fs, buf) <= max_w) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  return lo;
}

/* Writes the ellipsized prefix of text[0..n) that fits max_w into buf. */
static void ellipsize(const char *text, size_t n, int fs, float max_w, char *buf) {
  const char *el = "...";
  float avail = max_w - (float)MeasureText(el, fs);
  size_t k = avail > 0.0f ? fit_prefix(text, n, fs, avail, buf) : 0;
  while (k > 0 && (float)measure_prefix(text, k, fs, buf) > avail) {
    k--;
  }
  memcpy(buf, text, k);
  memcpy(buf + k, el, 4);
}

void dmx_ui_draw_text_fit(int x, int y, int max_w, const char *text, int fs, Color col) {
  if (!text || !text[0] || max_w < 8) {
    return;
  }
  char buf[DMX_UI_LINE_MAX + 4];
  size_t n = strlen(text);
  if (n > DMX_UI_LINE_MAX) {
    n = DMX_UI_LINE_MAX;
  }
  if ((float)measure_prefix(text, n, fs, buf) > (float)max_w) {
    ellipsize(text, n, fs, (float)max_w, buf);
  }
  DrawText(buf, x, y, fs, col);
}

static int is_break_char(char c) { return c == '/' || c == '\\' || c == ' ' || c == '-' || c == '_'; }

int dmx_ui_text_wrap(const char *text, float x, float y, float max_w, int fs, float line_h, Color col,
                     int max_lines, bool draw) {
  if (!text || !text[0] || max_w < 8.0f) {
    return 0;
  }
  char buf[DMX_UI_LINE_MAX + 4];
  const char *p = text;
  int lines = 0;
  while (*p) {
    size_t rem = strlen(p);
    if (rem > DMX_UI_LINE_MAX) {
      rem = DMX_UI_LINE_MAX;
    }
    size_t take = fit_prefix(p, rem, fs, max_w, buf);
    if (take < rem) {
      for (size_t k = take; k > take / 3 && k > 0; k--) {
        if (is_break_char(p[k - 1])) {
          take = k;
          break;
        }
      }
    }
    const bool last = max_lines > 0 && lines + 1 == max_lines && p[take] != '\0';
    if (draw) {
      if (last) {
        ellipsize(p, rem, fs, max_w, buf);
      } else {
        memcpy(buf, p, take);
        buf[take] = '\0';
      }
      DrawText(buf, (int)x, (int)(y + (float)lines * line_h), fs, col);
    }
    lines++;
    if (last) {
      break;
    }
    p += take;
    while (*p == ' ') {
      p++;
    }
  }
  return lines;
}
