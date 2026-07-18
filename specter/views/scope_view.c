#include "scope_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>

#define PLOT_TOP 14
#define PLOT_BOTTOM 63
#define PLOT_H (PLOT_BOTTOM - PLOT_TOP)
#define SCREEN_W 128
#define BIN_W (SCREEN_W / SPECTER_BINS)

// Visible dBm window: fixed span, floor pannable with Up/Down.
#define DBM_SPAN 90.0f
#define DBM_FLOOR_MIN -120
#define DBM_FLOOR_MAX -30
#define DBM_FLOOR_STEP 5
#define DBM_FLOOR_DEFAULT -100

struct SpecterScopeView {
    View* view;
    SpecterScopeWindowCallback on_window;
    void* window_ctx;
};

typedef struct {
    // Band limits for clamping the pan.
    uint32_t band_min_hz;
    uint32_t band_max_hz;
    SpecterDecay decay;

    // Current window (view is source of truth).
    uint32_t start_hz;
    uint32_t span_hz;
    SpecterTier tier;

    // Latest instantaneous readings + decayed peak-hold ("afterglow").
    float rssi[SPECTER_BINS];
    bool valid[SPECTER_BINS];
    float peak[SPECTER_BINS];

    int dbm_floor;
    uint32_t sweep_ms;
    bool have_data;

    // Diagnostic state (surfaced while have_data == false).
    bool dbg_started;
    bool dbg_snapshot;
    uint32_t dbg_sweeps;
    uint32_t dbg_res_start;
    uint32_t dbg_res_span;
} SpecterScopeModel;

static uint32_t scope_tier_span(SpecterTier tier) {
    switch(tier) {
    case SpecterTierCoarse:
        return SPECTER_TIER_COARSE_SPAN_HZ;
    case SpecterTierMid:
        return SPECTER_TIER_MID_SPAN_HZ;
    case SpecterTierFine:
    default:
        return SPECTER_TIER_FINE_SPAN_HZ;
    }
}

static char scope_tier_letter(SpecterTier tier) {
    switch(tier) {
    case SpecterTierCoarse:
        return 'C';
    case SpecterTierMid:
        return 'M';
    case SpecterTierFine:
    default:
        return 'F';
    }
}

// Clamp window so [start, start+span] stays within the band.
static void scope_clamp_window(SpecterScopeModel* m) {
    uint32_t max_start = (m->band_max_hz > m->span_hz) ? (m->band_max_hz - m->span_hz) : 0;
    if(m->start_hz < m->band_min_hz) m->start_hz = m->band_min_hz;
    if(m->start_hz > max_start) m->start_hz = max_start;
}

// Reset instantaneous + peak buffers (after a window change).
static void scope_reset_data(SpecterScopeModel* m) {
    for(uint32_t i = 0; i < SPECTER_BINS; i++) {
        m->rssi[i] = (float)DBM_FLOOR_MIN;
        m->peak[i] = (float)DBM_FLOOR_MIN;
        m->valid[i] = false;
    }
    m->have_data = false;
}

static int scope_dbm_to_y(const SpecterScopeModel* m, float dbm) {
    float frac = (dbm - (float)m->dbm_floor) / DBM_SPAN;
    if(frac < 0.0f) frac = 0.0f;
    if(frac > 1.0f) frac = 1.0f;
    return PLOT_BOTTOM - (int)(frac * PLOT_H);
}

