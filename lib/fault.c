/**
 * @file fault.c
 * @brief Fault manager implementation.
 *
 * Manages fault flags using bitwise operations and maintains
 * per-fault counters for persistence tracking across cycles.
 */

#include "fault.h"

void set_fault(FaultStatus *faults, uint32_t fault_bit)
{
    faults->active_faults |= fault_bit;
}

void clear_fault(FaultStatus *faults, uint32_t fault_bit)
{
    faults->active_faults &= ~fault_bit;
}

void clear_all_faults(FaultStatus *faults)
{
    faults->active_faults = FAULT_NONE;
    /* Note: counters are NOT cleared — they track persistence across cycles */
}

void increment_fault_counter(FaultStatus *faults, uint32_t fault_bit)
{
    if (fault_bit == FAULT_OVERSPEED) {
        faults->overspeed_counter++;
    }
    if (fault_bit == FAULT_OVERTEMP_CRITICAL) {
        faults->overtemp_critical_counter++;
    }
    if (fault_bit == FAULT_OVERTEMP_HIGH) {
        faults->overtemp_high_counter++;
    }
    if (fault_bit == FAULT_INVALID_GEAR) {
        faults->invalid_gear_counter++;
    }
    if (fault_bit == FAULT_ILLEGAL_MODE) {
        faults->illegal_mode_counter++;
    }
}

void update_fault_status(FaultStatus *faults)
{
    /* Increment counters for every fault that is active THIS cycle.
     * This must run AFTER all detection modules (validate, mode, control). */
    if (faults->active_faults & FAULT_OVERSPEED) {
        increment_fault_counter(faults, FAULT_OVERSPEED);
    }
    if (faults->active_faults & FAULT_OVERTEMP_CRITICAL) {
        increment_fault_counter(faults, FAULT_OVERTEMP_CRITICAL);
    }
    if (faults->active_faults & FAULT_OVERTEMP_HIGH) {
        increment_fault_counter(faults, FAULT_OVERTEMP_HIGH);
    }
    if (faults->active_faults & FAULT_INVALID_GEAR) {
        increment_fault_counter(faults, FAULT_INVALID_GEAR);
    }
    if (faults->active_faults & FAULT_ILLEGAL_MODE) {
        increment_fault_counter(faults, FAULT_ILLEGAL_MODE);
    }
}
