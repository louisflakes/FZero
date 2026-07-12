// DiffView: renders a set of captures as a bit matrix (one row per capture,
// one column per bit) plus a classification strip and field verdict. Fixed
// bits show up as clean vertical stripes; changing bits look noisy — the diff
// is meant to be legible at a glance.
#pragma once

#include <gui/view.h>
#include "../model/capture.h"

typedef struct DiffView DiffView;

// Invoked when the user presses Up in the diff view (jump to Craft & Call).
typedef void (*DiffCraftCallback)(void* ctx);

DiffView* diff_view_alloc(void);
void diff_view_free(DiffView* diff_view);
View* diff_view_get_view(DiffView* diff_view);

// Copy `set` into the view and (re)run the analysis. Resets the bit cursor.
void diff_view_set_data(DiffView* diff_view, const CaptureSet* set);

void diff_view_set_craft_callback(DiffView* diff_view, DiffCraftCallback cb, void* ctx);