static void scope_view_draw(Canvas* canvas, void* ctx) {
    SpecterScopeModel* m = ctx;
    canvas_clear(canvas);

    // Header: window center, span, tier, sweep time.
    canvas_set_font(canvas, FontSecondary);
    uint32_t center = m->start_hz + m->span_hz / 2;
    char head[40];
    snprintf(
        head,
        sizeof(head),
        "%lu.%02lu %c  %lums",
        (unsigned long)(center / 1000000UL),
        (unsigned long)((center % 1000000UL) / 10000UL),
        scope_tier_letter(m->tier),
        (unsigned long)m->sweep_ms);
    canvas_draw_str(canvas, 0, 10, head);

    // dBm floor marker at bottom-right.
    char floorlbl[16];
    snprintf(floorlbl, sizeof(floorlbl), "%d", m->dbm_floor);
    canvas_draw_str_aligned(canvas, SCREEN_W, 10, AlignRight, AlignBottom, floorlbl);

    // Baseline of the plot.
    canvas_draw_line(canvas, 0, PLOT_BOTTOM, SCREEN_W - 1, PLOT_BOTTOM);

    if(!m->have_data) {
        canvas_draw_str_aligned(
            canvas, 64, PLOT_TOP + 6, AlignCenter, AlignCenter, "scanning...");
        // Diagnostic: started? snapshots arriving? sweep count? window match?
        char d1[40];
        snprintf(
            d1,
            sizeof(d1),
            "start:%d snap:%d sw:%lu",
            m->dbg_started ? 1 : 0,
            m->dbg_snapshot ? 1 : 0,
            (unsigned long)m->dbg_sweeps);
        canvas_draw_str_aligned(canvas, 64, PLOT_TOP + 20, AlignCenter, AlignCenter, d1);
        char d2[48];
        snprintf(
            d2,
            sizeof(d2),
            "res %lu/%lu  win %lu/%lu",
            (unsigned long)(m->dbg_res_start / 1000000UL),
            (unsigned long)(m->dbg_res_span / 1000000UL),
            (unsigned long)(m->start_hz / 1000000UL),
            (unsigned long)(m->span_hz / 1000000UL));
        canvas_draw_str_aligned(canvas, 64, PLOT_TOP + 32, AlignCenter, AlignCenter, d2);
        return;
    }

    // Bars (instantaneous) + peak-hold dots (persistence afterglow).
    for(uint32_t i = 0; i < SPECTER_BINS; i++) {
        if(!m->valid[i]) continue;
        int x = (int)(i * BIN_W);
        int w = (BIN_W > 1) ? BIN_W - 1 : 1;

        int y = scope_dbm_to_y(m, m->rssi[i]);
        int h = PLOT_BOTTOM - y;
        if(h > 0) canvas_draw_box(canvas, x, y, w, h);

        if(m->decay != SpecterDecayOff) {
            int py = scope_dbm_to_y(m, m->peak[i]);
            if(py < y) canvas_draw_line(canvas, x, py, x + w - 1, py);
        }
    }
}

static bool scope_view_input(InputEvent* event, void* ctx) {
    SpecterScopeView* scope = ctx;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool window_changed = false;
    uint32_t new_start = 0, new_span = 0;
    bool consumed = false;

    with_view_model(
        scope->view,
        SpecterScopeModel * m,
        {
            uint32_t pan_step = m->span_hz / 4;
            if(event->key == InputKeyLeft) {
                m->start_hz = (m->start_hz > pan_step) ? m->start_hz - pan_step : 0;
                scope_clamp_window(m);
                scope_reset_data(m);
                consumed = true;
                window_changed = true;
            } else if(event->key == InputKeyRight) {
                m->start_hz += pan_step;
                scope_clamp_window(m);
                scope_reset_data(m);
                consumed = true;
                window_changed = true;
            } else if(event->key == InputKeyOk) {
                // Cycle tier, keeping the same center frequency.
                uint32_t center = m->start_hz + m->span_hz / 2;
                m->tier = (m->tier + 1) % 3;
                m->span_hz = scope_tier_span(m->tier);
                m->start_hz = (center > m->span_hz / 2) ? center - m->span_hz / 2 : 0;
                scope_clamp_window(m);
                scope_reset_data(m);
                consumed = true;
                window_changed = true;
            } else if(event->key == InputKeyUp) {
                if(m->dbm_floor + DBM_FLOOR_STEP <= DBM_FLOOR_MAX)
                    m->dbm_floor += DBM_FLOOR_STEP;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(m->dbm_floor - DBM_FLOOR_STEP >= DBM_FLOOR_MIN)
                    m->dbm_floor -= DBM_FLOOR_STEP;
                consumed = true;
            }
            new_start = m->start_hz;
            new_span = m->span_hz;
        },
        consumed);

    if(window_changed && scope->on_window) {
        scope->on_window(scope->window_ctx, new_start, new_span);
    }
    return consumed;
}

