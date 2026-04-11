/**
 * @file control.c
 * @brief Control logic implementation.
 *
 * IMPORTANT: This module does NOT clear active_faults.
 * Faults set by prior steps (validate_inputs, update_mode) must be preserved.
 * The scheduler clears faults at the start of each cycle.
 *
 * Priority order for reporting (handled in log module):
 *   1. Critical Overheat   (FAULT_OVERTEMP_CRITICAL)
 *   2. Invalid Gear / Mode (FAULT_INVALID_GEAR, FAULT_ILLEGAL_MODE)
 *   3. Overspeed           (FAULT_OVERSPEED)
 *   4. High Temperature    (FAULT_OVERTEMP_HIGH)
 */

#include "control.h"
#include "fault.h"

void run_control_checks(const VehicleInput *input, VehicleStatus *status, FaultStatus *faults)
{
    (void)status;  /* unused - kept for interface consistency (MISRA Rule 2.7) */

    /*
     * NOTE: Do NOT clear faults->active_faults here.
     * Upstream modules (validate_inputs, update_mode) may have already
     * set fault flags that must persist through the rest of the cycle.
     * Clearing here would erase FAULT_INVALID_GEAR and FAULT_ILLEGAL_MODE.
     */

    /* ---- Overspeed Check ---- */
    if (input->speed > OVERSPEED_THRESHOLD) {
        set_fault(faults, FAULT_OVERSPEED);
    }

    /* ---- Temperature Check (critical takes precedence over high) ---- */
    if (input->temperature > CRITICAL_TEMP_THRESHOLD) {
        set_fault(faults, FAULT_OVERTEMP_CRITICAL);
    } else if (input->temperature > HIGH_TEMP_THRESHOLD) {
        set_fault(faults, FAULT_OVERTEMP_HIGH);
    }

    /* ---- Gear Validity Check ---- */
    /* Note: validate_inputs already catches invalid gear and corrects it.
     * This check acts as a safety net for any uncorrected path. */
    if (input->gear < GEAR_MIN || input->gear > GEAR_MAX) {
        set_fault(faults, FAULT_INVALID_GEAR);
    }
}
