#include "diff_view.h"
#include "../model/analysis.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>

struct DiffView {
    View* view;
};

typedef struct {
    CaptureSet set;
    Analysis analysis;
    uint16_t cursor; // selected bit position, 0..bit_count-1
} DiffViewModel;

#define MATRIX_TOP 13
#define MATRIX_MAX_ROWS 6
#define ROW_H 4

static void diff_view_draw(Canvas* canvas, void* ctx) {
    DiffViewModel* m = ctx;
    canvas_clear(canvas);

    const uint16_t bits = m->analysis.bit_count;
    if(m->set.count == 0 || bits == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No captures");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 42, AlignCenter, AlignCenter, "Capture or load 2+ signals");
        return;
    }

    // Header: protocol, bit length, capture count.
    canvas_set_font(canvas, FontSecondary);
    char head[64];
    snprintf(
        head,
        sizeof(head),
        "%s %ub x%u",
        m->set.items[0].protocol,
        (unsigned)bits,
        (unsigned)m->set.count);
    canvas_draw_str(canvas, 0, 9, head);

    // Column geometry: fit all bits across the 128px width.
    int cell_w = 128 / bits;
    if(cell_w < 1) cell_w = 1;
    if(cell_w > 6) cell_w = 6;
    int grid_w = cell_w * bits;
    int x0 = (128 - grid_w) / 2;

    // Bit matrix: one row per capture (most recent captures win if > MAX_ROWS).
    int rows = (int)m->set.count;
    int first_row = 0;
    if(rows > MATRIX_MAX_ROWS) {
        first_row = rows - MATRIX_MAX_ROWS;
        rows = MATRIX_MAX_ROWS;
    }
    for(int r = 0; r < rows; r++) {
        const Capture* c = &m->set.items[first_row + r];
        int y = MATRIX_TOP + r * ROW_H;
        for(uint16_t p = 0; p < bits; p++) {
            int x = x0 + p * cell_w;
            if(capture_bit(c, p)) {
                canvas_draw_box(canvas, x, y, cell_w > 1 ? cell_w - 1 : 1, ROW_H - 1);
            } else {
                // draw a faint dot so empty cells still read as a grid
                canvas_draw_dot(canvas, x, y + ROW_H - 2);
            }
        }
    }

    // Classification strip under the matrix.
    int strip_y = MATRIX_TOP + MATRIX_MAX_ROWS * ROW_H + 1;
    for(uint16_t p = 0; p < bits; p++) {
        int x = x0 + p * cell_w;
        int cw = cell_w > 1 ? cell_w - 1 : 1;
        if(m->analysis.classes[p] == BitClassChange) {
            canvas_draw_box(canvas, x, strip_y, cw, 3);
        } else {
            canvas_draw_line(canvas, x, strip_y + 2, x + cw - 1, strip_y + 2);
        }
    }

    // Cursor: invert the selected column across matrix + strip.
    {
        int x = x0 + m->cursor * cell_w;
        int cw = cell_w > 1 ? cell_w - 1 : 1;
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_box(canvas, x, MATRIX_TOP - 1, cw, strip_y + 4 - (MATRIX_TOP - 1));
        canvas_set_color(canvas, ColorBlack);
    }

    // Footer: verdict + selected-bit detail.
    char foot[64];
    const char* cls = (m->analysis.classes[m->cursor] == BitClassChange) ? "chg" : "fix";
    snprintf(
        foot,
        sizeof(foot),
        "b%u %s %u/%u",
        (unsigned)(m->cursor + 1),
        cls,
        (unsigned)m->analysis.ones[m->cursor],
        (unsigned)m->analysis.n);
    canvas_draw_str(canvas, 0, 63, foot);

    const char* verdict = analysis_verdict_str(m->analysis.verdict);
    canvas_draw_str_aligned(canvas, 128, 63, AlignRight, AlignBottom, verdict);
}

static bool diff_view_input(InputEvent* event, void* ctx) {
    DiffView* diff_view = ctx;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    with_view_model(
        diff_view->view,
        DiffViewModel * m,
        {
            if(m->analysis.bit_count > 0) {
                if(event->key == InputKeyLeft) {
                    if(m->cursor > 0) m->cursor--;
                    consumed = true;
                } else if(event->key == InputKeyRight) {
                    if(m->cursor + 1 < m->analysis.bit_count) m->cursor++;
                    consumed = true;
                }
            }
        },
        consumed);
    // Back (and anything else) falls through to the ViewDispatcher for navigation.
    return consumed;
}

DiffView* diff_view_alloc(void) {
    DiffView* diff_view = malloc(sizeof(DiffView));
    diff_view->view = view_alloc();
    view_allocate_model(diff_view->view, ViewModelTypeLocking, sizeof(DiffViewModel));
    view_set_context(diff_view->view, diff_view);
    view_set_draw_callback(diff_view->view, diff_view_draw);
    view_set_input_callback(diff_view->view, diff_view_input);
    return diff_view;
}

void diff_view_free(DiffView* diff_view) {
    furi_assert(diff_view);
    view_free(diff_view->view);
    free(diff_view);
}

View* diff_view_get_view(DiffView* diff_view) {
    furi_assert(diff_view);
    return diff_view->view;
}

void diff_view_set_data(DiffView* diff_view, const CaptureSet* set) {
    furi_assert(diff_view);
    with_view_model(
        diff_view->view,
        DiffViewModel * m,
        {
            m->set = *set;
            analysis_run(&m->set, &m->analysis);
            m->cursor = 0;
        },
        true);
}
