#ifndef DMX_UI_THEME_H
#define DMX_UI_THEME_H

/* Pixel theme shared with RockNRoller (rnr_theme): black background, sharp corners,
 * chunky 2px borders, vivid accents. */

#include "raylib.h"

#include <stdbool.h>

#define DMX_BORDER_PX 2.0f
/* Bottom lip; the button face sits on it idle and drops onto it when held. */
#define DMX_BTN_LIP_PX 3.0f
#define DMX_SBAR_W 11.0f

#define DMX_COL_BG ((Color){0, 0, 0, 255})
#define DMX_COL_PANEL ((Color){14, 15, 19, 255})
#define DMX_COL_PANEL_RAISED ((Color){22, 24, 30, 255})
#define DMX_COL_FIELD ((Color){10, 11, 14, 255})
#define DMX_COL_BORDER ((Color){235, 236, 240, 255})
#define DMX_COL_BORDER_DIM ((Color){70, 72, 80, 255})
#define DMX_COL_HILIGHT ((Color){236, 32, 40, 255})
#define DMX_COL_ROW_EVEN ((Color){6, 6, 8, 255})
#define DMX_COL_ROW_ODD ((Color){16, 17, 22, 255})

#define DMX_COL_ACCENT_YELLOW ((Color){255, 205, 0, 255})
#define DMX_COL_ACCENT_BLUE ((Color){36, 120, 255, 255})
#define DMX_COL_ACCENT_LIGHT_BLUE ((Color){120, 190, 255, 255})
#define DMX_COL_ACCENT_GREEN ((Color){40, 235, 90, 255})
#define DMX_COL_ACCENT_DARK_GREEN ((Color){24, 150, 60, 255})
#define DMX_COL_ACCENT_RED ((Color){236, 32, 40, 255})
#define DMX_COL_ACCENT_ORANGE ((Color){255, 115, 0, 255})

#define DMX_COL_TEXT ((Color){235, 236, 240, 255})
#define DMX_COL_TEXT_MUTED ((Color){220, 222, 228, 255})
#define DMX_COL_TEXT_FAINT ((Color){200, 202, 210, 255})
#define DMX_COL_TEXT_PLACEHOLDER ((Color){158, 160, 168, 255})

Color dmx_ui_hover_lift(Color base, bool hovered);
bool dmx_ui_hovered(Rectangle r);

/* Filled sharp rect + 2px border. raised = lighter fill for popups. */
void dmx_ui_draw_panel(Rectangle r, bool raised);
/* Input-like surface with a caller-chosen border. */
void dmx_ui_draw_field(Rectangle r, Color border);

/* Chunky button: outlined when idle, accent fill when active. Returns hover (enabled only). */
bool dmx_ui_draw_btn(Rectangle r, const char *label, int fs, bool active, bool enabled, Color accent);

/* Faux-bold header: text drawn twice with a 1px x offset. Pass ALL-CAPS text. */
void dmx_ui_draw_header(int x, int y, const char *text, int fs, Color col);
int dmx_ui_measure_header(const char *text, int fs);

/* Single line clipped to max_w with a trailing "...". */
void dmx_ui_draw_text_fit(int x, int y, int max_w, const char *text, int fs, Color col);

/* Greedy wrap into max_w, preferring breaks after '/', '\\', ' ', '-', '_' so paths
 * split at folder boundaries. max_lines <= 0 means unlimited; the last allowed line
 * is ellipsized. Returns the line count; draw=false only measures. */
int dmx_ui_text_wrap(const char *text, float x, float y, float max_w, int fs, float line_h, Color col,
                     int max_lines, bool draw);

#endif
