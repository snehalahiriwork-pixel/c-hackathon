/**
 * @file main.c
 * @brief Scheduler loop and system integration.
 *
 * This is the entry point for the Vehicle ECU Simulator.
 * It implements a cyclic ECU-style execution model where every cycle
 * runs tasks in a fixed, deterministic order.
 *
 * ============================================================
 * SCHEDULER ORDER JUSTIFICATION (Mandatory Documentation)
 * ============================================================
 *
 * Execution order per cycle:
 *   1. Clear faults        - Reset per-cycle fault flags (counters preserved)
 *   2. Read inputs          - Get raw speed, temperature, gear, mode
 *   3. Validate inputs      - Range-check and sanitize; raise FAULT_INVALID_GEAR
 *   4. Update mode          - Check transition legality; raise FAULT_ILLEGAL_MODE
 *   5. Run control checks   - Detect overspeed, overtemp; set additional faults
 *   6. Update fault status  - Increment counters for all active faults
 *   7. Evaluate system state - Determine NORMAL / DEGRADED / SAFE
 *   8. Generate logs        - Print complete cycle summary
 *
 * WHY THIS ORDER:
 *   - Inputs must be validated BEFORE any logic uses them (garbage in = garbage out)
 *   - Mode must be determined BEFORE control checks (mode affects system behaviour)
 *   - All fault detections must complete BEFORE counters are updated
 *   - Counters must be updated BEFORE state evaluation (state depends on counts)
 *   - Logs must be LAST to capture the full picture of the cycle
 *
 * WHAT CAN GO WRONG IF ORDER CHANGES:
 *   - If control runs before validation: acts on corrupt/out-of-range data
 *   - If state evaluates before fault update: stale counter data, wrong escalation
 *   - If logs run before state evaluation: reports outdated state
 *   - If mode updates after control: control may apply wrong mode-dependent rules
 *   - If faults aren't cleared first: stale faults from previous cycle persist
 * ============================================================
 */

#include "types.h"
#include "input.h"
#include "mode.h"
#include "control.h"
#include "fault.h"
#include "state.h"
#include "log.h"
#include "perf.h"
#include <stdio.h>

/* Forward declaration — implemented in lib/ecu_control.c */
extern void run_all_test_cases(VehicleStatus *status, FaultStatus *faults);

int main(void)
{
    VehicleInput  input  = {0};
    VehicleStatus status = {0};
    FaultStatus   faults = {0};

    int choice = 0;

    printf("\n=== Vehicle ECU Simulator ===\n");
    printf("0 = Interactive mode (manual input)\n");
    printf("1 = Auto-run all 9 mandatory test cases\n");
    printf("Choice: ");
    (void)scanf("%d", &choice);

    if (choice == 1) {
        /* Test mode: skip initial init (tests reinitialize per test case) */
        run_all_test_cases(&status, &faults);
        return 0;
    }

    /* Interactive mode: initialize and show banner */
    init_system(&status, &faults);

    /* ---- Interactive Mode ---- */
    printf("\nInteractive mode started. Ctrl+C to exit.\n\n");

    while (1) {
        /* Step 1: Clear per-cycle fault flags (counters persist) */
        clear_all_faults(&faults);

        /* Step 2: Read raw inputs */
        read_inputs(&input);

        /* Step 3: Validate and sanitize inputs */
        validate_inputs(&input, &status, &faults);

        /* Step 4: Evaluate mode transition */
        update_mode(&status, &input, &faults);

        /* Step 5: Run control checks (overspeed, temp, gear) */
        run_control_checks(&input, &status, &faults);

        /* Step 6: Update fault counters */
        update_fault_status(&faults);

        /* Step 7: Evaluate system state (NORMAL/DEGRADED/SAFE) */
        evaluate_system_state(&status, &faults);

        /* Step 8: Generate cycle log */
        log_cycle_summary(&input, &status, &faults, NULL);
    }

    return 0;
}
