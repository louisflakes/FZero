#include "../specter_i.h"

// Standard sub-GHz ISM sub-bands the CC1101 (and Flipper's region tables)
// generally cover. "Full" is these three back-to-back, not one contiguous
// span -- the gaps between them are not scanned.
static const SpecterBand specter_band_300_348 = {
    .ranges = {{300000000UL, 348000000UL}},
    .range_count = 1,
};
static const SpecterBand specter_band_387_464 = {
    .ranges = {{387000000UL, 464000000UL}},
    .range_count = 1,
};
static const SpecterBand specter_band_779_928 = {
    .ranges = {{779000000UL, 928000000UL}},
    .range_count = 1,
};
static const SpecterBand specter_band_full = {
    .ranges =
        {{300000000UL, 348000000UL}, {387000000UL, 464000000UL}, {779000000UL, 928000000UL}},
    .range_count = 3,
};

const SpecterBand* specter_band_get(SpecterBandSelect select) {
    switch(select) {
    case SpecterBandSelect300_348:
        return &specter_band_300_348;
    case SpecterBandSelect387_464:
        return &specter_band_387_464;
    case SpecterBandSelect779_928:
        return &specter_band_779_928;
    case SpecterBandSelectFull:
    default:
        return &specter_band_full;
    }
}

const char* specter_band_label(SpecterBandSelect select) {
    switch(select) {
    case SpecterBandSelect300_348:
        return "300-348 MHz";
    case SpecterBandSelect387_464:
        return "387-464 MHz";
    case SpecterBandSelect779_928:
        return "779-928 MHz";
    case SpecterBandSelectFull:
    default:
        return "Full (all bands)";
    }
}

const char* specter_decay_label(SpecterDecay decay) {
    switch(decay) {
    case SpecterDecayOff:
        return "Off (no persistence)";
    case SpecterDecayShort:
        return "Short";
    case SpecterDecayMedium:
        return "Medium";
    case SpecterDecayLong:
    default:
        return "Long";
    }
}

uint32_t specter_band_min_hz(const SpecterBand* band) {
    return band->ranges[0].start_hz;
}

uint32_t specter_band_max_hz(const SpecterBand* band) {
    return band->ranges[band->range_count - 1].end_hz;
}

float specter_decay_rate(SpecterDecay decay) {
    switch(decay) {
    case SpecterDecayOff:
        return 0.0f; // no persistence marker
    case SpecterDecayShort:
        return 6.0f;
    case SpecterDecayMedium:
        return 3.0f;
    case SpecterDecayLong:
    default:
        return 1.0f;
    }
}
