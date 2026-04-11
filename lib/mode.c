/**
 * @file mode.c
 * @brief Mode controller implementation.
 *
 * Legal transitions:
 *   OFF         -> ACC              (allowed)
 *   ACC         -> IGNITION_ON      (allowed)
 *   ACC         -> OFF              (allowed)
 *   IGNITION_ON -> ACC              (allowed)
 *   IGNITION_ON -> OFF              (allowed)
 *   OFF         -> IGNITION_ON      (ILLEGAL)
 *   FAULT       -> any              (LOCKED until reset)
 *   same -> same                    (always allowed)
 */

#include "mode.h"
#include "fault.h"
#include "perf.h"
#include <stdio.h>

/**
 * @brief Check if a mode transition is legal.
 * @return 1 if legal, 0 if illegal.
 */
static int is_legal_transition(Mode current, Mode requested)
{
    if (current == requested) {
        return 1;   /* staying in same mode is always legal */
    }

    switch (current) {
        case MODE_OFF:
            return (requested == MODE_ACC) ? 1 : 0;

        case MODE_ACC:
            return (requested == MODE_IGNITION_ON || requested == MODE_OFF) ? 1 : 0;

        case MODE_IGNITION_ON:
            return (requested == MODE_ACC || requested == MODE_OFF) ? 1 : 0;

        case MODE_FAULT:
            return 0;   /* FAULT mode is locked - no transitions allowed */

        default:
            return 0;
    }
}

void update_mode(VehicleStatus *status, const VehicleInput *input, FaultStatus *faults)
{
    status->previous_mode = status->current_mode;

    if (is_legal_transition(status->current_mode, input->requested_mode)) {
        status->current_mode = input->requested_mode;
    } else {
        if (!g_suppress_io) {
            printf("[MODE] Illegal transition %d -> %d -> forcing FAULT mode\n",
                   (int)status->current_mode, (int)input->requested_mode);
        }
        set_fault(faults, FAULT_ILLEGAL_MODE);
        status->current_mode = MODE_FAULT;
    }
}