SpecterScopeView* scope_view_alloc(void) {
    SpecterScopeView* scope = malloc(sizeof(SpecterScopeView));
    scope->on_window = NULL;
    scope->window_ctx = NULL;
    scope->view = view_alloc();
    view_allocate_model(scope->view, ViewModelTypeLocking, sizeof(SpecterScopeModel));
    view_set_context(scope->view, scope);
    view_set_draw_callback(scope->view, scope_view_draw);
    view_set_input_callback(scope->view, scope_view_input);
    return scope;
}

void scope_view_free(SpecterScopeView* scope) {
    furi_assert(scope);
    view_free(scope->view);
    free(scope);
}

View* scope_view_get_view(SpecterScopeView* scope) {
    furi_assert(scope);
    return scope->view;
}

void scope_view_configure(SpecterScopeView* scope, const SpecterBand* band, SpecterDecay decay) {
    furi_assert(scope);
    with_view_model(
        scope->view,
        SpecterScopeModel * m,
        {
            m->band_min_hz = specter_band_min_hz(band);
            m->band_max_hz = specter_band_max_hz(band);
            m->decay = decay;
            m->tier = SpecterTierCoarse;
            m->span_hz = scope_tier_span(m->tier);
            m->start_hz = m->band_min_hz;
            m->dbm_floor = DBM_FLOOR_DEFAULT;
            m->sweep_ms = 0;
            scope_clamp_window(m);
            scope_reset_data(m);
        },
        true);
}

void scope_view_get_window(SpecterScopeView* scope, uint32_t* start_hz, uint32_t* span_hz) {
    furi_assert(scope);
    with_view_model(
        scope->view,
        SpecterScopeModel * m,
        {
            *start_hz = m->start_hz;
            *span_hz = m->span_hz;
        },
        false);
}

void scope_view_set_window_callback(
    SpecterScopeView* scope,
    SpecterScopeWindowCallback cb,
    void* ctx) {
    furi_assert(scope);
    scope->on_window = cb;
    scope->window_ctx = ctx;
}

void scope_view_set_started(SpecterScopeView* scope, bool started) {
    furi_assert(scope);
    with_view_model(
        scope->view, SpecterScopeModel * m, { m->dbg_started = started; }, true);
}

void scope_view_set_debug(
    SpecterScopeView* scope,
    bool snapshot_ok,
    uint32_t sweep_count,
    uint32_t res_start_hz,
    uint32_t res_span_hz) {
    furi_assert(scope);
    with_view_model(
        scope->view,
        SpecterScopeModel * m,
        {
            m->dbg_snapshot = snapshot_ok;
            m->dbg_sweeps = sweep_count;
            m->dbg_res_start = res_start_hz;
            m->dbg_res_span = res_span_hz;
        },
        true);
}

void scope_view_push_data(SpecterScopeView* scope, const SpecterScanResult* result) {
    furi_assert(scope);
    with_view_model(
        scope->view,
        SpecterScopeModel * m,
        {
            // Ignore data scanned at a window we've since moved away from.
            if(result->start_hz == m->start_hz && result->span_hz == m->span_hz) {
                float decay = specter_decay_rate(m->decay);
                for(uint32_t i = 0; i < SPECTER_BINS; i++) {
                    m->valid[i] = result->valid[i];
                    if(!result->valid[i]) continue;
                    m->rssi[i] = result->rssi[i];
                    // Peak-hold: jump up to a new peak, else decay toward it.
                    if(result->rssi[i] >= m->peak[i]) {
                        m->peak[i] = result->rssi[i];
                    } else if(decay > 0.0f) {
                        m->peak[i] -= decay;
                        if(m->peak[i] < m->rssi[i]) m->peak[i] = m->rssi[i];
                    } else {
                        m->peak[i] = m->rssi[i];
                    }
                }
                m->sweep_ms = result->sweep_ms;
                m->have_data = true;
            }
        },
        true);
}
