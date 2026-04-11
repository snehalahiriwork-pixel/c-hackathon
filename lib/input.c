/**
 * @file input.c
 * @brief Input handling and validation implementation.
 */

#include "input.h"
#include "fault.h"
#include "perf.h"
#include <stdio.h>

void read_inputs(VehicleInput *input)
{
    int temp_val;  /* temp variable to avoid scanf type mismatch */

    printf("\n--- New ECU Cycle ---\n");

    printf("Enter speed (0-200): ");
    temp_val = 0;
    (void)scanf("%d", &temp_val);
    input->speed = (int16_t)temp_val;

    printf("Enter temperature (-40 to 150): ");
    temp_val = 0;
    (void)scanf("%d", &temp_val);
    input->temperature = (int16_t)temp_val;

    printf("Enter gear (0-5): ");
    temp_val = 0;
    (void)scanf("%d", &temp_val);
    input->gear = (int8_t)temp_val;

    printf("Enter requested mode (0=OFF, 1=ACC, 2=IGNITION_ON, 3=FAULT): ");
    temp_val = 0;
    (void)scanf("%d", &temp_val);
    input->requested_mode = (Mode)temp_val;
}

void validate_inputs(VehicleInput *input, VehicleStatus *status, FaultStatus *faults)
{
    /* ---- Speed: 0 to 200 ---- */
    if (input->speed < SPEED_MIN || input->speed > SPEED_MAX) {
        if (!g_suppress_io) {
            printf("[INPUT] Invalid speed %d -> using last valid %d\n",
                   input->speed, status->last_valid_speed);
        }
        input->speed = status->last_valid_speed;
    } else {
        status->last_valid_speed = input->speed;
    }

    /* ---- Temperature: -40 to 150 ---- */
    if (input->temperature < TEMP_MIN || input->temperature > TEMP_MAX) {
        if (!g_suppress_io) {
            printf("[INPUT] Invalid temperature %d -> using last valid %d\n",
                   input->temperature, status->last_valid_temperature);
        }
        input->temperature = status->last_valid_temperature;
    } else {
        status->last_valid_temperature = input->temperature;
    }

    /* ---- Gear: 0 to 5 ---- */
    if (input->gear < GEAR_MIN || input->gear > GEAR_MAX) {
        set_fault(faults, FAULT_INVALID_GEAR);   /* raise fault BEFORE correcting */
        if (!g_suppress_io) {
            printf("[INPUT] Invalid gear %d -> using last valid %d\n",
                   input->gear, status->last_valid_gear);
        }
        input->gear = status->last_valid_gear;
    } else {
        status->last_valid_gear = input->gear;
    }

    /* ---- Requested Mode: valid enum only ---- */
    if (input->requested_mode < MODE_OFF || input->requested_mode > MODE_FAULT) {
        if (!g_suppress_io) {
            printf("[INPUT] Invalid mode %d -> using last valid %d\n",
                   input->requested_mode, status->last_valid_mode);
        }
        input->requested_mode = status->last_valid_mode;
    } else {
        status->last_valid_mode = input->requested_mode;
    }
}
