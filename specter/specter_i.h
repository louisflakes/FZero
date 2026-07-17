// Specter - real-time sub-GHz spectrum scope
// Shared internal definitions: app struct, view/scene ids, config constants.
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>

#define SPECTER_TAG "Specter"

// Tier window spans, fixed for v1 (Hz). OK cycles Coarse -> Mid -> Fine -> Coarse.
#define SPECTER_TIER_COARSE_SPAN_HZ 25000000UL
#define SPECTER_TIER_MID_SPAN_HZ 10000000UL
#define SPECTER_TIER_FINE_SPAN_HZ 2500000UL

typedef enum {
    SpecterTierCoarse,
    SpecterTierMid,
    SpecterTierFine,
} SpecterTier;

// A band is 1+ disjoint legal sub-GHz sub-ranges to scan/pan across.
// (Full = all three standard bands, back to back with gaps skipped.)
typedef struct {
    uint32_t start_hz;
    uint32_t end_hz;
} SpecterRange;

#define SPECTER_MAX_RANGES 3

typedef struct {
    SpecterRange ranges[SPECTER_MAX_RANGES];
    uint8_t range_count;
} SpecterBand;

typedef enum {
    SpecterBandSelect300_348,
    SpecterBandSelect387_464,
    SpecterBandSelect779_928,
    SpecterBandSelectFull,
} SpecterBandSelect;

typedef enum {
    SpecterDecayOff,
    SpecterDecayShort,
    SpecterDecayMedium,
    SpecterDecayLong,
} SpecterDecay;

// Returns the band definition for a selection (see model/bands.c).
const SpecterBand* specter_band_get(SpecterBandSelect select);
const char* specter_band_label(SpecterBandSelect select);
const char* specter_decay_label(SpecterDecay decay);

// Overall min/max frequency (Hz) spanned by a band (across all its ranges).
uint32_t specter_band_min_hz(const SpecterBand* band);
uint32_t specter_band_max_hz(const SpecterBand* band);

// dBm decay rate per rendered frame for a persistence setting (0 = off).
float specter_decay_rate(SpecterDecay decay);

typedef enum {
    SpecterViewWidget,
    SpecterViewVariableItemList,
    SpecterViewScope,
} SpecterView;

// Forward declarations (full types in their own headers, included by users).
typedef struct SpecterScopeView SpecterScopeView;
typedef struct SpecterScan SpecterScan;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Widget* widget;
    VariableItemList* variable_item_list;
    SpecterScopeView* scope_view;

    // Acquisition engine (app-lifetime; driven by the scan scene).
    SpecterScan* scan;
    FuriTimer* scan_timer; // GUI-thread redraw pump (scan scene only)

    // Config, chosen on the config screen; used by the scan scene.
    SpecterBandSelect band_select;
    SpecterDecay decay;
} Specter;
