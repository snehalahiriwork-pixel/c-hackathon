/**
 * @file log.h
 * @brief Structured console logging interface.
 *
 * Generates cycle summaries including inputs, mode, state,
 * fault flags, counters, and priority-ordered fault reporting.
 * Supports dual output: console always, file optionally.
 */

#ifndef LOG_H_
#define LOG_H_

#include "types.h"
#include "perf.h"
#include <stdio.h>

/**
 * @brief Print a complete cycle summary.
 *
 * Outputs to console always. If logfile is non-NULL, also writes to file.
 * Faults are reported in priority order:
 *   Priority 1: Critical Overheat
 *   Priority 2: Invalid Gear / Illegal Mode
 *   Priority 3: Overspeed
 *   Priority 4: High Temperature
 *
 * @param input    Pointer to VehicleInput (current cycle values).
 * @param status   Pointer to VehicleStatus (mode, state).
 * @param faults   Pointer to FaultStatus (flags, counters).
 * @param logfile  FILE pointer for file output (NULL = console only).
 */
void log_cycle_summary(const VehicleInput *input,
                       const VehicleStatus *status,
                       const FaultStatus *faults,
                       FILE *logfile);

/**
 * @brief Log CPU cycle timing for one scheduler cycle.
 * Writes per-step and total CPU cycle counts to console and/or logfile.
 * @param logfile   FILE pointer for file output (NULL = console only).
 * @param cycle_num Cycle number within the current test case.
 * @param timing    Pointer to CycleTiming with measured values.
 */
void log_cycle_timing(FILE *logfile, uint16_t cycle_num, const CycleTiming *timing);

#endif /* LOG_H_ */
