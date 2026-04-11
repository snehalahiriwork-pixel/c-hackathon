/**
 * @file state.c
 * @brief Safe-state manager implementation.
 *
 * Escalation thresholds:
 *   SAFE triggered by:
 *     - 2+ simultaneous active faults in one cycle
 *     - overtemp_critical_counter >= 2
 *     - overspeed_counter >= 3
 *     - illegal_mode_counter >= 2
 *
 * Recovery from SAFE:
 *     - Mode must be MODE_OFF (simulates power-cycle reset)
 *     - No active faults in the current cycle
 */

#include "state.h"
#include "perf.h"
#include <stdio.h>

/**
 * @brief Count the number of set bits in a fault flags word.
 */
static uint8_t count_active_faults(FaultFlags flags)
{
    uint8_t count = 0U;
    while (flags != 0U) {
        count += (uint8_t)(flags & 1U);
        flags >>= 1U;
    }
    return count;
}

void init_system(VehicleStatus *status, FaultStatus *faults)
{
    status->current_mode           = MODE_OFF;
    status->previous_mode          = MODE_OFF;
    status->system_state           = STATE_NORMAL;
    status->last_valid_speed       = 0;
    status->last_valid_temperature = 0;
    status->last_valid_gear        = 0;
    status->last_valid_mode        = MODE_OFF;

    faults->active_faults            = 0U;
    faults->overspeed_counter        = 0U;
    faults->overtemp_critical_counter = 0U;
    faults->overtemp_high_counter    = 0U;
    faults->invalid_gear_counter     = 0U;
    faults->illegal_mode_counter     = 0U;

    if (!g_suppress_io) {
        printf("[SYSTEM] ECU initialized - OFF mode, NORMAL state\n");
    }
}

void evaluate_system_state(VehicleStatus *status, const FaultStatus *faults)
{
    SystemState new_state;
    const char *reason = "";
    uint8_t active = count_active_faults(faults->active_faults);

    /* ---- SAFE State Latch ---- */
    /* Once in SAFE, system stays SAFE until MODE_OFF reset with no faults */
    if (status->system_state == STATE_SAFE) {
        if (status->current_mode == MODE_OFF && faults->active_faults == 0U) {
            new_state = STATE_NORMAL;
            reason = "recovery via MODE_OFF reset, no active faults";
        } else {
            new_state = STATE_SAFE;
            reason = "SAFE state latched (requires MODE_OFF reset with no faults)";
            /* Only log if it wasn't already SAFE (first latch message) */
            if (new_state == status->system_state) {
                return;  /* already SAFE, no transition to log */
            }
        }
    }
    /* ---- Normal Escalation Rules ---- */
    else if (faults->active_faults == 0U) {
        new_state = STATE_NORMAL;
        reason = "no active faults";
    }
    else if (active >= 2U) {
        new_state = STATE_SAFE;
        reason = "2+ simultaneous critical faults this cycle";
    }
    else if (faults->overtemp_critical_counter >= 2U) {
        new_state = STATE_SAFE;
        reason = "persistent critical overheat (counter >= 2)";
    }
    else if (faults->overspeed_counter >= 3U) {
        new_state = STATE_SAFE;
        reason = "persistent overspeed (counter >= 3)";
    }
    else if (faults->illegal_mode_counter >= 2U) {
        new_state = STATE_SAFE;
        reason = "persistent illegal mode transitions (counter >= 2)";
    }
    else {
        new_state = STATE_DEGRADED;
        reason = "one active fault or elevated counter";
    }

    /* ---- Log state transition with reason ---- */
    if (new_state != status->system_state) {
        if (!g_suppress_io) {
            printf("[STATE] %d -> %d | Reason: %s\n",
                   (int)status->system_state, (int)new_state, reason);
        }
    }

    status->system_state = new_state;
}
