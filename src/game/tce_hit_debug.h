/* Private opt-in diagnostics over existing EV_RAILTRAIL fields. No SDK ABI
 * layout or event-number changes; ordinary rail trails retain types 0/1. */
#ifndef TCE_HIT_DEBUG_H
#define TCE_HIT_DEBUG_H
enum {
    TCE_ANTILAG_ENABLE = 1, TCE_ANTILAG_REWOUND = 2, TCE_ANTILAG_CURRENT = 4,
    TCE_HIT_DEBUG_CONTINUOUS = 4, TCE_HIT_DEBUG_LOG = 8,
    TCE_HIT_DEBUG_BOX_EVENT = 100,
    TCE_HIT_DEBUG_CURRENT = 0, TCE_HIT_DEBUG_HISTORY = 1, TCE_HIT_DEBUG_SHOT = 2,
    TCE_HIT_DEBUG_LAYERS = 3, TCE_HIT_DEBUG_LIFETIME = 1100,
    TCE_HIT_DEBUG_INTERVAL = 100, TCE_HIT_DEBUG_BATCH = 8
};
#endif
