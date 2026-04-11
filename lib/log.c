/**
 * @file log.c
 * @brief Structured console logging implementation.
 *
 * Fault Reporting Priority (highest first):
 *   1. Critical Overheat   (FAULT_OVERTEMP_CRITICAL)
 *   2. Invalid Gear         (FAULT_INVALID_GEAR)
 *   2. Illegal Mode         (FAULT_ILLEGAL_MODE)
 *   3. Overspeed            (FAULT_OVERSPEED)
 *   4. High Temperature     (FAULT_OVERTEMP_HIGH)
 */

#include "log.h"
#include <stdio.h>

static const char* mode_to_str(Mode m)
{
    switch (m) {
        case MODE_OFF:         return "OFF";
        case MODE_ACC:         return "ACC";
        case MODE_IGNITION_ON: return "IGNITION_ON";
        case MODE_FAULT:       return "FAULT";
        default:               return "UNKNOWN";
    }
}

static const char* state_to_str(SystemState s)
{
    switch (s) {
        case STATE_NORMAL:   return "NORMAL";
        case STATE_DEGRADED: return "DEGRADED";
        case STATE_SAFE:     return "SAFE";
        default:             return "UNKNOWN";
    }
}

/**
 * @brief Write cycle summary to a given output stream.
 */
static void write_summary(FILE *out,
                           const VehicleInput *input,
                           const VehicleStatus *status,
                           const FaultStatus *faults)
{
    fprintf(out, "\n=== CYCLE SUMMARY ===\n");

    fprintf(out, "Input  : Speed=%d  Temp=%d  Gear=%d  Mode=%s\n",
            input->speed, input->temperature, input->gear,
            mode_to_str(input->requested_mode));

    fprintf(out, "Status : Mode=%s (prev=%s)  State=%s\n",
            mode_to_str(status->current_mode),
            mode_to_str(status->previous_mode),
            state_to_str(status->system_state));

    fprintf(out, "Faults : 0x%08X\n", faults->active_faults);

    fprintf(out, "Counters: OS=%u  CritT=%u  HighT=%u  InvGear=%u  IllMode=%u\n",
            faults->overspeed_counter,
            faults->overtemp_critical_counter,
            faults->overtemp_high_counter,
            faults->invalid_gear_counter,
            faults->illegal_mode_counter);

    /* Priority-ordered fault reporting */
    if (faults->active_faults & FAULT_OVERTEMP_CRITICAL) {
        fprintf(out, "  [PRIORITY 1] CRITICAL OVERHEAT\n");
    }
    if (faults->active_faults & FAULT_INVALID_GEAR) {
        fprintf(out, "  [PRIORITY 2] INVALID GEAR\n");
    }
    if (faults->active_faults & FAULT_ILLEGAL_MODE) {
        fprintf(out, "  [PRIORITY 2] ILLEGAL MODE TRANSITION\n");
    }
    if (faults->active_faults & FAULT_OVERSPEED) {
        fprintf(out, "  [PRIORITY 3] OVERSPEED\n");
    }
    if (faults->active_faults & FAULT_OVERTEMP_HIGH) {
        fprintf(out, "  [PRIORITY 4] HIGH TEMPERATURE\n");
    }

    if (faults->active_faults == 0U) {
        fprintf(out, "  [OK] No active faults\n");
    }

    fprintf(out, "======================\n");
}

void log_cycle_summary(const VehicleInput *input,
                       const VehicleStatus *status,
                       const FaultStatus *faults,
                       FILE *logfile)
{
    if (logfile == NULL) {
        /* Interactive mode: print to console */
        write_summary(stdout, input, status, faults);
    } else {
        /* Test mode: write to log file ONLY (console handled by test runner) */
        write_summary(logfile, input, status, faults);
    }
}

void log_cycle_timing(FILE *logfile, uint16_t cycle_num, const CycleTiming *timing)
{
    /* Write ONLY to log file (console summary handled by test runner) */
    if (logfile != NULL) {
        fprintf(logfile, "\n  [PERF] Cycle %u  CPU Cycles (excluding printf):\n", cycle_num);
        fprintf(logfile, "    validate_inputs       : %llu\n", (unsigned long long)timing->validate_inputs);
        fprintf(logfile, "    update_mode           : %llu\n", (unsigned long long)timing->update_mode);
        fprintf(logfile, "    run_control_checks    : %llu\n", (unsigned long long)timing->run_control_checks);
        fprintf(logfile, "    update_fault_status   : %llu\n", (unsigned long long)timing->update_fault_status);
        fprintf(logfile, "    evaluate_system_state : %llu\n", (unsigned long long)timing->evaluate_system_state);
        fprintf(logfile, "    TOTAL                 : %llu\n", (unsigned long long)timing->total);
    }
}
