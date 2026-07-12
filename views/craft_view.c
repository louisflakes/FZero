#include "craft_view.h"
#include <gui/canvas.h>
#include <furi.h>
#include <stdio.h>

struct CraftView {
    View* view;
    CraftSendStart on_start;
    CraftSendStop on_stop;
    void* cb_ctx;
};

typedef struct {
    char protocol[ROLLCALL_PROTO_LEN];
    char source_path[ROLLCALL_PATH_LEN];
    uint16_t bit_count;
    uint64_t seed_key; // original, for delta display
    uint64_t working_key; // edited value
    BitClass classes[ROLLCALL_MAX_BITS];
    uint16_t cursor; // bit position 0..bit_count-1
    bool sending; // TX active
    bool ok_holding; // OK-hold transmit in progress
} CraftViewModel;

static uint64_t craft_mask(uint16_t bits) {
    return (bits >= 64) ? ~0ull : ((1ull << bits) - 1);
}

static uint8_t craft_bit(uint64_t key, uint16_t bit_count, uint16_t pos) {
    return (uint8_t)((key >> (bit_count - 1u - pos)) & 1u);
}

// Is any bit of nibble `ni` (MSB-first) a changing bit?
static bool nibble_changing(const CraftViewModel* m, int ni) {
    for(int b = 0; b < 4; b++) {
        int pos = ni * 4 + b;
        if(pos >= m->bit_count) break;
        if(m->classes[pos] == BitClassChange) return true;
    }
    return false;
}

static void craft_view_draw(Canvas* canvas, void* ctx) {
    CraftViewModel* m = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Header: protocol + TX indicator.
    canvas_draw_str(canvas, 0, 9, m->protocol);
    if(m->sending) {
        canvas_draw_box(canvas, 104, 1, 24, 10);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 108, 9, "TX");
        canvas_set_color(canvas, ColorBlack);
    }

    // Hex value with changing nibbles boxed.
    int nibbles = (m->bit_count + 3) / 4;
    uint64_t v = m->working_key & craft_mask(m->bit_count);
    int x = 2;
    int y = 26;
    for(int ni = 0; ni < nibbles; ni++) {
        uint8_t nib = (v >> ((nibbles - 1 - ni) * 4)) & 0xF;
        char ch[2] = {(nib < 10) ? ('0' + nib) : ('A' + nib - 10), '\0'};
        int w = canvas_string_width(canvas, ch);
        if(nibble_changing(m, ni)) {
            canvas_draw_box(canvas, x - 1, y - 8, w + 1, 11);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y, ch);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, x, y, ch);
        }
        x += w + 2;
    }

    // Delta from the seed value (so you can see how far you've stepped).
    int64_t delta = (int64_t)(m->working_key & craft_mask(m->bit_count)) -
                    (int64_t)(m->seed_key & craft_mask(m->bit_count));
    char dbuf[20];
    snprintf(dbuf, sizeof(dbuf), "%+lld", (long long)delta);
    canvas_draw_str_aligned(canvas, 126, 26, AlignRight, AlignBottom, dbuf);

    // Bit row with cursor.
    int cell_w = 128 / m->bit_count;
    if(cell_w < 1) cell_w = 1;
    if(cell_w > 6) cell_w = 6;
    int grid_w = cell_w * m->bit_count;
    int x0 = (128 - grid_w) / 2;
    int by = 34;
    int cw = cell_w > 1 ? cell_w - 1 : 1;
    for(uint16_t p = 0; p < m->bit_count; p++) {
        int bx = x0 + p * cell_w;
        if(craft_bit(m->working_key, m->bit_count, p)) {
            canvas_draw_box(canvas, bx, by, cw, 6);
        } else if(cw >= 3) {
            canvas_draw_frame(canvas, bx, by, cw, 6);
        } else {
            canvas_draw_dot(canvas, bx, by + 4);
        }
    }
    // Cursor column.
    canvas_set_color(canvas, ColorXOR);
    canvas_draw_box(canvas, x0 + m->cursor * cell_w, by - 1, cw, 8);
    canvas_set_color(canvas, ColorBlack);

    // Footer.
    char foot[32];
    snprintf(
        foot,
        sizeof(foot),
        "b%u=%u",
        (unsigned)(m->cursor + 1),
        (unsigned)craft_bit(m->working_key, m->bit_count, m->cursor));
    canvas_draw_str(canvas, 0, 63, foot);
    canvas_draw_str_aligned(
        canvas, 128, 63, AlignRight, AlignBottom,
        m->sending ? "sending..." : "Up/Dn val  OK hold=send");
}

