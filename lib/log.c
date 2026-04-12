/**
 * @file log.c
 * @brief Structured console logging implementation — MISRA refactored.
 */

#include "log.h"
#include "system_io.h"

static const char* mode_to_str(Mode m)
{
    const char *s;
    switch (m) {
        case MODE_OFF:         s = "OFF";         break;
        case MODE_ACC:         s = "ACC";         break;
        case MODE_IGNITION_ON: s = "IGNITION_ON"; break;
        case MODE_FAULT:       s = "FAULT";       break;
        case MODE_UNKNOWN:
        default:               s = "UNKNOWN";     break;
    }
    return s;
}

static const char* state_to_str(SystemState s)
{
    const char *str;
    switch (s) {
        case STATE_NORMAL:   str = "NORMAL";   break;
        case STATE_DEGRADED: str = "DEGRADED"; break;
        case STATE_SAFE:     str = "SAFE";     break;
        case STATE_UNKNOWN:
        default:             str = "UNKNOWN";  break;
    }
    return str;
}

/**
 * @brief Write cycle summary to a given output stream.
 */
static void write_summary(sys_file_t out,
                           const VehicleInput *input,
                           const VehicleStatus *status,
                           const FaultStatus *faults)
{
    if ((input != NULL) && (status != NULL) && (faults != NULL)) {
        sys_fprintf(out, "\n=== CYCLE SUMMARY ===\n");

        sys_fprintf(out, "Input  : Speed=%d  Temp=%d  Gear=%d  Mode=%s\n",
                input->speed, input->temperature, input->gear,
                mode_to_str(input->requested_mode));

        sys_fprintf(out, "Status : Mode=%s (prev=%s)  State=%s\n",
                mode_to_str(status->current_mode),
                mode_to_str(status->previous_mode),
                state_to_str(status->system_state));

        sys_fprintf(out, "Faults : 0x%08X\n", faults->active_faults);

        sys_fprintf(out, "Counters: OS=%u  CritT=%u  HighT=%u  InvGear=%u  IllMode=%u\n",
                faults->overspeed_counter,
                faults->overtemp_critical_counter,
                faults->overtemp_high_counter,
                faults->invalid_gear_counter,
                faults->illegal_mode_counter);

        /* Priority-ordered fault reporting */
        if ((faults->active_faults & FAULT_OVERTEMP_CRITICAL) != 0U) {
            sys_fprintf(out, "  [PRIORITY 1] CRITICAL OVERHEAT\n");
        }
        if ((faults->active_faults & FAULT_INVALID_GEAR) != 0U) {
            sys_fprintf(out, "  [PRIORITY 2] INVALID GEAR\n");
        }
        if ((faults->active_faults & FAULT_ILLEGAL_MODE) != 0U) {
            sys_fprintf(out, "  [PRIORITY 2] ILLEGAL MODE TRANSITION\n");
        }
        if ((faults->active_faults & FAULT_OVERSPEED) != 0U) {
            sys_fprintf(out, "  [PRIORITY 3] OVERSPEED\n");
        }
        if ((faults->active_faults & FAULT_OVERTEMP_HIGH) != 0U) {
            sys_fprintf(out, "  [PRIORITY 4] HIGH TEMPERATURE\n");
        }

        if (faults->active_faults == 0U) {
            sys_fprintf(out, "  [OK] No active faults\n");
        }

        sys_fprintf(out, "======================\n");
    }
}

void log_cycle_summary(const VehicleInput *input,
                       const VehicleStatus *status,
                       const FaultStatus *faults,
                       sys_file_t logfile)
{
    write_summary(logfile, input, status, faults);
}

void log_cycle_timing(sys_file_t logfile, uint16_t cycle_num, const CycleTiming *timing)
{
    if ((logfile != NULL) && (timing != NULL)) {
        sys_fprintf(logfile, "\n  [PERF] Cycle %u  CPU Cycles (excluding printf):\n", (unsigned int)cycle_num);
        sys_fprintf(logfile, "    validate_inputs       : %llu\n", (unsigned long long)timing->validate_inputs);
        sys_fprintf(logfile, "    update_mode           : %llu\n", (unsigned long long)timing->update_mode);
        sys_fprintf(logfile, "    run_control_checks    : %llu\n", (unsigned long long)timing->run_control_checks);
        sys_fprintf(logfile, "    update_fault_status   : %llu\n", (unsigned long long)timing->update_fault_status);
        sys_fprintf(logfile, "    evaluate_system_state : %llu\n", (unsigned long long)timing->evaluate_system_state);
        sys_fprintf(logfile, "    TOTAL                 : %llu\n", (unsigned long long)timing->total);
    }
}