static bool craft_view_input(InputEvent* event, void* ctx) {
    CraftView* cv = ctx;
    bool consumed = false;

    // OK-hold transmit: Long starts, Release stops; a short OK-tap flips a bit.
    if(event->key == InputKeyOk) {
        if(event->type == InputTypeLong) {
            bool started = false;
            with_view_model(
                cv->view, CraftViewModel * m, { m->ok_holding = true; started = true; }, true);
            if(started && cv->on_start) {
                uint64_t key = 0;
                uint16_t bits = 0;
                char path[ROLLCALL_PATH_LEN];
                with_view_model(
                    cv->view,
                    CraftViewModel * m,
                    {
                        key = m->working_key;
                        bits = m->bit_count;
                        strncpy(path, m->source_path, ROLLCALL_PATH_LEN);
                    },
                    false);
                cv->on_start(cv->cb_ctx, path, key, bits);
            }
            return true;
        } else if(event->type == InputTypeRelease) {
            bool was_holding = false;
            with_view_model(
                cv->view,
                CraftViewModel * m,
                {
                    was_holding = m->ok_holding;
                    m->ok_holding = false;
                },
                true);
            if(was_holding && cv->on_stop) cv->on_stop(cv->cb_ctx);
            return was_holding;
        } else if(event->type == InputTypeShort) {
            with_view_model(
                cv->view,
                CraftViewModel * m,
                {
                    // flip the cursor bit
                    int kb = m->bit_count - 1 - m->cursor;
                    m->working_key ^= (1ull << kb);
                },
                true);
            return true;
        }
        return false;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    with_view_model(
        cv->view,
        CraftViewModel * m,
        {
            uint64_t mask = craft_mask(m->bit_count);
            switch(event->key) {
            case InputKeyUp:
                m->working_key = (m->working_key + 1) & mask; // +1 with carry
                consumed = true;
                break;
            case InputKeyDown:
                m->working_key = (m->working_key - 1) & mask; // -1 with borrow
                consumed = true;
                break;
            case InputKeyLeft:
                if(m->cursor > 0) m->cursor--;
                consumed = true;
                break;
            case InputKeyRight:
                if(m->cursor + 1 < m->bit_count) m->cursor++;
                consumed = true;
                break;
            default:
                break;
            }
        },
        consumed);
    return consumed;
}

CraftView* craft_view_alloc(void) {
    CraftView* cv = malloc(sizeof(CraftView));
    cv->on_start = NULL;
    cv->on_stop = NULL;
    cv->cb_ctx = NULL;
    cv->view = view_alloc();
    view_allocate_model(cv->view, ViewModelTypeLocking, sizeof(CraftViewModel));
    view_set_context(cv->view, cv);
    view_set_draw_callback(cv->view, craft_view_draw);
    view_set_input_callback(cv->view, craft_view_input);
    return cv;
}

void craft_view_free(CraftView* cv) {
    furi_assert(cv);
    view_free(cv->view);
    free(cv);
}

View* craft_view_get_view(CraftView* cv) {
    furi_assert(cv);
    return cv->view;
}

void craft_view_set_target(
    CraftView* cv,
    const Capture* seed,
    const Analysis* a,
    uint64_t start_key) {
    furi_assert(cv);
    with_view_model(
        cv->view,
        CraftViewModel * m,
        {
            strncpy(m->protocol, seed->protocol, ROLLCALL_PROTO_LEN);
            strncpy(m->source_path, seed->source_path, ROLLCALL_PATH_LEN);
            m->bit_count = seed->bit_count;
            m->seed_key = seed->key;
            m->working_key = start_key;
            memset(m->classes, 0, sizeof(m->classes));
            for(uint16_t p = 0; p < seed->bit_count && p < ROLLCALL_MAX_BITS; p++) {
                m->classes[p] = a->classes[p];
            }
            m->cursor = (a->change_lo >= 0) ? (uint16_t)a->change_lo : 0;
            m->sending = false;
            m->ok_holding = false;
        },
        true);
}

void craft_view_set_tx_callbacks(
    CraftView* cv,
    CraftSendStart on_start,
    CraftSendStop on_stop,
    void* ctx) {
    furi_assert(cv);
    cv->on_start = on_start;
    cv->on_stop = on_stop;
    cv->cb_ctx = ctx;
}

void craft_view_set_sending(CraftView* cv, bool sending) {
    furi_assert(cv);
    with_view_model(cv->view, CraftViewModel * m, { m->sending = sending; }, true);
}
